// Client.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Support.h"

#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#define BUFSIZE 4096
#define DEFAULT_BUFLEN 4096
#define IP_PORT_LEN 16
#define EXIT "exit"
#define HELP _T("--help")

#define DEFAULT_IP "127.0.0.1"
#define DEFAULT_PORT "27015"


struct ipAndPort {
	char *ip;
	char *port;
};


HANDLE hStdin;
bool isConnectionWithServerLost;

int connectToServer(SOCKET *ConnectSocket, PCSTR ip, PCSTR port);

DWORD WINAPI listenAndPrintThreadLoop(LPVOID lpParam);
int readStdinSendToServerLoop(SOCKET ConnectSocket);
int getInitMsgFromServer(SOCKET ConnectSocket);

struct ipAndPort *parseArguments(int argc, _TCHAR *argv[]);
void printUsage();


int _tmain(int argc, _TCHAR *argv[])
{
	struct ipAndPort *ipAndPort = parseArguments(argc, argv);
	if (!ipAndPort)
		return -1;

	char *ip = ipAndPort->ip;
	char *port = ipAndPort->port;
	SOCKET ConnectSocket = INVALID_SOCKET;
	int iResult = connectToServer(&ConnectSocket, ip, port);
	if (iResult != 0) {
		if (argc == 2) { free(ip); free(port); }
		return -1;
	}

	// Receive init cmd.exe output from server
	iResult = getInitMsgFromServer(ConnectSocket);
	if (iResult <= 0) {
		closesocket(ConnectSocket);
		if (argc == 2) { free(ip); free(port); }
		return -1;
	}

	HANDLE hThread;
	DWORD dwThreadId;
	hThread = CreateThread(
		NULL,								// default security attributes
		0,									// use default stack size  
		listenAndPrintThreadLoop,			// thread function name
		&ConnectSocket,						// argument to thread function 
		0,									// use default creation flags 
		&dwThreadId);

	// Simultaneously send data to server
	iResult = readStdinSendToServerLoop(ConnectSocket);
	if (iResult <= 0) {
		closesocket(ConnectSocket);
		if (argc == 2) { free(ip); free(port); }

		return -1;
	}

	// Kill thread
	DWORD exitCode;
	if (!GetExitCodeThread(hThread, &exitCode))
		ErrorLogRet(-1, "GetExitCodeThread failed");

	if (!TerminateThread(hThread, exitCode))
		ErrorLogRet(-1, "TerminateThread failed");

	// shutdown the connection since no more data will be sent
	iResult = shutdown(ConnectSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		closesocket(ConnectSocket);
		if (argc == 2) { free(ip); free(port); }
		WSAErrorLogRet(-1, "shutdown failed");
	}

	WSACleanup();
	if (argc == 2) { free(ip); free(port); }

	InfoLog("client closed successfully");
    return 0;
}

int connectToServer(SOCKET* ConnectSocket, PCSTR ip, PCSTR port) {
	WSADATA wsaData;
	struct addrinfo *result = NULL, *ptr = NULL, hints;

	// Initialize Winsock
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0)
		WSAErrorLogRet(-1, "WSAStartup failed");

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	iResult = getaddrinfo(ip, port, &hints, &result);
	if (iResult != 0) 
		WSACleanupErrorLogRet(-1, "getaddrinfo failed");
		
	// Attempt to connect to an address until one succeeds
	for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
		// Create a SOCKET for connecting to server
		*ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
			ptr->ai_protocol);
		if (*ConnectSocket == INVALID_SOCKET)
			WSACleanupErrorLogRet(-1, "socket failed");

		// Connect to server.
		iResult = connect(*ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			closesocket(*ConnectSocket);
			*ConnectSocket = INVALID_SOCKET;
			continue;
		}
		break;
	}

	freeaddrinfo(result);

	if (*ConnectSocket == INVALID_SOCKET)
		WSACleanupErrorLogRet(-1, "Unable to connect to server");
	
	return 0;
}

