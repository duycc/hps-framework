//===--------------------------- include/hps_c_slogic.h - [HP-Server] -----------------------------------*- C++ -*-===//
// brief :
//   业务逻辑处理
//
// author: YongDu
// date  : 2021-09-23
//===--------------------------------------------------------------------------------------------------------------===//

#if !defined(__HPS_C_SLOGIC_H__)
#define __HPS_C_SLOGIC_H__

#include <sys/socket.h>

#include "hps_c_socket.h"

// 处理逻辑和通讯的子类
class CLogicSocket : public CSocket {
public:
  CLogicSocket();
  virtual ~CLogicSocket();
  bool Initialize() override;

  bool _HandleRegister(lphps_connection_t pConn, LPSTRUC_MSG_HEADER pMsgHeader, char *pPkgBody,
                       unsigned short iBodyLength);
  bool _HandleLogIn(lphps_connection_t pConn, LPSTRUC_MSG_HEADER pMsgHeader, char *pPkgBody,
                    unsigned short iBodyLength);

  void threadRecvProcFunc(char *pMsgBuf) override;
};

#endif // __HPS_C_SLOGIC_H__
