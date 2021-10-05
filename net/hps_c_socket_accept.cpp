//===--------------------------- net/hps_c_socket_accept.cpp - [HP-Server] ------------------------------*- C++ -*-===//
// brief :
//   Process the newly connected TCP
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

// 新连接连入处理函数
void CSocket::hps_event_accept(lphps_connection_t oldc) {
  // LT模式下，可以只 accept 一次，如果没有处理，事件还会继续通知，避免阻塞
  struct sockaddr mysockaddr;
  socklen_t socklen;
  int err;
  int level;
  int s;
  static int use_accept4 = 1;
  lphps_connection_t newc;

  socklen = sizeof(mysockaddr);
  do {
    if (use_accept4) {
      // 获取一条非阻塞连接
      s = accept4(oldc->fd, &mysockaddr, &socklen, SOCK_NONBLOCK);
    } else {
      s = accept(oldc->fd, &mysockaddr, &socklen);
    }

    // 惊群：https://blog.csdn.net/russell_tao/article/details/7204260

    if (s == -1) {
      err = errno;
      if (err == EAGAIN) {
        return;
      }
      level = HPS_LOG_ALERT;
      if (err == ECONNABORTED) {
        // 软件引起的连接终止，客户端发送了 RST 包，可忽略该错误
        level = HPS_LOG_ERR;
      } else if (err == EMFILE || err == ENFILE) { // EMFILE: 进程的 fd 已使用完
        level = HPS_LOG_CRIT;
      }
      hps_log_error_core(level, errno, "CSocket::hps_event_accept()中accept4()失败!");

      if (use_accept4 && err == ENOSYS) { // 没有 accept4 函数，重新使用 accept 函数
        use_accept4 = 0;
        continue;
      }

      if (err == ECONNABORTED) {
      }

      if (err == EMFILE || err == ENFILE) {
      }
      return;
    }

    if (m_onlineUserCount >= m_worker_connections) { // 用户连接数过多
      hps_log_stderr(0, "超出系统允许的最大连入用户数(最大允许连入数%d)，关闭连入请求(%d)。", m_worker_connections, s);
      close(s);
      return;
    }

    newc = hps_get_connection(s);
    if (newc == NULL) {
      if (close(s) == -1) {
        hps_log_error_core(HPS_LOG_ALERT, errno, "CSocket::hps_event_accept()中close(%d)失败!", s);
      }
      return;
    }
    // 成功获取到连接
    memcpy(&newc->s_sockaddr, &mysockaddr, socklen);
    // {
    //   u_char ipaddr[100];
    //   memset(ipaddr, 0, sizeof(ipaddr));
    //   hps_sock_ntop(&newc->s_sockaddr, 1, ipaddr, sizeof(ipaddr) - 10);
    //   hps_log_stderr(0, "ip信息为%s\n", ipaddr);
    // }

    if (!use_accept4) {
      if (setnonblocking(s) == false) {
        hps_close_connection(newc);
        return;
      }
    }

    newc->listening = oldc->listening;                   // 关联到监听端口
    newc->rhandler = &CSocket::hps_read_request_handler; // 数据来时的读处理函数
    newc->whandler = &CSocket::hps_write_request_handler;

    // 将读事件加入epoll监控
    if (hps_epoll_oper_event(s, EPOLL_CTL_ADD, EPOLLIN | EPOLLRDHUP, 0, newc) == -1) {
      // 可以立即回收这个连接，无需延迟，因为其上还没有数据收发
      hps_close_connection(newc);
      return;
    }

    if (m_ifkickTimeCount == 1) {
      this->AddToTimerQueue(newc);
    }
    ++m_onlineUserCount;
    break;
  } while (1);
  return;
}
