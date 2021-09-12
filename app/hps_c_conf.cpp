/**
 * @file     hps_c_conf.cpp
 * @brief
 * @author   YongDu
 * @date     2021-09-12
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "hps_c_conf.h"
#include "hps_func.h"

CConfig *CConfig::m_instance = nullptr;

CConfig::CConfig() {}

CConfig::~CConfig() {
  for (auto *p_item : m_ConfigItemList) {
    delete p_item;
  }
  std::vector<LPCConfItem>().swap(m_ConfigItemList); // vector 需使用 swap() 释放内存
  return;
}

bool CConfig::Load(const char *p_conf_name) {
  if (nullptr == p_conf_name) {
    return false;
  }
  FILE *fp = fopen(p_conf_name, "r");
  if (nullptr == fp) {
    return false;
  }

  char linebuf[501]; // 一行配置文件内容
  while (!feof(fp)) {
    if (fgets(linebuf, 500, fp) == nullptr) {
      continue;
    }
    if (linebuf[0] == 0) {
      continue;
    }

    // 处理注释行
    if (*linebuf == ';' || *linebuf == ' ' || *linebuf == '#' || *linebuf == '\t' || *linebuf == '\n') {
      continue;
    }

    // 处理末尾的空格 换行 回车
    while (strlen(linebuf) > 0) {
      if (linebuf[strlen(linebuf) - 1] == 10 || linebuf[strlen(linebuf) - 1] == 13 ||
          linebuf[strlen(linebuf) - 1] == 32) {
        linebuf[strlen(linebuf) - 1] = 0;
      } else {
        break;
      }
    }

    if (linebuf[0] == 0) {
      continue;
    }
    if (*linebuf == '[') { // '['开头也不处理
      continue;
    }

    char *ptmp = strchr(linebuf, '=');
    if (ptmp != nullptr) {
      LPCConfItem p_conf_item = new CConfItem();
      memset(p_conf_item, 0, sizeof(CConfItem));
      strncpy(p_conf_item->ItemName, linebuf, (int)(ptmp - linebuf));
      strcpy(p_conf_item->ItemContent, ptmp + 1);

      // 去除左右两侧空格
      Rtrim(p_conf_item->ItemName);
      Ltrim(p_conf_item->ItemName);
      Rtrim(p_conf_item->ItemContent);
      Ltrim(p_conf_item->ItemContent);

      m_ConfigItemList.push_back(p_conf_item);
    }
  }
  fclose(fp);
  return true;
}

// 根据 ItemName 获取配置信息字符串，因为不修改不用保证互斥
const char *CConfig::GetString(const char *p_itemname) {
  for (auto *p_item : m_ConfigItemList) {
    if (strcasecmp(p_item->ItemName, p_itemname) == 0) {
      return p_item->ItemContent;
    }
  }
  return nullptr;
}

// 根据 ItemName 获取数字类型配置信息，不修改不用互斥
int CConfig::GetIntDefault(const char *p_itemname, const int def) {
  for (auto *p_item : m_ConfigItemList) {
    if (strcasecmp(p_item->ItemName, p_itemname) == 0) {
      return atoi(p_item->ItemContent);
    }
  }
  return def;
}
