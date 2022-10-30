//#include "stdafx.h" //���� VS ��Ŀ������Ԥ����ͷ�ļ�
#include <stdlib.h>
#include <time.h>
#include <WinSock2.h>
#include <fstream>
#include <iostream>
#include <cmath>

using namespace std;

// #pragma comment(lib,"ws2_32.lib")

#define SERVER_PORT 12340       //�˿ں�
#define SERVER_IP "0.0.0.0"     // IP ��ַ
const int BUFFER_LENGTH = 1026; //��������С������̫���� UDP ������ ֡�а�����ӦС�� 1480 �ֽڣ�
const int SEND_WIND_SIZE = 10;  //���ʹ��ڴ�СΪ 10��GBN ��Ӧ���� W + 1 <= N��W Ϊ���ʹ��ڴ�С��N Ϊ���кŸ�����
//����ȡ���к� 0...19 �� 20 ��
//��������ڴ�С��Ϊ 1����Ϊͣ-��Э��

const int SEQ_SIZE = 20; //���кŵĸ������� 0~19 ���� 20 ��
//���ڷ������ݵ�һ���ֽ����ֵΪ 0�������ݻᷢ��ʧ��
//��˽��ն����к�Ϊ 1~20���뷢�Ͷ�һһ��Ӧ

BOOL ack[SEQ_SIZE]; // �յ� ack �������Ӧ 0~19 �� ack
int curSeq;         // ��ǰ���ݰ��� seq
int curAck;         // ��ǰ�ȴ�ȷ�ϵ� ack
int totalSeq;       // �յ��İ�������
int totalPacket;    // ��Ҫ���͵İ�����

//************************************
// Method: getCurTime
// FullName: getCurTime
// Access: public
// Returns: void
// Qualifier: ��ȡ��ǰϵͳʱ�䣬������� ptime ��
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
// Method:    lossInLossRatio
// FullName:  lossInLossRatio
// Access:    public
// Returns:   BOOL
// Qualifier: ���ݶ�ʧ���������һ�����֣��ж��Ƿ�ʧ,��ʧ�򷵻� TRUE�����򷵻� FALSE
// Parameter: float lossRatio [0,1]
//************************************
BOOL lossInLossRatio(float lossRatio)
{
    int lossBound = (int)(lossRatio * 100);
    int r = rand() % 101;
    if (r <= lossBound)
    {
        return TRUE;
    }
    return FALSE;
}

//************************************
// Method: seqIsAvailable
// FullName: seqIsAvailable
// Access: public
// Returns: bool
// Qualifier: ��ǰ���к� curSeq �Ƿ����
//************************************
bool seqIsAvailable()
{
    int step;
    step = curSeq - curAck;
    step = step >= 0 ? step : step + SEQ_SIZE;
    //���к��Ƿ��ڵ�ǰ���ʹ���֮��
    if (step >= SEND_WIND_SIZE)
    {
        return false;
    }
    if (ack[curSeq])
    {
        return true;
    }
    return false;
}

//************************************
// Method: timeoutHandler
// FullName: timeoutHandler
// Access: public
// Returns: void
// Qualifier: ��ʱ�ش��������������������ڵ�����֡��Ҫ�ش�
//************************************
void timeoutHandler()
{
    printf("*****Time out\n");
    int index;
    for (int i = 0; i < SEND_WIND_SIZE; ++i)
    {
        index = (i + curAck) % SEQ_SIZE;
        ack[index] = TRUE;
    }
    int temp = curSeq - curAck;
    totalSeq -= temp > 0 ? temp : temp + SEQ_SIZE;
    curSeq = curAck;
    printf("*****Rensend from Packet %d\n\n", totalSeq);
}

