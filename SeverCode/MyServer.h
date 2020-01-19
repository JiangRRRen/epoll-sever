#ifndef _SERVER_
#define _SERVER

#include<stdlib.h>
#include<stdio.h>
#include<memory.h>
#include <iconv.h> 
#include <string.h>
#include <unistd.h>
#include <linux/unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <libintl.h>
#include <dlfcn.h>
#include <pthread.h>  
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/ioctl.h> 
#include <sched.h>
#include <sys/epoll.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <mysql/mysql.h>
#include "Error.h"
typedef char CHAR;
typedef unsigned char BYTE;
typedef unsigned char BOOLEAN;
typedef wchar_t WCHAR;
typedef unsigned short WORD;
typedef int INT;
typedef unsigned int UINT;
typedef long BOOL;
typedef long LONG;
typedef unsigned long ULONG;
typedef unsigned int DWORD;
typedef long long INT64;
typedef void* HANDLE;
//typedef unsigned long long UINT64;

typedef CHAR* LPSTR;
typedef const CHAR* LPCSTR;
typedef BYTE* LPBYTE;
typedef WCHAR* LPWSTR;
typedef const WCHAR* LPCWSTR;
typedef WORD* PWORD;
typedef WORD* LPWORD;
typedef INT* LPINT;
typedef UINT* LPUINT;;
typedef BOOL* LPBOOL;
typedef LONG* LPLONG;
typedef ULONG* LPULONG;
typedef DWORD* LPDWORD;

typedef void* PVOID;
typedef void* LPVOID;
typedef size_t SIZE_T;

typedef INT SOCKET;

#define IN
#define OUT

#define INVALID_SOCKET (SOCKET)(~0)

#define TCP_PORT			43857

#define MAX_LISTEN_EVENTS	128
#define MAX_CLIENT_EVENTS	1024
#define MAX_BLOCK_SIZE		(64*1024)
#define MAX_ACTION_COUNT	1024

#define MAX_WORK_THREAD		4
#define	DEVICEMAX			64

#define Print				printf
#define FormatError(x)		PrintfError( (x), __LINE__, __FILE__ )

#define FALSE				0
#define TRUE				1

#define SOCKET_ERROR		(-1)

#define P(x)				pthread_mutex_lock( (pthread_mutex_t *)x )
#define V(x)				pthread_mutex_unlock( (pthread_mutex_t *)x)
#define	ALIGN(x,b)			( ( (x) + (b) - 1 )/(b)*(b) )

#define ReTryNum			3
//#define CreatePV( x )		LinuxCreatePV( x )
//#define ClosePV( x )		LinuxClosePV( x )

#pragma pack(1)

#define		SIG_IOPACK					0xAABBCCDDul
#define		FUNCTION_CLIENT_QUERYVALUE	        14285  
#define		FUNCTION_CLIENT_INSERTVALUE			14287	
#define     FUNCTION_CLIENT_QUERYADDR			14283  
#define     FUNCTION_SENSOR_INSERTVALUE			14281


#define MYSQL_HOST		"cdb-axt937vt.gz.tencentcdb.com" //按所需修改
#define MYSQL_USER  	"jr" //按所需修改
#define MYSQL_PASSWORD	"zxcvb12345"//按所需修改
#define MYSQL_DB		"wx_elec"//按所需修改
#define MYSQL_MYPORT	10059

typedef void* (*LPTHREADFUNCTYPE)(void*);
static HANDLE  g_FreePVTcp = NULL;
static DWORD	g_FreeListNumsTcp = 0;

typedef struct tag_ADDRINFO
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
	DWORD dwDeviceid;      //设备ID
	DWORD dwRoomid;        //房间ID
	DWORD dwAddrid;        //地址ID

	DWORD dwSensorId;      //传感器ID
	DWORD dwSensorType;    //传感器类型

	DWORD dwSensorvalue;   //传感器值
	DWORD dwMAXSensorvalue;//最大上限
	DWORD dwMINSensorvalue;//最小下限


}SENSORDATA, * LPSENSORDATA;

typedef struct tag_ALARM_RECORD
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
///////////////////////

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

