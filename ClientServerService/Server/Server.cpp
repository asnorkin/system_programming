// Server.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Support.h"
#pragma comment(lib, "Ws2_32.lib")

#define BUFSIZE 1024
#define DEFAULT_BUFLEN 5000
#define DEFAULT_PORT "27015"
#define MAX_CLIENTS 30


HANDLE g_hChildStd_IN_Rd[MAX_CLIENTS];
HANDLE g_hChildStd_IN_Wr[MAX_CLIENTS];
HANDLE g_hChildStd_OUT_Rd[MAX_CLIENTS];
HANDLE g_hChildStd_OUT_Wr[MAX_CLIENTS];

SOCKET ListenSocket = INVALID_SOCKET;
SOCKET ClientSocket[MAX_CLIENTS];
HANDLE hThread[MAX_CLIENTS];
DWORD dwThreadId[MAX_CLIENTS];


int runServerProcess();
int closeServer();

int InitNewClientConnection(int idx);
int CloseClientConnection(int idx);
void CreateChildProcess(int idx);
DWORD WINAPI listenAndSend(LPVOID lpParam);
int InitPipe(int idx);
int ClosePipe(int idx);
void WriteToPipe(char* buf, int idx);
int ReadFromPipe(char* buf, int idx);
void ErrorExitMsgBox(PTSTR);


int _tmain(int argc, _TCHAR *argv[]) {
	return runServerProcess();
}

int runServerProcess()
{
	// Sockets
	for (int i = 0; i < MAX_CLIENTS; ++i) {
		ClientSocket[i] = INVALID_SOCKET;
		g_hChildStd_IN_Rd[i] = NULL;
		g_hChildStd_IN_Wr[i] = NULL;
		g_hChildStd_OUT_Rd[i] = NULL;
		g_hChildStd_OUT_Wr[i] = NULL;
	}
	int clients_num = 0;
	
	struct addrinfo *result = NULL;
	struct addrinfo hints;

	struct sockaddr_in server, address;
	int addrlen = sizeof(struct sockaddr_in);

	// Pool of sockets
	fd_set readfds;

	// Send-receive vars
	int iResult;
	int iSendResult;
	char recvbuf[DEFAULT_BUFLEN];
	int recvbuflen = DEFAULT_BUFLEN;

	// Initialize Winsock
	WSADATA wsaData;
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0)
		ErrorExit("WSAStartup failed with error: %d\n", iResult);

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	// Resolve the server address and port
	iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
	if (iResult != 0)
		WSACleanupInfoExit("getaddrinfo failed with error: %d\n", iResult);

	// Create a SOCKET for connecting to server
	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ListenSocket == INVALID_SOCKET) {
		freeaddrinfo(result);
		WSACleanupErrorExit("socket failed");
	}

	// Setup the TCP listening socket
	iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanupErrorExit("bind failed");
	}

	freeaddrinfo(result);

	// Listen the socket
	iResult = listen(ListenSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR) {
		closesocket(ListenSocket);
		WSACleanupErrorExit("listen failed");
	}
	
	// Client handle loop
	while (1) {
		FD_ZERO(&readfds);
		FD_SET(ListenSocket, &readfds);
		
		// Add active clients to the socket pool
		for (int i = 0; i < MAX_CLIENTS; ++i)
			if (ClientSocket[i] != INVALID_SOCKET)
				FD_SET(ClientSocket[i], &readfds);
		
		int activity = select(0, &readfds, NULL, NULL, NULL);
		if (activity == SOCKET_ERROR)
			WSAErrorExit("select failed");
		
		if (FD_ISSET(ListenSocket, &readfds)) {
			if (clients_num >= MAX_CLIENTS) {
				InfoLog("Max number of clients exceeded\n");
				// TODO: handle this more accurate
				continue;
			}
			
			SOCKET NewSocket = accept(ListenSocket, (struct sockaddr *)&address, (int *)&addrlen);
			if (NewSocket == INVALID_SOCKET)
				WSAErrorExit("accept failed\n");

			for (int i = 0; i < MAX_CLIENTS; ++i) {
				if (ClientSocket[i] == INVALID_SOCKET) {
					ClientSocket[i] = NewSocket;
					clients_num++;

					int err = InitNewClientConnection(i);
					if (err) {
						InfoLog("New client connection init failed: %d\n", err);
						break;
					}

					InfoLog("New client succesfully added to the list by index %d\n", i);
					break;
				}
			}
		}

		for (int i = 0; i < MAX_CLIENTS; ++i) {
			SOCKET CurrSocket = ClientSocket[i];

			if (FD_ISSET(CurrSocket, &readfds)) {
				getpeername(CurrSocket, (struct sockaddr *)&address, &addrlen);

				ZeroMemory(&recvbuf, recvbuflen);
				iResult = recv(CurrSocket, recvbuf, recvbuflen, 0);
				if (iResult > 0)
					WriteToPipe(recvbuf, i);
				else {
					if (iResult == 0)
						InfoLog("Client %d connection closing..\n", i);
					else
						WSAErrorLog("recv from client %d failed", i);

					int err = CloseClientConnection(i);
					if (err) {
						InfoLog("Client %d connection closing failed: %d\n", i, err);
						break;
					}

					ClientSocket[i] = INVALID_SOCKET;
					clients_num--;
				}
			}
		}
	}

	WSACleanup();
	return 0;
}

