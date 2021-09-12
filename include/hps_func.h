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
void ngx_init_setproctitle();
void ngx_setproctitle(const char *title);

#endif // __HPS_FUNC_H__
