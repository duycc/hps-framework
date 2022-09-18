//===--------------------------- include/hps_func.h - HP-Server -----------------------------------------*- C++ -*-===//
// brief :
//   Some FunctionDecl
//
// author: YongDu
// date  : 2021-09-12
//===--------------------------------------------------------------------------------------------------------------===//

#if !defined(__HPS_FUNC_H__)
#define __HPS_FUNC_H__

#include <stdarg.h>

//===----------------------------- 字符串处理相关函数 -----------------------------===//
void Rtrim(char* string);
void Ltrim(char* string);

//===------------------------ 设置可执行程序标题相关函数 ----------------------------===//
void hps_init_setproctitle();             // 移动环境变量位置
void hps_setproctitle(const char* title); // 设置进程名

//===-------------------------- 日志输出，打印相关函数 ------------------------------===//

void hps_log_init();                                               // 日志初始化
void hps_log_stderr(int err, const char* fmt, ...);                // 输出日志消息
void hps_log_error_core(int level, int err, const char* fmt, ...); // 写日志文件

// 由错误码获取错误信息
char* hps_log_errno(char* buf, char* last, int err);

//===----------------------------- 格式化输出函数 ------------------------------===//
char* hps_snprintf(char* buf, size_t max, const char* fmt, ...);
char* hps_slprintf(char* buf, char* last, const char* fmt, ...);
char* hps_vslprintf(char* buf, char* last, const char* fmt, va_list args);

//===----------------------------- 信号及进程相关函数 ------------------------------===//

int hps_init_signals();               // 信号相关初始化
void hps_master_process_cycle();      // 进程初始化
int hps_daemon();                     // 守护进程初始化
void hps_process_events_and_timers(); // 处理网络事件和定时器事件

#endif // __HPS_FUNC_H__
