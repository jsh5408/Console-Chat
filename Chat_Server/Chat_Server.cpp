#pragma comment(lib, "ws2_32")
#include <winsock2.h>
#include <stdlib.h>
#include <stdio.h>

#define SERVERPORT 9000
#define BUFSIZE    512

// 소켓 정보 저장을 위한 구조체와 변수
struct SOCKETINFO
{
	SOCKET sock;
	char buf[BUFSIZE+1];
	int recvbytes;
	int sendbytes;
	int roomnum;	// 방 번호 변수 추가
};

int nTotalSockets = 0;
SOCKETINFO *SocketInfoArray[FD_SETSIZE];

// 구현에 사용되는 변수
char namelist[10][10];	// 이름만 포함된 사용자 리스트 ex) user
char numlist[10][10];	// 방 번호 포함 사용자 리스트  ex) user(1): 1번 채팅방의 'user'
int cnt = 0;
// 대화명의 중복 여부 판단을 위해 socket이 우선적으로 생성됨
// 이 때, nTotalSockets는 1 증가
// 인원 계산의 복잡함을 방지하기 위해 같은 개념의 cnt 변수 생성
// cnt는 확실히 채팅방에 접속했을 때 1이 증가한다.
int flag = 0;
// flag = 0: 초기 상태 -> 사용자 리스트 저장용
// flag = 1: 중복 경고 or 사용자 리스트 출력용 (해당 사용자에게만 출력하도록)

// 소켓 관리 함수
BOOL AddSocketInfo(SOCKET sock);
void RemoveSocketInfo(int nIndex);

// 오류 출력 함수
void err_quit(char *msg);
void err_display(char *msg);

// 사용자 목록 변경
void changenamelist(SOCKETINFO *ptr, int i)
{
	for(int k=i;k<cnt;k++)
	{
		// 재정렬 후 맨 마지막 자리는 ""로 수정
		if(k == cnt-1)
		{
			strcpy(namelist[k], "");
			strcpy(numlist[k], "");
			break;
		}
		// 삭제된 자리부터 한칸씩 당김 (재정렬)
		strcpy(namelist[k], namelist[k+1]);
		strcpy(numlist[k], numlist[k+1]);
	}
}

