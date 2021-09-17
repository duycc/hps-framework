/**
 * @file     hps_event.cpp
 * @brief
 * @author   YongDu
 * @date     2021-09-17
 */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hps_c_conf.h"
#include "hps_func.h"
#include "hps_global.h"
#include "hps_macro.h"

void hps_process_events_and_timers() {
  g_socket.hps_epoll_process_events(-1);

  // ... 待扩充
  return;
}
