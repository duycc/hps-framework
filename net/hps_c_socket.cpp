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
#include "hps_c_memory.h"
#include "hps_c_socket.h"
#include "hps_func.h"
#include "hps_global.h"
#include "hps_macro.h"
#include "hps_c_lockmutex.h"

CSocket::CSocket() {
  m_worker_connections = 1;
  m_ListenPortCount = 1;
  m_RecyConnectionWaitTime = 60; // 待回收连接需要延迟 60s 回收

  m_epollhandle = -1;

  m_iLenPkgHeader = sizeof(COMM_PKG_HEADER);
  m_iLenMsgHeader = sizeof(STRUC_MSG_HEADER);

  m_iSendMsgQueueCount = 0;
  m_totol_recyconnection_n = 0;

  m_cur_size_ = 0;
  m_timer_value_ = 0;

  m_onlineUserCount = 0;

  return;
}

CSocket::~CSocket() {
  for (auto *p_hps_listen : m_ListenSocketList) {
    delete p_hps_listen;
  }
  std::vector<lphps_listening_t>().swap(m_ListenSocketList);
  return;
}

bool CSocket::Initialize() {
  ReadConf();
  return hps_open_listening_sockets();
}

// 子进程中需要执行的初始化内容
bool CSocket::Initialize_subproc() {
  // 发消息互斥量初始化
  if (pthread_mutex_init(&m_sendMessageQueueMutex, NULL) != 0) {
    hps_log_stderr(0, "CSocket::Initialize()中pthread_mutex_init(&m_sendMessageQueueMutex)失败.");
    return false;
  }
  // 连接相关互斥量初始化
  if (pthread_mutex_init(&m_connectionMutex, NULL) != 0) {
    hps_log_stderr(0, "CSocket::Initialize()中pthread_mutex_init(&m_connectionMutex)失败.");
    return false;
  }
  // 连接回收队列相关互斥量初始化
  if (pthread_mutex_init(&m_recyconnqueueMutex, NULL) != 0) {
    hps_log_stderr(0, "CSocket::Initialize()中pthread_mutex_init(&m_recyconnqueueMutex)失败.");
    return false;
  }
  // 和时间处理队列有关的互斥量初始化
  if (pthread_mutex_init(&m_timequeueMutex, NULL) != 0) {
    hps_log_stderr(0, "CSocket::Initialize_subproc()中pthread_mutex_init(&m_timequeueMutex)失败.");
    return false;
  }

  if (sem_init(&m_semEventSendQueue, 0, 0) == -1) {
    hps_log_stderr(0, "CSocket::Initialize()中sem_init(&m_semEventSendQueue,0,0)失败.");
    return false;
  }

  // 创建发送线程和回收线程
  int err;
  ThreadItem *pSendQueue;
  m_threadVector.push_back(pSendQueue = new ThreadItem(this));
  err = pthread_create(&pSendQueue->_Handle, NULL, ServerSendQueueThread, pSendQueue);
  if (err != 0) {
    return false;
  }

  ThreadItem *pRecyconn;
  m_threadVector.push_back(pRecyconn = new ThreadItem(this));
  err = pthread_create(&pRecyconn->_Handle, NULL, ServerRecyConnectionThread, pRecyconn);
  if (err != 0) {
    hps_log_stderr(0, "CSocket::Initialize_subproc()中pthread_create(ServerRecyConnectionThread)失败.");
    return false;
  }

  if (m_ifkickTimeCount == 1) //是否开启踢人时钟，1：开启   0：不开启
  {
    ThreadItem *pTimemonitor; //专门用来处理到期不发心跳包的用户踢出的线程
    m_threadVector.push_back(pTimemonitor = new ThreadItem(this));
    err = pthread_create(&pTimemonitor->_Handle, NULL, ServerTimerQueueMonitorThread, pTimemonitor);
    if (err != 0) {
      hps_log_stderr(0, "CSocket::Initialize_subproc()中pthread_create(ServerTimerQueueMonitorThread)失败.");
      return false;
    }
  }

  return true;
}