typedef struct tag_SENSORCMDSTR
{
	DWORD dwSensorId;
	DWORD dwSensorType;
	char  szCmd[128]; // 为0表示没有了
	//以&分符
	DWORD dwDeviceid;
	DWORD dwRoomid;
	DWORD dwAddrid;
	DWORD dwsensorid;

}SENSORCMDSTR, * LPSENSORCMDSTR;


typedef struct tag_SENSORVALUE
{
	DWORD dwSensorId;
	DWORD dwSensorType;
	DWORD dwValue;

	DWORD dwDeviceid;
	DWORD dwRoomid;
	DWORD dwAddrid;
	DWORD dwsensorid;
}SENSORVALUE, * LPSENSORVALUE;


// 0x04 03 02 01
// 1.2.3.4
typedef union tag_IPADDRESS_OS
{
	struct {
		BYTE	cIP1;
		BYTE	cIP2;
		BYTE	cIP3;
		BYTE	cIP4;
	};
	DWORD dwIP;
} IPADDRESS_OS;

typedef struct tag_IOPACKHEAD_LIST
{
	struct tag_IOPACKHEAD_LIST* lpNext;//链表
	struct tag_IOPACKHEAD_LIST* lpPrev;
	int				nClientSocket;
	int				iEpool;
	BYTE*			lpBuf;
	DWORD			dwBufflen;
	DWORD			dwClientIP;
	DWORD			dwFunction;
	DWORD			dwTime;

	DWORD			dwWillOPFunction;
	DWORD			dwAllSize;
	DWORD			dwCompleteSize;
	DWORD			dwReseaved2;

	BYTE			szMac[6];
}IOPACKHEAD_LIST;

typedef struct tag_CLIENTTHREADPARAM
{

	HANDLE			m_RWPV;
	IOPACKHEAD_LIST	m_RWHead;
	IOPACKHEAD_LIST* m_lpRWEnd;
	HANDLE			m_RWAction;


	HANDLE			m_FreePV;
	IOPACKHEAD_LIST	m_FreeHead;
	IOPACKHEAD_LIST* m_lpFreeEnd;
	HANDLE			m_FreeAction;


	HANDLE			m_PollPV;
	IOPACKHEAD_LIST	m_PollHead;
	IOPACKHEAD_LIST* m_lpPollEnd;
	HANDLE			m_PollAction;

	int				iClientEpoll;
}CLIENTTHREADPARAM;


DWORD PrintfError(IN DWORD dwError, IN DWORD dwLine, IN LPCSTR lpSourceFile);
HANDLE LinuxStartThread(LPTHREADFUNCTYPE lpFunc, void* dwParam);
HANDLE LinuxCreatPV();
void Dump(int signo);
DWORD MakeCheckSum(void* lpBufIn, DWORD dwSize);
DWORD Encrypt(void* lpBufIn, DWORD dwSize, DWORD dwEncryptWord);
int SetSocketNonBlocking(SOCKET nSocket);

IOPACKHEAD_LIST* GetIOList(IOPACKHEAD_LIST* lpHead, IOPACKHEAD_LIST** lpEnd, HANDLE hOptionPV, HANDLE hFreePV, DWORD dwWait);
void DelIOList(IN IOPACKHEAD_LIST* lpCatList, IOPACKHEAD_LIST* lpHead, IOPACKHEAD_LIST** lpEnd, HANDLE hOptionPV);
void PutIOList(IN IOPACKHEAD_LIST* lpCatList, IOPACKHEAD_LIST** lpEnd, HANDLE hOptionPV, HANDLE hFreePV);

void* MainTcpEpollThread(void* lpParam);
void* ActionTcpEpoolThread(void* lpParam);
void* ActionTcpConnectThread(void* lpParam);
BYTE ActionTcpPack(IN DWORD dwClientIP, IN LPIOPACKHEAD lpPackHead, IN DWORD dwPacketBytes);

DWORD ClientInsertValue(IN DWORD dwClientIP, IN LPIOPACKHEAD lpPackHead, IN DWORD dwPacketBytes);
DWORD ClientQueryValue(IN DWORD dwClientIP, IN LPIOPACKHEAD lpPackHead, IN DWORD dwPacketBytes);
DWORD ClientQueryAddr(IN DWORD dwClientIP, IN LPIOPACKHEAD lpPackHead, IN DWORD dwPacketBytes);

#pragma pack()
#endif // !_SER

