#include<iostream>
#include<Windows.h>
using namespace std;
#pragma comment(lib, "ws2_32.lib")

#define SIG_IOPACK				0xAABBCCDDul
#define TCP_PORT				43857
#define TCP_IP					0x6200a8c0
#define FUNCTION_QUERYVALUE		14285
#define FUNCTION_INSERTVALUE	14287
#define	FUNCTION_QUERYADDR		14283
#define SOCKET_ERROR			(-1) //加上括号避免符号出错
#define DEVICEMAX				128
#define TryConnect				5
#define TrySend					5
#pragma pack(1) //令结构体边界对齐为1

typedef struct tag_ADDRINFO //typedef定义一个数据类型，把已知数据类型struct变为ADDRINFO
{
	DWORD addrid;          //地址ID
	CHAR name[64];
	DWORD province;	       //省份代码
	DWORD city;            //城市代码
	DWORD area;            //区域代码
	CHAR address[64];         //地址
	CHAR sitename[64];        //定位地址
	CHAR proname[64];         //省份地址
	CHAR cityname[64];        //城市地址
	CHAR areaname[64];        //区域地址
	CHAR devicename[64];      //设备名
	CHAR sensorname[64];      //传感器名

	DWORD sensorid;
	DWORD deviceid;
	DWORD parentid;
	DWORD userid;
	CHAR alarm_phone[12];     //报警电话

	DWORD dwSensorvalue;   //传感器值
	DWORD dwMAXSensorvalue;//最大上限
	DWORD dwMINSensorvalue;//最小下限
	DWORD thresholdvalue;       //阀值  	

}ADDRINFO, * LPADDRINFO;

typedef struct tag_SENSORDATA
{
	//DWORD dwDev;      //test
	DWORD dwDeviceId;      //设备ID
	DWORD dwRoomid;        //房间ID
	DWORD dwAddrid;        //地址ID

	DWORD dwSensorId;      //传感器ID
	DWORD dwSensorType;    //传感器类型

	DWORD dwSensorvalue;   //传感器值
	DWORD dwMAXSensorvalue;//最大上限
	DWORD dwMINSensorvalue;//最小下限


}SENSORDATA, * LPSENSORDATA;

typedef struct tag_IOPackHead
{
	DWORD	dwSig;
	DWORD	dwFunction;
	DWORD	dwRetValue;
	DWORD	dwCheckSum;
	DWORD	dwBufferSize;
	DWORD	dwEncrptyWord;
	DWORD	dwRealSize;
	DWORD	dwReseaved;
}IOPACKHEAD, * LPIOPACKHEAD;

typedef struct ALARM_RECORD
{
	DWORD dwLevel;         //报警等级
	DWORD dwDeviceid;      //设备ID
	DWORD dwRoomid;        //房间ID
	DWORD dwAddrid;        //地址ID

	DWORD dwSensorId;      //传感器ID
	DWORD dwSensorType;    //传感器类型

	DWORD dwIs_Solve;      //是否处理 1:处理 0：未处理

	DWORD dwSensorvalue;   //传感器值
	DWORD dwMAXSensorvalue;//最大上限
	DWORD dwMINSensorvalue;//最小下限
	DWORD thresholdvalue;       //阀值

}ALARM_RECORD, * LPALARM_RECORD;

typedef struct tag_SENSORCMDSTR
{
	DWORD dwSensorId;
	DWORD dwSensorType;
	CHAR  dwCmd[128];			//命令码

	DWORD dwDeviceID;
	DWORD dwRoomID;
	DWORD dwAddrID;
	DWORD dwsensorID;
}SENSORCMDSTR, * LPSENSORCMDSTR;

typedef struct tag_SENSORVALUE
{
	DWORD dwSensorId;
	DWORD dwSensorType;
	DWORD dwvalue;

	DWORD dwDeviceid;
	DWORD dwRoomid;
	DWORD dwAddrid;
	DWORD dwsensorid;

}SENSORVALUE, * LPSENSORVALUE;

