//===--------------------------- include/hps_c_socket.h - HP-Server -------------------------------------*- C++ -*-===//
// brief :
//   Socket related operations
//
// author: YongDu
// date  : 2021-09-16
//===--------------------------------------------------------------------------------------------------------------===//

#if !defined(__HPS_C_SOCKET_H__)
#define __HPS_C_SOCKET_H__

#include <sys/epoll.h>
#include <sys/socket.h>
#include <vector>

#define HPS_LISTEN_BACKLOG 511 // 已完成连接队列最大值
#define HPS_MAX_EVENTS 512     // epoll_wait 一次最多接收事件数量

typedef struct hps_listening_s  hps_listening_t, *lphps_listening_t;
typedef struct hps_connection_s hps_connection_t, *lphps_connection_t;
typedef class CSocekt           CSocekt;

typedef void (CSocekt::*ngx_event_handler_pt)(lphps_connection_t c);

// 监听端口信息
struct hps_listening_s {
  int port; // 监听的端口号
  int fd;   // socket

  lphps_connection_t connection; // 连接池中一条连接
};

// 一条 TCP 连接信息
struct hps_connection_s {
  int               fd;
  lphps_listening_t listening;     // 对应的监听端口信息
  unsigned          instance : 1;  // 失效标志位: 0 有效 1 失效
  uint64_t          iCurrsequence; // 每次分配出去时+1，一定程度上检测错包废包
  struct sockaddr   s_sockaddr;    // 保存对方地址信息

  uint8_t r_ready; // 读准备好标记
  uint8_t w_ready; // 写准备好标记

  ngx_event_handler_pt rhandler; // 读事件处理
  ngx_event_handler_pt whandler; // 写事件处理

  lphps_connection_t data; // 后继指针，指向下一个本类型对象，用于把"空闲"的连接池对象构成一个单向链表，方便取用
};

// Socket 相关类
class CSocekt {
public:
  CSocekt();
  virtual ~CSocekt();

  // Socket 初始化
  virtual bool Initialize();

  int hps_epoll_init(); // epoll 初始化

  // epoll增加事件
  int hps_epoll_add_event(int fd, int readevent, int writeevent, uint32_t otherflag, uint32_t eventtype,
                          lphps_connection_t c);
  int hps_epoll_process_events(int timer); // epoll等待接收和处理事件

private:
  void ReadConf();                    // 读配置项
  bool hps_open_listening_sockets();  // 监听端口
  void hps_close_listening_sockets(); // 关闭监听套接字
  bool setnonblocking(int sockfd);    // 套接字设置非阻塞

  // 业务处理函数
  void hps_event_accept(lphps_connection_t oldc);      // 建立新连接
  void hps_wait_request_handler(lphps_connection_t c); // 来数据时的读处理函数

  void hps_close_accepted_connection(lphps_connection_t c); // 关闭连接，释放资源

  // 获取对端信息
  size_t hps_sock_ntop(struct sockaddr *sa, int port, u_char *text, size_t len);

  // 连接池操作
  lphps_connection_t hps_get_connection(int isock);
  void               hps_free_connection(lphps_connection_t c);

private:
  int m_worker_connections; // epoll最大连接数量
  int m_ListenPortCount;    // 监听的端口数量
  int m_epollhandle;        // epoll_create返回的句柄

  lphps_connection_t m_pconnections;      // 连接池首地址
  lphps_connection_t m_pfree_connections; // 连接池中空闲连接链表头

  int m_connection_n;      // 连接池大小
  int m_free_connection_n; // 空闲连接数量

  std::vector<lphps_listening_t> m_ListenSocketList; // 监听套接字队列

  struct epoll_event m_events[HPS_MAX_EVENTS]; // 存储 epoll_wait() 返回的事件
};
#endif // __HPS_C_SOCKET_H__
