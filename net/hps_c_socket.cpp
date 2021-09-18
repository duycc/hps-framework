//===--------------------------- net/hps_c_socket.cpp - [HP-Server] -------------------------------------*- C++ -*-===//
// brief :
//
//
// author: YongDu
// date  : 2021-09-18
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

CSocekt::CSocekt() {
  m_worker_connections = 1;
  m_ListenPortCount = 1;

  m_epollhandle = -1;
  m_pconnections = NULL;
  m_pfree_connections = NULL;

  m_iLenPkgHeader = sizeof(COMM_PKG_HEADER);
  m_iLenMsgHeader = sizeof(STRUC_MSG_HEADER);

  return;
}

CSocekt::~CSocekt() {
  for (auto *p_hps_listen : m_ListenSocketList) {
    delete p_hps_listen;
  }
  std::vector<lphps_listening_t>().swap(m_ListenSocketList);

  if (m_pconnections != NULL) // 释放连接池
    delete[] m_pconnections;  // 需要释放么？

  return;
}

bool CSocekt::Initialize() {
  ReadConf();
  return hps_open_listening_sockets();
}

void CSocekt::ReadConf() {
  CConfig *p_config = CConfig::GetInstance();
  m_worker_connections = p_config->GetIntDefault("worker_connections", m_worker_connections);
  m_ListenPortCount = p_config->GetIntDefault("ListenPortCount", m_ListenPortCount);
  return;
}

