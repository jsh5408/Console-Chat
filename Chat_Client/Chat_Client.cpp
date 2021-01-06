#pragma comment(lib, "ws2_32")
#include <winsock2.h>
#include <stdlib.h>
#include <stdio.h>
#include "resource.h"

//#define SERVERIP   "127.0.0.1"
//#define SERVERPORT 9000
#define BUFSIZE    512

char serverip[16];	// SERVERIP�� ������
int serverport;		// SERVERPORT�� ������
char userID[10];	// ����ID

int room;			// ä�ù� ��ȣ
int flag = 0;
// flag == 0: �ʱ� ����
// flag == 1: �ߺ� ���� �ǹ�
// flag == 2: ��ȭ���� ���� 

// ��ȭ���� ���ν���
BOOL CALLBACK DlgProc(HWND, UINT, WPARAM, LPARAM);
// ���� ��Ʈ�� ��� �Լ�
void DisplayText(char *fmt, ...);
// ���� ��� �Լ�
void err_quit(char *msg);
void err_display(char *msg);
// ����� ���� ������ ���� �Լ�
int recvn(SOCKET s, char *buf, int len, int flags);
// ���� ��� ������ �Լ�
DWORD WINAPI ClientMain(LPVOID arg);
// ����� �Է� �κ�
DWORD WINAPI ProcessInputSend(LPVOID arg);
// ����� ��� ��� �Լ� (��ư Ŭ�� ��, ����)
int UserListDisplay();

SOCKET sock; // ����
char buf[BUFSIZE+31]; // ������ �ۼ��� ����
HANDLE hReadEvent, hWriteEvent; // �̺�Ʈ
HWND hSendButton; // ������ ��ư
HWND hEdit1, hEdit2; // ���� ��Ʈ��

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
	// �̺�Ʈ ����
	hReadEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
	if(hReadEvent == NULL) return 1;
	hWriteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if(hWriteEvent == NULL) return 1;

	// ���� ��� ������ ����
	CreateThread(NULL, 0, ClientMain, NULL, 0, NULL);

	// ��ȭ���� ����
	DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, DlgProc);

	// �̺�Ʈ ����
	CloseHandle(hReadEvent);
	CloseHandle(hWriteEvent);

	// closesocket()
	closesocket(sock);

	// ���� ����
	WSACleanup();
	return 0;
}

// ��ȭ���� ���� ��, ����ϱ� ���� �ؿ� �Լ����� SetDlgItemText�� ���
char str[100] = "[IP �ּҸ� �Է��ϼ���] 127.0.0.1\r\n";

// ��ȭ���� ���ν���
BOOL CALLBACK DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg){
	case WM_INITDIALOG:
		hEdit1 = GetDlgItem(hDlg, IDC_EDIT1);
		hEdit2 = GetDlgItem(hDlg, IDC_EDIT2);
		hSendButton = GetDlgItem(hDlg, IDOK);
		SendMessage(hEdit1, EM_SETLIMITTEXT, BUFSIZE, 0);
		SetDlgItemText(hDlg, IDC_EDIT2, str);	// IP�ּ� �Է� �ȳ� �޽��� ���
		return TRUE;
	case WM_COMMAND:
		switch(LOWORD(wParam)){
		case IDOK:
			EnableWindow(hSendButton, FALSE); // ������ ��ư ��Ȱ��ȭ
			WaitForSingleObject(hReadEvent, INFINITE); // �б� �Ϸ� ��ٸ���
			GetDlgItemText(hDlg, IDC_EDIT1, buf, BUFSIZE+1);
			SetEvent(hWriteEvent); // ���� �Ϸ� �˸���
			SetFocus(hEdit1);
			SendMessage(hEdit1, EM_SETSEL, 0, -1);
			return TRUE;
		case IDCANCEL:
			EndDialog(hDlg, IDCANCEL);
			return TRUE;
		case IDC_BUTTON1:	// ����� ��� ���
			UserListDisplay();
			return TRUE;
		}
		return FALSE;
	}
	return FALSE;
}

// ���� ��Ʈ�� ��� �Լ�
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
	DisplayText("[%s] %s", msg, (char *)lpMsgBuf);
	LocalFree(lpMsgBuf);
}

// ����� ���� ������ ���� �Լ�
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

