#include"MyServer.h"

//从链表中取节点
IOPACKHEAD_LIST* GetIOList(IOPACKHEAD_LIST* lpHead, IOPACKHEAD_LIST** lpEnd, HANDLE hOptionPV, HANDLE hFreePV, DWORD dwWait)
{
	//hOptionPV要取的目标链表的PV锁，hFreePV空闲链表的PV锁
	IOPACKHEAD_LIST* lpCatList = (IOPACKHEAD_LIST*)NULL;
	while (1)
	{
		P(hOptionPV);//锁住目标链表
		if (lpHead->lpNext == NULL) //如果链表为空
		{
			V(hOptionPV);
			if (dwWait == 0) //如果不设置等待时间
			{
				printf("List has no empty node!\r\n");
				break;
			}
			sleep(1);
			P(hFreePV);
			//如果设置等待，会先将Free链表锁住，
			//下次运行到P(hFreePV)时由于P了两次，所以会阻塞
			//在PutIOList添加节点函数中会释放hFreePV，证明有节点进入
			continue;
		}

		lpCatList = lpHead->lpNext;
		lpHead->lpNext = lpCatList->lpNext;
		if (lpCatList->lpNext) lpCatList->lpNext->lpPrev = lpHead;
		else *lpEnd = lpHead; //一定要使用二级指针，否则会覆盖

		if (hOptionPV == g_FreePVTcp) g_FreeListNumsTcp--;
		V(hOptionPV);
		break;
	}
	return lpCatList;
}
//删除节点
void DelIOList(IN IOPACKHEAD_LIST* lpCatList, IOPACKHEAD_LIST* lpHead, IOPACKHEAD_LIST** lpEnd, HANDLE hOptionPV)
{
	P(hOptionPV);
	if (lpCatList->lpPrev) lpCatList->lpPrev->lpNext = lpCatList->lpNext;
	if (lpCatList->lpNext) lpCatList->lpNext->lpPrev = lpCatList->lpPrev;
	if (lpCatList == *lpEnd)* lpEnd = lpCatList->lpPrev;
	V(hOptionPV);
}
//添加节点
void PutIOList(IN IOPACKHEAD_LIST* lpCatList, IOPACKHEAD_LIST** lpEnd, HANDLE hOptionPV, HANDLE hFreePV)
{
	P(hOptionPV);
	(*lpEnd)->lpNext = lpCatList;
	lpCatList->lpPrev = *lpEnd;
	lpCatList->lpNext = (IOPACKHEAD_LIST*)NULL;
	*lpEnd = lpCatList;
	if (hOptionPV == g_FreePVTcp)
	{
		g_FreeListNumsTcp++;
	}
	V(hOptionPV);
	V(hFreePV);
}