int readStdinSendToServerLoop(SOCKET ConnectSocket)
{
	int iResult = 0;
	CHAR chBuf[BUFSIZE];
	DWORD dwRead;

	hStdin = GetStdHandle(STD_INPUT_HANDLE);
	if (hStdin == INVALID_HANDLE_VALUE)
		InfoLogRet(-1, "GetStdHandle failed", -1);

	for (;;) {
		ZeroMemory(&chBuf, BUFSIZE);
		BOOL bSuccess = ReadFile(hStdin, chBuf, BUFSIZE, &dwRead, NULL);
		if (isConnectionWithServerLost || !bSuccess) {
			if (!bSuccess)
				printf("ReadFile stdin failed: %d\n", GetLastError());

			WSACleanup();
			iResult = -1;
			break;
		}

		if (strncmp(chBuf, EXIT, strlen(EXIT)) == 0) {
			printf("goodbye...\n");
			iResult = 1;
			break;
		}

		iResult = send(ConnectSocket, chBuf, (int)strlen(chBuf), 0);
		if (iResult == SOCKET_ERROR) {
			if (ConnectSocket != INVALID_SOCKET)
				closesocket(ConnectSocket);

			WSACleanupErrorLogRet(-1, "send failed");
		}		
	}

	CloseHandle(hStdin);
	return iResult;
}

DWORD WINAPI listenAndPrintThreadLoop(LPVOID lpParam)
{
	SOCKET* ConnectSocket = (SOCKET*)lpParam;

	char recvbuf[DEFAULT_BUFLEN];
	int recvbuflen = DEFAULT_BUFLEN;
	int bufSize = 0;

	for (;;) {
		ZeroMemory(recvbuf, DEFAULT_BUFLEN);
		int iResult = recv(*ConnectSocket, recvbuf, recvbuflen, 0);
		if (iResult > 0) {
			printf("%s", recvbuf);
		}
		else if (iResult == 0) {
			printf("Connection closed\n");
			break;
		}
		else {
			printf("recv failed with error: %d\n", WSAGetLastError());
			isConnectionWithServerLost = true;
			CancelIoEx(hStdin, NULL);
			CloseHandle(hStdin);
			hStdin = INVALID_HANDLE_VALUE;
			break;
		}
	}

	closesocket(*ConnectSocket);
	*ConnectSocket = INVALID_SOCKET;
	return 0;
}

int getInitMsgFromServer(SOCKET ConnectSocket)
{
	char recvbuf[DEFAULT_BUFLEN];
	int recvbuflen = DEFAULT_BUFLEN;
	ZeroMemory(&recvbuf, DEFAULT_BUFLEN);

	int iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
	if (iResult > 0) {
		printf("%s", recvbuf);
	}
	else if (iResult == 0)
		printf("Connection closed\n");
	else
		printf("recv failed with error: %d\n", WSAGetLastError());
	return iResult;
}


struct ipAndPort *parseArguments(int argc, _TCHAR *argv[]) {
	struct ipAndPort *ipAndPort = (struct ipAndPort *)calloc(1, sizeof(struct ipAndPort));
	ipAndPort->ip = DEFAULT_IP;
	ipAndPort->port = DEFAULT_PORT;

	if (argc == 2) {
		if (!_tcscmp(argv[1], HELP)) {
			printUsage();
			return NULL;
		}

		_TCHAR *p = wcschr(argv[1], ':');
		if (!p || p == argv[1] || p == argv[1] + wcslen(argv[1])) {
			InfoLog("Invalid format of ip and port");
			printUsage();
			return NULL;
		}

		memcpy(ipAndPort->ip, argv[1], p - argv[1]);
		memcpy(ipAndPort->port, p + 1, argv[1] + wcslen(argv[1]) - p - 1);		
	}

	return ipAndPort;
}

void printUsage() {
	_tprintf(_T("Usage:"));
	_tprintf(_T("\tClient.exe\n"));
	_tprintf(_T("\tClient.exe <ip>:<port>\n"));
}

