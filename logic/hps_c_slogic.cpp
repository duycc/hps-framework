//===--------------------------- logic/hps_c_slogic.cpp - [HP-Server] -----------------------------------*- C++ -*-===//
// Brief :
//  业务逻辑处理相关
//
// Author: YongDu
// Date  : 2021-09-23
//===--------------------------------------------------------------------------------------------------------------===//

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
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
#include "hps_c_crc32.h"
#include "hps_c_lockmutex.h"
#include "hps_c_memory.h"
#include "hps_c_slogic.h"
#include "hps_func.h"
#include "hps_global.h"
#include "hps_logiccomm.h"
#include "hps_macro.h"

// 成员函数指针
typedef bool (CLogicSocket::*handler)(
    lphps_connection_t pConn,      // 连接池中连接
    LPSTRUC_MSG_HEADER pMsgHeader, // 消息头
    char* pPkgBody,                // 包体
    unsigned short iBodyLength);   // 包体长度

// 保存 成员函数指针 的数组
static const handler statusHandler[] = {
    // 前5个元素保留，以备将来增加一些基本服务器功能
    &CLogicSocket::_HandlePing, // 心跳包检测
    NULL,                       // 【1】
    NULL,                       // 【2】
    NULL,                       // 【3】
    NULL,                       // 【4】

    // 具体的业务逻辑
    &CLogicSocket::_HandleRegister, // 【5】 注册功能
    &CLogicSocket::_HandleLogIn,    // 【6】 登录功能
                                    // ... 待扩展

};

#define AUTH_TOTAL_COMMANDS sizeof(statusHandler) / sizeof(handler) // 所有逻辑功能处理个数

CLogicSocket::CLogicSocket() {}

CLogicSocket::~CLogicSocket() {}

bool CLogicSocket::Initialize() {
    bool bParentInit = CSocket::Initialize();
    return bParentInit;
}

// 处理收到的数据包
void CLogicSocket::threadRecvProcFunc(char* pMsgBuf) {
    LPSTRUC_MSG_HEADER pMsgHeader = (LPSTRUC_MSG_HEADER)pMsgBuf;
    LPCOMM_PKG_HEADER pPkgHeader = (LPCOMM_PKG_HEADER)(pMsgBuf + m_iLenMsgHeader);
    void* pPkgBody = NULL;
    unsigned short pkglen = ntohs(pPkgHeader->pkgLen); // 客户端指明的包宽度"包头+包体"

    if (m_iLenPkgHeader == pkglen) {
        // 只有包头
        if (pPkgHeader->crc32 != 0) {
            // 包头的 crc = 0
            return;
        }
        pPkgBody = NULL;
    } else {
        pPkgHeader->crc32 = ntohl(pPkgHeader->crc32);
        pPkgBody = (void*)(pMsgBuf + m_iLenMsgHeader + m_iLenPkgHeader);

        // 纯包体 CRC 检验
        int calccrc = CCRC32::GetInstance()->Get_CRC((unsigned char*)pPkgBody, pkglen - m_iLenPkgHeader);
        if (calccrc != pPkgHeader->crc32) {
            hps_log_stderr(0, "CLogicSocket::threadRecvProcFunc()中CRC错误，丢弃数据!");
            return;
        }
    }

    unsigned short imsgCode = ntohs(pPkgHeader->msgCode);
    lphps_connection_t p_Conn = pMsgHeader->pConn;

    // 客户端断开，过滤过期包
    if (p_Conn->iCurrsequence != pMsgHeader->iCurrsequence) {
        return;
    }

    // 过滤恶意包
    if (imsgCode >= AUTH_TOTAL_COMMANDS) {
        hps_log_stderr(0, "CLogicSocket::threadRecvProcFunc()中imsgCode=%d消息码不对!", imsgCode);
        return;
    }
    if (statusHandler[imsgCode] == NULL) {
        hps_log_stderr(0, "CLogicSocket::threadRecvProcFunc()中imsgCode=%d消息码找不到对应的处理函数!", imsgCode);
        return;
    }

    (this->*statusHandler[imsgCode])(p_Conn, pMsgHeader, (char*)pPkgBody, pkglen - m_iLenPkgHeader);
    return;
}

// 心跳包检测函数
void CLogicSocket::procPingTimeOutChecking(LPSTRUC_MSG_HEADER tmpmsg, time_t cur_time) {
    CMemory* p_memory = CMemory::GetInstance();

    if (tmpmsg->iCurrsequence == tmpmsg->pConn->iCurrsequence) { // 有效连接
        lphps_connection_t p_Conn = tmpmsg->pConn;

        // 超时踢出
        if (m_ifTimeOutKick == 1) {
            zdClosesocketProc(p_Conn);
        } else if ((cur_time - p_Conn->lastPingTime) > (m_iWaitTime * 3 + 10)) {
            // hps_log_stderr(0, "时间到不发心跳包，踢出去!");
            zdClosesocketProc(p_Conn);
        }

        p_memory->FreeMemory(tmpmsg);
    } else { // 此连接已断开
        p_memory->FreeMemory(tmpmsg);
    }
    return;
}

// 给客户端发送心跳包
void CLogicSocket::SendNoBodyPkgToClient(LPSTRUC_MSG_HEADER pMsgHeader, unsigned short iMsgCode) {
    CMemory* p_memory = CMemory::GetInstance();

    char* p_sendbuf = (char*)p_memory->AllocMemory(m_iLenMsgHeader + m_iLenPkgHeader, false);
    char* p_tmpbuf = p_sendbuf;

    memcpy(p_tmpbuf, pMsgHeader, m_iLenMsgHeader);
    p_tmpbuf += m_iLenMsgHeader;

    LPCOMM_PKG_HEADER pPkgHeader = (LPCOMM_PKG_HEADER)p_tmpbuf;
    pPkgHeader->msgCode = htons(iMsgCode);
    pPkgHeader->pkgLen = htons(m_iLenPkgHeader);
    pPkgHeader->crc32 = 0;
    sendMsg(p_sendbuf);
    return;
}

