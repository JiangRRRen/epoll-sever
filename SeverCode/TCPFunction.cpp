#include"MyServer.h"
void* MainTcpEpollThread(void* lpParam)
{
	int nSocketListen = INVALID_SOCKET;//socket描述字
	int nListenEpoll = 0; //连接对象监听Epoll描述字
	int nClientEpoll = 0; //对象动作监听Epoll描述字
	CLIENTTHREADPARAM* lpThreadParam = (CLIENTTHREADPARAM*)NULL;
	struct epoll_event* lpListenEvents = NULL;

	signal(SIGSEGV, &Dump);//遇到内存异常，调用dump函数记录崩溃信息
	do {
		signal(SIGPIPE, SIG_IGN);//如果管道中读写出现问题，让系统回收
		printf("Malloc Listen Events...\r\n");
		lpListenEvents = (struct epoll_event*)malloc(sizeof(struct epoll_event) * MAX_LISTEN_EVENTS);
		if (lpListenEvents == NULL) //分配失败
		{
			FormatError(ERROR_NOT_ENOUGH_MEMORY);
			break;
		}
		/*-------------------------------------------------------------------------*/
		/*--------------------------------设置socket-------------------------------*/
		/*-------------------------------------------------------------------------*/
		printf("Create Listen Socket...\r\n");
		nSocketListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (nSocketListen < 0)
		{
			FormatError(errno);
			break;
		}
		printf("Set Socket NonBlocking...\r\n");
		int nResult=SetSocketNonBlocking(nSocketListen); //非阻塞模式，不管IO是否完成线程会继续执行
		printf("Set Socket Option...\r\n");
		int on = 1;
		//允许重复捆绑端口，防止关闭某个端口后再次重用失败
		if (setsockopt(nSocketListen, SOL_SOCKET, SO_REUSEADDR, (char*)& on, sizeof(on)) < 0)
		{
			FormatError(errno);
			break;
		}
		printf("Bind the Port and Address...\r\n");
		struct sockaddr_in local;
		memset(&local, 0, sizeof(local));
		local.sin_family = AF_INET;
		local.sin_addr.s_addr = htonl(INADDR_ANY);//0.0.0.0所有地址都合法
		local.sin_port = htons(TCP_PORT);
		//将地址与socket绑定
		if (bind(nSocketListen, (struct sockaddr*) & local, sizeof(local)) < 0)
		{
			FormatError(errno);
			break;
		}
		printf("Listen...\r\n");
		if (listen(nSocketListen, 20) < 0)
		{
			FormatError(errno);
			break;
		}

		/*-------------------------------------------------------------------------*/
		/*--------------------------------创建Epoll--------------------------------*/
		/*-------------------------------------------------------------------------*/
		printf("Create Epoll for Listen...\r\n");
		//监听连接的epoll，用于监视新客户端的连接
		nListenEpoll = epoll_create(MAX_LISTEN_EVENTS);
		if (nListenEpoll < 0)
		{
			FormatError(errno); break;
		}
		printf("Create Epoll for Listen...\r\n");
		//处理客户端连接的epoll，用于处理已连接客户端事件
		nClientEpoll = epoll_create(MAX_CLIENT_EVENTS);
		if (nClientEpoll < 0)
		{
			FormatError(errno); break;
		}
		/*-------------------------------------------------------------------------*/
		/*--------------------------------设置子线程--------------------------------*/
		/*-------------------------------------------------------------------------*/
		printf("Malloc Memory for lpThreadParam...\r\n");
		lpThreadParam = (CLIENTTHREADPARAM*)malloc(sizeof(CLIENTTHREADPARAM));
		if (lpThreadParam == NULL)
		{
			FormatError(ERROR_NOT_ENOUGH_MEMORY);
			break;
		}
		//设置线程参数
		memset(lpThreadParam, 0, sizeof(CLIENTTHREADPARAM));
		lpThreadParam->iClientEpoll = nClientEpoll;
		lpThreadParam->m_RWAction = LinuxCreatPV(); //创建互斥锁，返回锁的句柄
		lpThreadParam->m_RWPV = LinuxCreatPV();
		lpThreadParam->m_FreeAction = LinuxCreatPV();
		lpThreadParam->m_FreePV = LinuxCreatPV();
		lpThreadParam->m_PollAction = LinuxCreatPV();
		lpThreadParam->m_PollPV = LinuxCreatPV();
		lpThreadParam->m_lpPollEnd = &(lpThreadParam->m_PollHead);
		lpThreadParam->m_lpFreeEnd = &(lpThreadParam->m_FreeHead);
		lpThreadParam->m_lpRWEnd = &(lpThreadParam->m_RWHead);
		g_FreePVTcp = lpThreadParam->m_FreePV;

		/*-------------------------------------------------------------------------*/
		/*---------------------------为Free链表设置内存-----------------------------*/
		/*-------------------------------------------------------------------------*/
		printf("Malloc Memory for FreeList...\r\n");
		IOPACKHEAD_LIST* lpOptionList = &lpThreadParam->m_FreeHead; //链表头
		size_t NodeSize = ALIGN(sizeof(IOPACKHEAD_LIST), 8) + MAX_BLOCK_SIZE; //ALIGN对齐
		for (int i = 0; i < MAX_ACTION_COUNT; i++) //连续开辟MAX_ACTION_COUNT * NodeSize大小的空间
		{
			lpOptionList->lpNext = (IOPACKHEAD_LIST*)malloc(NodeSize);
			memset(lpOptionList->lpNext, 0, NodeSize);
			lpOptionList->lpNext->lpBuf = (BYTE*)(lpOptionList->lpNext) + ALIGN(sizeof(IOPACKHEAD_LIST), 8);//需要传输的数据地址=Next的起始地址+包头地址
			lpOptionList->lpNext->dwBufflen = MAX_BLOCK_SIZE;
			lpOptionList->lpNext->lpPrev = lpOptionList;
			lpOptionList = lpOptionList->lpNext;
			lpThreadParam->m_lpFreeEnd = lpOptionList;//Head是一个实际存在的节点，End只是一个指针
		}
		g_FreeListNumsTcp = MAX_ACTION_COUNT;

		/*-------------------------------------------------------------------------*/
		/*--------------------------------启动子线程-------------------------------*/
		/*-------------------------------------------------------------------------*/
		printf("Create ActionTcpEpoolThread...\r\n");

		LinuxStartThread(ActionTcpEpoolThread, lpThreadParam);//子线程1，侦听连接上的客户的请求

		for (int j = 0; j < MAX_WORK_THREAD; j++)
		{
			printf("Create ActionTcpConnectThread %d\r\n", j + 1);
			LinuxStartThread(ActionTcpConnectThread, lpThreadParam);//交互子线程，处理已连接客户
		}
		/*-------------------------------------------------------------------------*/
		/*------------------------------侦听客户连接-------------------------------*/
		/*-------------------------------------------------------------------------*/
		struct epoll_event Ev;
		memset(&Ev, 0, sizeof(struct epoll_event));
		Ev.events = EPOLLIN | EPOLLET; //输入，边缘触发
		Ev.data.fd = nSocketListen;
		if (epoll_ctl(nListenEpoll, EPOLL_CTL_ADD, nSocketListen, &Ev) < 0)
		{
			FormatError(errno);
			break;
		}
		BYTE szMac[6] = { 0 };
		while (1)
		{
			//返回侦听到的连接数，-1表示不设置超时
			printf("MainTcpEpollThread Listen Epoll Wait..\r\n");
			
			//边缘触发只支持非阻塞模式
			int nFdNumber = epoll_wait(nListenEpoll, lpListenEvents, MAX_LISTEN_EVENTS, -1);
			/*测试部分*/
			if (nFdNumber < 0)
			{
				if (errno = EINTR) //EINTR是高级系统中断错误，应该直接忽略
				{
					continue;
				}
				else {
					FormatError(errno);
					break;
				}
			}
			printf("MainTcpEpollThread Listen Epoll Recieved nFdNumber=%d..\r\n",nFdNumber);
			/*--------------成功监听到客户连接，开始处理---------------*/
			for (int i = 0; i < nFdNumber; i++) //如果没有客户端连接则直接跳出
			{
				//epoll_wait将结果返回到lpListenEvents指向的地方，并将结果转化为lpListenEvents数组
				if (lpListenEvents[i].data.fd != nSocketListen) continue;
				struct sockaddr_in remote;
				int iRetlen = 0;
				/*--------------接收某个客户连接---------------*/
				while (1)
				{
					int nClientSocket = accept(nSocketListen, (struct sockaddr*) & remote, (socklen_t*)& iRetlen);
					if (nClientSocket < 0)
					{
						if (EAGAIN != errno) //非阻塞模式下太满，再次尝试
						{
							FormatError(errno);
						}
						break;
					}
					//获取当前连接的客户端的IP地址和端口号
					if (remote.sin_addr.s_addr == 0)
					{
						//获取对方socket地址
						getpeername(nClientSocket, (struct sockaddr*) & remote, (socklen_t*)& iRetlen);
					}
					int nResult = SetSocketNonBlocking(nClientSocket);
					if (nResult != 0)//设置非阻塞失败
					{
						shutdown(nClientSocket, 2);
						close(nClientSocket);
						continue;
					}
					/*--------------为这个客户连接添加Epoll事件，侦听它的动作---------------*/
					//从Free链表中取一个空闲令牌
					IOPACKHEAD_LIST* lpCatList = GetIOList(&(lpThreadParam->m_FreeHead), &(lpThreadParam->m_lpFreeEnd), lpThreadParam->m_FreePV, lpThreadParam->m_FreeAction, 1);
					//写令牌
					memset(lpCatList->lpBuf, 0, lpCatList->dwBufflen);
					lpCatList->nClientSocket = nClientSocket;
					lpCatList->dwFunction = 0;//无返回值
					lpCatList->dwAllSize = lpCatList->dwBufflen;//需要读写的长度
					lpCatList->dwCompleteSize = 0;
					lpCatList->iEpool = lpThreadParam->iClientEpoll;
					lpCatList->dwTime = (DWORD)time(NULL);//当前时间
					lpCatList->dwClientIP = remote.sin_addr.s_addr;//客户地址
					memcpy(lpCatList->szMac, szMac, 6);

					

					//向nClientEpoll添加侦听事件
					struct epoll_event Ev;
					memset(&Ev, 0, sizeof(epoll_event));
					Ev.events = EPOLLIN | EPOLLET | EPOLLERR | EPOLLHUP | EPOLLPRI;//感兴趣的事件
					Ev.data.ptr = lpCatList; //将lpCatList寄存在ptr中，wait触发后，会原封不动的将lpCatList回传，可以利用ptr传参
					if (epoll_ctl(nClientEpoll, EPOLL_CTL_ADD, nClientSocket, &Ev) < 0)//侦听与客户建立的连接通道
					{
						FormatError(errno);
						// 关闭链接
						shutdown(nClientSocket, 2);
						close(nClientSocket);
						// 从 Poll 链中去除
						// 重置 buf
						memset(lpCatList->lpBuf, 0, lpCatList->dwBufflen);
						lpCatList->dwCompleteSize = lpCatList->dwAllSize = 0;
						// 回收到 Free
						PutIOList(lpCatList, &lpThreadParam->m_lpFreeEnd, lpThreadParam->m_FreePV, lpThreadParam->m_FreeAction);
						continue;
					}
					else
					{
						PutIOList(lpCatList, &(lpThreadParam->m_lpPollEnd), (lpThreadParam->m_PollPV), (lpThreadParam->m_PollAction));
					}
				}
			}
		}
	} while (0);
	//出错后清空
	if (nSocketListen > 0) close(nSocketListen);
	if (nListenEpoll > 0) close(nListenEpoll);
	if (nClientEpoll > 0) close(nClientEpoll);
	if (lpListenEvents)	free(lpListenEvents);
	printf("MainTcpEpollThread End!\r\n");
	return NULL;
}
void* ActionTcpEpoolThread(void* lpParam)
{
	CLIENTTHREADPARAM* lpThreadParam = (CLIENTTHREADPARAM*)lpParam;
	struct epoll_event* lpEvent = NULL;
	signal(SIGSEGV, &Dump);
	do
	{
		lpEvent = (struct epoll_event*)malloc(sizeof(struct epoll_event) *4* MAX_CLIENT_EVENTS);
		if (NULL == lpEvent)
		{
			FormatError(ERROR_NOT_ENOUGH_MEMORY); 
			break;
		}
		signal(SIGPIPE, SIG_IGN);
		BYTE btCloseFlag = FALSE;
		BYTE btAddToPoolFlag = TRUE;
		while (1)
		{
			
			int nFdNumber = epoll_wait(lpThreadParam->iClientEpoll, lpEvent, MAX_CLIENT_EVENTS, -1);
			if (nFdNumber < 0)
			{
				if (errno = EINTR)
				{
					continue;
				}
				else {
					FormatError(errno);
					break;
				}
			}
			
			for (int i = 0; i < nFdNumber; i++)
			{
				//利用ptr的传参特性，获取MainTcpEpollThread线程中添加到poll的令牌
				IOPACKHEAD_LIST* lpCatList = (IOPACKHEAD_LIST*)lpEvent[i].data.ptr;
				DelIOList(lpCatList, &lpThreadParam->m_PollHead, &lpThreadParam->m_lpPollEnd, lpThreadParam->m_PollPV);
				btCloseFlag = FALSE;
				btAddToPoolFlag = TRUE;
				do
				{
				/*-------------------------------------------------------------------------*/
				/*--------------------------------写数据-----------------------------------*/
				/*-------------------------------------------------------------------------*/
					if (lpEvent[i].events & EPOLLOUT)//OUT在Connect线程中注册
					{
						if (lpCatList->dwFunction != 1)//=1表示需要接收
						{
							btCloseFlag = TRUE;
							btAddToPoolFlag = FALSE;
							break;
						}

						while (lpCatList->dwCompleteSize < lpCatList->dwAllSize)//allsize=Max_Block_size
						{
							DWORD NeedWriteSize = lpCatList->dwAllSize - lpCatList->dwCompleteSize;//需要发送的大小
							BYTE* lpWriteAddr = lpCatList->lpBuf + lpCatList->dwCompleteSize;//发送的起始地址
								int iRetLen = write(lpCatList->nClientSocket, lpWriteAddr, NeedWriteSize);
								if (iRetLen <= 0) break;//
								lpCatList->dwCompleteSize += iRetLen;
						}
						if (lpCatList->dwCompleteSize >= lpCatList->dwAllSize)
						{
							// 发送完成，不再处理这个链接，关闭并回收资源
							btCloseFlag = TRUE;
							btAddToPoolFlag = FALSE;
							break;
						}
					}
				/*-------------------------------------------------------------------------*/
				/*--------------------------------读数据-----------------------------------*/
				/*-------------------------------------------------------------------------*/
					else if (lpEvent[i].events & EPOLLIN)
					{
						if (lpCatList->dwFunction != 0)//不需要接收
						{
							btCloseFlag = TRUE;
							btAddToPoolFlag = FALSE;
						}
						int nCloseFlag = 0;
						int nTry = 0;
						while (lpCatList->dwCompleteSize < lpCatList->dwAllSize)
						{
							DWORD NeedReadSize = lpCatList->dwAllSize - lpCatList->dwCompleteSize;//需要发送的大小
							BYTE* lpReadAddr = lpCatList->lpBuf + lpCatList->dwCompleteSize;//发送的起始地址
							int nRetLen = read(lpCatList->nClientSocket, lpReadAddr, NeedReadSize);
							if (nRetLen <= 0)
							{
								if (errno == EAGAIN)//没读完，再尝试几次
								{
									nTry++;
									if (nTry < ReTryNum) continue;
								}
								if (errno != EAGAIN && errno != ECONNRESET)//ECONNRESET对端已关闭
								{
									FormatError(errno);
								}
								if (nCloseFlag==0) nCloseFlag = 1;
								break;
							}
							nTry = 0;
							nCloseFlag = 2;
							lpCatList->dwCompleteSize += nRetLen;
						}
						//如果已断开连接
						if (nCloseFlag == 1)
						{
							btCloseFlag = 1;
							btAddToPoolFlag = 0;
							break;
						}
						LPIOPACKHEAD lpPackHead = (LPIOPACKHEAD)lpCatList->lpBuf;
						/*--------------------------------初步校验包-----------------------------------*/
						if (lpPackHead->dwSig != SIG_IOPACK && lpCatList->dwCompleteSize >= sizeof(lpPackHead->dwSig))
						{
							FormatError(ERROR_INVALID_DATA);
							btCloseFlag = TRUE;
							btAddToPoolFlag = FALSE;
							break;
						}
						/*--------------------------------校验包头是否正确-----------------------------------*/
						if (lpCatList->dwCompleteSize >= sizeof(IOPACKHEAD))//接收到了包头，下面开始判断
						{
							if (lpPackHead->dwSig != SIG_IOPACK)//标志位不合法
							{
								FormatError(ERROR_INVALID_DATA);
								btCloseFlag = TRUE;
								btAddToPoolFlag = FALSE;
								break;
							}
							lpCatList->dwAllSize= sizeof(IOPACKHEAD) + lpPackHead->dwBufferSize;
							if (lpCatList->dwAllSize > lpCatList->dwBufflen)//超过了规定大小
							{
								FormatError(ERROR_INVALID_DATA);
								btCloseFlag = TRUE;
								btAddToPoolFlag = FALSE;
								break;
							}
							DWORD dwNeedCheckSum = MakeCheckSum(lpPackHead + 1, lpPackHead->dwBufferSize);/*checksum判断*/
							if (lpPackHead->dwCheckSum != dwNeedCheckSum)//check不合法
							{
								FormatError(ERROR_INVALID_DATA);
								printf("NeedCheckSum=0x%X, RealCheckSum=0x%X\r\n", dwNeedCheckSum, lpPackHead->dwCheckSum);
								btCloseFlag = TRUE;
								btAddToPoolFlag = FALSE;
								break;
							}
							/*--------------------------------接受完数据包-----------------------------------*/
							if (lpCatList->dwCompleteSize >= lpCatList->dwAllSize)//接受到的数据大于包数据，证明接收完成（接受数据会有很多无效的0）
							{

								//包完整有效，先从 ClientEpoll 中删除，再加入到 RW 链表
								struct epoll_event Ev;
								memset(&Ev, 0, sizeof(epoll_event));
								//Ev.data.u64 = lpEvent[i].data.u64;
								if (epoll_ctl(lpThreadParam->iClientEpoll, EPOLL_CTL_DEL, lpCatList->nClientSocket, &Ev)<0)
								{								
									FormatError(errno);
									btCloseFlag = TRUE;
									btAddToPoolFlag = FALSE;
									break;		 
								}
								PutIOList(lpCatList, &lpThreadParam->m_lpRWEnd, lpThreadParam->m_RWPV, lpThreadParam->m_RWAction);
								btAddToPoolFlag = FALSE;// 已经加入 RW 链表，不要再加入 Poll 链表
								break;
							}
						}
					}
					else
					{
						btCloseFlag = TRUE;
						btAddToPoolFlag = FALSE;
						break;
					}
				} while (0);
				/*--------------------------------如果断开，清理回收资源-----------------------------------*/
				if (btCloseFlag)
				{
					memset(lpCatList->lpBuf, 0, lpCatList->dwBufflen);
					lpCatList->dwCompleteSize = lpCatList->dwAllSize = 0;
					//从EPOLL中删除
					struct epoll_event Ev;
					memset(&Ev, 0, sizeof(epoll_event));
					Ev.data.u64 = lpEvent[i].data.u64;
					if (epoll_ctl(lpThreadParam->iClientEpoll, EPOLL_CTL_DEL, lpCatList->nClientSocket, &Ev)<0)
					{
						FormatError(errno);
					}
					//关闭连接
					shutdown(lpCatList->nClientSocket, 2);
					close(lpCatList->nClientSocket);
					//回收到Free
					PutIOList(lpCatList, &lpThreadParam->m_lpFreeEnd, lpThreadParam->m_FreePV, lpThreadParam->m_FreeAction);
				}
				/*--------------------------------没有断开，处理完成-------------------------------------*/
				else
				{
					if(btAddToPoolFlag)
					lpCatList->dwTime = (DWORD)time(NULL);
					//重新放入poll链中等待下一次IN/OUT
					PutIOList(lpCatList, &lpThreadParam->m_lpPollEnd, lpThreadParam->m_PollPV, lpThreadParam->m_PollAction);
				}
			}
		}

	} while (0);
	if (lpEvent) free(lpEvent);
	return NULL;
}


