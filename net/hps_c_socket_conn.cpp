//===--------------------------- net/hps_c_socket_conn.cpp - [HP-Server] --------------------------------*- C++ -*-===//
// brief :
//
//
// author: YongDu
// date  : 2021-09-16
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
#include "hps_c_lockmutex.h"
#include "hps_c_socket.h"
#include "hps_func.h"
#include "hps_global.h"
#include "hps_macro.h"
#include "hps_c_memory.h"

hps_connection_s::hps_connection_s() {
  iCurrsequence = 0;
  pthread_mutex_init(&logicProcMutex, NULL);
  return;
}

hps_connection_s::~hps_connection_s() {
  pthread_mutex_destroy(&logicProcMutex);
  return;
}

void hps_connection_s::getOneToUse() {
  ++iCurrsequence;
  fd = -1;
  curStat = _PKG_HD_INIT;
  precvbuf = dataHeadInfo;
  irecvlen = sizeof(COMM_PKG_HEADER);

  precvMemPointer = NULL;
  iThrowsendCount = 0;
  psendMemPointer = NULL;
  events = 0;
  lastPingTime = time(NULL);

  FloodAttackCount = 0;
  FloodkickLastTime = 0;
  iSendCount = 0;
  return;
}

void hps_connection_s::putOneToFree() {
  ++iCurrsequence;

  if (precvMemPointer != NULL) {
    CMemory::GetInstance()->FreeMemory(precvMemPointer);
    precvMemPointer = NULL;
  }
  if (psendMemPointer != NULL) {
    CMemory::GetInstance()->FreeMemory(psendMemPointer);
    psendMemPointer = NULL;
  }

  iThrowsendCount = 0;
  return;
}

void CSocket::initConnection() {
  lphps_connection_t p_Conn;
  CMemory *p_memory = CMemory::GetInstance();

  int ilenconnpool = sizeof(hps_connection_t);
  for (int i = 0; i < m_worker_connections; ++i) {
    p_Conn = (lphps_connection_t)p_memory->AllocMemory(ilenconnpool, true);
    p_Conn = new (p_Conn) hps_connection_t();
    p_Conn->getOneToUse();
    m_connectionList.push_back(p_Conn);
    m_freeconnectionList.push_back(p_Conn);
  }
  m_free_connection_n = m_total_connection_n = m_connectionList.size();
  return;
}

void CSocket::clearConnection() {
  lphps_connection_t p_Conn;
  CMemory *p_memory = CMemory::GetInstance();

  while (!m_connectionList.empty()) {
    p_Conn = m_connectionList.front();
    m_connectionList.pop_front();
    p_Conn->~hps_connection_t(); // 定位new构造的
    p_memory->FreeMemory(p_Conn);
  }
  return;
}

// 连接池中获取一个空闲连接
lphps_connection_t CSocket::hps_get_connection(int isock) {
  // 可能有其他线程要访问m_freeconnectionList，m_connectionList
  CLock lock(&m_connectionMutex);

  if (!m_freeconnectionList.empty()) {
    lphps_connection_t p_Conn = m_freeconnectionList.front();
    m_freeconnectionList.pop_front();
    p_Conn->getOneToUse();
    --m_free_connection_n;
    p_Conn->fd = isock;
    return p_Conn;
  }

  // 没有空闲连接
  CMemory *p_memory = CMemory::GetInstance();
  lphps_connection_t p_Conn = (lphps_connection_t)p_memory->AllocMemory(sizeof(hps_connection_t), true);
  p_Conn = new (p_Conn) hps_connection_t();
  p_Conn->getOneToUse();
  m_connectionList.push_back(p_Conn);
  ++m_total_connection_n;
  p_Conn->fd = isock;
  return p_Conn;
}

// pConn所代表的连接到到连接池中
void CSocket::hps_free_connection(lphps_connection_t pConn) {
  CLock lock(&m_connectionMutex);
  pConn->putOneToFree();
  m_freeconnectionList.push_back(pConn);
  ++m_free_connection_n;
  return;
}

void CSocket::inRecyConnectQueue(lphps_connection_t pConn) {
  bool iffind = false;
  CLock lock(&m_recyconnqueueMutex);
  for (auto *conn : m_recyconnectionList) {
    if (conn == pConn) {
      iffind = true;
      break;
    }
  }
  if (iffind) { // 防止重复释放
    return;
  }

  // hps_log_stderr(0, "CSocket::inRecyConnectQueue()执行，连接入到回收队列中.");

  pConn->inRecyTime = time(NULL);
  ++pConn->iCurrsequence;
  m_recyconnectionList.push_back(pConn);
  ++m_totol_recyconnection_n;
  --m_onlineUserCount;
  return;
}

