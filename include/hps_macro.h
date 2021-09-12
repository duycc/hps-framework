/**
 * @file     hps_macro.h
 * @brief
 * @author   YongDu
 * @date     2021-09-12
 */

#if !defined(__HPS_MACRO_H__)
#define __HPS_MACRO_H__

// 一些宏定义

#define HPS_MAX_ERROR_STR 2048 // 显示的错误信息最大数组长度

// 一般 memcpy 返回的是指向目标 dst 的指针，而 hps_memcpy 返回的是目标拷贝数据后的终点位置，方便多次复制数据
#define hps_memcpy(dst, src, n) (((u_char *)memcpy(dst, src, n)) + (n))
#define hps_min(val1, val2) ((val1 < val2) ? (val1) : (val2))

// 数字相关
#define HPS_MAX_UINT32_VALUE (uint32_t)0xffffffff // 最大的32位无符号数：十进制是‭4294967295‬
#define HPS_INT64_LEN (sizeof("-9223372036854775808") - 1)

// 日志级别
#define HPS_LOG_STDERR 0 // 控制台错误【stderr】日志不仅写文件，且尝试向标准输出输出错误信息
#define HPS_LOG_EMERG 1  // 紧急 【emerg】
#define HPS_LOG_ALERT 2  // 警戒 【alert】
#define HPS_LOG_CRIT 3   // 严重 【crit】
#define HPS_LOG_ERR 4    // 错误 【error】
#define HPS_LOG_WARN 5   // 警告 【warn】
#define HPS_LOG_NOTICE 6 // 注意 【notice】
#define HPS_LOG_INFO 7   // 信息 【info】
#define HPS_LOG_DEBUG 8  // 调试 【debug】

// 缺省的日志存放路径和文件名
#define HPS_ERROR_LOG_PATH "error.log"

// 当前进程类型
#define HPS_PROCESS_MASTER 0 // master 进程，管理进程
#define HPS_PROCESS_WORKER 1 // worker 进程，工作进程

#endif // __HPS_MACRO_H__
