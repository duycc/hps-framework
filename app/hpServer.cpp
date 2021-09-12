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

//和设置标题有关的全局量
char **g_os_argv;       //原始命令行参数数组,在main中会被赋值
char *gp_envmem = NULL; //指向自己分配的env环境变量的内存
int g_environlen = 0;   //环境变量所占内存大小

int main(int argc, char *const *argv) {
  g_os_argv = (char **)argv;
  ngx_init_setproctitle(); //把环境变量搬家

  // 加载配置文件
  CConfig *p_config = CConfig::GetInstance();
  if (p_config->Load("hp_server.conf") == false) {
    printf("配置文件载入失败，退出!\n");
    exit(1);
  }

  // 测试: 获取配置文件
  int port = p_config->GetIntDefault("ListenPort", 0); // 0是缺省值
  printf("port = %d\n", port);
  const char *pDBInfo = p_config->GetString("DBInfo");
  if (pDBInfo != NULL) {
    printf("DBInfo = %s\n", pDBInfo);
  }

  /*for (int i = 0; environ[i]; i++)
  {
      printf("evriron[%d]地址=%x    " ,i,(unsigned int)((unsigned long)environ[i]));
      printf("evriron[%d]内容=%s\n" ,i,environ[i]);
  }
  printf("--------------------------------------------------------");
  */

  /*
  for (int i = 0; environ[i]; i++)
  {
      printf("evriron[%d]地址=%x    " ,i,(unsigned int)((unsigned long)environ[i]));
      printf("evriron[%d]内容=%s\n" ,i,environ[i]);
  }*/

  //我要保证所有命令行参数我都不 用了
  // ngx_setproctitle("nginx: master process");

  // printf("argc=%d,argv[0]=%s\n",argc,argv[0]);
  // strcpy(argv[0],"ce");
  // strcpy(argv[0],"c2212212121322324323e");
  // printf("非常高兴，大家和老师一起学习《linux c++通讯架构实战》\n");
  // printf("evriron[0]=%s\n" , environ[0]);
  // printf("evriron[1]=%s\n" , environ[1]);
  // printf("evriron[2]=%s\n" , environ[2]);
  // printf("evriron[3]=%s\n" , environ[3]);
  // printf("evriron[4]=%s\n" , environ[4]);

  // for(int i = 0; i < argc; ++i)
  //{
  //    printf("argv[%d]地址=%x    " ,i,(unsigned int)((unsigned long)argv[i]));
  //    printf("argv[%d]内容=%s\n",i,argv[i]);
  //}
  //下边环境变量随便打两个
  // for(int i = 0; i < 2; ++i)
  //{
  //    printf("evriron[%d]地址=%x    " ,i,(unsigned int)((unsigned long)environ[i]));
  //    printf("evriron[%d]内容=%s\n" ,i,environ[i]);
  //}

  // myconf();
  // mysignal();

  // for (;;) {
  //   sleep(1); //休息1秒
  //   printf("休息1秒\n");
  // }

  //对于因为设置可执行程序标题导致的环境变量分配的内存，我们应该释放
  if (gp_envmem) {
    delete[] gp_envmem;
    gp_envmem = NULL;
  }
  printf("程序退出，再见!\n");
  return 0;
}
