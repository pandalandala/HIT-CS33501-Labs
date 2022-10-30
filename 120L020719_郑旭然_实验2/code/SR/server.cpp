//#include "stdafx.h" //创建 VS 项目包含的预编译头文件
#include <stdlib.h>
#include <time.h>
#include <WinSock2.h>
#include <fstream>
#include <iostream>
#include <cmath>

using namespace std;

// #pragma comment(lib,"ws2_32.lib")

#define SERVER_PORT 12340		//端口号
#define SERVER_IP "0.0.0.0"		//IP 地址
const int BUFFER_LENGTH = 1026; //缓冲区大小， （以太网中 UDP 的数据帧中包长度应小于 1480 字节）
const int SEND_WIND_SIZE = 10;	//发送窗口大小为 10，GBN 中应满足 W + 1 <=N（W 为发送窗口大小，N 为序列号个数）

//本例取序列号 0...19 共 20 个
//如果将窗口大小设为 1，则为停-等协议
const int SEQ_SIZE = 20; //序列号的个数，从 0~19 共计 20 个
const int SEQ_NUMBER = 9;

//由于发送数据第一个字节如果值为 0， 则数据会发送失败
//因此接收端序列号为 1~20，与发送端一一对应
BOOL ack[SEQ_SIZE]; //收到 ack 情况，对应 0~19 的 ack
char dataBuffer[SEQ_SIZE][BUFFER_LENGTH];
int curSeq;		 //当前数据包的 seq
int curAck;		 //当前等待确认的 ack
int totalPacket; //需要发送的包总数
int totalSeq;	 //已发送的包的总数
int totalAck;	 //确认收到（ack）的包的总数

//************************************
// Method: getCurTime
// FullName: getCurTime
// Access: public
// Returns: void
// Qualifier: 获取当前系统时间，结果存入 ptime 中
// Parameter: char * ptime
//************************************
void getCurTime(char *ptime)
{
	char buffer[128];
	memset(buffer, 0, sizeof(buffer));
	time_t c_time;
	struct tm *p;
	time(&c_time);
	p = localtime(&c_time);
	sprintf(buffer, "%d/%d/%d %d:%d:%d",
			p->tm_year + 1900,
			p->tm_mon + 1,
			p->tm_mday,
			p->tm_hour,
			p->tm_min,
			p->tm_sec);
	strcpy(ptime, buffer);
}

//************************************
// Method: seqIsAvailable
// FullName: seqIsAvailable
// Access: public
// Returns: bool
// Qualifier: 当前序列号 curSeq 是否可用
//************************************
int seqIsAvailable()
{
	int step;
	step = curSeq - curAck;
	step = step >= 0 ? step : step + SEQ_SIZE;
	//序列号是否在当前发送窗口之内
	if (step >= SEND_WIND_SIZE)
	{
		return 0;
	}
	if (!ack[curSeq])
	{ //ack[curSeq]==FALSE
		return 1;
	}
	return 2;
}

//************************************
// Method: timeoutHandler
// FullName: timeoutHandler
// Access: public
// Returns: void
// Qualifier: 超时重传处理函数，滑动窗口内的数据帧都要重传
//************************************
void timeoutHandler()
{
	printf("******Time out\n");

	if (totalSeq == totalPacket)
	{ //之前发送到了最后一个数据包
		if (curSeq > curAck)
		{
			totalSeq -= (curSeq - curAck);
		}
		else if (curSeq < curAck)
		{
			totalSeq -= (curSeq - curAck + 20);
		}
	}
	else
	{ //之前没发送到最后一个数据包
		totalSeq -= SEND_WIND_SIZE;
	}

	curSeq = curAck;
	printf("*****Rensend from Packet %d\n\n", totalSeq + 1);
}

