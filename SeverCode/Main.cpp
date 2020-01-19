#include "MyServer.h"

int main(int argc, char* argv[])
{
	INT nResult = 0;
	pthread_t	hTcpThread = 0;
	LPSTR lpszResult = NULL;

	signal(SIGSEGV, &Dump);//segmentation violation试图访问未分配的内存
	do
	{
		pthread_create((pthread_t*)& hTcpThread, (pthread_attr_t*)NULL, MainTcpEpollThread, (void*)NULL);
		// 等待线程结束
		if (hTcpThread)
		{
			pthread_join(hTcpThread, (void**)& lpszResult);
		}
	} while (0);
	printf("Exit!\r\n");

	return 0;
}