/**
 * @file     hpServer.cpp
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

static void free_resource(); // 内存释放

int main(int argc, char *const *argv) {
  g_os_argv = (char **)argv;
  hps_init_setproctitle(); //把环境变量搬家

  // 加载配置文件
  CConfig *p_config = CConfig::GetInstance();
  if (p_config->Load("hpServer.conf") == false) {
    printf("配置文件载入失败，退出!\n");
    exit(1);
  }

  // 设置进程名，必须保证启动参数不再使用之后才可以设置
  hps_setproctitle("hpServer: master process");

  while (true) {
    sleep(1);
    printf("休息1秒\n");
  }

  free_resource();
  printf("程序退出，再见!\n");
  return 0;
}

void free_resource() {
  if (gp_envmem) {
    delete[] gp_envmem;
    gp_envmem = NULL;
  }
  return;
}
