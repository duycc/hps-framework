//===--------------------------- include/hps_comm.h - [HP-Server] ---------------------------------------*- C++ -*-===//
// brief :
//
//
// author: YongDu
// date  : 2021-09-18
//===--------------------------------------------------------------------------------------------------------------===//

#if !defined(__HPS_COMM_H__)
#define __HPS_COMM_H__

#define _PKG_MAX_LENGTH 30000 // "包头 + 包体"的最大长度

// 收包状态
#define _PKG_HD_INIT 0    // 初始状态，准备接收数据包头
#define _PKG_HD_RECVING 1 // 接收包头中，包头不完整，继续接收中
#define _PKG_BD_INIT 2    // 包头接收完，准备接收包体
#define _PKG_BD_RECVING 3 // 接收包体中，包体不完整，继续接收中，处理后直接回到_PKG_HD_INIT状态

#define _DATA_BUFSIZE_ 20 // 存放包头数组大小

#pragma pack(1)

// 包头结构
typedef struct _COMM_PKG_HEADER {
  unsigned short pkgLen;  // "包头 + 包体"的总长度
  unsigned short msgCode; // 消息类型
  int            crc32;   // CRC32效验
} COMM_PKG_HEADER, *LPCOMM_PKG_HEADER;

#pragma pack()

#endif // __HPS_COMM_H__