void *CSocket::ServerSendQueueThread(void *threadData) {
  ThreadItem *pThread = static_cast<ThreadItem *>(threadData);
  CSocket *pSocketObj = pThread->_pThis;
  int err;

  std::list<char *>::iterator pos, pos2, posend;

  char *pMsgBuf;
  LPSTRUC_MSG_HEADER pMsgHeader;
  LPCOMM_PKG_HEADER pPkgHeader;
  lphps_connection_t p_Conn;
  unsigned short itmp;
  ssize_t sendsize;

  CMemory *p_memory = CMemory::GetInstance();

  while (g_stopEvent == 0) {
    if (sem_wait(&pSocketObj->m_semEventSendQueue) == -1) {
      if (errno != EINTR)
        hps_log_stderr(errno, "CSocket::ServerSendQueueThread()中sem_wait(&pSocketObj->m_semEventSendQueue)失败.");
    }

    if (g_stopEvent != 0)
      break;

    if (pSocketObj->m_iSendMsgQueueCount > 0) {
      err = pthread_mutex_lock(&pSocketObj->m_sendMessageQueueMutex);
      if (err != 0)
        hps_log_stderr(err, "CSocket::ServerSendQueueThread()中pthread_mutex_lock()失败，返回的错误码为%d!", err);

      pos = pSocketObj->m_MsgSendQueue.begin();
      posend = pSocketObj->m_MsgSendQueue.end();

      while (pos != posend) {
        pMsgBuf = (*pos);
        pMsgHeader = (LPSTRUC_MSG_HEADER)pMsgBuf;
        pPkgHeader = (LPCOMM_PKG_HEADER)(pMsgBuf + pSocketObj->m_iLenMsgHeader);
        p_Conn = pMsgHeader->pConn;

        // 过滤过期包
        if (p_Conn->iCurrsequence != pMsgHeader->iCurrsequence) {
          pos2 = pos;
          pos++;
          pSocketObj->m_MsgSendQueue.erase(pos2);
          --pSocketObj->m_iSendMsgQueueCount;
          p_memory->FreeMemory(pMsgBuf);
          continue;
        }

        if (p_Conn->iThrowsendCount > 0) {
          // 靠系统驱动来发送消息，此处不能再发送
          pos++;
          continue;
        }

        --p_Conn->iSendCount;

        p_Conn->psendMemPointer = pMsgBuf;
        pos2 = pos;
        pos++;
        pSocketObj->m_MsgSendQueue.erase(pos2);
        --pSocketObj->m_iSendMsgQueueCount;
        p_Conn->psendbuf = (char *)pPkgHeader;
        itmp = ntohs(pPkgHeader->pkgLen);
        p_Conn->isendlen = itmp;

        // hps_log_stderr(0, "即将发送数据%ud。", p_Conn->isendlen);

        sendsize = pSocketObj->sendproc(p_Conn, p_Conn->psendbuf, p_Conn->isendlen);
        if (sendsize > 0) {
          if (sendsize == p_Conn->isendlen) {
            p_memory->FreeMemory(p_Conn->psendMemPointer);
            p_Conn->psendMemPointer = NULL;
            p_Conn->iThrowsendCount = 0;
            // hps_log_stderr(0, "CSocket::ServerSendQueueThread()中数据发送完毕，很好。");
          } else {

            p_Conn->psendbuf = p_Conn->psendbuf + sendsize;
            p_Conn->isendlen = p_Conn->isendlen - sendsize;

            ++p_Conn->iThrowsendCount;
            if (pSocketObj->hps_epoll_oper_event(p_Conn->fd, EPOLL_CTL_MOD, EPOLLOUT, 0, p_Conn) == -1) {
              hps_log_stderr(errno, "CSocket::ServerSendQueueThread()hps_epoll_oper_event()失败.");
            }

            // hps_log_stderr(
            //     errno,
            //     "CSocket::ServerSendQueueThread()中数据没发送完毕【发送缓冲区满】，整个要发送%d，实际发送了%d。",
            //     p_Conn->isendlen, sendsize);
          }
          continue;
        } else if (sendsize == 0) {
          p_memory->FreeMemory(p_Conn->psendMemPointer);
          p_Conn->psendMemPointer = NULL;
          p_Conn->iThrowsendCount = 0;
          continue;
        } else if (sendsize == -1) {
          // 标记发送缓冲区满了，需要通过epoll事件来驱动消息的继续发送
          ++p_Conn->iThrowsendCount;
          if (pSocketObj->hps_epoll_oper_event(p_Conn->fd, EPOLL_CTL_MOD, EPOLLOUT, 0, p_Conn) == -1) {
            hps_log_stderr(errno, "CSocket::ServerSendQueueThread()中hps_epoll_add_event()_2失败.");
          }
          continue;
        } else {
          p_memory->FreeMemory(p_Conn->psendMemPointer);
          p_Conn->psendMemPointer = NULL;
          p_Conn->iThrowsendCount = 0;
          continue;
        }
      }

      err = pthread_mutex_unlock(&pSocketObj->m_sendMessageQueueMutex);
      if (err != 0)
        hps_log_stderr(err, "CSocket::ServerSendQueueThread()pthread_mutex_unlock()失败，返回的错误码为%d!", err);
    }
  }
  return (void *)0;
}

