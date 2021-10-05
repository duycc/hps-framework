//===--------------------------- include/hps_c_socket.h - HP-Server -------------------------------------*- C++ -*-===//
// brief :
//   Socket related operations
//
// author: YongDu
// date  : 2021-09-16
//===--------------------------------------------------------------------------------------------------------------===//

#if !defined(__HPS_C_SOCKET_H__)
#define __HPS_C_SOCKET_H__

#include <atomic>
#include <list>
#include <pthread.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <vector>
#include <semaphore.h>
#include <map>

#include "hps_comm.h"

#define HPS_LISTEN_BACKLOG 511 // 已完成连接队列最大值
#define HPS_MAX_EVENTS 512     // epoll_wait 一次最多接收事件数量

typedef struct hps_listening_s hps_listening_t, *lphps_listening_t;
typedef struct hps_connection_s hps_connection_t, *lphps_connection_t;
typedef class CSocket CSocket;

typedef void (CSocket::*hps_event_handler_pt)(lphps_connection_t c);

// 监听端口信息
struct hps_listening_s {
  int port; // 监听的端口号
  int fd;   // socket

  lphps_connection_t connection; // 连接池中一条连接
};

// 一条 TCP 连接信息
struct hps_connection_s {
  hps_connection_s();
  virtual ~hps_connection_s();
  void getOneToUse();
  void putOneToFree();

  int fd;
  lphps_listening_t listening; // 对应的监听端口信息
  // unsigned          instance : 1;  // 失效标志位: 0 有效 1 失效
  uint64_t iCurrsequence;     // 每次分配出去时+1，一定程度上检测错包废包
  struct sockaddr s_sockaddr; // 保存对方地址信息

  hps_event_handler_pt rhandler; // 读事件处理
  hps_event_handler_pt whandler; // 写事件处理

  uint32_t events; // epoll事件相关

  // 收包相关信息
  unsigned char curStat;             // 当前收包状态
  char dataHeadInfo[_DATA_BUFSIZE_]; // 存放包头信息
  char *precvbuf;                    // 接收缓冲区的头指针
  unsigned int irecvlen;             // 需要接受多少数据
  char *precvMemPointer;

  pthread_mutex_t logicProcMutex; // 逻辑处理互斥量

  // 发包相关
  std::atomic<int> iThrowsendCount; // 发送缓冲区满了
  char *psendMemPointer;            // 发送完成后释放用，消息头 + 包头 + 包体
  char *psendbuf;                   // 发送数据的缓冲区的头指针，包头+包体
  unsigned int isendlen;            // 要发送多少数据

  time_t inRecyTime;   // 判断回收时间
  time_t lastPingTime; // 心跳包检测

  lphps_connection_t next; // 后继指针，指向下一个本类型对象，用于把"空闲"的连接池对象构成一个单向链表，方便取用
};

// 消息头
typedef struct _STRUC_MSG_HEADER {
  lphps_connection_t pConn; // 记录对应的连接
  uint64_t iCurrsequence;   // 收到数据包时记录对应连接的序号，过滤过期包
} STRUC_MSG_HEADER, *LPSTRUC_MSG_HEADER;

// Socket 相关类
class CSocket {
public:
  CSocket();
  virtual ~CSocket();

  // Socket 初始化
  virtual bool Initialize();
  virtual bool Initialize_subproc();
  virtual void Shutdown_subproc();

  virtual void threadRecvProcFunc(char *pMsgBuf); // 处理客户端请求
  virtual void procPingTimeOutChecking(LPSTRUC_MSG_HEADER tmpmsg, time_t cur_time);

  int hps_epoll_init(); // epoll 初始化

  // epoll操作事件
  int hps_epoll_oper_event(int fd, uint32_t eventtype, uint32_t flag, int bcaction, lphps_connection_t pConn);

  // epoll等待接收和处理事件
  int hps_epoll_process_events(int timer);

protected:
  // 数据发送相关
  void sendMsg(char *sendBuf);
  void zdClosesocketProc(lphps_connection_t p_Conn); // 主动关闭一个连接时的资源释放

private:
  void ReadConf();                    // 读配置项
  bool hps_open_listening_sockets();  // 监听端口
  void hps_close_listening_sockets(); // 关闭监听套接字
  bool setnonblocking(int sockfd);    // 套接字设置非阻塞

  // 业务处理函数
  void hps_event_accept(lphps_connection_t oldc);           // 建立新连接
  void hps_read_request_handler(lphps_connection_t c);      // 来数据时的读处理函数
  void hps_write_request_handler(lphps_connection_t pConn); // 数据发送时的写处理函数
  void hps_close_connection(lphps_connection_t c);          // 关闭连接，释放资源