void CSocket::Shutdown_subproc() {
  if (sem_post(&m_semEventSendQueue) == -1) {
    hps_log_stderr(0, "CSocket::Shutdown_subproc()中sem_post(&m_semEventSendQueue)失败.");
  }
  std::vector<ThreadItem *>::iterator iter;
  for (iter = m_threadVector.begin(); iter != m_threadVector.end(); iter++) {
    pthread_join((*iter)->_Handle, NULL);
  }
  for (iter = m_threadVector.begin(); iter != m_threadVector.end(); iter++) {
    if (*iter)
      delete *iter;
  }
  std::vector<ThreadItem *>().swap(m_threadVector);

  this->clearMsgSendQueue();
  this->clearConnection();
  this->clearAllFromTimerQueue();

  pthread_mutex_destroy(&m_connectionMutex);
  pthread_mutex_destroy(&m_sendMessageQueueMutex);
  pthread_mutex_destroy(&m_recyconnqueueMutex);
  pthread_mutex_destroy(&m_timequeueMutex);
  sem_destroy(&m_semEventSendQueue);
  return;
}

void CSocket::ReadConf() {
  CConfig *p_config = CConfig::GetInstance();
  m_worker_connections = p_config->GetIntDefault("worker_connections", m_worker_connections);
  m_ListenPortCount = p_config->GetIntDefault("ListenPortCount", m_ListenPortCount);
  m_RecyConnectionWaitTime = p_config->GetIntDefault("Sock_RecyConnectionWaitTime", m_RecyConnectionWaitTime);

  m_ifkickTimeCount = p_config->GetIntDefault("Sock_WaitTimeEnable", 0);
  m_iWaitTime = p_config->GetIntDefault("Sock_MaxWaitTime", m_iWaitTime);
  m_iWaitTime = (m_iWaitTime > 5) ? m_iWaitTime : 5;
  m_ifTimeOutKick = p_config->GetIntDefault("Sock_TimeOutKick", 0);

  m_floodAkEnable = p_config->GetIntDefault("Sock_FloodAttackKickEnable", 0);
  m_floodTimeInterval = p_config->GetIntDefault("Sock_FloodTimeInterval", 100);
  m_floodKickCount = p_config->GetIntDefault("Sock_FloodKickCounter", 10);

  return;
}