// 处理连接回收线程
void *CSocket::ServerRecyConnectionThread(void *threadData) {
  ThreadItem *pThread = static_cast<ThreadItem *>(threadData);
  CSocket *pSocketObj = pThread->_pThis;

  time_t currtime;
  int err;
  lphps_connection_t p_Conn;

  std::list<lphps_connection_t>::iterator pos, posend;

  while (1) {
    usleep(200 * 1000); // 阻塞 200ms

    if (pSocketObj->m_totol_recyconnection_n > 0) {
      currtime = time(NULL);
      err = pthread_mutex_lock(&pSocketObj->m_recyconnqueueMutex);
      if (err != 0)
        hps_log_stderr(err, "CSocket::ServerRecyConnectionThread()中pthread_mutex_lock()失败，返回的错误码为%d!", err);

    lblRRTD:
      pos = pSocketObj->m_recyconnectionList.begin();
      posend = pSocketObj->m_recyconnectionList.end();
      for (; pos != posend; ++pos) {
        p_Conn = (*pos);
        if (((p_Conn->inRecyTime + pSocketObj->m_RecyConnectionWaitTime) > currtime) && (g_stopEvent == 0)) {
          continue; // 未到释放时间
        }

        // ... 一些判断，待扩展
        if (p_Conn->iThrowsendCount > 0) {
          hps_log_stderr(
              0, "CSocket::ServerRecyConnectionThread()中到释放时间却发现p_Conn.iThrowsendCount>0，这个不该发生");
        }

        --pSocketObj->m_totol_recyconnection_n;
        pSocketObj->m_recyconnectionList.erase(pos);

        // hps_log_stderr(0, "CSocket::ServerRecyConnectionThread()执行，连接%d被归还.", p_Conn->fd);

        pSocketObj->hps_free_connection(p_Conn);
        goto lblRRTD;
      }
      err = pthread_mutex_unlock(&pSocketObj->m_recyconnqueueMutex);
      if (err != 0)
        hps_log_stderr(err, "CSocket::ServerRecyConnectionThread()pthread_mutex_unlock()失败，返回的错误码为%d!", err);
    }

    if (g_stopEvent == 1) // 退出整个程序
    {
      if (pSocketObj->m_totol_recyconnection_n > 0) {
        err = pthread_mutex_lock(&pSocketObj->m_recyconnqueueMutex);
        if (err != 0)
          hps_log_stderr(err, "CSocket::ServerRecyConnectionThread()中pthread_mutex_lock2()失败，返回的错误码为%d!",
                         err);

      lblRRTD2:
        pos = pSocketObj->m_recyconnectionList.begin();
        posend = pSocketObj->m_recyconnectionList.end();
        for (; pos != posend; ++pos) {
          p_Conn = (*pos);
          --pSocketObj->m_totol_recyconnection_n;
          pSocketObj->m_recyconnectionList.erase(pos);
          pSocketObj->hps_free_connection(p_Conn);
          goto lblRRTD2;
        }
        err = pthread_mutex_unlock(&pSocketObj->m_recyconnqueueMutex);
        if (err != 0)
          hps_log_stderr(err, "CSocket::ServerRecyConnectionThread()pthread_mutex_unlock2()失败，返回的错误码为%d!",
                         err);
      }
      break;
    }
  }
  return (void *)0;
}

// 当 accept 成功，后续流程失败时，需要回收这个 socket
void CSocket::hps_close_connection(lphps_connection_t pConn) {
  hps_free_connection(pConn);
  if (pConn->fd != -1) {
    close(pConn->fd);
    pConn->fd = -1;
  }
  return;
}
