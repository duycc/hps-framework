/**
 * @file     hps_log.cpp
 * @brief
 * @author   YongDu
 * @date     2021-09-12
 */

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "hps_c_conf.h"
#include "hps_func.h"
#include "hps_global.h"
#include "hps_macro.h"

hps_log_t hps_log; // 全局的日志信息

// 错误等级，和 hps_macro.h 里的日志等级宏一一对应
static u_char err_levels[][20] = {
    {"stderr"}, // 0：控制台错误
    {"emerg"},  // 1：紧急
    {"alert"},  // 2：警戒
    {"crit"},   // 3：严重
    {"error"},  // 4：错误
    {"warn"},   // 5：警告
    {"notice"}, // 6：注意
    {"info"},   // 7：信息
    {"debug"}   // 8：调试
};

/**
 * @brief 格式化输出错误信息
 *
 * @param err 错误码
 * @param fmt
 * @param ...
 */
void hps_log_stderr(int err, const char *fmt, ...) {
  va_list args;
  u_char  errstr[HPS_MAX_ERROR_STR + 1];
  u_char *p, *last;

  memset(errstr, 0, sizeof(errstr));

  last = errstr + HPS_MAX_ERROR_STR;

  p = hps_memcpy(errstr, "hpServer: ", 10);

  va_start(args, fmt);                   // 使args指向起始的参数
  p = hps_vslprintf(p, last, fmt, args); // 组合出这个字符串保存在errstr里
  va_end(args);                          // 释放args

  if (err) {
    // 有错误，错误信息写入
    p = hps_log_errno(p, last, err);
  }

  if (p >= (last - 1)) {
    // 位置不够，硬插入，即使覆盖其它内容
    p = (last - 1) - 1;
  }
  *p++ = '\n'; // 在格式化字符串后便可不加'\n'

  // 写到标准错误
  write(STDERR_FILENO, errstr, p - errstr);
  if (hps_log.fd > STDERR_FILENO) {
    // 日志文件打开，写入日志
    err = 0; // 不要重复显示错误信息
    p--;
    *p = 0; // 去掉末尾'\n',hps_log_error_core()会再次增加
    hps_log_error_core(HPS_LOG_STDERR, err, (const char *)errstr);
  }
  return;
}

/**
 * @brief 由错误编号获取错误信息描述
 *
 * @param buf 错误信息保存位置
 * @param last
 * @param err 错误编号
 * @return u_char*
 */
u_char *hps_log_errno(u_char *buf, u_char *last, int err) {
  char * perrorinfo = strerror(err);
  size_t len = strlen(perrorinfo);

  // 组合输出格式
  char leftstr[10] = {0};
  sprintf(leftstr, " (%d: ", err);
  size_t leftlen = strlen(leftstr);

  char   rightstr[] = ") ";
  size_t rightlen = strlen(rightstr);

  size_t extralen = leftlen + rightlen;
  if ((buf + len + extralen) < last) {
    buf = hps_memcpy(buf, leftstr, leftlen);
    buf = hps_memcpy(buf, perrorinfo, len);
    buf = hps_memcpy(buf, rightstr, rightlen);
  }
  return buf;
}

/**
 * @brief 写日志文件
 *
 * @param level 日志等级
 * @param err 错误码
 * @param fmt
 * @param ...
 */
void hps_log_error_core(int level, int err, const char *fmt, ...) {
  u_char *last;
  u_char  errstr[HPS_MAX_ERROR_STR + 1];

  memset(errstr, 0, sizeof(errstr));
  last = errstr + HPS_MAX_ERROR_STR;

  struct timeval tv;
  struct tm      tm;
  time_t         sec;
  u_char *       p;
  va_list        args;

  memset(&tv, 0, sizeof(struct timeval));
  memset(&tm, 0, sizeof(struct tm));

  gettimeofday(&tv, NULL); // 获取当前时间

  sec = tv.tv_sec;
  localtime_r(&sec, &tm); // 把sec转化为本地时间保存到tm
  tm.tm_mon++;            // 调整月份
  tm.tm_year += 1900;     // 调整年份

  u_char strcurrtime[40] = {0}; // 时间字符串
  hps_slprintf(strcurrtime, (u_char *)-1,
               "%4d/%02d/%02d %02d:%02d:%02d", // 2021/09/13 19:57:11
               tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

  p = hps_memcpy(errstr, strcurrtime, strlen((const char *)strcurrtime));
  p = hps_slprintf(p, last, " [%s] ", err_levels[level]); // 日志级别
  p = hps_slprintf(p, last, "%P: ", hps_pid);             // 进程ID

  va_start(args, fmt);
  p = hps_vslprintf(p, last, fmt, args);
  va_end(args);

  if (err) {
    //错误码和错误信息
    p = hps_log_errno(p, last, err);
  }
  if (p >= (last - 1)) {
    // 位置不够，硬插入，即使覆盖其它内容
    p = (last - 1) - 1;
  }
  *p++ = '\n';

  ssize_t n;
  do {
    if (level > hps_log.log_level) {
      // 没必要记录的日志
      break;
    }

    // 写日志文件
    n = write(hps_log.fd, errstr, p - errstr);
    if (n == -1) {
      if (errno == ENOSPC) {
        // 憨憨，磁盘满了
      } else {
        if (hps_log.fd != STDERR_FILENO) {
          // 其它错误，输出到标准错误
          n = write(STDERR_FILENO, errstr, p - errstr);
        }
      }
    }
  } while (false);
  return;
}

// 初始化日志文件
void hps_log_init() {
  u_char *plogname = NULL;
  size_t  nlen;

  CConfig *p_config = CConfig::GetInstance();
  plogname = (u_char *)p_config->GetString("Log");
  if (NULL == plogname) {
    plogname = (u_char *)HPS_ERROR_LOG_PATH; // 读取失败，使用默认路径
  }

  // 缺省日志等级为 6【注意】
  hps_log.log_level = p_config->GetIntDefault("LogLevel", HPS_LOG_NOTICE);

  hps_log.fd = open((const char *)plogname, O_WRONLY | O_APPEND | O_CREAT, 0644);
  if (hps_log.fd == -1) {
    hps_log_stderr(errno, "[alert] could not open error log file: open() \"%s\" failed", plogname);
    hps_log.fd = STDERR_FILENO; // 定位到标准错误
  }
  return;
}