// TCP Ŭ���̾�Ʈ ���� �κ�
DWORD WINAPI ClientMain(LPVOID arg)
{
	// IP�ּ�, ��Ʈ ��ȣ ���� üũ�� ����ϴ� ����
	char addr[20];
	char port[10];
	int addrnum;
	int portnum;
	int err = 0;

	// IP�ּҰ� �ùٸ��� �Ǵ�
	while(1) {
		WaitForSingleObject(hWriteEvent, INFINITE); // ���� �Ϸ� ��ٸ���

		// Ŭ������ ������ �����Ƿ� ���̷� �Ǵ�
		if(strlen(buf) > 8 && strlen(buf) < 17) {
			// serverip ������ IP�ּ� ����
			strcpy(serverip, buf);
			EnableWindow(hSendButton, TRUE); // ������ ��ư Ȱ��ȭ
			SetEvent(hReadEvent); // �б� �Ϸ� �˸���
			break;
		}
		else {
			DisplayText("�߸��� �ּ��Դϴ�. �ٽ� �Է����ּ���.\r\n");
			EnableWindow(hSendButton, TRUE); // ������ ��ư Ȱ��ȭ
			SetEvent(hReadEvent); // �б� �Ϸ� �˸���
		}
	}

	// ��Ʈ ��ȣ�� �ùٸ��� �Ǵ�
	while(1) {
		DisplayText("[��Ʈ ��ȣ�� �Է��ϼ���] 9000\r\n");
		WaitForSingleObject(hWriteEvent, INFINITE); // ���� �Ϸ� ��ٸ���
		portnum = atoi(buf);

		// �ѱ��̳� ������, special character ���� ����
		for(int i=0;i<strlen(buf);i++) {
			// ���ڰ� �����ִ��� Ȯ�� -> ���� ������ ����� ����
			if(buf[i] < 48 || buf[i] > 57) {
				DisplayText("�߸��� ��Ʈ ��ȣ�Դϴ�. �ٽ� �Է����ּ���.\r\n");
				err = 1;	// �ݺ��� ���� �ݺ����̹Ƿ� err ���� Ȱ��
				break;
			}
		}

		if(err == 1) {
			err = 0;
			EnableWindow(hSendButton, TRUE); // ������ ��ư Ȱ��ȭ
			SetEvent(hReadEvent); // �б� �Ϸ� �˸���
			continue;
		}

		// �����ʰ� ���� ����
		if(portnum > 65535) {
			DisplayText("�߸��� ��Ʈ ��ȣ�Դϴ�. �ٽ� �Է����ּ���.\r\n");
			EnableWindow(hSendButton, TRUE); // ������ ��ư Ȱ��ȭ
			SetEvent(hReadEvent); // �б� �Ϸ� �˸���
			continue;
		}
		// server ��Ʈ ��ȣ�� 9000 �����̹Ƿ� 9000�� �ƴ� ��� ���Է� ��û
		else if(portnum != 9000)
		{
			DisplayText("������ �� �ִ� ��Ʈ ��ȣ�� 9000 �Դϴ�. �ٽ� �Է����ּ���.\r\n");
			EnableWindow(hSendButton, TRUE); // ������ ��ư Ȱ��ȭ
			SetEvent(hReadEvent); // �б� �Ϸ� �˸���
			continue;
		}

		// serverport ������ ��Ʈ��ȣ ����
		serverport = portnum;
		EnableWindow(hSendButton, TRUE); // ������ ��ư Ȱ��ȭ
		SetEvent(hReadEvent); // �б� �Ϸ� �˸���
		break;
	}

	// ä�ù� ��ȣ �Է�
	while(1) {
		DisplayText("[�� ��ȣ�� �Է��ϼ���] 1 or 2\r\n");
		WaitForSingleObject(hWriteEvent, INFINITE); // ���� �Ϸ� ��ٸ���
		int roomnum;
		roomnum = atoi(buf);
		if(roomnum == 1 || roomnum == 2) {	// �ΰ����� ��ȣ �Է� �ÿ��� �� ��ȣ ���� �� ����
			room = roomnum;
			EnableWindow(hSendButton, TRUE); // ������ ��ư Ȱ��ȭ
			SetEvent(hReadEvent); // �б� �Ϸ� �˸���
			break;
		}
		else {
			DisplayText("���� �� ��ȣ�Դϴ�. �ٽ� �Է����ּ���.\r\n");
			EnableWindow(hSendButton, TRUE); // ������ ��ư Ȱ��ȭ
			SetEvent(hReadEvent); // �б� �Ϸ� �˸���
		}
	}

	int retval;

	// ���� �ʱ�ȭ
	WSADATA wsa;
	if(WSAStartup(MAKEWORD(2,2), &wsa) != 0)
		return 1;

	// ������ ����
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

	// �� ��ȣ ������
	retval = send(sock, buf, strlen(buf), 0);
	if(retval == SOCKET_ERROR){
		err_display("send()");
		return 0;
	}

	// ������ �ޱ�
	retval = recv(sock, buf,  BUFSIZE+1, 0);
	if(retval == SOCKET_ERROR){
		err_display("recv()");
		return 0;
	}
	else if(retval == 0)
		return 0;
	buf[retval] = '\0';

	// ��ȭ�� �Է� �ޱ�
	DisplayText("[��ȭ���� �Է����ּ���] ����: user\r\n");
	WaitForSingleObject(hWriteEvent, INFINITE); // ���� �Ϸ� ��ٸ���
	strcpy(userID, buf);	// userID ������ ��ȭ�� ����

	// '\n' ���� ����
	int len = strlen(buf);
	if(buf[len-1] == '\n')
		buf[len-1] = '\0';

	// ���ڿ� ���̰� 0�̸� ������ ����
	if(strlen(buf) != 0){
		// ������ ������
		retval = send(sock, buf, strlen(buf), 0);
		if(retval == SOCKET_ERROR){
			err_display("send()");
			return 0;
		}
	}

	EnableWindow(hSendButton, TRUE); // ������ ��ư Ȱ��ȭ
	SetEvent(hReadEvent); // �б� �Ϸ� �˸���

	// ������ ����
	hThread = CreateThread(NULL, 0, ProcessInputSend, NULL, 0, NULL);
	if (hThread == NULL) {
		printf("fail make thread\n");
	}
	else {
		CloseHandle(hThread);
	}
	
	// ������ ������ ���
	while(1){
		// ������ �ޱ�
		retval = recv(sock, buf,  BUFSIZE+1, 0);
		if(retval == SOCKET_ERROR){
			err_display("recv()");
			break;
		}
		else if(retval == 0)
			break;
		buf[retval] = '\0';

		// �ߺ��� ��ȭ���� ��� (�ߺ��� ���, �������� 0�� ������)
		if(strcmp(buf, "0") == 0)
		{
			DisplayText("\r\n������ ��ȭ���� �ֽ��ϴ�. �������ּ���.\r\n", userID);
			DisplayText("[��ȭ���� �Է����ּ���] ����: user\r\n\r\n");
			flag = 1;
		}
		else
		{
			// ��ȭ���� �ޱ�
			if(buf[0] == '[')
			{
				// ���� ������ ���
				buf[retval] = '\0';
				DisplayText("[���� ������] %s\r\n", buf);
			}
			// ����� ��� �ޱ�
			else if(buf[0] == '*')
			{
				// ���� ������ ���
				buf[retval] = '\0';
				DisplayText("\r\n%s\r\n", buf);
			}
			// ä�ù� ��ȣ �ޱ�
			else if(strcmp(buf, "1") == 0 || strcmp(buf, "2") == 0)
			{
				continue;
			}
			// �̸� �ޱ� -> ä�� ���� �ȳ�
			else
			{
				flag = 2;
				buf[retval] = '\0';
				DisplayText("\r\n%s���� ��ȭ�濡 �����߽��ϴ�.\r\n\r\n", buf);
			}
		}
	}
	
	return 0;
}

