#pragma comment(lib, "ws2_32")
#include <winsock2.h>
#include <stdlib.h>
#include <stdio.h>
#include "resource.h"

//#define SERVERIP   "127.0.0.1"
//#define SERVERPORT 9000
#define BUFSIZE    512

char serverip[16];	// SERVERIP를 변수로
int serverport;		// SERVERPORT를 변수로
char userID[10];	// 유저ID

int room;			// 채팅방 번호
int flag = 0;
// flag == 0: 초기 상태
// flag == 1: 중복 상태 의미
// flag == 2: 대화내용 전송 

// 대화상자 프로시저
BOOL CALLBACK DlgProc(HWND, UINT, WPARAM, LPARAM);
// 편집 컨트롤 출력 함수
void DisplayText(char *fmt, ...);
// 오류 출력 함수
void err_quit(char *msg);
void err_display(char *msg);
// 사용자 정의 데이터 수신 함수
int recvn(SOCKET s, char *buf, int len, int flags);
// 소켓 통신 스레드 함수
DWORD WINAPI ClientMain(LPVOID arg);
// 사용자 입력 부분
DWORD WINAPI ProcessInputSend(LPVOID arg);
// 사용자 목록 출력 함수 (버튼 클릭 시, 실행)
int UserListDisplay();

SOCKET sock; // 소켓
char buf[BUFSIZE+31]; // 데이터 송수신 버퍼
HANDLE hReadEvent, hWriteEvent; // 이벤트
HWND hSendButton; // 보내기 버튼
HWND hEdit1, hEdit2; // 편집 컨트롤

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
	// 이벤트 생성
	hReadEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
	if(hReadEvent == NULL) return 1;
	hWriteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if(hWriteEvent == NULL) return 1;

	// 소켓 통신 스레드 생성
	CreateThread(NULL, 0, ClientMain, NULL, 0, NULL);

	// 대화상자 생성
	DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, DlgProc);

	// 이벤트 제거
	CloseHandle(hReadEvent);
	CloseHandle(hWriteEvent);

	// closesocket()
	closesocket(sock);

	// 윈속 종료
	WSACleanup();
	return 0;
}

// 대화상자 시작 시, 출력하기 위해 밑에 함수에서 SetDlgItemText로 사용
char str[100] = "[IP 주소를 입력하세요] 127.0.0.1\r\n";

// 대화상자 프로시저
BOOL CALLBACK DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg){
	case WM_INITDIALOG:
		hEdit1 = GetDlgItem(hDlg, IDC_EDIT1);
		hEdit2 = GetDlgItem(hDlg, IDC_EDIT2);
		hSendButton = GetDlgItem(hDlg, IDOK);
		SendMessage(hEdit1, EM_SETLIMITTEXT, BUFSIZE, 0);
		SetDlgItemText(hDlg, IDC_EDIT2, str);	// IP주소 입력 안내 메시지 출력
		return TRUE;
	case WM_COMMAND:
		switch(LOWORD(wParam)){
		case IDOK:
			EnableWindow(hSendButton, FALSE); // 보내기 버튼 비활성화
			WaitForSingleObject(hReadEvent, INFINITE); // 읽기 완료 기다리기
			GetDlgItemText(hDlg, IDC_EDIT1, buf, BUFSIZE+1);
			SetEvent(hWriteEvent); // 쓰기 완료 알리기
			SetFocus(hEdit1);
			SendMessage(hEdit1, EM_SETSEL, 0, -1);
			return TRUE;
		case IDCANCEL:
			EndDialog(hDlg, IDCANCEL);
			return TRUE;
		case IDC_BUTTON1:	// 사용자 목록 출력
			UserListDisplay();
			return TRUE;
		}
		return FALSE;
	}
	return FALSE;
}

// 편집 컨트롤 출력 함수
void DisplayText(char *fmt, ...)
{
	va_list arg;
	va_start(arg, fmt);

	char cbuf[BUFSIZE+256];
	vsprintf(cbuf, fmt, arg);

	int nLength = GetWindowTextLength(hEdit2);
	SendMessage(hEdit2, EM_SETSEL, nLength, nLength);
	SendMessage(hEdit2, EM_REPLACESEL, FALSE, (LPARAM)cbuf);

	va_end(arg);
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
	DisplayText("[%s] %s", msg, (char *)lpMsgBuf);
	LocalFree(lpMsgBuf);
}

// 사용자 정의 데이터 수신 함수
int recvn(SOCKET s, char *buf, int len, int flags)
{
	int received;
	char *ptr = buf;
	int left = len;

	while(left > 0){
		received = recv(s, ptr, left, flags);
		if(received == SOCKET_ERROR)
			return SOCKET_ERROR;
		else if(received == 0)
			break;
		left -= received;
		ptr += received;
	}

	return (len - left);
}

