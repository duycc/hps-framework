## Linux C++ 高性能服务器框架

此仓库是依据王健伟老师的《Linux C++通信架构实战》书籍写的一个高性能服务器框架，自己跟着这个课程敲了一遍代码，基本和书中保持一致，约2000多行代码，统计如下：

```
+------------+------------+------------+------------+------------+------------+
| language   | files      | code       | comment    | blank      | total      |
+------------+------------+------------+------------+------------+------------+
| C++        |         32 |      2,750 |        727 |        655 |      4,132 |
| Makefile   |          9 |         61 |          6 |         35 |        102 |
| Properties |          1 |         19 |         17 |          9 |         45 |
| Markdown   |          1 |          1 |          0 |          1 |          2 |
| Log        |          1 |          0 |          0 |          1 |          1 |
+------------+------------+------------+------------+------------+------------+
```

### 总体概览

项目分模块编写，主要有日志模块，进程及信号模块，网络通信模块，业务处理模块和其它一些辅助模块。

**涉及技术**：

* 水平触发模式下的`epoll`高并发通信；
* 线程池技术处理业务逻辑；
* 线程间通信技术：互斥锁，条件变量，信号量；
* 进程间通信技术：信号及如何创建守护进程，如何避免僵尸进程等；
* 配置文件读取，日志的打印和输出；
* 连接池连接延迟回收技术，保证服务器的稳定；
* 自定义数据包格式解决TCP粘包问题；
* 其它安全和性能问题：
  * 心跳包
  * 控制并发连入数量
  * 检测`flood`攻击
  * 检测超时连接保证服务的性能

```
├── HPServer.conf
├── Makefile
├── README.md
├── app
│   ├── HPServer.cpp
│   ├── Makefile
│   ├── hps_c_conf.cpp
│   ├── hps_log.cpp
│   ├── hps_printf.cpp
│   ├── hps_setproctitle.cpp
│   └── hps_string.cpp
├── common.mk
├── config.mk
├── error.log
├── include
│   ├── hps_c_conf.h
│   ├── hps_c_crc32.h
│   ├── hps_c_lockmutex.h
│   ├── hps_c_memory.h
│   ├── hps_c_slogic.h
│   ├── hps_c_socket.h
│   ├── hps_c_threadpool.h
│   ├── hps_comm.h
│   ├── hps_func.h
│   ├── hps_global.h
│   ├── hps_logiccomm.h
│   └── hps_macro.h
├── logic
│   ├── Makefile
│   └── hps_c_slogic.cpp
├── misc
│   ├── Makefile
│   ├── hps_c_crc32.cpp
│   ├── hps_c_memory.cpp
│   └── hps_c_threadpool.cpp
├── net
│   ├── Makefile
│   ├── hps_c_socket.cpp
│   ├── hps_c_socket_accept.cpp
│   ├── hps_c_socket_conn.cpp
│   ├── hps_c_socket_inet.cpp
│   ├── hps_c_socket_request.cpp
│   └── hps_c_socket_time.cpp
├── proc
│   ├── Makefile
│   ├── hps_daemon.cpp
│   ├── hps_event.cpp
│   └── hps_process_cycle.cpp
└── signal
    ├── Makefile
    └── hps_signal.cpp
```