// 打开监听端口，需在创建 worker 进程之前执行
bool CSocekt::hps_open_listening_sockets() {
  int                isock;
  struct sockaddr_in serv_addr;
  int                iport;
  char               strinfo[100];

  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;                // 选择协议族为IPV4
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 监听本地所有的IP

  CConfig *p_config = CConfig::GetInstance();
  for (int i = 0; i < m_ListenPortCount; ++i) {
    isock = socket(AF_INET, SOCK_STREAM, 0);
    if (isock == -1) {
      hps_log_stderr(errno, "CSocekt::Initialize()中socket()失败, i=%d.", i);
      return false;
    }

    int reuseaddr = 1;
    if (setsockopt(isock, SOL_SOCKET, SO_REUSEADDR, (const void *)&reuseaddr, sizeof(reuseaddr)) == -1) {
      hps_log_stderr(errno, "CSocekt::Initialize()中setsockopt(SO_REUSEADDR)失败, i=%d.", i);
      close(isock);
      return false;
    }

    // 设置非阻塞
    if (setnonblocking(isock) == false) {
      hps_log_stderr(errno, "CSocekt::Initialize()中setnonblocking()失败, i=%d.", i);
      close(isock);
      return false;
    }

    // 绑定 ip + port
    strinfo[0] = 0;
    sprintf(strinfo, "ListenPort%d", i);
    iport = p_config->GetIntDefault(strinfo, 10000);
    serv_addr.sin_port = htons((in_port_t)iport);

    if (bind(isock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
      hps_log_stderr(errno, "CSocekt::Initialize()中bind()失败, i=%d.", i);
      close(isock);
      return false;
    }

    if (listen(isock, HPS_LISTEN_BACKLOG) == -1) {
      hps_log_stderr(errno, "CSocekt::Initialize()中listen()失败, i=%d.", i);
      close(isock);
      return false;
    }

    lphps_listening_t p_listensocketitem = new hps_listening_t;
    memset(p_listensocketitem, 0, sizeof(hps_listening_t));
    p_listensocketitem->port = iport;
    p_listensocketitem->fd = isock;
    hps_log_error_core(HPS_LOG_INFO, 0, "监听%d端口成功!", iport);
    m_ListenSocketList.push_back(p_listensocketitem);
  }
  if (m_ListenSocketList.size() <= 0)
    return false;
  return true;
}

bool CSocekt::setnonblocking(int sockfd) {
  int nb = 1; // 0 清除, 1 设置
  if (ioctl(sockfd, FIONBIO, &nb) == -1) {
    return false;
  }
  return true;
}

void CSocekt::hps_close_listening_sockets() {
  for (int i = 0; i < m_ListenPortCount; ++i) {
    close(m_ListenSocketList[i]->fd);
    hps_log_error_core(HPS_LOG_INFO, 0, "关闭监听端口%d!", m_ListenSocketList[i]->port);
  }
  return;
}

//===----------------------------- epoll相关在 worker 进程中调用 ------------------------------===//

int CSocekt::hps_epoll_init() {
  m_epollhandle = epoll_create(m_worker_connections);
  if (m_epollhandle == -1) {
    hps_log_stderr(errno, "CSocekt::hps_epoll_init()中epoll_create()失败.");
    exit(2);
  }

  // 连接池
  m_connection_n = m_worker_connections;
  m_pconnections = new hps_connection_t[m_connection_n];

  lphps_connection_t next = NULL;
  lphps_connection_t c = m_pconnections;

  int i = m_connection_n;
  do {
    i--;
    // 从后向前链接
    c[i].data = next;
    c[i].fd = -1;
    c[i].instance = 1;
    c[i].iCurrsequence = 0;
    next = &c[i];
  } while (i);

  m_pfree_connections = next;
  m_free_connection_n = m_connection_n;

  // 监听socket绑定一个连接对象
  std::vector<lphps_listening_t>::iterator pos;
  for (pos = m_ListenSocketList.begin(); pos != m_ListenSocketList.end(); ++pos) {
    c = hps_get_connection((*pos)->fd);
    if (c == NULL) {
      hps_log_stderr(errno, "CSocekt::hps_epoll_init()中ngx_get_connection()失败.");
      exit(2); // 直接结束程序
    }
    c->listening = (*pos);  // 连接对象 和监听对象关联，方便通过连接对象找监听对象
    (*pos)->connection = c; // 监听对象 和连接对象关联，方便通过监听对象找连接对象

    // 监听端口的读事件处理，监听端口是用来等对方连接的发送三路握手的
    c->rhandler = &CSocekt::hps_event_accept;

    // 往监听socket上增加监听事件
    if (hps_epoll_add_event((*pos)->fd, 1, 0, 0, EPOLL_CTL_ADD, c) == -1) {
      exit(2);
    }
  }
  return 1;
}

/**
 * @brief epoll增加事件
 *
 * @param fd 对应的socket
 * @param readevent 是否是读事件
 * @param writeevent 是否是写事件
 * @param otherflag 额外标记
 * @param eventtype 事件类型
 * @param c 对应连接池
 * @return int
 */
int CSocekt::hps_epoll_add_event(int fd, int readevent, int writeevent, uint32_t otherflag, uint32_t eventtype,
                                 lphps_connection_t c) {
  struct epoll_event ev;
  memset(&ev, 0, sizeof(ev));

  if (readevent == 1) {
    ev.events = EPOLLIN | EPOLLRDHUP; // EPOLLIN    新连接建立
                                      // EPOLLRDHUP 对端关闭连接
                                      // 详细说明：https://blog.csdn.net/heluan123132/article/details/77891720

    // ev.events |= (ev.events | EPOLLET);
  } else {
    // ... 其他事件待扩充
  }

  if (otherflag != 0) {
    ev.events |= otherflag;
  }

  // 指针的二进制位最后一位一定是0，因此可以通过'|'运算利用指针最后一位携带一个0或1的标志位
  ev.data.ptr = (void *)((uintptr_t)c | c->instance);

  if (epoll_ctl(m_epollhandle, eventtype, fd, &ev) == -1) {
    hps_log_stderr(errno, "CSocekt::hps_epoll_add_event()中epoll_ctl(%d, %d, %d, %u, %u)失败.", fd, readevent,
                   writeevent, otherflag, eventtype);
    return -1;
  }
  return 1;
}

/**
 * @brief 获取发生的事件消息，由 hps_process_events_and_timers() 调用
 *
 * @param timer 阻塞时长
 * @return int 1 正常，0 非正常
 */
int CSocekt::hps_epoll_process_events(int timer) {
  int events = epoll_wait(m_epollhandle, m_events, HPS_MAX_EVENTS, timer);
  /* timer =
    -1  一直阻塞，
    0   立即返回，即使没有任何事件
    return =
    -1  发生错误
    0   发生超时
    >0  捕获到返回值数量事件
  */

  if (events == -1) {
    if (errno == EINTR) {
      // 信号导致，可认为正常
      hps_log_error_core(HPS_LOG_INFO, errno, "CSocekt::hps_epoll_process_events()中epoll_wait()失败!");
      return 1;
    } else {
      hps_log_error_core(HPS_LOG_ALERT, errno, "CSocekt::hps_epoll_process_events()中epoll_wait()失败!");
      return 0;
    }
  }
  if (events == 0) {
    if (timer != -1) {
      return 1; // 阻塞时间到了
    }
    // 一直阻塞的情况下返回0，发生错误
    hps_log_error_core(HPS_LOG_ALERT, 0, "CSocekt::hps_epoll_process_events()中epoll_wait()没超时却没返回任何事件!");
    return 0;
  }

  // 会惊群，一个telnet上来，4个 worker 进程都会被惊动，都将执行以下代码：
  // hps_log_stderr(errno,"惊群测试1:%d",events);

  // 收到事件
  lphps_connection_t c;
  uintptr_t          instance;
  uint32_t           revents;

  for (int i = 0; i < events; ++i) {
    // events 收集到的事件数量
    c = (lphps_connection_t)(m_events[i].data.ptr);         // hps_epoll_add_event()给进去的
    instance = (uintptr_t)c & 1;                            // 取出标志位
    c = (lphps_connection_t)((uintptr_t)c & (uintptr_t)~1); // 去除 instance 标志位，取 c 真正地址

    if (c->fd == -1) {
      /* 过滤过期事件：
        用epoll_wait取得三个事件，处理第一个事件时，因为业务需要，需要把这个连接关闭，会把c->fd设置为-1；
        第二个事件照常处理；
        假如第三个事件，跟第一个事件对应的是同一个连接，那这个条件就会成立，这种事件，属于过期事件，不应该处理
      */
      hps_log_error_core(HPS_LOG_DEBUG, 0, "CSocekt::hps_epoll_process_events()中遇到了fd=-1的过期事件:%p.", c);
      continue;
    }

    if (c->instance != instance) {
      //===----------------------------- 过滤过期事件 ------------------------------===//
      //  a. 处理第一个事件时，因为业务需要，把这个连接关闭，同时设置c->fd = -1，
      //     并且调用hps_free_connection将该连接归还给连接池；
      //  b. 处理第二个事件，恰好第二个事件是建立新连接事件，调用hps_get_connection从连接池中取出的连接
      //     可能就是刚刚释放的第一个事件对应的连接池中的连接，又因为a中套接字被释放了，所以会被操作系统拿来复用
      //     复用给了b；
      //  c. 当处理第三个事件时，第三个事件其实是已经过期的，应该不处理。
      //
      //  解决：当调用hps_get_connection从连接池中获取一个新连接时，将instance标志位置反，所以这个条件如果不成立，
      //    说明这个连接已经被挪作他用

      hps_log_error_core(HPS_LOG_DEBUG, 0, "CSocekt::hps_epoll_process_events()中遇到了instance值改变的过期事件:%p.",
                         c);
      continue;
    }

    // 正常事件
    revents = m_events[i].events;
    if (revents & (EPOLLERR | EPOLLHUP)) {
      // 客户端断连
      revents |= EPOLLIN | EPOLLOUT;
    }
    if (revents & EPOLLIN) {
      (this->*(c->rhandler))(c);
    }

    if (revents & EPOLLOUT) {
      // 写事件
      // ... 待扩展
    }
  }
  return 1;
}