SENSORCMDSTR* lpSensor = NULL;
IOPACKHEAD* lpPackHead = NULL;
int isocket = SOCKET_ERROR;
DWORD Encrypt(BYTE* lpBuf, DWORD	dwSize, DWORD dwEncrpytWord)
{
	DWORD	dwRet = 0;
	DWORD i;
	DWORD	dwValue = dwEncrpytWord;
	for (i = 0; i + 4 <= dwSize; i += 4)
	{
		dwValue ^= (i + 533353) * 533777;
		*(DWORD*)(lpBuf + i) ^= dwValue;
	}
	return dwRet;
}
DWORD MakeCheckSum(BYTE* lpBuf, DWORD	dwSize)
{
	DWORD	dwRet = 0;
	DWORD i;
	for (i = 0; i + 4 <= dwSize; i += 4)
	{
		dwRet += *(DWORD*)(lpBuf + i);
	}
	if (dwRet == 0) dwRet = 0x30303030ul; //避免dwSize<4,循环不进入
	return dwRet;
}
DWORD communicate_server_query(DWORD dwFunction, BYTE *lpBufSend, DWORD dwBufSendSize,BYTE *lpBufRec,DWORD *lpBufRecSize)
{
	DWORD	dwRet = FALSE;
	WORD wVersion = MAKEWORD(1, 1);
	WSADATA wsaData;
	dwRet = WSAStartup(wVersion, (LPWSADATA)& wsaData); //执行成功返回0，否则返回错误代码
	if (dwRet != 0)
	{
		printf("Init Socket Failed!\n");
		return dwRet;
	}
	if (!lpPackHead)
	lpPackHead = (IOPACKHEAD*)malloc(sizeof(IOPACKHEAD) + 1024 * 1024); 
	if (lpPackHead == NULL)
	{	
		printf("lpPackHead Creat Failed!\n");
		return FALSE;
	}
	memset(lpPackHead, 0, sizeof(IOPACKHEAD) + 1024 * 1024);
	printf("Init Socket Success!\n");
	
	
	DWORD dwPackSize = sizeof(IOPACKHEAD);
	DWORD dwRetSize = sizeof(IOPACKHEAD);

	//连接
	int i;
	for (i = 0; i < TryConnect; i++)
	{
		printf("Try to Connect %d\n",i+1);
		Sleep(1000);
		//先清空
		if (isocket && isocket != SOCKET_ERROR)
		{
			shutdown(isocket, 2);
			closesocket(isocket);
		}
		isocket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
		if (isocket == SOCKET_ERROR)
		{	
			printf("Socket Creat Failed!\n");
			continue;
		}

		sockaddr_in m_sin;
		m_sin.sin_family = AF_INET;
		m_sin.sin_port = htons(TCP_PORT);
		m_sin.sin_addr.s_addr = inet_addr("Put your own IP");

		dwRet = connect(isocket, (sockaddr*)& m_sin, sizeof(m_sin));
		if (dwRet ==-1)
		{
			printf("Socket Connect Failed! %d \n",i);
			shutdown(isocket, 2);
			closesocket(isocket);
			isocket = SOCKET_ERROR;
			continue;
		}
		break;
	}
	if (i == TryConnect )
	{
		printf("Connect Outtime!\n");
		return FALSE;
	}
	printf("Connect Success!\n");

	//////////////////////////////////
	lpPackHead->dwSig = SIG_IOPACK;
	lpPackHead->dwFunction = dwFunction;
	lpPackHead->dwRetValue = 0x30303030ul;
	lpPackHead->dwBufferSize = dwBufSendSize;
	//lpPackHead->dwEncrptyWord = (DWORD)0x7533253 ^ 0x73132353;
	lpPackHead->dwEncrptyWord = (DWORD)0x753 ^ 0x731;

	memcpy((BYTE*)(lpPackHead + 1), lpBufSend, dwBufSendSize);//从lpBufSend指针处开始的dwBufSendSize个数据拷贝到lpPackHead + 1处，此时lpBufSend是空的
	Encrypt((BYTE*)(lpPackHead + 1), lpPackHead->dwBufferSize, lpPackHead->dwEncrptyWord);
	lpPackHead->dwCheckSum= MakeCheckSum((BYTE*)(lpPackHead + 1), lpPackHead->dwBufferSize);
	dwPackSize= sizeof(IOPACKHEAD) + lpPackHead->dwBufferSize;
	//////////////////////////////////////////
	//发送
	for (i = 0; i < TrySend; i++)
	{
		printf("Try to Send %d\n",i+1);
		Sleep(1000);
		dwRet = send(isocket, (char*)lpPackHead, dwPackSize, 0);
		printf("SendSize:%d PackSize：%d\r\n", dwRet, dwPackSize);
		if (dwRet != dwPackSize)
		{
			printf("Send Failed!\n");
			continue;
		}
		break;
	}
	if (i == TryConnect)
	{
		printf("Connect Outtime!\n");
		return FALSE;
	}
	printf("Send Success!\n");
	//////////////////////////////
	dwRet = 0;
	lpPackHead->dwSig = 0;
	DWORD	dwRetedSize = 0;
	char* lpRetBuf = (char*)lpPackHead;
	dwRetSize = 1024 * 1024 + sizeof(IOPACKHEAD); 
	////////////////
	//接收
	while (dwRetSize)
	{
		printf("Try to Recieve %d...\n", i);
		int iRet = recv(isocket, lpRetBuf, dwRetSize, 0);
		printf("----1.iRet:%d  2.dwRetSize:%d----\r\n", iRet, dwRetSize);
		if (iRet > 0 && iRet <= (int)dwRetSize)
		{
			if (dwRetSize == 1024 * 1024 + sizeof(IOPACKHEAD) && lpPackHead->dwSig == SIG_IOPACK)
			{
				dwRetSize = sizeof(IOPACKHEAD) + lpPackHead->dwBufferSize;
			}
			dwRetedSize += iRet;
			dwRetSize -= iRet;
			lpRetBuf += iRet;
		}
		else
		{
			dwRet = FALSE;
			break;
		}

	}
	dwRet = 1;
	if (lpPackHead->dwBufferSize && lpPackHead->dwCheckSum != MakeCheckSum((BYTE*)(lpPackHead + 1), lpPackHead->dwBufferSize))
	{
		printf("CheckSum Error!\n");
		dwRet = FALSE;
	}

	Encrypt((BYTE*)(lpPackHead + 1), lpPackHead->dwBufferSize, lpPackHead->dwEncrptyWord);
	if (lpBufRec)memcpy(lpBufRec, lpPackHead + 1, lpPackHead->dwBufferSize);
	if (lpBufRecSize)* lpBufRecSize = lpPackHead->dwBufferSize;

	return dwRet;
}
DWORD alarm_system_init()
{
	DWORD	dwRet = FALSE;
	SENSORDATA SensorData = { 0 };
	LPSENSORDATA recvsensordata = (LPSENSORDATA)malloc(sizeof(recvsensordata)); //用于保存返回的的结构体

	if (recvsensordata == NULL)
	{
		printf("lpReSensorData Creat Failed!\n");
		return FALSE;
	}
	/*test for insert*/
	//typedef struct ALARM_RECORD
	//{
	//	DWORD dwLevel;         //报警等级
	//	DWORD dwDeviceid;      //设备ID
	//	DWORD dwRoomid;        //房间ID
	//	DWORD dwAddrid;        //地址ID

	//	DWORD dwSensorId;      //传感器ID
	//	DWORD dwSensorType;    //传感器类型

	//	DWORD dwIs_Solve;      //是否处理 1:处理 0：未处理

	//	DWORD dwSensorvalue;   //传感器值
	//	DWORD dwMAXSensorvalue;//最大上限
	//	DWORD dwMINSensorvalue;//最小下限
	//	DWORD thresholdvalue;       //阀值

	//}ALARM_RECORD, * LPALARM_RECORD;
	ALARM_RECORD SensorDataInsert = { 0 };
	LPSENSORDATA recvsensordata2 = (LPSENSORDATA)malloc(sizeof(recvsensordata2)); //用于保存返回的的结构体
	ADDRINFO addrinfo = { 0 };
	LPADDRINFO recaddrinfo = (LPADDRINFO)malloc(sizeof(recaddrinfo));
	addrinfo.addrid = 1;
	addrinfo.deviceid = 1001001;
	addrinfo.sensorid = 1001001001;
	memset(recvsensordata2, 0, sizeof(recvsensordata2));
	//SensorDataInsert.dwDeviceId = 1111;
	//SensorDataInsert.dwAddrid = 4396;
	//SensorDataInsert.dwRoomid = 777;
	//
	//SensorDataInsert.dwSensorId = 777;
	//SensorDataInsert.dwSensorType = 777;
	//
	//SensorDataInsert.dwSensorvalue = 2800;
	//SensorDataInsert.dwMAXSensorvalue = 1;
	//SensorDataInsert.dwMINSensorvalue = 5000;
	//SensorDataInsert.dwAddrid = 1111;
	//SensorDataInsert.dwDeviceid = 111;
	//SensorDataInsert.dwIs_Solve = 111;
	//SensorDataInsert.dwLevel = 111;
	//SensorDataInsert.dwMAXSensorvalue = 111;
	//SensorDataInsert.dwMINSensorvalue = 2222;
	//SensorDataInsert.dwRoomid = 123;
	//SensorDataInsert.dwSensorId = 3333;
	//SensorDataInsert.dwSensorType = 333;
	//SensorDataInsert.dwSensorvalue = 2323;
	//SensorDataInsert.thresholdvalue = 2222;
	DWORD testRet = communicate_server_query(FUNCTION_INSERTVALUE,
		(BYTE*)&SensorDataInsert, sizeof(SensorDataInsert), (BYTE*)recvsensordata2, NULL);


	/*------------------*/
	while (1)
	{
		memset(recvsensordata, 0, sizeof(recvsensordata));
		memset(recaddrinfo, 0, sizeof(recaddrinfo));
		printf("Ready for Connect...\n");
		//dwRet=communicate_server_qury(FUNCTION_QUERYVALUE, 
		//	(BYTE*)& SensorData, sizeof(SensorData), (BYTE*)recvsensordata,NULL);
		dwRet = communicate_server_query(FUNCTION_QUERYADDR, (BYTE*)&addrinfo, sizeof(addrinfo), (BYTE*)recaddrinfo, NULL);
		if (dwRet == FALSE)
		{
			printf("Connect Failed!\n");
			break;
		}
		recaddrinfo = (LPADDRINFO)recaddrinfo;
		recvsensordata = (LPSENSORDATA)recvsensordata;
		printf("Roomid:%u\r\n",
			recvsensordata->dwRoomid);
		//printf(Mes);

		printf("Wait...\n");
		Sleep(200000); //win32
	}
	free(recvsensordata);
	return dwRet;
}
int main()
{
	DWORD	dwRet = FALSE; //windows类定义FALSE=0
	dwRet = alarm_system_init();
	return 0;
}