//************************************
// Method: ackHandler
// FullName: ackHandler
// Access: public
// Returns: void
// Qualifier: 收到 ack，累积确认，取数据帧的第一个字节
//由于发送数据时，第一个字节（序列号）为 0（ASCII）时发送失败，因此加一了，此处需要减一还原
// Parameter: char c
//************************************
void ackHandler(char c)
{
	unsigned char index = (unsigned char)c - 1;	 //序列号减一
	printf("Recv a ack of seq %d\n", index + 1); //从接收方收到的确认收到的序列号
	int next;

	if (curAck == index)
	{
		totalAck += 1;
		ack[index] = FALSE;
		curAck = (index + 1) % SEQ_SIZE;
		for (int i = 1; i < SEQ_SIZE; i++)
		{
			next = (i + index) % SEQ_SIZE;
			if (ack[next] == TRUE)
			{
				ack[next] = FALSE;
				curAck = (next + 1) % SEQ_SIZE;
				totalSeq++;
				curSeq++;
				curSeq %= SEQ_SIZE;
			}
			else
			{
				break;
			}
		}
	}
	else if (curAck < index && index - curAck + 1 <= SEND_WIND_SIZE)
	{ //要保证是要接受的消息（在滑动窗口内）
		if (!ack[index])
		{
			totalAck += 1;
			ack[index] = TRUE;
		}
	}
	else if (SEQ_SIZE + index - curAck + 1 <= SEND_WIND_SIZE && curAck > index)
	{ //要保证是要接受的消息（在滑动窗口内）
		if (!ack[index])
		{
			totalAck += 1;
			ack[index] = TRUE;
		}
	}
}

//************************************
// 当前收到的序列号 recvSeq 是否在可收范围内
//************************************
BOOL seqRecvAvailable(int recvSeq)
{
	int step;
	int index;
	index = recvSeq - 1;
	step = index - curAck;
	step = step >= 0 ? step : step + SEQ_SIZE;
	//序列号是否在当前发送窗口之内
	if (step >= SEND_WIND_SIZE)
	{
		return FALSE;
	}
	return TRUE;
}

