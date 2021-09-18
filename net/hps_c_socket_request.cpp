//===--------------------------- net/hps_c_socket_request.cpp - [HP-Server] -----------------------------*- C++ -*-===//
// brief :
//
//
// author: YongDu
// date  : 2021-09-17
//===--------------------------------------------------------------------------------------------------------------===//

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "hps_c_socket.h"
#include "hps_func.h"

// TCP 连接发送数据来处理
void CSocekt::hps_wait_request_handler(lphps_connection_t c) {
  // ET 测试
  // unsigned char buf[10] = {0};
  // memset(buf, 0, sizeof(buf));
  // do {
  //   int n = recv(c->fd, buf, 2, 0);
  //   if (n == -1 && errno == EAGAIN) {
  //     break;
  //   } else if (n == 0) {
  //     break;
  //   }
  //   hps_log_stderr(0, "收到的字节数位%d, 内容为: %s", n, buf);
  // } while (1);

  // LT测试
  unsigned char buf[10] = {0};
  memset(buf, 0, sizeof(buf));
  int n = recv(c->fd, buf, 2, 0);
  if (0 == n) {
    hps_free_connection(c);
    close(c->fd);
    c->fd = -1;
  }
  hps_log_stderr(0, "收到的字节数位%d, 内容为: %s", n, buf);
  return;
}
