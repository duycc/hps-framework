//===--------------------------- proc/hps_process_cycle.cpp - [HP-Server] -------------------------------*- C++ -*-===//
// brief :
//   Process related
//
// author: YongDu
// date  : 2021-09-14
//===--------------------------------------------------------------------------------------------------------------===//

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hps_c_conf.h"
#include "hps_func.h"
#include "hps_macro.h"

// master 创建 worker 子进程，并初始化子进程
static void hps_start_worker_processes(int threadnums);
static int hps_spawn_process(int threadnums, const char* pprocname);
static void hps_worker_process_cycle(int inum, const char* pprocname);
static void hps_worker_process_init(int inum);

static char master_process[] = "master process"; // master 进程名

// 创建 worker 子进程
void hps_master_process_cycle() {
    sigset_t set; // 信号集
    sigemptyset(&set);

    // 屏蔽以下信号，防止执行临界区代码时被信号中断
    sigaddset(&set, SIGCHLD);  // 子进程状态改变
    sigaddset(&set, SIGALRM);  // 定时器超时
    sigaddset(&set, SIGIO);    // 异步I/O
    sigaddset(&set, SIGINT);   // 终端中断符
    sigaddset(&set, SIGHUP);   // 连接断开
    sigaddset(&set, SIGUSR1);  // 用户定义信号
    sigaddset(&set, SIGUSR2);  // 用户定义信号
    sigaddset(&set, SIGWINCH); // 终端窗口大小改变
    sigaddset(&set, SIGTERM);  // 终止
    sigaddset(&set, SIGQUIT);  // 终端退出符

    if (sigprocmask(SIG_BLOCK, &set, NULL) == -1) {
        hps_log_error_core(HPS_LOG_ALERT, errno, "hps_master_process_cycle()中sigprocmask()执行失败！");
    } // 失败影响不大，不退出继续执行

    // 设置 master process 标题
    size_t size;
    size = sizeof(master_process);
    size += g_argvneedmem;
    if (size < 1000) {
        char title[1000] = {0};
        strcpy(title, (const char*)master_process);
        hps_setproctitle(title);
        hps_log_error_core(HPS_LOG_NOTICE, 0, "%s %P 【master】进程启动并执行......!", title, hps_pid);
    }

    // 准备创建 worker 子进程
    CConfig* p_config = CConfig::GetInstance();

    int workprocess = p_config->GetIntDefault("WorkerProcesses", 1); // 默认创建 1 个

    // 开启子进程
    hps_start_worker_processes(workprocess);

    // only master can walk here.
    sigemptyset(&set);
    for (;;) {
        sigsuspend(&set); // sigsuspend()
                          // a. 以指定参数设置新的mask，并阻塞当前进程（信号驱动）
                          // b. 收到信号，恢复原先的信号屏蔽，sigpromask()所设置的信号集
                          // c. 执行相应信号处理程序
                          // d. 信号处理程序返回，sigsuspend()返回，继续向下执行
        sleep(1);
        // hps_log_stderr(0, "master 进程 %P 休息1秒", hps_pid);

        // 根据实际需求完善...
    }
    return;
}

/**
 * @brief 创建指定数量子进程
 *
 * @param threadnums
 */
static void hps_start_worker_processes(int threadnums) {
    for (int i = 0; i < threadnums; ++i) {
        hps_spawn_process(i, "worker process");
    }
    return;
}

/**
 * @brief 具体创建一个子进程
 *
 * @param inum 进程编号
 * @param pprocname 进程名
 * @return int
 */
static int hps_spawn_process(int inum, const char* pprocname) {
    pid_t pid;
    pid = fork();
    switch (pid) {
    case -1:
        hps_log_error_core(HPS_LOG_ALERT, errno, "hps_spawn_process() fork()产生子进程num=%d, procname=\"%s\"失败!");
        return -1;
    case 0:
        // 子进程分支
        hps_parent = hps_pid; // 子进程，原来的 pid 为子进程的 ppid
        hps_pid = getpid();
        hps_worker_process_cycle(inum, pprocname); // worker 子进程业务流，且不再继续向下执行
        break;
    default:
        // 父进程分支
        break;
    }
    // 父进程若有需要，可在此扩展...
    return pid;
}

/**
 * @brief 子进程具体逻辑，处理网络事件，定时器事件及提供web服务
 *
 * @param inum
 * @param pprocname
 */

static void hps_worker_process_cycle(int inum, const char* pprocname) {
    hps_process = HPS_PROCESS_WORKER;

    hps_worker_process_init(inum);
    hps_setproctitle(pprocname);
    hps_log_error_core(HPS_LOG_NOTICE, 0, "%s %P 【worker】进程启动并执行......!", pprocname, hps_pid);

    // 测试代码，测试线程池的关闭
    // sleep(5)
    // g_threadpool.StopAll()
    // 测试Create()后立即释放的效果

    for (;;) {
        // hps_log_error_core(0, 0, "worker process %d pid = %P", inum, hps_pid);
        // sleep(1);
        hps_process_events_and_timers(); // 处理网络事件和定时器事件
    }

    g_threadpool.StopAll();
    g_socket.Shutdown_subproc(); // socket需要释放的资源
    return;
}

// 子进程初始化工作
static void hps_worker_process_init(int inum) {
    sigset_t set;
    sigemptyset(&set);

    if (sigprocmask(SIG_SETMASK, &set, NULL) == -1) {
        // 取消信号屏蔽，接受任何信号
        hps_log_error_core(HPS_LOG_ALERT, errno, "hps_worker_process_init()中sigprocmask()失败!");
    }

    // 线程池代码，率先创建
    CConfig* p_config = CConfig::GetInstance();
    int tmpthreadnums = p_config->GetIntDefault("ProcMsgRecvWorkThreadCount", 5);
    if (g_threadpool.Create(tmpthreadnums) == false) {
        exit(-2);
    }
    if (g_socket.Initialize_subproc() == false) {
        exit(-2);
    }
    g_socket.hps_epoll_init(); // 初始化epoll

    // ... 待扩充
    return;
}