//************************************
// Method: ackHandler
// FullName: ackHandler
// Access: public
// Returns: void
// Qualifier: �յ� ack���ۻ�ȷ�ϣ�ȡ����֡�ĵ�һ���ֽ�
//���ڷ�������ʱ����һ���ֽڣ����кţ�Ϊ 0��ASCII��ʱ����ʧ�ܣ���˼�һ�ˣ��˴���Ҫ��һ��ԭ
// Parameter: char c
//************************************
void ackHandler(char c)
{
    unsigned char index = (unsigned char)c - 1; //���кż�һ
    printf("Recv a ack of %d\n", index);

    //����յ������кŴ��� curAck ������֮ǰ�ı��Ķ�ȫ��������
    if (curAck <= index)
    {
        for (int i = curAck; i <= index; ++i)
        {
            ack[i] = TRUE;
        }
        curAck = (index + 1) % SEQ_SIZE;
    }
    else
    {
        if (ack[index] == FALSE)
        {
            for (int i = curAck; i < SEQ_SIZE; ++i)
            {
                ack[i] = TRUE;
            }
            for (int i = 0; i <= index; ++i)
            {
                ack[i] = TRUE;
            }
            curAck = index + 1;
        }
    }
}

//������
int main(int argc, char *argv[])
{
    //�����׽��ֿ⣨���룩
    WORD wVersionRequested;
    WSADATA wsaData;
    //�׽��ּ���ʱ������ʾ
    int err;
    //�汾 2.2
    wVersionRequested = MAKEWORD(2, 2);
    //���� dll �ļ� Scoket ��
    err = WSAStartup(wVersionRequested, &wsaData);
    if (err != 0)
    {
        //�Ҳ��� winsock.dll
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
    //�����׽���Ϊ������ģʽ
    int iMode = 1;                                          // 1����������0������
    ioctlsocket(sockServer, FIONBIO, (u_long FAR *)&iMode); //����������
    SOCKADDR_IN addrServer;                                 //��������ַ
    // addrServer.sin_addr.S_un.S_addr = inet_addr(SERVER_IP);
    addrServer.sin_addr.S_un.S_addr = htonl(INADDR_ANY); //���߾���
    addrServer.sin_family = AF_INET;
    addrServer.sin_port = htons(SERVER_PORT);
    err = bind(sockServer, (SOCKADDR *)&addrServer, sizeof(SOCKADDR));
    if (err)
    {
        err = GetLastError();
        printf("Could not bind the port %d for socket.Error code is %d\n", SERVER_PORT, err);
        WSACleanup();
        return -1;
    }

    /*˫����Ķ�����*/
    float packetLossRatio = 0.2; //Ĭ�ϰ���ʧ�� 0.2
    float ackLossRatio = 0.2;    //Ĭ�� ACK ��ʧ�� 0.2
    //��ʱ����Ϊ������ӣ�����ѭ����������
    srand((unsigned)time(NULL));

    SOCKADDR_IN addrClient; //�ͻ��˵�ַ
    int length = sizeof(SOCKADDR);
    char buffer[BUFFER_LENGTH]; //���ݷ��ͽ��ջ�����
    ZeroMemory(buffer, sizeof(buffer));
    //���������ݶ����ڴ�
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

    //��ʼ��ack
    for (int i = 0; i < SEQ_SIZE; ++i)
    {
        ack[i] = TRUE;
    }
    while (true)
    {
        //���������գ���û���յ����ݣ�����ֵΪ-1
        recvSize = recvfrom(sockServer, buffer, BUFFER_LENGTH, 0, ((SOCKADDR *)&addrClient), &length);
        if (recvSize < 0)
        {
            Sleep(200);
            continue;
        }
        else if (strcmp(buffer, "Data Transfer Is Complete") == 0)
        {
            printf("Data Transfer Is Complete\n");
            continue;
        }
        printf("recv from client: %s\n", buffer);

        char cmd[128];
        sscanf(buffer, "%s%f%f", &cmd, &packetLossRatio, &ackLossRatio);

        if (strcmp(buffer, "-time") == 0)
        {
            getCurTime(buffer);
        }
        else if (strcmp(buffer, "-quit") == 0)
        {
            strcpy(buffer, "Good bye!");
        }
        else if (strcmp(buffer, "-testgbn") == 0)
        {
            //���� gbn ���Խ׶�
            //���� server��server ���� 0 ״̬���� client ���� 205 ״̬�루server ���� 1 ״̬��
            // server �ȴ� client �ظ� 200 ״̬�룬����յ���server ���� 2 ״̬���� ��ʼ�����ļ���������ʱ�ȴ�ֱ����ʱ
            //���ļ�����׶Σ�server ���ʹ��ڴ�С��Ϊ
            ZeroMemory(buffer, sizeof(buffer));
            int recvSize;
            int waitCount = 0;
            printf("Begain to test GBN protocol, please don't abort the process\n");
            //������һ�����ֽ׶�
            //���ȷ�������ͻ��˷���һ�� 205 ��С��״̬�루���Լ�����ģ� ��ʾ������׼�����ˣ����Է�������
            //�ͻ����յ� 205 ֮��ظ�һ�� 200 ��С��״̬�룬��ʾ�ͻ���׼ �����ˣ����Խ���������
            //�������յ� 200 ״̬��֮�󣬾Ϳ�ʼʹ�� GBN ����������
            printf("Shake hands stage\n");
            int stage = 0;
            bool runFlag = true;
            while (runFlag)
            {
                switch (stage)
                {
                case 0: //���� 205 �׶�
                    buffer[0] = 205;
                    buffer[1] = '\0';
                    sendto(sockServer, buffer, strlen(buffer) + 1, 0, (SOCKADDR *)&addrClient, sizeof(SOCKADDR));
                    Sleep(100);
                    stage = 1;
                    break;
                case 1: //�ȴ����� 200 �׶Σ�û���յ��������+1����ʱ������˴Ρ����ӡ����ȴ��ӵ�һ����ʼ
                    recvSize = recvfrom(sockServer, buffer, BUFFER_LENGTH, 0, ((SOCKADDR *)&addrClient), &length);
                    if (recvSize < 0)
                    {
                        waitCount++;
                        if (waitCount > 20)
                        {
                            runFlag = false;
                            printf("200 Timeout error\n");
                            break;
                        }
                        Sleep(500);
                        continue;
                    }
                    else
                    {
                        // waitCount = 0;
                        if ((unsigned char)buffer[0] == 200)
                        {
                            printf("Begin a file transfer\n");
                            printf("File size is %dB, each packet is 1024B and packet total num is %d...\n\n", sizeof(data), totalPacket);
                            curSeq = 0;
                            curAck = 0;
                            totalSeq = 0;
                            waitCount = 0;
                            stage = 2;
                        }
                    }
                    break;
                case 2: //���ݴ���׶�

                    /*Ϊ�ж����ݴ����Ƿ�������ӻ��޸ĵ����*/
                    if (seqIsAvailable() && totalSeq < totalPacket)
                    {
                        //���͸��ͻ��˵����кŴ� 1 ��ʼ
                        buffer[0] = curSeq + 1;
                        ack[curSeq] = FALSE;
                        //���ݷ��͵Ĺ�����Ӧ���ж��Ƿ������
                        //Ϊ�򻯹��̴˴���δʵ��
                        memcpy(&buffer[1], data + 1024 * totalSeq, 1024);
                        printf("send a packet with a seq of %d\n", curSeq);
                        sendto(sockServer, buffer, BUFFER_LENGTH, 0, (SOCKADDR *)&addrClient, sizeof(SOCKADDR));
                        ++curSeq;
                        curSeq %= SEQ_SIZE;
                        ++totalSeq;
                        Sleep(500);
                    }
                    // �ȴ� Ack����û���յ����򷵻�ֵΪ-1��������+1
                    // ������û���յ�ACK���������������
                    recvSize = recvfrom(sockServer, buffer, BUFFER_LENGTH, 0, ((SOCKADDR *)&addrClient), &length);
                    if (recvSize < 0)
                    {
                        waitCount++;
                        // 20 �εȴ� ack ��ʱ�ش�
                        if (waitCount > 20)
                        {
                            waitCount = 0;
                            timeoutHandler();
                            // printf("\t----totalSeq Now is : %d\n", totalSeq);
                        }
                    }
                    else
                    {
                        //�յ� ack
                        ackHandler(buffer[0]);
                        waitCount = 0;
                    }
                    Sleep(500);
                    break;
                }

                // �����꣬��֤�Ƿ���յ�ȫ��ACK
                if (totalSeq == totalPacket)
                {
                    BOOL isFinish = TRUE;
                    for (int i = 0; i < SEQ_SIZE; ++i)
                    {
                        if (ack[i] == FALSE)
                        {
                            isFinish = FALSE;
                        }
                    }
                    // �յ���ȫ��ACK����������źţ��˳����У�runFlag = false��
                    if (isFinish == TRUE)
                    {
                        // printf("Data Transfer Is Complete\n");
                        strcpy(buffer, "Data Transfer Is Complete");
                        sendto(sockServer, buffer, strlen(buffer) + 1, 0, (SOCKADDR *)&addrClient, sizeof(SOCKADDR));
                        // break;
                        totalSeq = 0;
                        runFlag = false;
                    }
                }
            }
        }
        /* ˫�����ݴ��� */
        else if (strcmp(cmd, "-testgbn2") == 0)
        {
            iMode = 0;                                              // 1����������0������
            ioctlsocket(sockServer, FIONBIO, (u_long FAR *)&iMode); // ��������
            printf("%s\n", "Begin to test GBN protocol, please don't abort the  process");
            printf("The loss ratio of packet is %.2f, the loss ratio of ack  is %.2f\n", packetLossRatio, ackLossRatio);
            int stage = 0;
            BOOL b;
            unsigned short seq;     //�������к�
            unsigned short recvSeq; //���մ��ڴ�СΪ 1����ȷ�ϵ����к�
            unsigned short waitSeq; //�ȴ������к�
            sendto(sockServer, "-testgbn2", strlen("-testgbn2") + 1, 0, (SOCKADDR *)&addrClient, sizeof(SOCKADDR));
            while (true)
            {
                //�ȴ� server �ظ����� UDP Ϊ����ģʽ
                recvfrom(sockServer, buffer, BUFFER_LENGTH, 0, (SOCKADDR *)&addrClient, &length);

                if (strcmp(buffer, "Data Transfer Is Complete") == 0)
                {
                    break;
                }

                switch (stage)
                {
                case 0: //�ȴ����ֽ׶�
                    if ((unsigned char)buffer[0] == 205)
                    {
                        printf("Ready for file transmission\n");
                        buffer[0] = 200;
                        buffer[1] = '\0';
                        sendto(sockServer, buffer, 2, 0, (SOCKADDR *)&addrClient, sizeof(SOCKADDR));
                        stage = 1;
                        recvSeq = 0;
                        waitSeq = 1;
                    }
                    break;
                case 1: //�ȴ��������ݽ׶�
                    seq = (unsigned short)buffer[0];
                    //�����ģ����Ƿ�ʧ
                    b = lossInLossRatio(packetLossRatio);
                    printf("\nThe packet wished: %d\n", waitSeq - 1);
                    if (b)
                    {
                        printf("The packet with a seq of %d loss\n", seq - 1);
                        continue;
                    }
                    printf("recv a packet with a seq of %d\n", seq - 1);
                    //������ڴ��İ�����ȷ���գ�����ȷ�ϼ���
                    if (!(waitSeq - seq))
                    {
                        ++waitSeq;
                        if (waitSeq == 21)
                        {
                            waitSeq = 1;
                        }
                        //�������
                        // printf("\n\n%s\n\n", &buffer[1]);
                        buffer[0] = seq;
                        recvSeq = seq;
                        buffer[1] = '\0';
                    }
                    else
                    {
                        //�����ǰһ������û���յ�����ȴ� Seq Ϊ 1 �����ݰ��������򲻷��� ACK����Ϊ��û����һ����ȷ�� ACK��
                        if (!recvSeq)
                        {
                            continue;
                        }
                        buffer[0] = recvSeq;
                        buffer[1] = '\0';
                    }
                    b = lossInLossRatio(ackLossRatio);
                    if (b)
                    {
                        printf("The ack of %d loss\n", (unsigned char)buffer[0] - 1);
                        continue;
                    }
                    sendto(sockServer, buffer, 2, 0, (SOCKADDR *)&addrClient, sizeof(SOCKADDR));
                    printf("send a ack of %d\n", (unsigned char)buffer[0] - 1);
                    break;
                }
                Sleep(500);
            }
            iMode = 1;                                              // 1����������0������
            ioctlsocket(sockServer, FIONBIO, (u_long FAR *)&iMode); //����������
        }

        sendto(sockServer, buffer, strlen(buffer) + 1, 0, (SOCKADDR *)&addrClient, sizeof(SOCKADDR));
        Sleep(500);
    }
    //�ر��׽��֣�ж�ؿ�
    closesocket(sockServer);
    WSACleanup();
    return 0;
}