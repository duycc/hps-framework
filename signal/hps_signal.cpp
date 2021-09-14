/**
 * @file     hps_signal.cpp
 * @brief
 * @author   YongDu
 * @date     2021-09-13
 */

#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include "hps_func.h"
#include "hps_global.h"
#include "hps_macro.h"

//===----------------------------- 信号相关函数 ------------------------------===//

// 信号结构体
typedef struct {
  int         signo;   // 编号: 1
  const char *signame; // 对应的名称: SIGHUP

  // 信号处理函数指针
  void (*handler)(int signo, siginfo_t *siginfo, void *ucontext);
} hps_signal_t;

// 信号处理函数
static void hps_signal_handler(int signo, siginfo_t *siginfo, void *ucontext);

// 获取子进程的结束状态，防止单独kill子进程时子进程变成僵尸进程
static void hps_process_get_status(void);

// 要处理的相关信号
hps_signal_t signals[] = {
    {SIGHUP, "SIGHUP", hps_signal_handler},
    {SIGINT, "SIGINT", hps_signal_handler},
    {SIGTERM, "SIGTERM", hps_signal_handler},
    {SIGCHLD, "SIGCHLD", hps_signal_handler}, // 子进程退出时，父进程会收到此信号
    {SIGQUIT, "SIGQUIT", hps_signal_handler},
    {SIGIO, "SIGIO", hps_signal_handler}, // 通用异步I/O信号
    {SIGSYS, "SIGSYS, SIG_IGN", NULL},    // 无效系统调用，不忽略该进程会被操作系统kill掉
    {0, NULL, NULL}                       // 特殊标记
};

/**
 * @brief 注册信号处理函数
 *
 * @return int 0 成功 -1 失败
 */
int hps_init_signals() {
  hps_signal_t *   sig; // 指向 signals
  struct sigaction sa;  // 注册信号使用

  for (sig = signals; sig->signo != 0; ++sig) { // signals 特殊标记 0
    memset(&sa, 0, sizeof(struct sigaction));

    if (sig->handler) { // 存在信号处理函数
      sa.sa_sigaction = sig->handler;
      sa.sa_flags = SA_SIGINFO; // 使信号处理函数生效
    } else {
      sa.sa_handler = SIG_IGN; // 没有信号处理函数的信号一律忽略，防止系统kill进程
    }

    sigemptyset(&sa.sa_mask); // 清空信号集合，表示不阻塞任何信号

    // 绑定信号处理动作
    if (sigaction(sig->signo, &sa, NULL) == -1) {
      hps_log_error_core(HPS_LOG_EMERG, errno, "sigaction(%s) failed", sig->signame);
      return -1;
    }
  }
  return 0;
}

/**
 * @brief 信号处理函数
 *
 * @param signo
 * @param siginfo 信号产生相关信息
 * @param ucontext
 */
static void hps_signal_handler(int signo, siginfo_t *siginfo, void *ucontext) {
  hps_signal_t *sig;    // signals
  char *        action; // 信号相关信息描述

  for (sig = signals; sig->signo != 0; ++sig) {
    // 这个循环有点冗余，没有该信号该如何处理？暂时待修复吧
    if (sig->signo == signo) {
      break;
    }
  }
  action = (char *)"";
  if (hps_process == HPS_PROCESS_MASTER) {
    // master 管理进程
    switch (signo) {
    case SIGCHLD:   // 子进程退出
      hps_reap = 1; // 标记子进程状态，是否需要重新创建一个子进程
      break;

      // 后续完善其它信号...

    default:
      break;
    }
  } else if (hps_process == HPS_PROCESS_WORKER) {
    // TODO
  } else {
    // TODO
  }

  if (siginfo && siginfo->si_pid) { // si_pid 发送此信号的进程
    hps_log_error_core(HPS_LOG_NOTICE, 0, "signal %d (%s) received from %P%s", signo, sig->signame, siginfo->si_pid,
                       action);
  } else {
    hps_log_error_core(HPS_LOG_NOTICE, 0, "signal %d (%s) received %s", signo, sig->signame, action);
  }

  if (signo == SIGCHLD) { // 处理子进程
    hps_process_get_status();
  }
  return;
}

/**
 * @brief 获取子进程的结束状态，防止单独kill子进程时子进程变成僵尸进程
 *
 */
static void hps_process_get_status(void) {
  pid_t pid;
  int   status;
  int   err;
  int   one = 0; // 标记是否处理过

  while (true) {
    pid = waitpid(-1, &status, WNOHANG); // -1 等待任何子进程
                                         // status 保存子进程状态信息
                                         // 不要阻塞，立即返回

    if (pid == 0) {
      return; // 没有收集到已退出的子进程，返回
    }
    if (pid == -1) {
      err = errno;
      if (err == EINTR) { // 调用被信号中断
        continue;
      }
      if (err == ECHILD && one) { // 非调用进程的子进程或者没有子进程
        return;
      }
      if (err == ECHILD) {
        hps_log_error_core(HPS_LOG_INFO, err, "waitpid() failed!");
        return;
      }
      hps_log_error_core(HPS_LOG_ALERT, err, "waitpid() failed!");
      return;
    }
    // 收集到子进程ID(waitpid返回)，日志记录子进程退出信息
    one = 1;
    if (WTERMSIG(status)) { // 获取使子进程终止的信号编号
      hps_log_error_core(HPS_LOG_ALERT, 0, "pid = %P exited on signal %d!", pid, WTERMSIG(status));
    } else {
      hps_log_error_core(HPS_LOG_NOTICE, 0, "pid = %P exited with code %d!", pid, WEXITSTATUS(status));
    }
  }
  return;
}