  ssize_t recvproc(lphps_connection_t c, char *buff, ssize_t buflen); // 接收从客户端来的数据
  void hps_wait_request_handler_proc_p1(lphps_connection_t c);        // 包头收完整后的处理
  void hps_wait_request_handler_proc_plast(lphps_connection_t c);     // 收到一个完整包后的处理

  void clearMsgSendQueue();
  ssize_t sendproc(lphps_connection_t c, char *buff, ssize_t size); // 发送数据到客户端

  // 获取对端信息
  size_t hps_sock_ntop(struct sockaddr *sa, int port, u_char *text, size_t len);

  // 连接池操作
  void initConnection();  // 初始化连接池
  void clearConnection(); // 回收连接池

  lphps_connection_t hps_get_connection(int isock);
  void hps_free_connection(lphps_connection_t c);

  // 收集待回收连接
  void inRecyConnectQueue(lphps_connection_t pConn);

  // 时间相关的函数
  void AddToTimerQueue(lphps_connection_t pConn); // 设置踢出时钟
  time_t GetEarliestTime();                       // 定时器中取得最早的时间
  LPSTRUC_MSG_HEADER RemoveFirstTimer(); // 从m_timeQueuemap移除最早的时间，调用者负责互斥，本函数不互斥，
  LPSTRUC_MSG_HEADER GetOverTimeTimer(time_t cur_time); // 根据给的当前时间，从m_timeQueuemap找到超时的节点
  void DeleteFromTimerQueue(lphps_connection_t pConn); // 把指定用户tcp连接从timer移除
  void clearAllFromTimerQueue();                       // 清空时间队列

  // 线程处理函数
  static void *ServerSendQueueThread(void *threadData);      // 发送数据的线程
  static void *ServerRecyConnectionThread(void *threadData); // 回收连接的线程
  static void *ServerTimerQueueMonitorThread(void *threadData); // 时间队列监视线程，处理到期不发心跳包的用户

protected:
  // 网络通讯相关变量
  size_t m_iLenPkgHeader; // sizeof(COMM_PKG_HEADER);
  size_t m_iLenMsgHeader; // sizeof(STRUC_MSG_HEADER);

  int m_iWaitTime; // 检测心跳包时间

private:
  struct ThreadItem {
    pthread_t _Handle;
    CSocket *_pThis;
    bool ifrunning;

    ThreadItem(CSocket *pthis) : _pThis(pthis), ifrunning(false) {}
    ~ThreadItem() {}
  };

  int m_worker_connections; // epoll最大连接数量
  int m_ListenPortCount;    // 监听的端口数量
  int m_epollhandle;        // epoll_create返回的句柄

  // 连接池相关
  std::list<lphps_connection_t> m_connectionList;     // 连接池
  std::list<lphps_connection_t> m_freeconnectionList; // 空闲连接列表
  std::atomic<int> m_total_connection_n;              // 总连接数
  std::atomic<int> m_free_connection_n;               // 空闲连接数

  pthread_mutex_t m_connectionMutex;    // 连接相关互斥量，互斥m_freeconnectionList，m_connectionList
  pthread_mutex_t m_recyconnqueueMutex; // 连接回收队列相关的互斥量

  std::list<lphps_connection_t> m_recyconnectionList; // 待释放的连接
  std::atomic<int> m_totol_recyconnection_n;
  int m_RecyConnectionWaitTime; // 等待释放时间

  std::vector<lphps_listening_t> m_ListenSocketList; // 监听套接字队列
  struct epoll_event m_events[HPS_MAX_EVENTS];       // 存储 epoll_wait() 返回的事件

  std::list<char *> m_MsgSendQueue; // 发送数据消息队列
  std::atomic<int> m_iSendMsgQueueCount;

  std::vector<ThreadItem *> m_threadVector;
  pthread_mutex_t m_sendMessageQueueMutex;
  sem_t m_semEventSendQueue;

  // 时间相关
  int m_ifkickTimeCount;                                     // 是否开启踢人时钟，1：开启   0：不开启
  pthread_mutex_t m_timequeueMutex;                          // 和时间队列有关的互斥量
  std::multimap<time_t, LPSTRUC_MSG_HEADER> m_timerQueuemap; // 时间队列
  size_t m_cur_size_;                                        // 时间队列的尺寸
  time_t m_timer_value_;                                     // 当前计时队列头部时间值
};
#endif // __HPS_C_SOCKET_H__
