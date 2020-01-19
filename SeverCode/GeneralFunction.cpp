#include"MyServer.h"
//打印错误
DWORD PrintfError(IN DWORD dwError, IN DWORD dwLine, IN LPCSTR lpSourceFile)
{
	LPCSTR lpFileName = "";

	do
	{
		if (NULL != lpSourceFile)
		{
			lpFileName = strrchr(lpSourceFile, '/');
			if (NULL == lpFileName)
			{
				lpFileName = lpSourceFile;
			}
			else
			{
				lpFileName += 1;//
			}
		}
		Print("ErrCode: %u, Line: %u, Source: %s\r\r\n", dwError, dwLine, (char*)lpFileName);
	} while (0);

	return dwError;
}
//创建线程
HANDLE LinuxStartThread(LPTHREADFUNCTYPE lpFunc, void* dwParam)
{
	//宏定义 HANDLE=void *
	HANDLE hRet = (HANDLE)NULL;
	DWORD Ret = pthread_create((pthread_t*)& hRet, NULL, lpFunc, dwParam);
	if (Ret != 0)
	{
		FormatError(Ret);
	}
	return hRet;//需要返回void*类型，所以hRet不能变，hRet指针指向的地方为线程号
}
//创建锁
HANDLE LinuxCreatPV()
{
	pthread_mutex_t* hRet;
	hRet = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	if (hRet == NULL) return (HANDLE)hRet;
	if (pthread_mutex_init(hRet, (pthread_mutexattr_t*)NULL))//互斥锁创建成功返回0
	{
		free(hRet); hRet = (pthread_mutex_t*)NULL;
	}
	//else
	//{
	//	if (x) pthread_mutex_lock(hRet); //传入的是0，所以不锁
	//}
	return (HANDLE)hRet;
}
//信号处理函数
void Dump(int signo)
{
	char CMD[64];//写入到错误日志
	snprintf(CMD, sizeof(CMD), "gdb -pid=%d -ex=\"thread apply all bt\" -batch > ./error.txt", getpid());
	system(CMD);
	exit(0);
}
//包校验和
DWORD MakeCheckSum(void* lpBufIn, DWORD dwSize)
{
	DWORD	dwRet = 0;
	BYTE* lpBuf = (BYTE*)lpBufIn;
	int i;
	for ( i= 0; i + 4 <= dwSize; i += 4)
	{
		dwRet += *(DWORD*)(lpBuf + i);
	}
	if (dwRet == 0) dwRet = 0x30303030ul;
	return dwRet;
}
//加密解密
DWORD Encrypt(void* lpBufIn, DWORD dwSize, DWORD dwEncryptWord)
{
	DWORD	dwRet = 0;
	BYTE*	lpBuf = (BYTE*)lpBufIn;
	DWORD	dwValue = dwEncryptWord;
	int i;
	for (i = 0; i + 4 <= dwSize; i += 4)
	{
		dwValue ^= (i + 533353ul) * 533777ul;
		*(DWORD*)(lpBuf + i) ^= dwValue;
	}
	return dwRet;

}
//设置为非阻塞
int SetSocketNonBlocking(SOCKET nSock)
{
	int nResult = FALSE;
	INT nOpts = 0;

	do
	{
		nOpts = fcntl(nSock, F_GETFL);
		if (0 > nOpts)
		{
			nResult = FormatError(errno); break;
		}
		nOpts = nOpts | O_NONBLOCK;
		if (0 > fcntl(nSock, F_SETFL, nOpts))
		{
			nResult = FormatError(errno); break;
		}
	} while (0);

	return nResult;
}