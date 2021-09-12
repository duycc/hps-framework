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

#endif // __HPS_FUNC_H__
