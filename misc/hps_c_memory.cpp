//===--------------------------- misc/hps_c_memory.cpp - [HP-Server] ------------------------------------*- C++ -*-===//
// brief :
//   Memory manage
//
// author: YongDu
// date  : 2021-09-19
//===--------------------------------------------------------------------------------------------------------------===//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hps_c_memory.h"

CMemory* CMemory::m_instance = NULL;

/**
 * @brief 分配内存
 *
 * @param memCount 分配大小
 * @param ifmemset 分配时是否需要初始化
 * @return void*
 */
void* CMemory::AllocMemory(int memCount, bool ifmemset) {
    void* tmpData = (void*)new char[memCount];
    if (ifmemset) {
        memset(tmpData, 0, memCount);
    }
    return tmpData;
}

void CMemory::FreeMemory(void* point) {
    delete[]((char*)point);
    return;
}
