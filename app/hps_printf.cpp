//===--------------------------- app/hps_printf.cpp - HP-Server -----------------------------------------*- C++ -*-===//
// brief :
//   Formatted output
//
// author: YongDu
// date  : 2021-09-12
//===--------------------------------------------------------------------------------------------------------------===//

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hps_func.h"
#include "hps_global.h"
#include "hps_macro.h"

/**
 * @brief 格式化输出数字
 *
 * @param buf 数字存放位置
 * @param last 数据不能超过 last
 * @param ui64 显示的数字
 * @param zero 数值长度不足指定长度时，填充的字符
 * @param hexadecimal 是否以16进制输出
 * @param width 输出的长度
 * @return char*
 */
static char* hps_sprintf_num(char* buf, char* last, uint64_t ui64, char zero, uintptr_t hexadecimal, uintptr_t width);

// 对 hps_vslprintf() 函数的封装
char* hps_slprintf(char* buf, char* last, const char* fmt, ...) {
    va_list args;
    char* p;

    va_start(args, fmt); // 使args指向起始的参数
    p = hps_vslprintf(buf, last, fmt, args);
    va_end(args); // 释放args
    return p;
}

// 对 hps_vslprintf() 函数的封装，提供了缓冲区结束位置，更为安全
char* hps_snprintf(char* buf, size_t max, const char* fmt, ...) {
    char* p;
    va_list args;

    va_start(args, fmt);
    p = hps_vslprintf(buf, buf + max, fmt, args);
    va_end(args);
    return p;
}

/**
 * @brief 格式化输出，类似 printf(), vprintf()
 *
 * @param buf 存放数据
 * @param last 所存放数据不能超过此位置
 * @param fmt 可变参数
 * @param args %d, %xd, %Xd, %s, %f, %p
 * @return char*
 */
char* hps_vslprintf(char* buf, char* last, const char* fmt, va_list args) {
    char zero;
    /*
    #ifdef _WIN64
        typedef unsigned __int64  uintptr_t;
    #else
        typedef unsigned int uintptr_t;
    #endif
    */
    uintptr_t width, sign, hex, frac_width, scale, n;

    int64_t i64;   // 保存 %d 对应的可变参
    uint64_t ui64; // 保存 %ud 对应的可变参
    char* p;       // 保存 %s 对应的可变参
    double f;      // 保存 %f 对应的可变参
    uint64_t frac; // %f可变参数,根据 %.2f 等，取得小数部分的2位后的内容

    while (*fmt && buf < last) {
        // 每次处理一个字符
        if (*fmt == '%') {
            zero = (char)((*++fmt == '0') ? '0' : ' '); // hps_sprintf_num() 格式化输出数字长度不足时的填充字符

            sign = 1;       // 是否显示有符号数，默认1，有符号，如果是 %u 无符号
            width = 0;      // hps_sprintf_num() 格式化输出数字的长度
            hex = 0;        // 是否以16进制显示
            frac_width = 0; // 小数点后位数字，%.10f, frac_width: 10
            i64 = 0;        // %d 对应的可变参中的实际数字
            ui64 = 0;       // %ud 对应的可变参中的实际数字

            while (*fmt >= '0' && *fmt <= '9') {
                // 解析width，%16: width = 16
                width = width * 10 + (*fmt++ - '0');
            }

            while (true) {
                // '%' 之后的特殊字符
                switch (*fmt) {
                case 'u':
                    sign = 0; // 无符号数
                    fmt++;
                    continue;
                case 'X':
                    hex = 2; // 大写16进制
                    sign = 0;
                    fmt++;
                    continue;
                case 'x':
                    hex = 1; // 小写16进制
                    sign = 0;
                    fmt++;
                    continue;
                case '.':
                    // 其后边必须跟个数字，须与%f配合使用
                    // %.10f: 小数点后必须保证10位数字，不足10位则用0来填补
                    fmt++;
                    while (*fmt >= '0' && *fmt <= '9') {
                        frac_width = frac_width * 10 + (*fmt++ - '0');
                    }
                    break;
                default:
                    break;
                }
                break;
            }

            switch (*fmt) {
            case '%':
                // "%%"
                *buf++ = '%';
                fmt++;
                continue;
            case 'd':
                if (sign) {
                    i64 = (int64_t)va_arg(args, int);
                } else {
                    ui64 = (uint64_t)va_arg(args, u_int);
                }
                break; // 跳到 break 后继续处理
            case 'i':
                if (sign) {
                    i64 = (int64_t)va_arg(args, intptr_t);
                } else {
                    ui64 = (uint64_t)va_arg(args, uintptr_t);
                }
                break;
            case 'L': // int64, '%uL': uint64
                if (sign) {
                    i64 = va_arg(args, int64_t);
                } else {
                    ui64 = va_arg(args, uint64_t);
                }
                break;
            case 'p':
                ui64 = (uintptr_t)va_arg(args, void*);
                hex = 2;
                sign = 0;
                zero = '0';
                width = 2 * sizeof(void*);
                break;
            case 's':
                p = va_arg(args, char*);
                while (*p && buf < last) {
                    // 将%s对应的实际参数存到buf里
                    *buf++ = *p++;
                }
                fmt++;
                continue;

            case 'P':
                i64 = (int64_t)va_arg(args, pid_t);
                sign = 1;
                break;
            case 'f':
                f = va_arg(args, double);
                if (f < 0) {
                    // 负数增加'-'号
                    *buf++ = '-';
                    f = -f;
                }
                ui64 = (int64_t)f;
                frac = 0;
                if (frac_width) {
                    scale = 1;
                    for (n = frac_width; n; n--) {
                        scale *= 10;
                    }

                    // 按指定位数取出小数部分，且做四舍五入处理
                    // %.2f 12.537 --> frac = 0.54
                    frac = (uint64_t)((f - (double)ui64) * scale + 0.5);

                    if (frac == scale) {
                        // 进位
                        // %.2f, 12.999 --> frac = 100, scale = 100 ==> f = 13.00
                        ui64++;
                        frac = 0;
                    }
                }

                // 存放正整数部分
                buf = hps_sprintf_num(buf, last, ui64, zero, 0, width);
                // 存放小数部分
                if (frac_width) {
                    if (buf < last) {
                        *buf++ = '.';
                    }
                    buf = hps_sprintf_num(buf, last, frac, '0', 0, frac_width);
                }
                fmt++;
                continue;

                // 其它字符，后续考虑增加

            default:
                *buf++ = *fmt++;
                continue;
            }

            // 存放%d内容
            if (sign) {
                if (i64 < 0) {
                    *buf++ = '-';
                    ui64 = (uint64_t)-i64;
                } else {
                    ui64 = (uint64_t)i64;
                }
            }
            buf = hps_sprintf_num(buf, last, ui64, zero, hex, width);
            fmt++;
        } else {
            // 普通字符
            *buf++ = *fmt++;
        }
    }

    return buf;
}