// TCP 클라이언트 시작 부분
DWORD WINAPI ClientMain(LPVOID arg)
{
	// IP주소, 포트 번호 범위 체크에 사용하는 변수
	char addr[20];
	char port[10];
	int addrnum;
	int portnum;
	int err = 0;

	// IP주소가 올바른지 판단
	while(1) {
		WaitForSingleObject(hWriteEvent, INFINITE); // 쓰기 완료 기다리기

		// 클래스의 제한이 없으므로 길이로 판단
		if(strlen(buf) > 8 && strlen(buf) < 17) {
			// serverip 변수에 IP주소 저장
			strcpy(serverip, buf);
			EnableWindow(hSendButton, TRUE); // 보내기 버튼 활성화
			SetEvent(hReadEvent); // 읽기 완료 알리기
			break;
		}
		else {
			DisplayText("잘못된 주소입니다. 다시 입력해주세요.\r\n");
			EnableWindow(hSendButton, TRUE); // 보내기 버튼 활성화
			SetEvent(hReadEvent); // 읽기 완료 알리기
		}
	}

	// 포트 번호가 올바른지 판단
	while(1) {
		DisplayText("[포트 번호를 입력하세요] 9000\r\n");
		WaitForSingleObject(hWriteEvent, INFINITE); // 쓰기 완료 기다리기
		portnum = atoi(buf);

		// 한글이나 영문자, special character 오류 설정
		for(int i=0;i<strlen(buf);i++) {
			// 문자가 섞여있는지 확인 -> 숫자 범위를 벗어나면 오류
			if(buf[i] < 48 || buf[i] > 57) {
				DisplayText("잘못된 포트 번호입니다. 다시 입력해주세요.\r\n");
				err = 1;	// 반복문 안의 반복문이므로 err 변수 활용
				break;
			}
		}

		if(err == 1) {
			err = 0;
			EnableWindow(hSendButton, TRUE); // 보내기 버튼 활성화
			SetEvent(hReadEvent); // 읽기 완료 알리기
			continue;
		}

		// 범위초과 오류 설정
		if(portnum > 65535) {
			DisplayText("잘못된 포트 번호입니다. 다시 입력해주세요.\r\n");
			EnableWindow(hSendButton, TRUE); // 보내기 버튼 활성화
			SetEvent(hReadEvent); // 읽기 완료 알리기
			continue;
		}
		// server 포트 번호는 9000 기준이므로 9000이 아닐 경우 재입력 요청
		else if(portnum != 9000)
		{
			DisplayText("입장할 수 있는 포트 번호는 9000 입니다. 다시 입력해주세요.\r\n");
			EnableWindow(hSendButton, TRUE); // 보내기 버튼 활성화
			SetEvent(hReadEvent); // 읽기 완료 알리기
			continue;
		}

		// serverport 변수에 포트번호 저장
		serverport = portnum;
		EnableWindow(hSendButton, TRUE); // 보내기 버튼 활성화
		SetEvent(hReadEvent); // 읽기 완료 알리기
		break;
	}

	// 채팅방 번호 입력
	while(1) {
		DisplayText("[방 번호를 입력하세요] 1 or 2\r\n");
		WaitForSingleObject(hWriteEvent, INFINITE); // 쓰기 완료 기다리기
		int roomnum;
		roomnum = atoi(buf);
		if(roomnum == 1 || roomnum == 2) {	// 두가지의 번호 입력 시에만 방 번호 저장 후 시작
			room = roomnum;
			EnableWindow(hSendButton, TRUE); // 보내기 버튼 활성화
			SetEvent(hReadEvent); // 읽기 완료 알리기
			break;
		}
		else {
			DisplayText("없는 방 번호입니다. 다시 입력해주세요.\r\n");
			EnableWindow(hSendButton, TRUE); // 보내기 버튼 활성화
			SetEvent(hReadEvent); // 읽기 완료 알리기
		}
	}

	int retval;

	// 윈속 초기화
	WSADATA wsa;
	if(WSAStartup(MAKEWORD(2,2), &wsa) != 0)
		return 1;

	// 스레드 생성
	HANDLE hThread;

	// socket()
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock == INVALID_SOCKET) err_quit("socket()");

	// connect()
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = inet_addr(serverip);
	serveraddr.sin_port = htons(serverport);
	retval = connect(sock, (SOCKADDR *)&serveraddr, sizeof(serveraddr));
	if(retval == SOCKET_ERROR) err_quit("connect()");

	// 방 번호 보내기
	retval = send(sock, buf, strlen(buf), 0);
	if(retval == SOCKET_ERROR){
		err_display("send()");
		return 0;
	}

	// 데이터 받기
	retval = recv(sock, buf,  BUFSIZE+1, 0);
	if(retval == SOCKET_ERROR){
		err_display("recv()");
		return 0;
	}
	else if(retval == 0)
		return 0;
	buf[retval] = '\0';

	// 대화명 입력 받기
	DisplayText("[대화명을 입력해주세요] 예시: user\r\n");
	WaitForSingleObject(hWriteEvent, INFINITE); // 쓰기 완료 기다리기
	strcpy(userID, buf);	// userID 변수에 대화명 저장

	// '\n' 문자 제거
	int len = strlen(buf);
	if(buf[len-1] == '\n')
		buf[len-1] = '\0';

	// 문자열 길이가 0이면 보내지 않음
	if(strlen(buf) != 0){
		// 데이터 보내기
		retval = send(sock, buf, strlen(buf), 0);
		if(retval == SOCKET_ERROR){
			err_display("send()");
			return 0;
		}
	}

	EnableWindow(hSendButton, TRUE); // 보내기 버튼 활성화
	SetEvent(hReadEvent); // 읽기 완료 알리기

	// 스레드 생성
	hThread = CreateThread(NULL, 0, ProcessInputSend, NULL, 0, NULL);
	if (hThread == NULL) {
		printf("fail make thread\n");
	}
	else {
		CloseHandle(hThread);
	}
	
	// 서버와 데이터 통신
	while(1){
		// 데이터 받기
		retval = recv(sock, buf,  BUFSIZE+1, 0);
		if(retval == SOCKET_ERROR){
			err_display("recv()");
			break;
		}
		else if(retval == 0)
			break;
		buf[retval] = '\0';

		// 중복된 대화명일 경우 (중복일 경우, 서버에서 0을 보내줌)
		if(strcmp(buf, "0") == 0)
		{
			DisplayText("\r\n동일한 대화명이 있습니다. 변경해주세요.\r\n", userID);
			DisplayText("[대화명을 입력해주세요] 예시: user\r\n\r\n");
			flag = 1;
		}
		else
		{
			// 대화내용 받기
			if(buf[0] == '[')
			{
				// 받은 데이터 출력
				buf[retval] = '\0';
				DisplayText("[받은 데이터] %s\r\n", buf);
			}
			// 사용자 목록 받기
			else if(buf[0] == '*')
			{
				// 받은 데이터 출력
				buf[retval] = '\0';
				DisplayText("\r\n%s\r\n", buf);
			}
			// 채팅방 번호 받기
			else if(strcmp(buf, "1") == 0 || strcmp(buf, "2") == 0)
			{
				continue;
			}
			// 이름 받기 -> 채팅 접속 안내
			else
			{
				flag = 2;
				buf[retval] = '\0';
				DisplayText("\r\n%s님이 대화방에 참여했습니다.\r\n\r\n", buf);
			}
		}
	}
	
	return 0;
}