/* ����� �Է� */
DWORD WINAPI ProcessInputSend(LPVOID arg)
{
	// ������ �Է�
	int retval;
	char line[BUFSIZE+11];
	int len;
	while(1) {
		WaitForSingleObject(hWriteEvent, INFINITE); // ���� �Ϸ� ��ٸ���

		// '\n' ���� ����
		len = strlen(buf);
		if(buf[len-1] == '\n')
			buf[len-1] = '\0';

		// ��ȭ���� ������ -> userID�� �����ؼ� ����
		if(flag == 2)
		{
			sprintf(line, "[%s] : %s", userID, buf);
		}
		// �ߺ��� ���, �̸��� ����
		else if(flag == 1)
		{
			sprintf(line, "%s", buf);
		}
		
		// ���ڿ� ���̰� 0�̸� ������ ����
		if(strlen(buf) == 0){
			EnableWindow(hSendButton, TRUE); // ������ ��ư Ȱ��ȭ
			SetEvent(hReadEvent); // �б� �Ϸ� �˸���
			continue;
		}

		// ������ ������
		retval = send(sock, line, strlen(line), 0);
		if(retval == SOCKET_ERROR){
			err_display("send()");
			return 0;
		}

		EnableWindow(hSendButton, TRUE); // ������ ��ư Ȱ��ȭ
		SetEvent(hReadEvent); // �б� �Ϸ� �˸���
	}
}

// ������ ����� ����� ��û�ϴ� �Լ�
int UserListDisplay()
{
	char call[25] = "����� ��� ����";
	int retval;

	// ������ ������
	retval = send(sock, call, strlen(call), 0);
	if(retval == SOCKET_ERROR){
		err_display("send()");
		return 0;
	}

	EnableWindow(hSendButton, TRUE); // ������ ��ư Ȱ��ȭ
	SetEvent(hReadEvent); // �б� �Ϸ� �˸���

	return 0;
}