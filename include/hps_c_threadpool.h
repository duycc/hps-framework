//===--------------------------- include/hps_c_threadpool.h - [HP-Server] -------------------------------*- C++ -*-===//
// brief :
//   ThreadPool
//
// author: YongDu
// date  : 2021-09-22
//===--------------------------------------------------------------------------------------------------------------===//

#if !defined(__HPS_C_THREADPOOL_H__)
#define __HPS_C_THREADPOOL_H__

#include <atomic>
#include <pthread.h>
#include <vector>

// 线程池
class CThreadPool {
public:
  CThreadPool();
  ~CThreadPool();

  bool Create(int threadNum); // 创建线程池
  void StopAll();             // 退出线程池中的所有线程
  void Call(int irmqc);

private:
  static void *ThreadFunc(void *threadData); // 线程回调函数

private:
  struct ThreadItem {
    pthread_t    _Handle;   // 线程句柄
    CThreadPool *_pThis;    // 线程池的指针
    bool         ifrunning; // 是否启动

    ThreadItem(CThreadPool *pthis) : _pThis(pthis), ifrunning(false) {}
    ~ThreadItem() {}
  };

private:
  static pthread_mutex_t m_pthreadMutex; // 线程互斥锁
  static pthread_cond_t  m_pthreadCond;  // 线程同步条件变量
  static bool            m_shutdown;     // 线程退出标志

  int m_iThreadNum; // 需要创建的线程数量

  std::atomic<int> m_iRunningThreadNum; // 运行中的线程数
  time_t           m_iLastEmgTime;      // 上次线程不够使用的告警时间，防止日志太多

  std::vector<ThreadItem *> m_threadVector; // 线程容器
};

#endif // __HPS_C_THREADPOOL_H__