/* 사용자 입력 */
DWORD WINAPI ProcessInputSend(LPVOID arg)
{
	// 데이터 입력
	int retval;
	char line[BUFSIZE+11];
	int len;
	while(1) {
		WaitForSingleObject(hWriteEvent, INFINITE); // 쓰기 완료 기다리기

		// '\n' 문자 제거
		len = strlen(buf);
		if(buf[len-1] == '\n')
			buf[len-1] = '\0';

		// 대화내용 보내기 -> userID를 포함해서 전송
		if(flag == 2)
		{
			sprintf(line, "[%s] : %s", userID, buf);
		}
		// 중복일 경우, 이름만 전송
		else if(flag == 1)
		{
			sprintf(line, "%s", buf);
		}
		
		// 문자열 길이가 0이면 보내지 않음
		if(strlen(buf) == 0){
			EnableWindow(hSendButton, TRUE); // 보내기 버튼 활성화
			SetEvent(hReadEvent); // 읽기 완료 알리기
			continue;
		}

		// 데이터 보내기
		retval = send(sock, line, strlen(line), 0);
		if(retval == SOCKET_ERROR){
			err_display("send()");
			return 0;
		}

		EnableWindow(hSendButton, TRUE); // 보내기 버튼 활성화
		SetEvent(hReadEvent); // 읽기 완료 알리기
	}
}

// 서버에 사용자 목록을 요청하는 함수
int UserListDisplay()
{
	char call[25] = "사용자 목록 보기";
	int retval;

	// 데이터 보내기
	retval = send(sock, call, strlen(call), 0);
	if(retval == SOCKET_ERROR){
		err_display("send()");
		return 0;
	}

	EnableWindow(hSendButton, TRUE); // 보내기 버튼 활성화
	SetEvent(hReadEvent); // 읽기 완료 알리기

	return 0;
}