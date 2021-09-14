/**
 * @file     hps_string.cpp
 * @brief
 * @author   YongDu
 * @date     2021-09-12
 */

#include <stdio.h>
#include <string.h>

// 去掉字符串尾部空格
void Rtrim(char *string) {
  if (NULL == string) {
    return;
  }
  size_t len = strlen(string);
  while (len > 0 && string[len - 1] == ' ') {
    string[--len] = 0;
  }
  return;
}

// 去掉字符串首部空格
void Ltrim(char *string) {
  if (NULL == string) {
    return;
  }
  size_t len = strlen(string);
  char * p_tmp = string;
  if ((*p_tmp) != ' ') { // 不是以空格开头
    return;
  }
  while ((*p_tmp) == ' ') {
    p_tmp++;
  }
  char *p_tmp2 = string;
  while ((*p_tmp) != '\0') {
    (*p_tmp2) = *p_tmp;
    p_tmp++;
    p_tmp2++;
  }
  (*p_tmp2) = '\0';
  return;
}