//处理已连接客户端
void* ActionTcpConnectThread(void* lpParam)
{
	CLIENTTHREADPARAM* lpThreadParam = (CLIENTTHREADPARAM*)lpParam;
	IOPACKHEAD_LIST* lpCatList = (IOPACKHEAD_LIST*)NULL;
	IOPACKHEAD*	lpPackHead = NULL;
	BOOL IsSend = FALSE;
	signal(SIGSEGV, &Dump);
	do {
		signal(SIGPIPE, SIG_IGN);
		while (1)
		{
			//当未有连接，未有消息时，会阻塞在Get函数中
			lpCatList = GetIOList(&lpThreadParam->m_RWHead, &lpThreadParam->m_lpRWEnd, lpThreadParam->m_RWPV, lpThreadParam->m_RWAction, 1);
			lpPackHead = (IOPACKHEAD*)lpCatList->lpBuf;
			
			IsSend = ActionTcpPack(lpCatList->dwClientIP, lpPackHead, lpCatList->dwCompleteSize);
			/*-----------------------------------------------------------------*/
			/*-----------------------------发送--------------------------------*/
			/*-----------------------------------------------------------------*/
			if (IsSend)//如果要发送，需要送到POLL中处理，poll时专门处理收发
			{
				//重写令牌信息
				lpCatList->dwFunction = 1;//将发送标志位置1
				lpCatList->dwAllSize = sizeof(IOPACKHEAD) + lpPackHead->dwBufferSize;
				lpCatList->dwCompleteSize = 0;
				lpCatList->dwTime = (DWORD)time(NULL);
				//加入到poll中处理
				PutIOList(lpCatList, &lpThreadParam->m_lpPollEnd,lpThreadParam->m_PollPV, lpThreadParam->m_PollAction);
				//为iClientEpoll注册
				struct epoll_event Ev;
				memset(&Ev, 0, sizeof(Ev));
				Ev.events= EPOLLOUT | EPOLLET | EPOLLERR | EPOLLHUP | EPOLLPRI;
				Ev.data.ptr = lpCatList;
				BYTE ErrFlag = 0;
				if (epoll_ctl(lpThreadParam->iClientEpoll, EPOLL_CTL_ADD, lpCatList->nClientSocket, &Ev) < 0)
				{
					if (errno == EEXIST)
					{//如果已经注册，就将其修改，能够重新触发
						if (epoll_ctl(lpThreadParam->iClientEpoll, EPOLL_CTL_MOD, lpCatList->nClientSocket, &Ev) < 0)
						{
							ErrFlag = FormatError(errno);
						}
					}
					else
					{
						ErrFlag = FormatError(errno);
					}
					if (ErrFlag)//读写失败，清除并回收资源
					{
						DelIOList(lpCatList, &lpThreadParam->m_PollHead, &lpThreadParam->m_lpPollEnd, lpThreadParam->m_PollPV);
						memset(lpCatList->lpBuf, 0, lpCatList->dwBufflen);//清理掉buf
						lpCatList->dwCompleteSize = lpCatList->dwAllSize = 0;
						shutdown(lpCatList->nClientSocket, 2);
						close(lpCatList->nClientSocket);
						//回收到free
						PutIOList(lpCatList, &lpThreadParam->m_lpFreeEnd, lpThreadParam->m_FreePV, lpThreadParam->m_FreeAction);
					}
				}
			}
			/*-----------------------------------------------------------------*/
			/*-----------------------------接收--------------------------------*/
			/*-----------------------------------------------------------------*/
			else
			{
				//接收已经在ActionTcpPack将数据处理完毕，这里只需要删除epoll事件，回收资源即可
				// 重置 buf
				memset(lpCatList->lpBuf, 0, lpCatList->dwBufflen);
				lpCatList->dwCompleteSize = lpCatList->dwAllSize = 0;
				struct epoll_event Ev;
				Ev.data.ptr = lpCatList;
				if (epoll_ctl(lpThreadParam->iClientEpoll, EPOLL_CTL_DEL, lpCatList->nClientSocket, NULL) < 0)
				{
					FormatError(errno);
				}
				// 关闭连接
				shutdown(lpCatList->nClientSocket, 2);
				close(lpCatList->nClientSocket);
				// 回收到 Free
				PutIOList(lpCatList, &lpThreadParam->m_lpFreeEnd, lpThreadParam->m_FreePV, lpThreadParam->m_FreeAction);
			}
		}
	} while (0);
	return NULL;
}