int closeServer() {
	int err = 0;
	for (int i = 0; i < MAX_CLIENTS; ++i) {
		if (ClientSocket[i] != INVALID_SOCKET) {
			err = CloseClientConnection(i);
			if (err)
				InfoLogRet(err, "CloseClientConnection failed");
		}
	}

	ListenSocket = INVALID_SOCKET;
	WSACleanup();

	return err;
}

int InitNewClientConnection(int idx) {
	int err = 0;

	err = InitPipe(idx);
	if (err)
		goto end;

	CreateChildProcess(idx);
	hThread[idx] = CreateThread(
		NULL,                   // default security attributes
		0,                      // use default stack size
		listenAndSend,			// thread function name
		&idx,					// argument to thread function
		0,                      // use default creation flags
		&(dwThreadId[idx]));

end:
	return err;
}

int CloseClientConnection(int idx) {
	int err = 0;

	DWORD exitCode = 0;
	if (!GetExitCodeThread(hThread[idx], &exitCode))
		ErrorLogRet(-1, "GetExitCodeThread failed");

	if (!TerminateThread(hThread[idx], exitCode))
		ErrorLogRet(-1, "TerminateThread failed");

	err = ClosePipe(idx);
	if (err)
		goto end;

	int iResult = shutdown(ClientSocket[idx], SD_SEND);
	if (iResult == INVALID_SOCKET) {
		closesocket(ClientSocket[idx]);
		WSACleanupErrorLogRet(-1, "shutdown failed");
	}

	err = closesocket(ClientSocket[idx]);
	if (err) {
		WSAErrorLog("closesocket failed");
		goto end;
	}

end:
	return err;
}

int InitPipe(int idx) {
	SECURITY_ATTRIBUTES saAttr;
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;

	if (!CreatePipe(&g_hChildStd_OUT_Rd[idx], &g_hChildStd_OUT_Wr[idx], &saAttr, 0))
		ErrorExitMsgBox(TEXT("StdoutRd CreatePipe"));
	// Ensure the read handle to the pipe for STDOUT is not inherited.
	if (!SetHandleInformation(g_hChildStd_OUT_Rd[idx], HANDLE_FLAG_INHERIT, 0))
		ErrorExitMsgBox(TEXT("Stdout SetHandleInformation"));
	// Create a pipe for the child process's STDIN. 
	if (!CreatePipe(&g_hChildStd_IN_Rd[idx], &g_hChildStd_IN_Wr[idx], &saAttr, 0))
		ErrorExitMsgBox(TEXT("Stdin CreatePipe"));
	// Ensure the write handle to the pipe for STDIN is not inherited. 
	if (!SetHandleInformation(g_hChildStd_IN_Wr[idx], HANDLE_FLAG_INHERIT, 0))
		ErrorExitMsgBox(TEXT("Stdin SetHandleInformation"));

	return 0;
}

