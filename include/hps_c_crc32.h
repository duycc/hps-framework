//===--------------------------- include/hps_c_crc32.h - [HP-Server] ------------------------------------*- C++ -*-===//
// brief :
//   CRC32
//
// author: YongDu
// date  : 2021-09-23
//===--------------------------------------------------------------------------------------------------------------===//

#if !defined(__HPS_C_CRC32_H__)
#define __HPS_C_CRC32_H__

#include <stddef.h>

class CCRC32 {
public:
  ~CCRC32();

  static CCRC32 *GetInstance() {
    if (m_instance == NULL) {
      // lock
      if (m_instance == NULL) {
        m_instance = new CCRC32();
        static CGarhuishou gc;
      }
      // unlock
    }
    return m_instance;
  }

  struct CGarhuishou {
    ~CGarhuishou() {
      if (CCRC32::m_instance) {
        delete CCRC32::m_instance;
        CCRC32::m_instance = NULL;
      }
    }
  };

  void Init_CRC32_Table();

  unsigned int Reflect(unsigned int ref, char ch); // Reflects CRC bits in the lookup table

  int Get_CRC(unsigned char *buffer, unsigned int dwSize);

  unsigned int crc32_table[256]; // Lookup table arrays

private:
  CCRC32();

  static CCRC32 *m_instance;
};

#endif // __HPS_C_CRC32_H__
