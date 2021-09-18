//===--------------------------- app/hps_setproctitle.cpp - HP-Server -----------------------------------*- C++ -*-===//
// brief :
//   Set process title
//
// author: YongDu
// date  : 2021-09-18
//===--------------------------------------------------------------------------------------------------------------===//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hps_global.h"

/*
  进程名及启动参数和该进程相关环境变量存放位置是相邻的，在修改进程名时，
  为了避免影戏环境变量，需要将环境变量位置移动
*/

void hps_init_setproctitle() {
  gp_envmem = new char[g_envneedmem];
  memset(gp_envmem, 0, g_envneedmem);

  char *ptmp = gp_envmem;
  for (int i = 0; environ[i]; ++i) {
    size_t size = strlen(environ[i]) + 1;
    strcpy(ptmp, environ[i]); // 拷贝
    environ[i] = ptmp;        // environ[i] 指向新地址
    ptmp += size;
  }
  return;
}

void hps_setproctitle(const char *title) {
  // 1. 忽略所有启动参数
  // 2. 新进程名长度不能超过原始标题长度 + 原始环境变量

  size_t new_title_len = strlen(title);

  size_t total_len = g_argvneedmem + g_envneedmem; // argv + environ
  if (total_len <= new_title_len) {
    return; // new_title is too long
  }

  g_os_argv[1] = NULL; // 后续命令行参数清空，防止 argv 误用

  char *ptmp = g_os_argv[0];
  strcpy(ptmp, title);
  ptmp += new_title_len; // 跳过进程名

  size_t invalid = total_len - new_title_len;
  memset(ptmp, 0, invalid); // 清空进程名后的内存，否则有可能造成残留的内容误显示
  return;
}