int ClosePipe(int idx) {
	if ((CloseHandle(g_hChildStd_OUT_Wr[idx]) == 0) ||
		(CloseHandle(g_hChildStd_IN_Wr[idx]) == 0) ||
		(CloseHandle(g_hChildStd_IN_Rd[idx]) == 0) ||
		(CloseHandle(g_hChildStd_OUT_Rd[idx]) == 0))
		ErrorLogRet(-1, "ClosePipe failed");

	return 0;
}

void CreateChildProcess(int idx)
// Create a child process that uses the previously created pipes for STDIN and STDOUT.
{
	TCHAR szCmdline[] = TEXT("cmd.exe");
	PROCESS_INFORMATION piProcInfo;
	STARTUPINFO siStartInfo;
	BOOL bSuccess = FALSE;

	// Set up members of the PROCESS_INFORMATION structure. 
	ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

	// Set up members of the STARTUPINFO structure. 
	// This structure specifies the STDIN and STDOUT handles for redirection.

	ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
	siStartInfo.cb = sizeof(STARTUPINFO);
	siStartInfo.hStdError = g_hChildStd_OUT_Wr[idx];
	siStartInfo.hStdOutput = g_hChildStd_OUT_Wr[idx];
	siStartInfo.hStdInput = g_hChildStd_IN_Rd[idx];
	siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

	// Create the child process. 

	bSuccess = CreateProcess(NULL,
		szCmdline,     // command line 
		NULL,          // process security attributes 
		NULL,          // primary thread security attributes 
		TRUE,          // handles are inherited 
		0,             // creation flags 
		NULL,          // use parent's environment 
		NULL,          // use parent's current directory 
		&siStartInfo,  // STARTUPINFO pointer 
		&piProcInfo);  // receives PROCESS_INFORMATION 

					   // If an error occurs, exit the application.
	if (!bSuccess)
		ErrorExitMsgBox(TEXT("CreateProcess"));
	else {
		CloseHandle(piProcInfo.hProcess);
		CloseHandle(piProcInfo.hThread);
	}
}

void WriteToPipe(char* buf, int idx)
{
	DWORD dwRead, dwWritten;
	BOOL bSuccess = FALSE;
	dwRead = strlen(buf);
	bSuccess = WriteFile(g_hChildStd_IN_Wr[idx], buf, dwRead, &dwWritten, NULL);
}

int ReadFromPipe(char* buf, int idx)
{
	DWORD dwRead, dwWritten;
	CHAR chBuf[BUFSIZE];
	ZeroMemory(chBuf, BUFSIZE);

	BOOL bSuccess = FALSE;
	HANDLE hParentStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
	ReadFile(g_hChildStd_OUT_Rd[idx], buf, BUFSIZE, &dwRead, NULL);
	return dwRead;
}

DWORD WINAPI listenAndSend(LPVOID lpParam)
{
	int idx = *((int *)lpParam);

	char recvbuf[DEFAULT_BUFLEN];
	int recvbuflen = DEFAULT_BUFLEN;
	int bufSize = 0;
	int iSendResult;
	for (;;) {
		ZeroMemory(&recvbuf, recvbuflen);
		bufSize = ReadFromPipe(recvbuf, idx);
		iSendResult = send(ClientSocket[idx], recvbuf, bufSize, 0);
		if (iSendResult == SOCKET_ERROR) {
			closesocket(ClientSocket[idx]);
			WSACleanupErrorLogRet(-1, "send failed");
		}
	}
}

void ErrorExitMsgBox(PTSTR lpszFunction)
{
	LPVOID lpMsgBuf;
	LPVOID lpDisplayBuf;
	DWORD dw = GetLastError();

	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		dw,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf,
		0, NULL);

	lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
		(lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40) * sizeof(TCHAR));
	StringCchPrintf((LPTSTR)lpDisplayBuf,
		LocalSize(lpDisplayBuf) / sizeof(TCHAR),
		TEXT("%s failed with error %d: %s"),
		lpszFunction, dw, lpMsgBuf);
	MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK);

	LocalFree(lpMsgBuf);
	LocalFree(lpDisplayBuf);
	ExitProcess(1);
}
