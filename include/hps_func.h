/**
 * @file     hps_func.h
 * @brief
 * @author   YongDu
 * @date     2021-09-12
 */

#if !defined(__HPS_FUNC_H__)
#define __HPS_FUNC_H__

// 字符串处理相关函数
void Rtrim(char *string);
void Ltrim(char *string);

// 设置可执行程序标题相关函数
void hps_init_setproctitle();             // 移动环境变量位置
void hps_setproctitle(const char *title); // 设置进程名

// 日志输出，打印相关函数
void hps_log_init();                                               // 日志初始化
void hps_log_stderr(int err, const char *fmt, ...);                // 输出日志消息
void hps_log_error_core(int level, int err, const char *fmt, ...); // 写日志文件
u_char *hps_log_errno(u_char *buf, u_char *last, int err);         // 由错误码获取错误信息

// 格式化输出函数
u_char *hps_snprintf(u_char *buf, size_t max, const char *fmt, ...);
u_char *hps_slprintf(u_char *buf, u_char *last, const char *fmt, ...);
u_char *hps_vslprintf(u_char *buf, u_char *last, const char *fmt, va_list args);

#endif // __HPS_FUNC_H__