//事件分派器
BYTE ActionTcpPack(IN DWORD dwClientIP, IN LPIOPACKHEAD lpPackHead, IN DWORD dwPacketBytes)
{
	BYTE IsSendBack = FALSE;//默认设置为不返回
	IPADDRESS_OS  IpOs; //Union方法
	DWORD	dwResult = 0;
	do
	{
		IpOs.dwIP = dwClientIP;
		//先将内容解密
		if (lpPackHead->dwBufferSize)
		{
			Encrypt(lpPackHead + 1, lpPackHead->dwBufferSize, lpPackHead->dwEncrptyWord);
		}
		switch (lpPackHead->dwFunction)
		{
		case FUNCTION_CLIENT_INSERTVALUE:
			printf(" %u.%u.%u.%u Insert Value... \r\r\n", IpOs.cIP1, IpOs.cIP2, IpOs.cIP3, IpOs.cIP4);
			IsSendBack = FALSE;
			dwResult = ClientInsertValue(dwClientIP, lpPackHead, dwPacketBytes);
			break;
		case FUNCTION_CLIENT_QUERYVALUE:
			printf(" %u.%u.%u.%u Query Value... \r\r\n", IpOs.cIP1, IpOs.cIP2, IpOs.cIP3, IpOs.cIP4);
			IsSendBack = TRUE;
			dwResult = ClientQueryValue(dwClientIP, lpPackHead, dwPacketBytes);
			break;
		default:
			// 不返回数据包
			dwResult = FALSE;
			break;
		}
	} while (0);
	lpPackHead->dwRetValue = dwResult;
	if (dwResult)//工作完成
	{
		lpPackHead->dwBufferSize = 0;
	}
	lpPackHead->dwEncrptyWord = (DWORD)time((time_t*)NULL) ^ 0x73132353;
	Encrypt(lpPackHead + 1, lpPackHead->dwBufferSize, lpPackHead->dwEncrptyWord);
	lpPackHead->dwCheckSum = MakeCheckSum(lpPackHead + 1, lpPackHead->dwBufferSize);

	return IsSendBack;
}