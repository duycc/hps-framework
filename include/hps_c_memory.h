//===--------------------------- include/hps_c_memory.h - [HP-Server] -----------------------------------*- C++ -*-===//
// brief :
//   Memory manage
//
// author: YongDu
// date  : 2021-09-19
//===--------------------------------------------------------------------------------------------------------------===//

#if !defined(__HPS_C_MEMORY_H__)
#define __HPS_C_MEMORY_H__

#include <stddef.h>

class CMemory {
  public:
    static CMemory* GetInstance() {
        if (m_instance == NULL) {
            // lock
            if (m_instance == NULL) {
                m_instance = new CMemory();
                static CGarCollection gc;
            }
            // unlock
        }
        return m_instance;
    }
    class CGarCollection {
      public:
        ~CGarCollection() {
            if (CMemory::m_instance) {
                delete CMemory::m_instance;
                CMemory::m_instance = NULL;
            }
        }
    };

    void* AllocMemory(int memCount, bool ifmemset);

    void FreeMemory(void* point);

    ~CMemory(){};

  private:
    CMemory() {}

    static CMemory* m_instance;
};

#endif // __HPS_C_MEMORY_H__
