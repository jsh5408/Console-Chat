#pragma comment(lib, "ws2_32")
#include <winsock2.h>
#include <stdlib.h>
#include <stdio.h>

#define SERVERPORT 9000
#define BUFSIZE    512

// ���� ���� ������ ���� ����ü�� ����
struct SOCKETINFO
{
	SOCKET sock;
	char buf[BUFSIZE+1];
	int recvbytes;
	int sendbytes;
	int roomnum;	// �� ��ȣ ���� �߰�
};

int nTotalSockets = 0;
SOCKETINFO *SocketInfoArray[FD_SETSIZE];

// ������ ���Ǵ� ����
char namelist[10][10];	// �̸��� ���Ե� ����� ����Ʈ ex) user
char numlist[10][10];	// �� ��ȣ ���� ����� ����Ʈ  ex) user(1): 1�� ä�ù��� 'user'
int cnt = 0;
// ��ȭ���� �ߺ� ���� �Ǵ��� ���� socket�� �켱������ ������
// �� ��, nTotalSockets�� 1 ����
// �ο� ����� �������� �����ϱ� ���� ���� ������ cnt ���� ����
// cnt�� Ȯ���� ä�ù濡 �������� �� 1�� �����Ѵ�.
int flag = 0;
// flag = 0: �ʱ� ���� -> ����� ����Ʈ �����
// flag = 1: �ߺ� ��� or ����� ����Ʈ ��¿� (�ش� ����ڿ��Ը� ����ϵ���)

// ���� ���� �Լ�
BOOL AddSocketInfo(SOCKET sock);
void RemoveSocketInfo(int nIndex);

// ���� ��� �Լ�
void err_quit(char *msg);
void err_display(char *msg);

// ����� ��� ����
void changenamelist(SOCKETINFO *ptr, int i)
{
	for(int k=i;k<cnt;k++)
	{
		// ������ �� �� ������ �ڸ��� ""�� ����
		if(k == cnt-1)
		{
			strcpy(namelist[k], "");
			strcpy(numlist[k], "");
			break;
		}
		// ������ �ڸ����� ��ĭ�� ��� (������)
		strcpy(namelist[k], namelist[k+1]);
		strcpy(numlist[k], numlist[k+1]);
	}
}