bool CLogicSocket::_HandleRegister(
    lphps_connection_t pConn, LPSTRUC_MSG_HEADER pMsgHeader, char* pPkgBody, unsigned short iBodyLength) {

    // 判断包体的合法性
    if (pPkgBody == NULL) {
        return false;
    }

    int iRecvLen = sizeof(STRUCT_REGISTER);
    if (iRecvLen != iBodyLength) {
        return false;
    }

    // 对于同一客户端的同一类型的消息，需要互斥处理
    CLock lock(&pConn->logicProcMutex);

    LPSTRUCT_REGISTER p_RecvInfo = (LPSTRUCT_REGISTER)pPkgBody;
    p_RecvInfo->iType = ntohl(p_RecvInfo->iType);
    p_RecvInfo->username[sizeof(p_RecvInfo->username) - 1] = 0;
    p_RecvInfo->password[sizeof(p_RecvInfo->password) - 1] = 0;

    // 返回数据给客户端
    LPCOMM_PKG_HEADER pPkgHeader;
    CMemory* p_memory = CMemory::GetInstance();
    CCRC32* p_crc32 = CCRC32::GetInstance();

    int iSendLen = sizeof(STRUCT_REGISTER);
    // iSendLen = 65000; // 测试

    char* p_sendbuf = (char*)p_memory->AllocMemory(m_iLenMsgHeader + m_iLenPkgHeader + iSendLen, false);
    memcpy(p_sendbuf, pMsgHeader, m_iLenMsgHeader);

    pPkgHeader = (LPCOMM_PKG_HEADER)(p_sendbuf + m_iLenMsgHeader);
    pPkgHeader->msgCode = _CMD_REGISTER;
    pPkgHeader->msgCode = htons(pPkgHeader->msgCode);
    pPkgHeader->pkgLen = htons(m_iLenPkgHeader + iSendLen);
    LPSTRUCT_REGISTER p_sendInfo = (LPSTRUCT_REGISTER)(p_sendbuf + m_iLenMsgHeader + m_iLenPkgHeader);

    pPkgHeader->crc32 = p_crc32->Get_CRC((unsigned char*)p_sendInfo, iSendLen);
    pPkgHeader->crc32 = htonl(pPkgHeader->crc32);

    this->sendMsg(p_sendbuf);

    // hps_log_stderr(0, "执行了CLogicSocket::_HandleRegister()!");
    return true;
}

bool CLogicSocket::_HandleLogIn(
    lphps_connection_t pConn, LPSTRUC_MSG_HEADER pMsgHeader, char* pPkgBody, unsigned short iBodyLength) {
    if (pPkgBody == NULL) {
        return false;
    }
    int iRecvLen = sizeof(STRUCT_LOGIN);
    if (iRecvLen != iBodyLength) {
        return false;
    }
    CLock lock(&pConn->logicProcMutex);

    LPSTRUCT_LOGIN p_RecvInfo = (LPSTRUCT_LOGIN)pPkgBody;
    p_RecvInfo->username[sizeof(p_RecvInfo->username) - 1] = 0;
    p_RecvInfo->password[sizeof(p_RecvInfo->password) - 1] = 0;

    LPCOMM_PKG_HEADER pPkgHeader;
    CMemory* p_memory = CMemory::GetInstance();
    CCRC32* p_crc32 = CCRC32::GetInstance();

    int iSendLen = sizeof(STRUCT_LOGIN);
    char* p_sendbuf = (char*)p_memory->AllocMemory(m_iLenMsgHeader + m_iLenPkgHeader + iSendLen, false);
    memcpy(p_sendbuf, pMsgHeader, m_iLenMsgHeader);
    pPkgHeader = (LPCOMM_PKG_HEADER)(p_sendbuf + m_iLenMsgHeader);
    pPkgHeader->msgCode = _CMD_LOGIN;
    pPkgHeader->msgCode = htons(pPkgHeader->msgCode);
    pPkgHeader->pkgLen = htons(m_iLenPkgHeader + iSendLen);
    LPSTRUCT_LOGIN p_sendInfo = (LPSTRUCT_LOGIN)(p_sendbuf + m_iLenMsgHeader + m_iLenPkgHeader);
    pPkgHeader->crc32 = p_crc32->Get_CRC((unsigned char*)p_sendInfo, iSendLen);
    pPkgHeader->crc32 = htonl(pPkgHeader->crc32);
    // hps_log_stderr(0,"成功收到了登录并返回结果！");
    sendMsg(p_sendbuf);
    return true;
}

// 接收并处理客户端发送过来的ping包
bool CLogicSocket::_HandlePing(
    lphps_connection_t pConn, LPSTRUC_MSG_HEADER pMsgHeader, char* pPkgBody, unsigned short iBodyLength) {
    if (iBodyLength != 0)
        return false;

    CLock lock(&pConn->logicProcMutex); // 凡是和本用户有关的访问都考虑用互斥
    pConn->lastPingTime = time(NULL);

    SendNoBodyPkgToClient(pMsgHeader, _CMD_PING);
    // hps_log_stderr(0, "成功收到了心跳包并返回结果！");
    return true;
}
