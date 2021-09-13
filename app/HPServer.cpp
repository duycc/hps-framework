/**
 * @file     HPServer.cpp
 * @brief
 * @author   YongDu
 * @date     2021-09-12
 */

// 程序入口函数

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hps_c_conf.h" // 配置文件处理
#include "hps_func.h"   // 函数声明
#include "hps_signal.h"

char **g_os_argv = NULL; // 原始命令行参数数组

pid_t hps_pid;    // 当前进程 id
pid_t hps_parent; // 当前进程父进程 id

static void free_resource(); // 内存释放

int main(int argc, char *const *argv) {
  int exit_code = 0; // 0     正常退出
                     // 1, -1 异常退出
                     // 2     系统找不到指定文件

  hps_pid = getpid();
  hps_parent = getppid();

  g_os_argv = (char **)argv;

  hps_log.fd = -1;

  hps_init_setproctitle(); //把环境变量搬家

  do { // do...while(false) 方便退出释放资源
    // 加载配置文件
    CConfig *p_config = CConfig::GetInstance();
    if (p_config->Load("HPServer.conf") == false) {
      hps_log_stderr(0, "配置文件[%s]载入失败，退出！", "HPServer.conf");
      exit_code = 2;
      break;
    }

    // 初始化日志
    hps_log_init();

    // 设置进程名，必须保证启动参数不再使用之后才可以设置
    hps_setproctitle("hpServer: master process");

    // while (true) {
    //   sleep(1);
    //   printf("休息1秒\n");
    // }
  } while (false);

  hps_log_stderr(0, "%s", argv[0]);
  hps_log_stderr(0, "%10d", 21);
  hps_log_stderr(0, "%.6f", 21.378);
  hps_log_stderr(0, "%.6f", 12.999);
  hps_log_stderr(0, "%xd", 1678);
  hps_log_stderr(0, "%Xd", 1678);

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