int main(int argc, char *argv[])
{
	int retval;

	printf("** ä�� ���α׷� ���� **\n");

	// ���� �ʱ�ȭ
	WSADATA wsa;
	if(WSAStartup(MAKEWORD(2,2), &wsa) != 0)
		return 1;

	// socket()
	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if(listen_sock == INVALID_SOCKET) err_quit("socket()");

	// bind()
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = bind(listen_sock, (SOCKADDR *)&serveraddr, sizeof(serveraddr));
	if(retval == SOCKET_ERROR) err_quit("bind()");

	// listen()
	retval = listen(listen_sock, SOMAXCONN);
	if(retval == SOCKET_ERROR) err_quit("listen()");

	// �ͺ��ŷ �������� ��ȯ
	u_long on = 1;
	retval = ioctlsocket(listen_sock, FIONBIO, &on);
	if(retval == SOCKET_ERROR) err_display("ioctlsocket()");

	// ������ ��ſ� ����� ����
	FD_SET rset, wset;
	SOCKET client_sock;
	SOCKADDR_IN clientaddr;
	int addrlen, i, j;
	char buf[BUFSIZE+1];
	// ���ڿ� ������ ���� ����� ����
	char list1[120] = "\0";
	char list2[120] = "\0";
	char line[BUFSIZE+31];

	while(1){
		// ���� �� �ʱ�ȭ
		FD_ZERO(&rset);
		FD_ZERO(&wset);
		FD_SET(listen_sock, &rset);
		for(i=0; i<nTotalSockets; i++){
			if(SocketInfoArray[i]->recvbytes > SocketInfoArray[i]->sendbytes)
				FD_SET(SocketInfoArray[i]->sock, &wset);
			else
				FD_SET(SocketInfoArray[i]->sock, &rset);
		}

		// select()
		retval = select(0, &rset, &wset, NULL, NULL);
		if(retval == SOCKET_ERROR) err_quit("select()");

		// ���� �� �˻�(1): Ŭ���̾�Ʈ ���� ����
		if(FD_ISSET(listen_sock, &rset)){
			addrlen = sizeof(clientaddr);
			client_sock = accept(listen_sock, (SOCKADDR *)&clientaddr, &addrlen);
			if(client_sock == INVALID_SOCKET){
				err_display("accept()");
			}
			else{
				printf("\n[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d\n",
					inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));
				// ���� ���� �߰�
				AddSocketInfo(client_sock);
			}
		}

		// ���� �� �˻�(2): ������ ���
		for(i=0; i<nTotalSockets; i++){
			SOCKETINFO *ptr = SocketInfoArray[i];
			if(FD_ISSET(ptr->sock, &rset)){
				// ������ �ޱ�
				retval = recv(ptr->sock, ptr->buf, BUFSIZE, 0);
				if(retval == SOCKET_ERROR){
					err_display("recv()");
					// ���� ������ ��, ����� ��Ͽ����� ���� �� ���������ִ� �Լ�
					changenamelist(ptr, i);
					if(cnt == nTotalSockets)
					{
						// �ߺ� ��� �޽����� ���� ���¿��� �����ϴ� ����
						// ä�ù濡 ������ �������� �ʾ����Ƿ�
						// nTotalSockets�� cnt�� ���� �ٸ� -> cnt�� -1 �Ǹ� X
						// ���ϵ� �����ǰ� ä�ù濡�� ������ ������ ���� (nTotalSockets == cnt)
						// cnt�� -1�� ���ش�.
						cnt--;
					}
					RemoveSocketInfo(i);
					continue;
				}
				else if(retval == 0){
					RemoveSocketInfo(i);
					continue;
				}
				ptr->recvbytes = retval;
				// ���� ������ ���
				addrlen = sizeof(clientaddr);
				getpeername(ptr->sock, (SOCKADDR *)&clientaddr, &addrlen);
				ptr->buf[retval] = '\0';

				////////// ���� ������ �з� //////////
				// �̸� & �� ��ȣ �ޱ� (��ȭ������ '['�� ���� )
				if(ptr->buf[0] != '[' && strcmp(ptr->buf, "����� ��� ����") != 0)
				{
					// �� ��ȣ �Է� ��, SocketInfo�� roomnum�� ä����
					if(strcmp(ptr->buf, "1") == 0 || strcmp(ptr->buf, "2") == 0)
					{
						ptr->roomnum = atoi(ptr->buf);
					}
					// �̸� �Է�
					else
					{
						// �ߺ� �޽��� ���� (0�� �����־� Ŭ���̾�Ʈ���� ��� �޽��� ����ϵ���)
						for(int k=0;k<cnt;k++)
						{
							int len = strlen(numlist[k]);
							// numlist�� 'user(1)'�� ���·� ����ǹǷ�
							// numlist[k][len-2]�� �� ��ȣ�� �ǹ� => -48�� ���� ���ڷ� ��ȯ�ؼ� �񱳿� ���
							int a = numlist[k][len-2] - 48;
							int b = ptr->roomnum;
							// ���� ä�ù�(a == b)�� ����ڵ�� �̸� ��(strcmp)�ؼ� �ߺ� �Ǵ�
							if(a == b && strcmp(ptr->buf, namelist[k]) == 0)
							{
								flag = 1;
								strcpy(ptr->buf, "0");
								printf("�ߺ�\n");
								break;
							}
						}
						// �ʱ�: namelist�� ����
						if(flag == 0)
						{
							strcpy(namelist[cnt], ptr->buf);
							// numlist�� �� ��ȣ�� �����ؼ� ����
							sprintf(line, "%s(%d)", ptr->buf, ptr->roomnum);
							strcpy(numlist[cnt++], line);
						}
					}
				}
				// Ŭ���̾�Ʈ�� ����� ����� ��û���� ��
				else if(strcmp(ptr->buf, "����� ��� ����") == 0)
				{
					flag = 1;	// �ش� ����ڿ��Ը� ���
					if(cnt == 0)	// ä�ù濡 ������ ����ڰ� ���� ���
					{
						strcpy(ptr->buf, "����� ����");
						continue;
					}
					// ����� ����� ���ڿ��� ����
					strcat(list1, "[1�� ��]\r\n");
					strcat(list2, "[2�� ��]\r\n");
					for(int k=0;k<cnt;k++)
					{
						int len = strlen(numlist[k]);
						// numlist�� 'user(1)'�� ���·� ����ǹǷ�
						// numlist[k][len-2]�� �� ��ȣ�� �ǹ�
						// 1�� ä�ù��� list1��, 2���� list2�� ���� ��, line���� ���ļ� ��� ����� ���� ���
						if(numlist[k][len-2] == '1')
						{
							strcat(list1, namelist[k]);
							strcat(list1, "\r\n");
						}
						else
						{
							strcat(list2, namelist[k]);
							strcat(list2, "\r\n");
						}
					}
					sprintf(line, "** ����� ��� ���� **\r\n%s\r\n%s", list1, list2);
					strcpy(ptr->buf, line);
					printf("\n%s\n\n", line);
				}
				// ��ȭ���� �ޱ�
				else
				{
					printf("[TCP/%s:%d] %s\n", inet_ntoa(clientaddr.sin_addr),
						ntohs(clientaddr.sin_port), ptr->buf);
				}
			}
			if(FD_ISSET(ptr->sock, &wset)){
				// �ش� ����ڿ��Ը� ������
				if(flag == 1)
				{
					retval = send(ptr->sock, ptr->buf, strlen(ptr->buf), 0);
					if(retval == SOCKET_ERROR){
						err_display("send()");
						RemoveSocketInfo(i);
						continue;
					}
					flag = 0;
					ptr->recvbytes = ptr->sendbytes = 0;
					break;
				}
				// ���� �����ڿ��� ������ ������
				for (j=0; j<nTotalSockets; j++){
					SOCKETINFO *sptr = SocketInfoArray[j];
					// ���� ���� ����ڿ��Ը� ����
					if(sptr->roomnum == ptr->roomnum)
					{
						retval = send(sptr->sock, ptr->buf + ptr->sendbytes, 
							ptr->recvbytes - ptr->sendbytes, 0);
						if(retval == SOCKET_ERROR){
							err_display("send()");
							RemoveSocketInfo(i);
							continue;
						}
					}
				}
				ptr->sendbytes += retval;
				if(ptr->recvbytes == ptr->sendbytes){
					ptr->recvbytes = ptr->sendbytes = 0;
				}
			}
		}
	}

	// ���� ����
	WSACleanup();
	return 0;
}