//主函数
int main(int argc, char *argv[])
{
	//加载套接字库（必须）
	WORD wVersionRequested;
	WSADATA wsaData;
	//套接字加载时错误提示
	int err;
	//版本 2.2
	wVersionRequested = MAKEWORD(2, 2);
	//加载 dll 文件 Scoket 库
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0)
	{
		//找不到 winsock.dll
		printf("WSAStartup failed with error: %d\n", err);
		return -1;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		printf("Could not find a usable version of Winsock.dll\n");
		WSACleanup();
	}
	else
	{
		printf("The Winsock 2.2 dll was found okay\n");
	}
	SOCKET sockServer = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	//设置套接字为非阻塞模式
	int iMode = 1;											//1：非阻塞，0：阻塞
	ioctlsocket(sockServer, FIONBIO, (u_long FAR *)&iMode); //非阻塞设置
	SOCKADDR_IN addrServer;									//服务器地址
	//addrServer.sin_addr.S_un.S_addr = inet_addr(SERVER_IP);
	addrServer.sin_addr.S_un.S_addr = htonl(INADDR_ANY); //两者均可
	addrServer.sin_family = AF_INET;
	addrServer.sin_port = htons(SERVER_PORT);
	err = bind(sockServer, (SOCKADDR *)&addrServer, sizeof(SOCKADDR));
	if (err)
	{
		err = GetLastError();
		printf("Could not bind the port %d for socket. Error code is %d\n", SERVER_PORT, err);
		WSACleanup();
		return -1;
	}
	//用时间作为随机种子，放在循环的最外面
	srand((unsigned)time(NULL));
	SOCKADDR_IN addrClient; //客户端地址
	int length = sizeof(SOCKADDR);
	char buffer[BUFFER_LENGTH]; //数据发送接收缓冲区
	ZeroMemory(buffer, sizeof(buffer));
	//将测试数据读入内存
	std::ifstream icin;
	icin.open("server_file.txt");
	icin.seekg(0, ios::end);
	int fileSize = (int)icin.tellg();
	icin.seekg(0, ios::beg);
	char data[fileSize + 1];
	icin.read(data, fileSize);
	data[fileSize] = 0;
	icin.close();
	totalPacket = ceil(sizeof(data) / 1024.0);
	printf("totalPacket is: %d\n\n", totalPacket);

	int recvSize;
	for (int i = 0; i < SEQ_SIZE; ++i)
	{
		ack[i] = FALSE;
	}
	while (true)
	{
		//非阻塞接收，若没有收到数据，返回值为-1
		recvSize = recvfrom(sockServer, buffer, BUFFER_LENGTH, 0, ((SOCKADDR *)&addrClient), &length);
		if (recvSize < 0)
		{
			Sleep(200);
			continue;
		}
		printf("recv from client: %s\n", buffer);
		if (strcmp(buffer, "-time") == 0)
		{
			getCurTime(buffer);
		}
		else if (strcmp(buffer, "-quit") == 0)
		{
			strcpy(buffer, "Good bye!");
		}
		else if (strcmp(buffer, "-testsr") == 0)
		{
			//进入 gbn 测试阶段
			//首先 server（server 处于 0 状态）向 client 发送 205 状态码（server进入 1 状态）
			//server 等待 client 回复 200 状态码， 如果收到 （server 进入 2 状态） ，则开始传输文件，否则延时等待直至超时
			//在文件传输阶段，server 发送窗口大小设为
			for (int i = 0; i < SEQ_SIZE; ++i)
			{
				ack[i] = FALSE;
			}
			ZeroMemory(buffer, sizeof(buffer));
			int recvSize;
			int waitCount = 0;
			printf("Begin to test SR protocol,please don't abort the process\n");
			//加入了一个握手阶段
			//首先服务器向客户端发送一个 205 大小的状态码（我自己定义的）表示服务器准备好了，可以发送数据
			//客户端收到 205 之后回复一个 200 大小的状态码，表示客户端备好了，可以接收数据了
			//服务器收到 200 状态码之后，就开始使用 GBN 发送数据了
			printf("Shake hands stage\n");
			int stage = 0;
			bool runFlag = true;
			while (runFlag)
			{
				switch (stage)
				{
				case 0: //发送 205 阶段
					buffer[0] = 205;
					sendto(sockServer, buffer, strlen(buffer) + 1, 0, (SOCKADDR *)&addrClient, sizeof(SOCKADDR));
					Sleep(100);
					stage = 1;
					break;
				case 1: //等待接收 200 阶段，没有收到则计数器+1，超时则放弃此次“连接”，等待从第一步开始
					recvSize = recvfrom(sockServer, buffer, BUFFER_LENGTH, 0, ((SOCKADDR *)&addrClient), &length);
					if (recvSize < 0)
					{
						++waitCount;
						if (waitCount > 20)
						{
							runFlag = false;
							printf("Timeout error\n");
							break;
						}
						Sleep(500);
						continue;
					}
					else
					{
						if ((unsigned char)buffer[0] == 200)
						{
							printf("Begin a file transfer\n");
							printf("File size is %dB, each packet is 1024B  and packet total num is %d\n", sizeof(data), totalPacket);
							curSeq = 0;
							curAck = 0;
							totalSeq = 0;
							waitCount = 0;
							totalAck = 0;
							stage = 2;
						}
					}
					break;
				case 2: //数据传输阶段

					if (seqIsAvailable() == 1 && totalSeq <= (totalPacket - 1))
					{ //totalSeq<=(totalPacket-1)：未传到最后一个数据包

						//发送给客户端的序列号从 1 开始
						buffer[0] = curSeq + 1;
						ack[curSeq] = FALSE;
						//数据发送的过程中应该判断是否传输完成
						memcpy(&buffer[1], data + 1024 * totalSeq, 1024);
						printf("send a packet with a seq of: %d \n", curSeq + 1);
						// printf("totalSeq now is: %d\n", totalSeq+1);
						sendto(sockServer, buffer, BUFFER_LENGTH, 0, (SOCKADDR *)&addrClient, sizeof(SOCKADDR));
						curSeq++;
						curSeq %= SEQ_SIZE;
						totalSeq++;
						Sleep(500);
					}
					else if (seqIsAvailable() == 2 && totalSeq <= (totalPacket - 1))
					{
						curSeq++;
						curSeq %= SEQ_SIZE;
						totalSeq++;
						break;
					}
					//等待 Ack，若没有收到，则返回值为-1，计数器+1
					recvSize = recvfrom(sockServer, buffer, BUFFER_LENGTH, 0, ((SOCKADDR *)&addrClient), &length);
					if (recvSize < 0)
					{
						waitCount++;
						//20 次等待 ack 则超时重传
						if (waitCount > 20)
						{
							timeoutHandler();
							waitCount = 0;
						}
					}
					else
					{
						//收到 ack
						ackHandler(buffer[0]);
						waitCount = 0;

						if (totalAck == totalPacket)
						{ //数据传输完成
							printf("Data Transfer Complete\n");
							strcpy(buffer, "Data Transfer Complete\n");
							runFlag = false;
							break;
						}
					}
					Sleep(500);
					break;
				}
			}
		}

		sendto(sockServer, buffer, strlen(buffer) + 1, 0, (SOCKADDR *)&addrClient, sizeof(SOCKADDR));
		Sleep(500);
	}
	//关闭套接字，卸载库
	closesocket(sockServer);
	WSACleanup();
	return 0;
}