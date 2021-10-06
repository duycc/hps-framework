//===--------------------------- app/HPServer.cpp - HP-Server -------------------------------------------*- C++ -*-===//
// brief :
//   Program entry function
//
// author: YongDu
// date  : 2021-09-12
//===--------------------------------------------------------------------------------------------------------------===//

#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "hps_c_conf.h" // 配置文件处理
#include "hps_c_memory.h"
#include "hps_c_socket.h"
#include "hps_func.h" // 函数声明
#include "hps_macro.h"
#include "hps_c_threadpool.h"
#include "hps_c_crc32.h"
#include "hps_c_slogic.h"

char **g_os_argv;         // 原始命令行参数数组
int g_os_argc;            // 启动参数个数
size_t g_argvneedmem = 0; // 启动参数内存大小
size_t g_envneedmem = 0;  // 相关环境变量总大小
char *gp_envmem = NULL;   // 环境变量内存新位置

pid_t hps_pid;        // 当前进程 id
pid_t hps_parent;     // 当前进程父进程 id
int hps_process;      // 进程类型
int g_daemonized = 0; // 是否以守护进程方式运行
int g_stopEvent;

CLogicSocket g_socket;    // 全局 socket 管理
CThreadPool g_threadpool; // 线程池

sig_atomic_t hps_reap; // 标识子进程状态变化

static void free_resource(); // 内存释放

int main(int argc, char *const *argv) {
  int exit_code = 0; // 0     正常退出
                     // 1, -1 异常退出
                     // 2     系统找不到指定文件

  g_stopEvent = 0;

  hps_pid = getpid();
  hps_parent = getppid();

  g_argvneedmem = 0;
  for (int i = 0; i < argc; ++i) {
    g_argvneedmem += strlen(argv[i] + 1);
  }
  for (int i = 0; environ[i]; ++i) {
    g_envneedmem += strlen(environ[i]) + 1;
  }

  g_os_argc = argc;
  g_os_argv = (char **)argv;

  hps_log.fd = -1;
  hps_process = HPS_PROCESS_MASTER;
  hps_reap = 0;

  do { // do...while(false) 方便退出释放资源
    // 加载配置文件
    CConfig *p_config = CConfig::GetInstance();
    if (p_config->Load("HPServer.conf") == false) {
      hps_log_init();
      hps_log_stderr(0, "配置文件[%s]载入失败，退出！", "HPServer.conf");
      exit_code = 2;
      break;
    }

    // 初始化内存分配函数
    CMemory::GetInstance();

    CCRC32::GetInstance();

    // 初始化日志
    hps_log_init();

    // 注册信号处理函数
    if (hps_init_signals() != 0) {
      exit_code = 1;
      break;
    }

    if (g_socket.Initialize() == false) {
      exit_code = 1;
      break;
    }

    hps_init_setproctitle(); // 移动环境变量

    // 创建守护进程
    if (p_config->GetIntDefault("Daemon", 0) == 1) {
      int cdaemonresult = hps_daemon();
      if (cdaemonresult == -1) {
        exit_code = 1;
        break;
      }
      if (cdaemonresult == 1) {
        free_resource();
        return 0; // 原始的父进程，需要（正常）退出
      }
      g_daemonized = 1; // 守护进程模式的 master
    }

    // 创建 worker 子进程，并执行具体业务逻辑
    hps_master_process_cycle();
  } while (false);

  hps_log_stderr(0, "程序退出，再见了！");
  free_resource();
  return exit_code;
}

void free_resource() {
  // 移动环境变量所分配的内存需要释放
  if (gp_envmem) {
    delete[] gp_envmem;
    gp_envmem = NULL;
  }

  // 关闭日志文件
  if (hps_log.fd != STDERR_FILENO && hps_log.fd != -1) {
    close(hps_log.fd);
    hps_log.fd = -1; // 标记，防止重复close
  }
  return;
}
