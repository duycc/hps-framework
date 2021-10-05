//===--------------------------- net/hps_c_socket_time.cpp - [HP-Server] --------------------------------*- C++ -*-===//
// brief :
//
//
// author: YongDu
// date  : 2021-10-05
//===--------------------------------------------------------------------------------------------------------------===//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>

#include "hps_c_conf.h"
#include "hps_macro.h"
#include "hps_global.h"
#include "hps_func.h"
#include "hps_c_socket.h"
#include "hps_c_memory.h"
#include "hps_c_lockmutex.h"

void CSocket::AddToTimerQueue(lphps_connection_t pConn) {
  CMemory *p_memory = CMemory::GetInstance();

  time_t futtime = time(NULL);
  futtime += m_iWaitTime;

  CLock lock(&m_timequeueMutex);
  LPSTRUC_MSG_HEADER tmpMsgHeader = (LPSTRUC_MSG_HEADER)p_memory->AllocMemory(m_iLenMsgHeader, false);
  tmpMsgHeader->pConn = pConn;
  tmpMsgHeader->iCurrsequence = pConn->iCurrsequence;
  m_timerQueuemap.insert(std::make_pair(futtime, tmpMsgHeader));
  m_cur_size_++;
  m_timer_value_ = GetEarliestTime();
  return;
}

// 调用者负责互斥，调用者确保m_timeQueuemap中一定不为空
time_t CSocket::GetEarliestTime() { return m_timerQueuemap.begin()->first; }

LPSTRUC_MSG_HEADER CSocket::RemoveFirstTimer() {
  std::multimap<time_t, LPSTRUC_MSG_HEADER>::iterator pos;
  LPSTRUC_MSG_HEADER p_tmp;
  if (m_cur_size_ <= 0) {
    return NULL;
  }
  pos = m_timerQueuemap.begin();
  p_tmp = pos->second;
  m_timerQueuemap.erase(pos);
  --m_cur_size_;
  return p_tmp;
}

// 获取超时连接
LPSTRUC_MSG_HEADER CSocket::GetOverTimeTimer(time_t cur_time) {
  CMemory *p_memory = CMemory::GetInstance();
  LPSTRUC_MSG_HEADER ptmp;

  if (m_cur_size_ == 0 || m_timerQueuemap.empty())
    return NULL;

  time_t earliesttime = GetEarliestTime();
  if (earliesttime <= cur_time) {
    ptmp = RemoveFirstTimer();

    // 因为需要判断下次超时的时间，把此节点再进行保存
    time_t newinqueutime = cur_time + (m_iWaitTime);
    LPSTRUC_MSG_HEADER tmpMsgHeader = (LPSTRUC_MSG_HEADER)p_memory->AllocMemory(sizeof(STRUC_MSG_HEADER), false);
    tmpMsgHeader->pConn = ptmp->pConn;
    tmpMsgHeader->iCurrsequence = ptmp->iCurrsequence;
    m_timerQueuemap.insert(std::make_pair(newinqueutime, tmpMsgHeader));
    m_cur_size_++;

    if (m_cur_size_ > 0) {
      m_timer_value_ = GetEarliestTime();
    }
    return ptmp;
  }
  return NULL;
}

// 踢出超时用户
void CSocket::DeleteFromTimerQueue(lphps_connection_t pConn) {
  std::multimap<time_t, LPSTRUC_MSG_HEADER>::iterator pos, posend;
  CMemory *p_memory = CMemory::GetInstance();

  CLock lock(&m_timequeueMutex);

lblMTQM:
  pos = m_timerQueuemap.begin();
  posend = m_timerQueuemap.end();
  for (; pos != posend; ++pos) {
    if (pos->second->pConn == pConn) {
      p_memory->FreeMemory(pos->second);
      m_timerQueuemap.erase(pos);
      --m_cur_size_;
      goto lblMTQM;
    }
  }
  if (m_cur_size_ > 0) {
    m_timer_value_ = GetEarliestTime();
  }
  return;
}

void CSocket::clearAllFromTimerQueue() {
  std::multimap<time_t, LPSTRUC_MSG_HEADER>::iterator pos, posend;

  CMemory *p_memory = CMemory::GetInstance();
  pos = m_timerQueuemap.begin();
  posend = m_timerQueuemap.end();
  for (; pos != posend; ++pos) {
    p_memory->FreeMemory(pos->second);
    --m_cur_size_;
  }
  m_timerQueuemap.clear();
}

// 时间队列监视和处理线程，处理到期不发心跳包的用户
void *CSocket::ServerTimerQueueMonitorThread(void *threadData) {
  ThreadItem *pThread = static_cast<ThreadItem *>(threadData);
  CSocket *pSocketObj = pThread->_pThis;

  time_t absolute_time, cur_time;
  int err;

  while (g_stopEvent == 0) {
    if (pSocketObj->m_cur_size_ > 0) {
      absolute_time = pSocketObj->m_timer_value_;
      cur_time = time(NULL);
      if (absolute_time < cur_time) {
        std::list<LPSTRUC_MSG_HEADER> m_lsIdleList;
        LPSTRUC_MSG_HEADER result;

        err = pthread_mutex_lock(&pSocketObj->m_timequeueMutex);
        if (err != 0)
          hps_log_stderr(err, "CSocket::ServerTimerQueueMonitorThread()中pthread_mutex_lock()失败，返回的错误码为%d!",
                         err);
        while ((result = pSocketObj->GetOverTimeTimer(cur_time)) != NULL) {
          m_lsIdleList.push_back(result);
        }
        err = pthread_mutex_unlock(&pSocketObj->m_timequeueMutex);
        if (err != 0)
          hps_log_stderr(err, "CSocket::ServerTimerQueueMonitorThread()pthread_mutex_unlock()失败，返回的错误码为%d!",
                         err);
        LPSTRUC_MSG_HEADER tmpmsg;
        while (!m_lsIdleList.empty()) {
          tmpmsg = m_lsIdleList.front();
          m_lsIdleList.pop_front();
          pSocketObj->procPingTimeOutChecking(tmpmsg, cur_time);
        }
      }
    }

    usleep(500 * 1000);
  }

  return (void *)0;
}

// 心跳包检测时间到，该去检测心跳包是否超时的事宜，本函数只是把内存释放，子类执行具体的操作
void CSocket::procPingTimeOutChecking(LPSTRUC_MSG_HEADER tmpmsg, time_t cur_time) {
  CMemory *p_memory = CMemory::GetInstance();
  p_memory->FreeMemory(tmpmsg);
}
