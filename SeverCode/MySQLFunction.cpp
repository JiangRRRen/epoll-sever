#include"MyServer.h"
DWORD ClientInsertValue(IN DWORD dwClientIP, IN LPIOPACKHEAD lpPackHead, IN DWORD dwPacketBytes)
{
	//客户端插入报警信息
	DWORD dwResult = 0;//返回标志，0表示失败，1表示成功
	IPADDRESS_OS IpOs;
	LPALARM_RECORD lpAlarmRecord = NULL;
	MYSQL hMySql;//句柄
	CHAR SQL_CMD[256] = { 0 };
	do
	{
		IpOs.dwIP = dwClientIP;
		//包校验
		if (dwPacketBytes != sizeof(IOPACKHEAD) + sizeof(ALARM_RECORD))
		{
			dwResult = FormatError(ERROR_INVALID_DATA); 
			break;
		}
		lpAlarmRecord = (LPALARM_RECORD)(lpPackHead + 1);
		/*-------------------------MySQL操作开始------------------------*/

		do {
			mysql_init(&hMySql);
			printf("Connect % s: % u with Database % s ...\r\r\n", MYSQL_HOST, MYSQL_MYPORT, MYSQL_DB);
			if (NULL == mysql_real_connect(&hMySql, MYSQL_HOST, MYSQL_USER, MYSQL_PASSWORD, MYSQL_DB, MYSQL_MYPORT, NULL, 0))
			{
				dwResult = FormatError(mysql_errno(&hMySql));
				break; // 2003=mySql服务未启动
			}
			sprintf(SQL_CMD,"INSERT INTO alarm(createtime,level,dsensorid,status,deviceid,roomid,addrid,sensorid) VALUES(UNIX_TIMESTAMP(NOW()),%d,%d,%d,%d,%d,%d,%d)",\
				lpAlarmRecord->dwLevel, lpAlarmRecord->dwDeviceid, \
				lpAlarmRecord->dwIs_Solve,lpAlarmRecord->dwDeviceid,\
				lpAlarmRecord->dwRoomid,lpAlarmRecord->dwAddrid, \
				lpAlarmRecord->dwSensorId);
			dwResult = mysql_query(&hMySql, SQL_CMD);
			if (dwResult)
			{
				dwResult = FormatError(mysql_errno(&hMySql));
				printf(mysql_error(&hMySql));
				break;
			}
		} while (0);
		mysql_close(&hMySql);
		/*-------------------------MySQL操作结束------------------------*/
		if(dwResult)
		{
			FormatError(dwResult);
		}
		else
		{
			printf("Inser Success!\r\n");
		}
	} while (0);
	lpPackHead->dwBufferSize = 0;

	return dwResult;
}
DWORD ClientQueryValue(IN DWORD dwClientIP, IN LPIOPACKHEAD lpPackHead, IN DWORD dwPacketBytes)
{
	DWORD dwResult = 0;//返回标志，0表示失败，1表示成功
	IPADDRESS_OS IpOs;
	LPSENSORDATA lpSensorCmd = NULL;
	CHAR SQL_CMD[1024] = { 0 };
	MYSQL hMySql;
	MYSQL_RES* lpResults = NULL;
	DWORD dwCount = 0;
	LPSENSORDATA lpSensordata = NULL;
	MYSQL_ROW record;
	do {
		IpOs.dwIP = dwClientIP;
		if (dwPacketBytes != sizeof(IOPACKHEAD) + sizeof(SENSORDATA))//包校验
		{
			dwResult = FormatError(ERROR_INVALID_DATA);
			break;
		}
		lpSensorCmd = (LPSENSORDATA)(lpPackHead + 1);

		lpPackHead->dwBufferSize = 0;
		memset(lpSensorCmd, 0,sizeof(SENSORDATA));
		/*-------------------------MySQL操作开始------------------------*/
		do
		{
			mysql_init(&hMySql);
			printf("Connect % s: % u with Database % s ...\r\r\n", MYSQL_HOST, MYSQL_MYPORT, MYSQL_DB);
			if (NULL == mysql_real_connect(&hMySql, MYSQL_HOST, MYSQL_USER, MYSQL_PASSWORD, MYSQL_DB, MYSQL_MYPORT, NULL, 0))
			{
				dwResult = FormatError(mysql_errno(&hMySql));
				break; // 2003=mySql服务未启动
			}
			sprintf(SQL_CMD,"SELECT sensor_value.sendata,sensor_value.sensortype,sensor_value.deviceid,sensor_value.roomid,sensor_value.addrid,sensor_value.sensorid,device_sensor.upplimit,device_sensor.lowlimit FROM sensor_value,device_sensor WHERE sensor_value.sensorid=device_sensor.sensorid and sensor_value.createtime=(select max(createtime) FROM sensor_value)");
			dwResult = mysql_real_query(&hMySql, SQL_CMD, strlen(SQL_CMD));//内存出错
			if (dwResult)
			{
				dwResult = FormatError(mysql_errno(&hMySql));
				break;
			}
			lpResults = mysql_store_result(&hMySql);
			if (NULL == lpResults)
			{
				dwResult = FormatError(mysql_errno(&hMySql));
				break;
			}
			if (0 == mysql_num_rows(lpResults))
			{
				printf("Empty!\r\n");
				break;
			}
			while (record = mysql_fetch_row(lpResults))
			{
				lpSensordata = lpSensorCmd + dwCount;
				memset(lpSensordata, 0,sizeof(SENSORDATA));
				if (record[0] != NULL)
				{
					double record_value = 0;
					record_value = atof(record[0]);
					record_value = (record_value * 100);
					lpSensordata->dwSensorvalue = record_value;
				}
				//从字符串读取格式化输入
				if (record[1] != NULL)sscanf(record[1], "%d", &(lpSensordata->dwSensorType));
				if (record[2] != NULL)sscanf(record[2], "%d", &(lpSensordata->dwDeviceid));
				if (record[3] != NULL)sscanf(record[3], "%d", &(lpSensordata->dwRoomid));
				if (record[4] != NULL)sscanf(record[4], "%d", &(lpSensordata->dwAddrid));
				if (record[5] != NULL)sscanf(record[5], "%d", &(lpSensordata->dwSensorId));
				if (record[6] != NULL)sscanf(record[6], "%d", &(lpSensordata->dwMAXSensorvalue));
				lpSensordata->dwMAXSensorvalue = (lpSensordata->dwMAXSensorvalue * 100);
				if (record[7] != NULL)sscanf(record[7], "%d", &(lpSensordata->dwMINSensorvalue));
				lpSensordata->dwMINSensorvalue = (lpSensordata->dwMINSensorvalue * 100);

				dwCount++;
				 printf( "------Roomid:%uDeviceid:%u  Addrid:%u SensorId:%u SensorType:%u Sensorvalue:%u MAXSensorvalue:%u MINSensorvalue:%u------\r\r\n", 
 lpSensordata->dwRoomid,lpSensordata->dwDeviceid,lpSensordata->dwAddrid,lpSensordata->dwSensorId,lpSensordata->dwSensorType,lpSensordata->dwSensorvalue,
 lpSensordata->dwMAXSensorvalue,lpSensordata->dwMINSensorvalue);
			}
		} while (0);
		/*-------------------------MySQL操作结束------------------------*/
		if (dwResult)
		{
			FormatError(dwResult); 
			break;
		}
		// 要发送n个数据，必须发送n+1个(n+1<=DEVICEMAX)，后生一个须全0
		lpPackHead->dwBufferSize = (dwCount) * sizeof(SENSORDATA); 
		
	} while (0);
	return dwResult;

}
