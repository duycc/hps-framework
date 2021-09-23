//===--------------------------- net/hps_c_socket_request.cpp - [HP-Server] -----------------------------*- C++ -*-===//
// brief :
//   Receive data
//
// author: YongDu
// date  : 2021-09-17
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
#include <unistd.h>

#include "hps_c_conf.h"
#include "hps_c_lockmutex.h"
#include "hps_c_memory.h"
#include "hps_c_socket.h"
#include "hps_func.h"
#include "hps_global.h"
#include "hps_macro.h"

// 当连接上有数据来的时候，此函数会被hps_epoll_process_events()调用
void CSocekt::hps_wait_request_handler(lphps_connection_t c) {
  ssize_t reco = recvproc(c, c->precvbuf, c->irecvlen);
  if (reco <= 0) {
    return; // recvproc()函数已经释放资源，可直接return
  }

  if (c->curStat == _PKG_HD_INIT) {
    if (reco == m_iLenPkgHeader) {
      // 恰好收到完整包头 -> 拆解包头
      hps_wait_request_handler_proc_p1(c); // 处理包头
    } else {
      // 收到不完整包头
      c->curStat = _PKG_HD_RECVING;
      c->precvbuf = c->precvbuf + reco;
      c->irecvlen = c->irecvlen - reco;
    }
  } else if (c->curStat == _PKG_HD_RECVING) {
    // 包头不完整，继续接收中
    if (c->irecvlen == reco) {
      // 包头收完整了
      hps_wait_request_handler_proc_p1(c);
    } else {
      c->precvbuf = c->precvbuf + reco;
      c->irecvlen = c->irecvlen - reco;
    }
  } else if (c->curStat == _PKG_BD_INIT) {
    // 包头刚好收完，准备接收包体
    if (reco == c->irecvlen) {
      // 收到的宽度等于要收的宽度，包体收完整了
      hps_wait_request_handler_proc_plast(c);
    } else {
      c->curStat = _PKG_BD_RECVING;
      c->precvbuf = c->precvbuf + reco;
      c->irecvlen = c->irecvlen - reco;
    }
  } else if (c->curStat == _PKG_BD_RECVING) {
    if (c->irecvlen == reco) {
      hps_wait_request_handler_proc_plast(c);
    } else {
      c->precvbuf = c->precvbuf + reco;
      c->irecvlen = c->irecvlen - reco;
    }
  }
  return;
}

/**
 * @brief 接受客户端数据
 *
 * @param c
 * @param buff 接受数据的缓冲区
 * @param buflen 接受收据大小
 * @return ssize_t -1 有错误发生，>0 实际收到的字节数
 */
ssize_t CSocekt::recvproc(lphps_connection_t c, char *buff, ssize_t buflen) {
  ssize_t n;
  n = recv(c->fd, buff, buflen, 0);
  if (n == 0) {
    // 客户端关闭
    hps_close_connection(c);
    return -1;
  }
  if (n < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      // epoll为LT模式不应该出现这个返回值，不当做错误
      hps_log_stderr(errno, "CSocekt::recvproc()中errno == EAGAIN || errno == EWOULDBLOCK成立！");
      return -1;
    }
    if (errno == EINTR) {
      hps_log_stderr(errno, "CSocekt::recvproc()中errno == EINTR成立！");
      return -1;
    }
    if (errno == ECONNRESET) {
      // 客户端非正常退出，后续释放连接即可
    } else {
      hps_log_stderr(errno, "CSocekt::recvproc()中发生错误！");
    }
    hps_close_connection(c);
    return -1;
  }

  return n;
}

// 处理接受到的包头
void CSocekt::hps_wait_request_handler_proc_p1(lphps_connection_t c) {
  CMemory *p_memory = CMemory::GetInstance();

  LPCOMM_PKG_HEADER pPkgHeader;
  pPkgHeader = (LPCOMM_PKG_HEADER)c->dataHeadInfo;

  unsigned short e_pkgLen;
  e_pkgLen = ntohs(pPkgHeader->pkgLen); // 网络字节序 -> 主机字节序

  // 判断是否为恶意包或错误包
  if (e_pkgLen < m_iLenPkgHeader) {
    // pkgLen = 包头 + 包体 总长度，不应该小于包头长度
    // 恢复收包状态机
    c->curStat = _PKG_HD_INIT;
    c->precvbuf = c->dataHeadInfo;
    c->irecvlen = m_iLenPkgHeader;
  } else if (e_pkgLen > (_PKG_MAX_LENGTH - 1000)) {
    // 恶意包，已规定了包最大长度
    c->curStat = _PKG_HD_INIT;
    c->precvbuf = c->dataHeadInfo;
    c->irecvlen = m_iLenPkgHeader;
  } else {
    // 合法包头，分配内存开始收包体
    char *pTmpBuffer = (char *)p_memory->AllocMemory(m_iLenMsgHeader + e_pkgLen, false); // 消息头 + 包头 + 包体
    c->ifnewrecvMem = true;
    c->pnewMemPointer = pTmpBuffer;

    // 保存消息头
    LPSTRUC_MSG_HEADER ptmpMsgHeader = (LPSTRUC_MSG_HEADER)pTmpBuffer;
    ptmpMsgHeader->pConn = c;
    ptmpMsgHeader->iCurrsequence = c->iCurrsequence;

    // 保存包头
    pTmpBuffer += m_iLenMsgHeader;
    memcpy(pTmpBuffer, pPkgHeader, m_iLenPkgHeader);
    if (e_pkgLen == m_iLenPkgHeader) {
      // 特殊包，无包体有包头，认为是一个完整包
      hps_wait_request_handler_proc_plast(c);
    } else {
      // 开始收包体
      c->curStat = _PKG_BD_INIT;
      c->precvbuf = pTmpBuffer + m_iLenPkgHeader;
      c->irecvlen = e_pkgLen - m_iLenPkgHeader;
    }
  }

  return;
}

// 收到一个完整包后的处理
void CSocekt::hps_wait_request_handler_proc_plast(lphps_connection_t c) {
  g_threadpool.inMsgRecvQueueAndSignal(c->pnewMemPointer);

  c->ifnewrecvMem = false; // 内存交给消息队列管理
  c->pnewMemPointer = NULL;

  // 接受下一个包
  c->curStat = _PKG_HD_INIT;
  c->precvbuf = c->dataHeadInfo;
  c->irecvlen = m_iLenPkgHeader;
  return;
}

void CSocekt::threadRecvProcFunc(char *pMsgBuf) { return; }
