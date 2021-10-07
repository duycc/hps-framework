//===--------------------------- include/hps_c_conf.h - [hp-server] -------------------------------------*- C++ -*-===//
// Brief :
//   read conf item
//   Singalton
//
// Author: YongDu
// Date  : 2021-09-12
//===--------------------------------------------------------------------------------------------------------------===//

#if !defined(__HPS_C_CONF_H__)
#define __HPS_C_CONF_H__

#include <vector>

#include "hps_global.h"

// 处理配置文件类 - 单例
class CConfig {
public:
  ~CConfig();

  static CConfig *GetInstance() {
    if (nullptr == m_instance) {
      // lock
      if (nullptr == m_instance) {
        m_instance = new CConfig();
        static CGarCollection gc; // RAII 释放对象
      }
      // unlock
    }
    return m_instance;
  }

  // 加载配置文件
  bool Load(const char *p_confname);

  // 获取配置项内容
  const char *GetString(const char *p_itemname);

  // 获取数字型配置项
  int GetIntDefault(const char *p_itemname, const int def);

  struct CGarCollection {
    ~CGarCollection() {
      if (CConfig::m_instance) {
        delete CConfig::m_instance;
        CConfig::m_instance = nullptr;
      }
    }
  };

private:
  CConfig();

  static CConfig *m_instance;

  std::vector<LPCConfItem> m_ConfigItemList; // 存储配置文件项
};

#endif // __HPS_C_CONF_H__