/**
 * @brief 格式化输出数字
 *
 * @param buf 数字存放位置
 * @param last 数据不能超过 last
 * @param ui64 显示的数字
 * @param zero 数值长度不足指定长度时，填充的字符
 * @param hexadecimal 是否以16进制输出
 * @param width 输出的长度
 * @return char*
 */
static char* hps_sprintf_num(char* buf, char* last, uint64_t ui64, char zero, uintptr_t hexadecimal, uintptr_t width) {
    char *p, temp[HPS_INT64_LEN + 1]; // sizeof("-9223372036854775808") - 1 = 20
    size_t len;
    uint32_t ui32;

    static char hex[] = "0123456789abcdef"; // %xd格式符相关，显示的16进制数中 a-f 小写
    static char HEX[] = "0123456789ABCDEF"; // %Xd格式符相关，显示的16进制数中 A-F 大写

    p = temp + HPS_INT64_LEN; // p 指向数组最后一个元素位置

    if (hexadecimal == 0) { // 非16进制显示
        if (ui64 <= (uint64_t)HPS_MAX_UINT32_VALUE) {
            ui32 = (uint32_t)ui64;
            do { // uin32 = 7654321 --> temp = [            7654321 ]
                *--p = (char)(ui32 % 10 + '0');
            } while (ui32 /= 10);
        } else {
            do {
                *--p = (char)(ui64 % 10 + '0');
            } while (ui64 /= 10);
        }
    } else if (hexadecimal == 1) {
        // 16进制显示，且格式为 %xd，即以小写方式显示
        // ui64: 1234567(DEC)
        // BIN: 0001 0010 1101 0110 1000 0111
        // HEX: 12D687
        do {
            // 0xf: 二进制为：1111
            // ui64 & 0xf 取出 ui64 末尾四个二进制位，并转化为对应的16进制
            *--p = hex[(uint32_t)(ui64 & 0xf)];
        } while (ui64 >>= 4); // ui64 >>= 4 右移4位，相当于截取掉末尾四个二进制位
    } else {
        // 16进制显示，且格式为 %Xd，即以大写方式显示
        do {
            *--p = HEX[(uint32_t)(ui64 & 0xf)];
        } while (ui64 >>= 4);
    }

    len = (temp + HPS_INT64_LEN) - p; // 处理后数字字符串长度

    while (len++ < width && buf < last) {
        *buf++ = zero; // 长度不足时，填充指定的字符
    }

    len = (temp + HPS_INT64_LEN) - p; // 还原 len，准备拷贝到 buf 中

    if ((buf + len) >= last) { // buf 不足以拷贝整个数值字符串
        len = last - buf;      // 有多少容量便拷贝多少
    }
    return hps_memcpy(buf, p, len); // 返回 buf
}
