/**
 * @file     hps_global.h
 * @brief
 * @author   YongDu
 * @date     2021-09-12
 */

#if !defined(__HPS_GLOBAL_H__)
#define __HPS_GLOBAL_H__

// 配置项
typedef struct _CConfItem {
  char ItemName[50];
  char ItemContent[500];
} CConfItem, *LPCConfItem;

// 日志
typedef struct {
  int log_level; // 日志级别
  int fd;        // 日志文件描述符
} hps_log_t;

// 外部全局量声明
extern char **g_os_argv;
extern char *gp_envmem;
extern int g_environlen;

#endif // __HPS_GLOBAL_H__
