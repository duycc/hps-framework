//===--------------------------- include/hps_c_lockmutex.h - [HP-Server] --------------------------------*- C++ -*-===//
// brief :
//   Manage mutex
//
// author: YongDu
// date  : 2021-09-22
//===--------------------------------------------------------------------------------------------------------------===//

#if !defined(__HPS_C_LOCKMUTEX_H__)
#define __HPS_C_LOCKMUTEX_H__

#include <pthread.h>

// 自动管理互斥锁
class CLock {
  public:
    CLock(pthread_mutex_t* pMutex) {
        m_pMutex = pMutex;
        pthread_mutex_lock(m_pMutex);
        return;
    }

    ~CLock() {
        pthread_mutex_unlock(m_pMutex);
        return;
    }

  private:
    pthread_mutex_t* m_pMutex;
};

#endif // __HPS_C_LOCKMUTEX_H__
