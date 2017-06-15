#pragma once


typedef struct _PATCH_INFO {
	LPVOID pBase;
	LPVOID pEntryPoint;
	UCHAR origInstr[PATCH_SIZE];
	CONTEXT context;
} PATCH_INFO, *PPATCH_INFO;


typedef struct _THREAD_INFO {
	LPVOID code;
	SIZE_T codeSize;
	LPVOID arg;
	SIZE_T argSize;
	DWORD retVal;
} THREAD_INFO, *PTHREAD_INFO;


typedef HMODULE(*LoadLibrary_t)(_In_ LPCSTR lpLibFileName);
typedef DWORD(*GetLastError_t)(VOID);

typedef struct _SHELLCODE {
	LoadLibrary_t loadLibrary;
	GetLastError_t getLastError;
	WCHAR pathToDll[PATH_SIZE];
} SHELLCODE, *PSHELLCODE;


class DllInjector
{
public:
	DllInjector(PWCHAR programPath);
	~DllInjector();

	bool inject(PWCHAR dllPath);

private:
	PWCHAR programPath;
	PWCHAR programDir;

	PROCESS_INFORMATION pi;

	bool patch(PPATCH_INFO pPatchInfo);
	bool unpatch(PPATCH_INFO pPatchInfo);

	bool executeShellcode(LPVOID pProcessKernel32);
	LoadLibrary_t getProcessLoadLibrary(LPVOID pProcessKernel32);
	GetLastError_t getProcessGetLastError(LPVOID pProcessKernel32);

	bool getProcess_pBase(LPVOID p_pBase);
	bool getProcessEntryPoint(LPVOID pBase, LPVOID p_pEntryPoint);

	LPVOID getProcessKernel32x64();
	bool executeProcessThread(PTHREAD_INFO pThreadInfo);
	LPVOID allocProcessMemory(SIZE_T size);
	bool freeProcessMemory(LPVOID lpAdress);
	bool readProcessMemory(LPCVOID baseAddress, LPVOID buffer, SIZE_T size);
	bool writeProcessMemory(LPVOID baseAddress, LPVOID buffer, SIZE_T size);
};

