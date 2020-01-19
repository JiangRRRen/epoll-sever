## 项目简介

本项目基于Linux下的epoll机制，设计的一种适用于高并发场景下的服务器。由C语言编写，具有较好的内存管理能力，尤其适用于嵌入式方案。

## 功能特性

- 利用Epoll机制提高监视文件描述符限制，避免Select中大量复制的句柄数据结构，产生巨大开销。监听时不需要遍历所有文件描述符，提高程序执行效率。
- 设计了Free，Poll，RW三个链表，用于管理服务器内存开支，能有效避免服务器上内存的频繁开辟和浪费。

## 使用说明

必装软件：make，mysql，openssl。

测试时，需要一台Linux电脑作服务器和一台Windows电脑作客户端。

将服务器代码放到Linux下的一个文件夹中，运行make即可。

<img src="https://uk-1259555870.cos.eu-frankfurt.myqcloud.com/20200118171116.png"/>

客户端需要修改如下位置，将IP改为Linux服务器的IP，确保顺利连接。

```C
sockaddr_in m_sin;
m_sin.sin_family = AF_INET;
m_sin.sin_port = htons(TCP_PORT);
m_sin.sin_addr.s_addr = inet_addr("Put your own IP");
```

测试用数据库是电网传感器侦测数据，已开放测试账号，在`MyServer.h`头文件中已有记录。

## 原理描述

整个项目实现高并发实现的基于以下三点：

- Epoll高效的事件处理能力
- 多线程协同处理
- 优秀的内存管理机制，提高读写速度

整体框架由3类线程+3条链表组成：

**3类线程：**

- 主线程
  - 初始化内存
  - 创建子线程
  - 侦听客户端连接
- 交互子线程
  - 读客户端请求
  - 回复客户端
- 读写子线程
  - 从数据库读取信息
  - 向数据库写入信息

**3条链表**：

- Free链表

  在主线程中开辟空间，作为内存管理的基本要素，链表中的节点表示空余内存，可供后面的操作取用。

- Poll链表

  与客户端建立连接后，从Free链表中取一个节点，向其添加信息，作为一个**令牌**，将这个令牌放入Poll链表，起到进程间通信的作用。

- RW链表

  从Poll链表取出令牌放入RW链表，读出命令要求，进行操作，操作完后放回Poll链表或者清空回收到Free链表。

## 运行流程

**（1）客户端连接**

<img src="https://uk-1259555870.cos.eu-frankfurt.myqcloud.com/20200118144928.png"/>

如图所示，连接过程如下：

1. 客户端向服务器发送连接请求
2. Epoll侦测到连接请求，`MainTcpEpollThread`中的`epoll_wait`函数被激发。
3. 从Free链表取出一个**令牌**
4. 向空令牌中写入连接信息
5. 放入Poll链表中

Free链表的内存开辟工作在主线程的初始化部分已经完成。内存分配固定，**所有线程想要使用内存必须向Free链表申请**。

`MainTcpEpollThread`函数的重要作用就是负责客户端的连接，可以想象为:house:中介：负责拉租户，找到租户后，剩下的工作就直接交给房东(poll链表)。

**（2）客户端交互**

<img src="https://uk-1259555870.cos.eu-frankfurt.myqcloud.com/20200118150337.png"/>

如图所示，客户端交互有两个过程：

1. 将客户端的请求送达RW链表
2. 将RW链表中处理好的，需要送回客户端的信息，反馈给客户端。

当客户端和服务器构建好连接后，客户端开始向服务器发送请求，请求激发了`ActionTcpEpollThread`中的`epoll_wait`函数，将请求写在令牌中，送入RW链表。

数据库读写后，如果有返回消息，则消息也会激发`epoll_wait`函数，促使`ActionTcpEpollThread`函数将消息反馈给客户端socket。

当读写请求到来时，如何从Poll链表中准确找到相应的客户端？答：利用`epoll_event`结构中的`ptr`指针，实现传参，具体参见[Epoll结构体分析](https://jiangren.work/2019/07/31/详解Epoll-下篇/)

**（3）数据库读写**

<img src="https://uk-1259555870.cos.eu-frankfurt.myqcloud.com/20200118150934.png"/>

流程：

1. **以阻塞的形式**从RW链表中取出令牌
2. 放入事件分派器`ActionTcpPack`中
3. 事件分派器根据需求，调用读写函数
4. 读写函数操作数据库
5. 数据库返回信息

到这一步时，如果有信息需要送回客户端，则事件分派器将信息打包好，添加到poll链表中，这就承接了（2）中介绍的poll链表第二个功能：

<img src="https://uk-1259555870.cos.eu-frankfurt.myqcloud.com/20200118151302.png"/>