int main(int argc, char *argv[])
{
	int retval;

	printf("** 채팅 프로그램 서버 **\n");

	// 윈속 초기화
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

	// 넌블로킹 소켓으로 전환
	u_long on = 1;
	retval = ioctlsocket(listen_sock, FIONBIO, &on);
	if(retval == SOCKET_ERROR) err_display("ioctlsocket()");

	// 데이터 통신에 사용할 변수
	FD_SET rset, wset;
	SOCKET client_sock;
	SOCKADDR_IN clientaddr;
	int addrlen, i, j;
	char buf[BUFSIZE+1];
	// 문자열 수정을 위해 사용할 변수
	char list1[120] = "\0";
	char list2[120] = "\0";
	char line[BUFSIZE+31];

	while(1){
		// 소켓 셋 초기화
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

		// 소켓 셋 검사(1): 클라이언트 접속 수용
		if(FD_ISSET(listen_sock, &rset)){
			addrlen = sizeof(clientaddr);
			client_sock = accept(listen_sock, (SOCKADDR *)&clientaddr, &addrlen);
			if(client_sock == INVALID_SOCKET){
				err_display("accept()");
			}
			else{
				printf("\n[TCP 서버] 클라이언트 접속: IP 주소=%s, 포트 번호=%d\n",
					inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));
				// 소켓 정보 추가
				AddSocketInfo(client_sock);
			}
		}

		// 소켓 셋 검사(2): 데이터 통신
		for(i=0; i<nTotalSockets; i++){
			SOCKETINFO *ptr = SocketInfoArray[i];
			if(FD_ISSET(ptr->sock, &rset)){
				// 데이터 받기
				retval = recv(ptr->sock, ptr->buf, BUFSIZE, 0);
				if(retval == SOCKET_ERROR){
					err_display("recv()");
					// 소켓 삭제될 때, 사용자 목록에서도 삭제 후 재정렬해주는 함수
					changenamelist(ptr, i);
					if(cnt == nTotalSockets)
					{
						// 중복 경고 메시지만 받은 상태에서 종료하는 경우는
						// 채팅방에 완전히 접속하지 않았으므로
						// nTotalSockets과 cnt의 값이 다름 -> cnt는 -1 되면 X
						// 소켓도 생성되고 채팅방에도 완전히 접속한 경우는 (nTotalSockets == cnt)
						// cnt도 -1을 해준다.
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
				// 받은 데이터 출력
				addrlen = sizeof(clientaddr);
				getpeername(ptr->sock, (SOCKADDR *)&clientaddr, &addrlen);
				ptr->buf[retval] = '\0';

				////////// 받은 데이터 분류 //////////
				// 이름 & 방 번호 받기 (대화내용은 '['로 시작 )
				if(ptr->buf[0] != '[' && strcmp(ptr->buf, "사용자 목록 보기") != 0)
				{
					// 방 번호 입력 시, SocketInfo의 roomnum을 채워줌
					if(strcmp(ptr->buf, "1") == 0 || strcmp(ptr->buf, "2") == 0)
					{
						ptr->roomnum = atoi(ptr->buf);
					}
					// 이름 입력
					else
					{
						// 중복 메시지 전달 (0을 보내주어 클라이언트에서 경고 메시지 출력하도록)
						for(int k=0;k<cnt;k++)
						{
							int len = strlen(numlist[k]);
							// numlist는 'user(1)'의 형태로 저장되므로
							// numlist[k][len-2]는 방 번호를 의미 => -48을 통해 숫자로 변환해서 비교에 사용
							int a = numlist[k][len-2] - 48;
							int b = ptr->roomnum;
							// 같은 채팅방(a == b)의 사용자들과 이름 비교(strcmp)해서 중복 판단
							if(a == b && strcmp(ptr->buf, namelist[k]) == 0)
							{
								flag = 1;
								strcpy(ptr->buf, "0");
								printf("중복\n");
								break;
							}
						}
						// 초기: namelist에 저장
						if(flag == 0)
						{
							strcpy(namelist[cnt], ptr->buf);
							// numlist는 방 번호를 포함해서 저장
							sprintf(line, "%s(%d)", ptr->buf, ptr->roomnum);
							strcpy(numlist[cnt++], line);
						}
					}
				}
				// 클라이언트가 사용자 목록을 요청했을 때
				else if(strcmp(ptr->buf, "사용자 목록 보기") == 0)
				{
					flag = 1;	// 해당 사용자에게만 출력
					if(cnt == 0)	// 채팅방에 접속한 사용자가 없을 경우
					{
						strcpy(ptr->buf, "사용자 없음");
						continue;
					}
					// 사용자 목록을 문자열로 저장
					strcat(list1, "[1번 룸]\r\n");
					strcat(list2, "[2번 룸]\r\n");
					for(int k=0;k<cnt;k++)
					{
						int len = strlen(numlist[k]);
						// numlist는 'user(1)'의 형태로 저장되므로
						// numlist[k][len-2]는 방 번호를 의미
						// 1번 채팅방은 list1에, 2번은 list2에 저장 후, line으로 합쳐서 모든 사용자 전부 출력
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
					sprintf(line, "** 사용자 목록 보기 **\r\n%s\r\n%s", list1, list2);
					strcpy(ptr->buf, line);
					printf("\n%s\n\n", line);
				}
				// 대화내용 받기
				else
				{
					printf("[TCP/%s:%d] %s\n", inet_ntoa(clientaddr.sin_addr),
						ntohs(clientaddr.sin_port), ptr->buf);
				}
			}
			if(FD_ISSET(ptr->sock, &wset)){
				// 해당 사용자에게만 보내기
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
				// 여러 접속자에게 데이터 보내기
				for (j=0; j<nTotalSockets; j++){
					SOCKETINFO *sptr = SocketInfoArray[j];
					// 같은 방의 사용자에게만 전송
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

	// 윈속 종료
	WSACleanup();
	return 0;
}

// 소켓 정보 추가
BOOL AddSocketInfo(SOCKET sock)
{
	if(nTotalSockets >= FD_SETSIZE){
		printf("[오류] 소켓 정보를 추가할 수 없습니다!\n");
		return FALSE;
	}

	SOCKETINFO *ptr = new SOCKETINFO;
	if(ptr == NULL){
		printf("[오류] 메모리가 부족합니다!\n");
		return FALSE;
	}

	ptr->sock = sock;
	ptr->recvbytes = 0;
	ptr->sendbytes = 0;
	ptr->roomnum = 1;
	SocketInfoArray[nTotalSockets++] = ptr;

	return TRUE;
}

// 소켓 정보 삭제
void RemoveSocketInfo(int nIndex)
{
	SOCKETINFO *ptr = SocketInfoArray[nIndex];
	flag = 0;	// 초기화

	// 클라이언트 정보 얻기
	SOCKADDR_IN clientaddr;
	int addrlen = sizeof(clientaddr);
	getpeername(ptr->sock, (SOCKADDR *)&clientaddr, &addrlen);
	printf("[TCP 서버] 클라이언트 종료: IP 주소=%s, 포트 번호=%d\n", 
		inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));

	closesocket(ptr->sock);
	delete ptr;

	if(nIndex != (nTotalSockets-1))
		SocketInfoArray[nIndex] = SocketInfoArray[nTotalSockets-1];

	--nTotalSockets;
}

// 소켓 함수 오류 출력 후 종료
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

// 소켓 함수 오류 출력
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