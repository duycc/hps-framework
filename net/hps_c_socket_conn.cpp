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
#include "hps_c_socket.h"
#include "hps_func.h"
#include "hps_global.h"
#include "hps_macro.h"

// 连接池中获取一个空闲连接
lphps_connection_t CSocekt::hps_get_connection(int isock) {
  lphps_connection_t c = m_pfree_connections;

  if (c == NULL) {
    hps_log_stderr(0, "CSocekt::hps_get_connection()中空闲链表为空!");
    return NULL;
  }

  m_pfree_connections = c->data;
  m_free_connection_n--;

  uintptr_t instance = c->instance;
  uint64_t  iCurrsequence = c->iCurrsequence;

  memset(c, 0, sizeof(hps_connection_t));
  c->fd = isock;

  c->instance = !instance;
  c->iCurrsequence = iCurrsequence;
  ++c->iCurrsequence;

  return c;
}

// c所代表的连接到到连接池中
void CSocekt::hps_free_connection(lphps_connection_t c) {
  c->data = m_pfree_connections;
  ++c->iCurrsequence; // 回收后，该值增加1，以用于判断某些网络事件是否过期

  m_pfree_connections = c;
  ++m_free_connection_n;
  return;
}
