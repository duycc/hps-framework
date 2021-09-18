//===--------------------------- net/hps_c_socket_inet.cpp - [HP-Server] --------------------------------*- C++ -*-===//
// brief :
//
//
// author: YongDu
// date  : 2021-09-17
//===--------------------------------------------------------------------------------------------------------------===//

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "hps_c_conf.h"
#include "hps_c_socket.h"
#include "hps_func.h"
#include "hps_global.h"
#include "hps_macro.h"

/**
 * @brief 将socket绑定的地址转换为文本格式（地址端口）
 *
 * @param sa 对端信息
 * @param port 1 保存端口号 0 不保存端口号
 * @param text 组合的信息字符串
 * @param len
 * @return size_t 信息字符串长度
 */
size_t CSocekt::hps_sock_ntop(struct sockaddr *sa, int port, u_char *text, size_t len) {
  struct sockaddr_in *sin;
  u_char *            p;

  switch (sa->sa_family) {
  case AF_INET:
    sin = (struct sockaddr_in *)sa;
    p = (u_char *)&sin->sin_addr;
    if (port) {
      p = hps_snprintf(text, len, "%ud.%ud.%ud.%ud:%d", p[0], p[1], p[2], p[3], ntohs(sin->sin_port));
    } else {
      p = hps_snprintf(text, len, "%ud.%ud.%ud.%ud", p[0], p[1], p[2], p[3]);
    }
    return (p - text);
    break;
  default:
    break;
  }
  return 0;
}