// ���� ���� �߰�
BOOL AddSocketInfo(SOCKET sock)
{
	if(nTotalSockets >= FD_SETSIZE){
		printf("[����] ���� ������ �߰��� �� �����ϴ�!\n");
		return FALSE;
	}

	SOCKETINFO *ptr = new SOCKETINFO;
	if(ptr == NULL){
		printf("[����] �޸𸮰� �����մϴ�!\n");
		return FALSE;
	}

	ptr->sock = sock;
	ptr->recvbytes = 0;
	ptr->sendbytes = 0;
	ptr->roomnum = 1;
	SocketInfoArray[nTotalSockets++] = ptr;

	return TRUE;
}

// ���� ���� ����
void RemoveSocketInfo(int nIndex)
{
	SOCKETINFO *ptr = SocketInfoArray[nIndex];
	flag = 0;	// �ʱ�ȭ

	// Ŭ���̾�Ʈ ���� ���
	SOCKADDR_IN clientaddr;
	int addrlen = sizeof(clientaddr);
	getpeername(ptr->sock, (SOCKADDR *)&clientaddr, &addrlen);
	printf("[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d\n", 
		inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));

	closesocket(ptr->sock);
	delete ptr;

	if(nIndex != (nTotalSockets-1))
		SocketInfoArray[nIndex] = SocketInfoArray[nTotalSockets-1];

	--nTotalSockets;
}

// ���� �Լ� ���� ��� �� ����
void err_quit(char *msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	MessageBox(NULL, (LPCTSTR)lpMsgBuf, msg, MB_ICONERROR);
	LocalFree(lpMsgBuf);
	exit(1);
}

// ���� �Լ� ���� ���
void err_display(char *msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	printf("[%s] %s", msg, (char *)lpMsgBuf);
	LocalFree(lpMsgBuf);
}