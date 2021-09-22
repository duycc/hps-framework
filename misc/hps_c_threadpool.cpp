//===--------------------------- misc/hps_c_threadpool.cpp - [HP-Server] --------------------------------*- C++ -*-===//
// brief :
//   ThreadPool
//
// author: YongDu
// date  : 2021-09-22
//===--------------------------------------------------------------------------------------------------------------===//

#include <unistd.h>
#include <stdarg.h>

#include "hps_global.h"
#include "hps_func.h"
#include "hps_c_memory.h"
#include "hps_macro.h"

pthread_mutex_t CThreadPool::m_pthreadMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  CThreadPool::m_pthreadCond = PTHREAD_COND_INITIALIZER;

bool CThreadPool::m_shutdown = false;

CThreadPool::CThreadPool() {
  m_iRunningThreadNum = 0;
  m_iLastEmgTime = 0;
  return;
}

CThreadPool::~CThreadPool() {
  return; // 资源释放在StopAll()里
}

bool CThreadPool::Create(int threadNum) {
  ThreadItem *pNew;
  int         err;

  m_iThreadNum = threadNum; // 要创建的线程数量

  for (int i = 0; i < m_iThreadNum; ++i) {
    m_threadVector.push_back(pNew = new ThreadItem(this));
    err = pthread_create(&pNew->_Handle, NULL, ThreadFunc, pNew);
    if (err != 0) {
      hps_log_stderr(err, "CThreadPool::Create()创建线程%d失败，返回的错误码为%d!", i, err);
      return false;
    } else {
      hps_log_stderr(0, "CThreadPool::Create()创建线程%d成功,线程id=%d", pNew->_Handle);
    }
  }

  // 保证每个线程都启动并运行到pthread_cond_wait()之后此函数才返回
  std::vector<ThreadItem *>::iterator iter;
lblfor:
  for (iter = m_threadVector.begin(); iter != m_threadVector.end(); iter++) {
    if ((*iter)->ifrunning == false) {
      // 存在未启动完全的线程
      usleep(100 * 1000);
      goto lblfor;
    }
  }
  return true;
}

// 线程入口函数，当用pthread_create()创建线程后，ThreadFunc()函数立即执行
void *CThreadPool::ThreadFunc(void *threadData) {
  ThreadItem * pThread = static_cast<ThreadItem *>(threadData);
  CThreadPool *pThreadPoolObj = pThread->_pThis;

  char *   jobbuf = NULL;
  CMemory *p_memory = CMemory::GetInstance();
  int      err;

  pthread_t tid = pthread_self(); // 获取线程自身id
  while (true) {
    err = pthread_mutex_lock(&m_pthreadMutex);
    if (err != 0)
      hps_log_stderr(err, "CThreadPool::ThreadFunc()pthread_mutex_lock()失败，返回的错误码为%d!", err);

    // 存在惊群现象，需使用while循环来处理逻辑
    while ((jobbuf = g_socket.outMsgRecvQueue()) == NULL && m_shutdown == false) {
      if (pThread->ifrunning == false) {
        pThread->ifrunning = true;
      }
      // 阻塞在此处，并且释放m_pthreadMutex
      pthread_cond_wait(&m_pthreadCond, &m_pthreadMutex);
    }

    err = pthread_mutex_unlock(&m_pthreadMutex); // 解锁mutex
    if (err != 0)
      hps_log_stderr(err, "CThreadPool::ThreadFunc()pthread_mutex_unlock()失败，返回的错误码为%d!", err);

    if (m_shutdown) {
      if (jobbuf != NULL) {
        p_memory->FreeMemory(jobbuf);
      }
      break;
    }

    ++pThreadPoolObj->m_iRunningThreadNum;

    // g_socket.threadRecvProcFunc(jobbuf); // 处理消息队列中来的消息

    hps_log_stderr(0, "执行开始---begin,tid=%ui!", tid);
    sleep(5);
    hps_log_stderr(0, "执行结束---end,tid=%ui!", tid);

    p_memory->FreeMemory(jobbuf);
    --pThreadPoolObj->m_iRunningThreadNum;
  }

  return (void *)0;
}

// 停止所有线程
void CThreadPool::StopAll() {
  if (m_shutdown == true) {
    // 防止重复调用
    return;
  }
  m_shutdown = true;

  int err = pthread_cond_broadcast(&m_pthreadCond);
  if (err != 0) {
    hps_log_stderr(err, "CThreadPool::StopAll()中pthread_cond_broadcast()失败，返回的错误码为%d!", err);
    return;
  }

  std::vector<ThreadItem *>::iterator iter;
  for (iter = m_threadVector.begin(); iter != m_threadVector.end(); iter++) {
    pthread_join((*iter)->_Handle, NULL);
  }

  pthread_mutex_destroy(&m_pthreadMutex);
  pthread_cond_destroy(&m_pthreadCond);

  for (iter = m_threadVector.begin(); iter != m_threadVector.end(); iter++) {
    if (*iter)
      delete *iter;
  }
  std::vector<CThreadPool::ThreadItem *>().swap(m_threadVector);
  hps_log_stderr(0, "CThreadPool::StopAll()成功返回，线程池中线程全部正常结束!");
  return;
}

void CThreadPool::Call(int irmqc) {
  int err = pthread_cond_signal(&m_pthreadCond); // 唤醒一个阻塞在pthread_cond_wait()的线程
  if (err != 0) {
    hps_log_stderr(err, "CThreadPool::Call()中pthread_cond_signal()失败，返回的错误码为%d!", err);
  }

  if (m_iThreadNum == m_iRunningThreadNum) {
    // 线程不够用
    time_t currtime = time(NULL);
    if (currtime - m_iLastEmgTime > 10) {
      // 最少间隔10秒钟报一次线程池中线程不够用
      m_iLastEmgTime = currtime;
      hps_log_stderr(0, "CThreadPool::Call()中发现线程池中当前空闲线程数量为0，要考虑扩容线程池了!");
    }
  }
  return;
}