// 打开监听端口，需在创建 worker 进程之前执行
bool CSocket::hps_open_listening_sockets() {
  int isock;
  struct sockaddr_in serv_addr;
  int iport;
  char strinfo[100];

  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;                // 选择协议族为IPV4
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 监听本地所有的IP

  CConfig *p_config = CConfig::GetInstance();
  for (int i = 0; i < m_ListenPortCount; ++i) {
    isock = socket(AF_INET, SOCK_STREAM, 0);
    if (isock == -1) {
      hps_log_stderr(errno, "CSocket::Initialize()中socket()失败, i=%d.", i);
      return false;
    }

    int reuseaddr = 1;
    if (setsockopt(isock, SOL_SOCKET, SO_REUSEADDR, (const void *)&reuseaddr, sizeof(reuseaddr)) == -1) {
      hps_log_stderr(errno, "CSocket::Initialize()中setsockopt(SO_REUSEADDR)失败, i=%d.", i);
      close(isock);
      return false;
    }

    // 设置非阻塞
    if (setnonblocking(isock) == false) {
      hps_log_stderr(errno, "CSocket::Initialize()中setnonblocking()失败, i=%d.", i);
      close(isock);
      return false;
    }

    // 绑定 ip + port
    strinfo[0] = 0;
    sprintf(strinfo, "ListenPort%d", i);
    iport = p_config->GetIntDefault(strinfo, 10000);
    serv_addr.sin_port = htons((in_port_t)iport);

    if (bind(isock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
      hps_log_stderr(errno, "CSocket::Initialize()中bind()失败, i=%d.", i);
      close(isock);
      return false;
    }

    if (listen(isock, HPS_LISTEN_BACKLOG) == -1) {
      hps_log_stderr(errno, "CSocket::Initialize()中listen()失败, i=%d.", i);
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

bool CSocket::setnonblocking(int sockfd) {
  int nb = 1; // 0 清除, 1 设置
  if (ioctl(sockfd, FIONBIO, &nb) == -1) {
    return false;
  }
  return true;
}

void CSocket::hps_close_listening_sockets() {
  for (int i = 0; i < m_ListenPortCount; ++i) {
    close(m_ListenSocketList[i]->fd);
    hps_log_error_core(HPS_LOG_INFO, 0, "关闭监听端口%d!", m_ListenSocketList[i]->port);
  }
  return;
}

void CSocket::clearMsgSendQueue() {
  char *sTmpMempoint;
  CMemory *p_memory = CMemory::GetInstance();

  while (!m_MsgSendQueue.empty()) {
    sTmpMempoint = m_MsgSendQueue.front();
    m_MsgSendQueue.pop_front();
    p_memory->FreeMemory(sTmpMempoint);
  }
  return;
}

// 主动关闭一个连接时的资源释放
void CSocket::zdClosesocketProc(lphps_connection_t p_Conn) {
  if (m_ifkickTimeCount == 1) {
    DeleteFromTimerQueue(p_Conn);
  }
  if (p_Conn->fd != -1) {
    close(p_Conn->fd);
    p_Conn->fd = -1;
  }

  if (p_Conn->iThrowsendCount > 0)
    --p_Conn->iThrowsendCount;

  inRecyConnectQueue(p_Conn);
  return;
}

// 测试是否flood攻击成立，成立则返回true，否则返回false
bool CSocket::TestFlood(lphps_connection_t pConn) {
  struct timeval sCurrTime;
  uint64_t iCurrTime;
  bool reco = false;

  gettimeofday(&sCurrTime, NULL);                                     // 取得当前时间
  iCurrTime = (sCurrTime.tv_sec * 1000 + sCurrTime.tv_usec / 1000);   // 毫秒
  if ((iCurrTime - pConn->FloodkickLastTime) < m_floodTimeInterval) { // 两次收到包的时间 < 100毫秒
    // 发包太频繁记录
    pConn->FloodAttackCount++;
    pConn->FloodkickLastTime = iCurrTime;
  } else {
    pConn->FloodAttackCount = 0;
    pConn->FloodkickLastTime = iCurrTime;
  }

  if (pConn->FloodAttackCount >= m_floodKickCount) {
    reco = true; // 是否可以踢出此客户端
  }
  return reco;
}

int CSocket::hps_epoll_init() {
  m_epollhandle = epoll_create(m_worker_connections);
  if (m_epollhandle == -1) {
    hps_log_stderr(errno, "CSocket::hps_epoll_init()中epoll_create()失败.");
    exit(2);
  }

  // 连接池
  this->initConnection();

  // 监听socket绑定一个连接对象
  std::vector<lphps_listening_t>::iterator pos;
  for (pos = m_ListenSocketList.begin(); pos != m_ListenSocketList.end(); ++pos) {
    lphps_connection_t p_Conn = hps_get_connection((*pos)->fd);
    if (p_Conn == NULL) {
      hps_log_stderr(errno, "CSocket::hps_epoll_init()中hps_get_connection()失败.");
      exit(2); // 直接结束程序
    }
    p_Conn->listening = (*pos);  // 连接对象 和监听对象关联，方便通过连接对象找监听对象
    (*pos)->connection = p_Conn; // 监听对象 和连接对象关联，方便通过监听对象找连接对象

    // 监听端口的读事件处理，监听端口是用来等对方连接的发送三路握手的
    p_Conn->rhandler = &CSocket::hps_event_accept;

    // 往监听socket上增加监听事件
    if (hps_epoll_oper_event((*pos)->fd, EPOLL_CTL_ADD, EPOLLIN | EPOLLRDHUP, 0, p_Conn) == -1) {
      exit(2);
    }
  }
  return 1;
}

void CSocket::sendMsg(char *psendbuf) {
  CLock lock(&m_sendMessageQueueMutex);
  m_MsgSendQueue.push_back(psendbuf);
  ++m_iSendMsgQueueCount;

  if (sem_post(&m_semEventSendQueue) == -1) {
    hps_log_stderr(0, "CSocket::sendMsg()sem_post(&m_semEventSendQueue)失败.");
  }
  return;
}

int CSocket::hps_epoll_oper_event(int fd,             // socket
                                  uint32_t eventtype, // 事件类型 EPOLL_CTL_ADD，EPOLL_CTL_MOD，EPOLL_CTL_DEL
                                  uint32_t flag,      // 取决于eventtype
                                  int bcaction,       // 补充标志
                                  lphps_connection_t pConn) {
  struct epoll_event ev;
  memset(&ev, 0, sizeof(ev));

  if (eventtype == EPOLL_CTL_ADD) {
    ev.events = flag;
    pConn->events = flag;
  } else if (eventtype == EPOLL_CTL_MOD) {
    ev.events = pConn->events;
    if (bcaction == 0) {
      ev.events |= flag; // 增加标记
    } else if (bcaction == 1) {
      ev.events &= ~flag; // 去掉某个标记
    } else {
      ev.events = flag; // 覆盖
    }
    pConn->events = ev.events;
  } else {
    // ... 待扩展
    return 1;
  }

  ev.data.ptr = (void *)pConn;

  if (epoll_ctl(m_epollhandle, eventtype, fd, &ev) == -1) {
    hps_log_stderr(errno, "CSocket::hps_epoll_oper_event()中epoll_ctl(%d,%ud,%ud,%d)失败.", fd, eventtype, flag,
                   bcaction);
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
int CSocket::hps_epoll_process_events(int timer) {
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
      hps_log_error_core(HPS_LOG_INFO, errno, "CSocket::hps_epoll_process_events()中epoll_wait()失败!");
      return 1;
    } else {
      hps_log_error_core(HPS_LOG_ALERT, errno, "CSocket::hps_epoll_process_events()中epoll_wait()失败!");
      return 0;
    }
  }
  if (events == 0) {
    if (timer != -1) {
      return 1; // 阻塞时间到了
    }
    // 一直阻塞的情况下返回0，发生错误
    hps_log_error_core(HPS_LOG_ALERT, 0, "CSocket::hps_epoll_process_events()中epoll_wait()没超时却没返回任何事件!");
    return 0;
  }

  // 会惊群，一个telnet上来，4个 worker 进程都会被惊动，都将执行以下代码：
  // hps_log_stderr(errno,"惊群测试1:%d",events);

  // 收到事件
  lphps_connection_t p_Conn;
  // uintptr_t          instance;
  uint32_t revents;

  for (int i = 0; i < events; ++i) {
    // events 收集到的事件数量
    p_Conn = (lphps_connection_t)(m_events[i].data.ptr); // hps_epoll_add_event()给进去的
    // instance = (uintptr_t)p_Conn & 1;                            // 取出标志位
    // p_Conn = (lphps_connection_t)((uintptr_t)p_Conn & (uintptr_t)~1); // 去除 instance 标志位，取 p_Conn 真正地址

    // if (p_Conn->fd == -1) {
    //   /* 过滤过期事件：
    //     用epoll_wait取得三个事件，处理第一个事件时，因为业务需要，需要把这个连接关闭，会把c->fd设置为-1；
    //     第二个事件照常处理；
    //     假如第三个事件，跟第一个事件对应的是同一个连接，那这个条件就会成立，这种事件，属于过期事件，不应该处理
    //   */
    //   hps_log_error_core(HPS_LOG_DEBUG, 0, "CSocket::hps_epoll_process_events()中遇到了fd=-1的过期事件:%p.", p_Conn);
    //   continue;
    // }

    // if (p_Conn->instance != instance) {
    //   //===----------------------------- 过滤过期事件 ------------------------------===//
    //   //  a. 处理第一个事件时，因为业务需要，把这个连接关闭，同时设置c->fd = -1，
    //   //     并且调用hps_free_connection将该连接归还给连接池；
    //   //  b. 处理第二个事件，恰好第二个事件是建立新连接事件，调用hps_get_connection从连接池中取出的连接
    //   //     可能就是刚刚释放的第一个事件对应的连接池中的连接，又因为a中套接字被释放了，所以会被操作系统拿来复用
    //   //     复用给了b；
    //   //  c. 当处理第三个事件时，第三个事件其实是已经过期的，应该不处理。
    //   //
    //   //  解决：当调用hps_get_connection从连接池中获取一个新连接时，将instance标志位置反，所以这个条件如果不成立，
    //   //    说明这个连接已经被挪作他用

    //   hps_log_error_core(HPS_LOG_DEBUG, 0, "CSocket::hps_epoll_process_events()中遇到了instance值改变的过期事件:%p.",
    //                      p_Conn);
    //   continue;
    // }

    // 正常事件
    revents = m_events[i].events;
    // if (revents & (EPOLLERR | EPOLLHUP)) {
    //   // 客户端断连
    //   revents |= EPOLLIN | EPOLLOUT;
    // }
    if (revents & EPOLLIN) {
      (this->*(p_Conn->rhandler))(p_Conn);
    }

    if (revents & EPOLLOUT) {
      if (revents & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
        // EPOLLERR：对应的连接发生错误                     8     = 1000
        // EPOLLHUP：对应的连接被挂起                       16    = 0001 0000
        // EPOLLRDHUP：表示TCP连接的远端关闭或者半关闭连接   8192   = 0010  0000   0000   0000
        --p_Conn->iThrowsendCount;
      } else {
        // 数据没有发送完毕，由系统驱动来发送 CSocket::hps_write_request_handler()
        (this->*(p_Conn->whandler))(p_Conn);
      }
    }
  }
  return 1;
}
