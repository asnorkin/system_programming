#include "stdafx.h"
#include "DllInjector.h"
#include "binary.h"
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")


DllInjector::DllInjector(PWCHAR programPath) {
	this->programPath = PROGRAM_PATH;
	this->programDir = PROGRAM_DIR;
	/*this->programDir = (PWCHAR)calloc(wcslen(this->programPath), sizeof(WCHAR));
	memcpy(this->programDir, this->programPath, wcslen(this->programPath));
	PathRemoveFileSpecW(this->programDir);
	wprintf(L"%s\n", this->programDir);*/
}


DllInjector::~DllInjector() {
}

bool DllInjector::inject(PWCHAR dllPath) {
	STARTUPINFO cif;
	ZeroMemory(&cif, sizeof(STARTUPINFO));
	ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));

	if (!CreateProcess(
		programPath,		// process path
		NULL,				// command line 
		NULL,				// process security attributes 
		NULL,				// primary thread security attributes 
		FALSE,				// handles are inherited 
		CREATE_SUSPENDED,	// creation flags 
		NULL,				// use parent's environment 
		NULL,				// use parent's current directory 
		&cif,				// STARTUPINFO pointer 
		&pi					// receives PROCESS_INFORMATION 
	))
		ErrorLogRet(false, "CreateProcess failed");

	PATCH_INFO patchInfo;

	if (!patch(&patchInfo))
		ErrorLogRet(false, "patch failed");

	if (ResumeThread(pi.hThread) == -1)
		ErrorLogRet(false, "ResumeThread failed");

	Sleep(200);

	LPVOID pProcessKernel32 = getProcessKernel32x64();
	if (!pProcessKernel32)
		ErrorLogRet(false, "getProcessKernel32x64 failed");

	printf("pKernel32: %p\n", pProcessKernel32);

	if (SuspendThread(pi.hThread) == -1)
		ErrorLogRet(false, "SuspendThread failed");

	if (!unpatch(&patchInfo))
		ErrorLogRet(false, "unpatch failed");

	if (!executeShellcode(pProcessKernel32))
		ErrorLogRet(false, "executeShellcode failed");

	if (ResumeThread(pi.hThread) == -1)
		ErrorLogRet(false, "ResumeThread failed");

	if (WaitForSingleObject(pi.hThread, INFINITE) == 0xFFFFFFFF)
		ErrorLogRet(false, "WaitForSingleObject failed");

	DWORD retVal;
	if (!GetExitCodeThread(pi.hThread, &retVal))
		ErrorLogRet(false, "GetExitCodeThread failed");

	if (!CloseHandle(pi.hThread))
		ErrorLogRet(false, "CloseHandle failed");

	InfoLog("Dll succesfully injected!");
	return true;
}

//#################################################################################################
//
//											Patching
//
//#################################################################################################
bool DllInjector::patch(PPATCH_INFO pPatchInfo) {
	LPVOID pBase;
	if (!getProcess_pBase(&pBase))
		ErrorLogRet(false, "getProcess_pBase failed");

	printf("pBase: %p\n", pBase);

	LPVOID pEntryPoint;
	if (!getProcessEntryPoint(pBase, &pEntryPoint))
		ErrorLogRet(false, "getProcessEntryPoint failed");

	printf("pEntryPoint: %p\n", pEntryPoint);

	pPatchInfo->pBase = pBase;
	pPatchInfo->pEntryPoint = pEntryPoint;

	if (!readProcessMemory(pEntryPoint, pPatchInfo->origInstr, sizeof(loop)))
		ErrorLogRet(false, "readProcessMemory failed during reading instructions before patch");

	if (!writeProcessMemory(pEntryPoint, loop, sizeof(loop)))
		ErrorLogRet(false, "writeProcessMemory failed during patching");

	pPatchInfo->context.ContextFlags = CONTEXT_FULL;
	if (!GetThreadContext(pi.hThread, &(pPatchInfo->context)))
		ErrorLogRet(false, "GetThreadContext failed");

	return true;
}

bool DllInjector::unpatch(PPATCH_INFO pPatchInfo) {
	CONTEXT context;
	context.ContextFlags = CONTEXT_FULL;
	if (!GetThreadContext(pi.hThread, &context))
		ErrorLogRet(false, "GetThreadContext failed during unpatching");

	context.ContextFlags = CONTEXT_FULL;
	context.Rip = (DWORD64)(pPatchInfo->pEntryPoint);
	if (!SetThreadContext(pi.hThread, &context))
		ErrorLogRet(false, "SetThreadContext failed during unpatching");

	if (!writeProcessMemory(pPatchInfo->pEntryPoint, pPatchInfo->origInstr, sizeof(loop)))
		ErrorLogRet(false, "writeProcessMemory failed during unpatching");

	return true;
}

//#################################################################################################
//
//											Shellcode
//
//#################################################################################################
bool DllInjector::executeShellcode(LPVOID pProcessKernel32) {
	SHELLCODE shellcode;
	shellcode.loadLibrary = getProcessLoadLibrary(pProcessKernel32);
	shellcode.getLastError = getProcessGetLastError(pProcessKernel32);
	wcsncpy_s(shellcode.pathToDll, DLL_PATH, sizeof(DLL_PATH));

	THREAD_INFO threadInfo;
	threadInfo.code = byte_code_myLoadLibrary;
	threadInfo.codeSize = sizeof(byte_code_myLoadLibrary);
	threadInfo.arg = (LPVOID)&shellcode;
	threadInfo.argSize = sizeof(shellcode);

	if (!executeProcessThread(&threadInfo))
		ErrorLogRet(false, "executeProcessThread failed during shellcode execution");

	if (threadInfo.retVal != 0)
		InfoLogRet(false, "LoadLibraryW returned non zero code during execution inside the process: %d", 
			threadInfo.retVal);

	return true;
}

LoadLibrary_t DllInjector::getProcessLoadLibrary(LPVOID pProcessKernel32) {
	HMODULE hKernel32 = GetModuleHandle(KERNEL32DLL);
	if (!hKernel32)
		ErrorLogRet(false, "GetModuleHandle failed during getting kernel32.dll handle");

	ULONG_PTR LoadLibraryRVA = (ULONG_PTR)LoadLibraryW - (ULONG_PTR)hKernel32;
	return (LoadLibrary_t)((ULONG_PTR)pProcessKernel32 + LoadLibraryRVA);
}

GetLastError_t DllInjector::getProcessGetLastError(LPVOID pProcessKernel32) {
	HMODULE hKernel32 = GetModuleHandle(KERNEL32DLL);
	if (!hKernel32)
		ErrorLogRet(false, "GetModuleHandle failed during getting kernel32.dll handle");

	ULONG_PTR GetLastErrorRVA = (ULONG_PTR)GetLastError - (ULONG_PTR)hKernel32;
	return (GetLastError_t)((ULONG_PTR)pProcessKernel32 + GetLastErrorRVA);
}

//#################################################################################################
//
//										Process management
//
//#################################################################################################
bool DllInjector::getProcess_pBase(LPVOID p_pBase) {
	THREAD_INFO threadInfo;
	threadInfo.code = get_pBase;
	threadInfo.codeSize = sizeof(get_pBase);
	threadInfo.arg = p_pBase;
	threadInfo.argSize = sizeof(LPVOID); // pBase is also LPVOID

	if (!executeProcessThread(&threadInfo))
		ErrorLogRet(false, "executeProcessThread failed");

	return true;
}

bool DllInjector::getProcessEntryPoint(LPVOID pBase, LPVOID p_pEntryPoint) {
	IMAGE_DOS_HEADER DosHeader;
	if (!readProcessMemory(pBase, &DosHeader, sizeof(IMAGE_DOS_HEADER)))
		ErrorLogRet(false, "readProcessMemory failed during Dos header reading");

	LPVOID pPEHeader = (LPVOID)((ULONG_PTR)pBase + DosHeader.e_lfanew); // only address
	IMAGE_NT_HEADERS PEHeader;											// pointer to struct
	if (!readProcessMemory(pPEHeader, &PEHeader, sizeof(IMAGE_NT_HEADERS)))
		ErrorLogRet(false, "readProcessMemory failed during PE header reading");

	printf("%c%c%x%x\n", PEHeader.Signature & 0xFF, (PEHeader.Signature >> 8) & 0xFF,
		(PEHeader.Signature << 16) & 0xFF, (PEHeader.Signature << 24) & 0xFF);

	ULONG_PTR pEntryPoint = PEHeader.OptionalHeader.AddressOfEntryPoint + (ULONG_PTR)pBase;
	if (!memcpy(p_pEntryPoint, &pEntryPoint, sizeof(pEntryPoint)))
		ErrorLogRet(false, "memcpy failed during p_pEntryPoint copying");

	return true;
}

LPVOID DllInjector::getProcessKernel32x64() {
	LPVOID pKernel32;

	THREAD_INFO threadInfo;
	threadInfo.code = get_base_adr_x64;
	threadInfo.codeSize = sizeof(get_base_adr_x64);
	threadInfo.arg = (LPVOID)&pKernel32;
	threadInfo.argSize = sizeof(pKernel32);
	
	if (!executeProcessThread(&threadInfo))
		ErrorLogRet(NULL, "executeProcessThread failed during getting kernel32 addr");

	return pKernel32;
}

bool DllInjector::executeProcessThread(PTHREAD_INFO pThreadInfo) {
	LPVOID pProcessCodeMem = allocProcessMemory(pThreadInfo->codeSize);
	if (!pProcessCodeMem)
		ErrorLogRet(false, "allocProcessMemory for code failed");

	if (!writeProcessMemory(pProcessCodeMem, pThreadInfo->code, pThreadInfo->codeSize))
		ErrorLogRet(false, "writeProcessMemory for code failed");

	LPVOID pProcessArgMem = allocProcessMemory(pThreadInfo->argSize);
	if (!pProcessArgMem)
		ErrorLogRet(false, "allocProcessMemory for arg failed");

	if (!writeProcessMemory(pProcessArgMem, pThreadInfo->arg, pThreadInfo->argSize))
		ErrorLogRet(false, "writeProcessMemory for arg failed");

	HANDLE hThread = CreateRemoteThread(
		pi.hProcess,
		NULL,
		0,
		(LPTHREAD_START_ROUTINE)pProcessCodeMem,
		pProcessArgMem,
		0,
		NULL);
	if (!hThread)
		ErrorLogRet(false, "CreateRemoteThread failed");

	if (WaitForSingleObject(hThread, INFINITE) == 0xFFFFFFFF)
		ErrorLogRet(false, "WaitForSingleObject failed");

	if (!GetExitCodeThread(hThread, &(pThreadInfo->retVal)))
		ErrorLogRet(false, "GetExitCodeThread failed");

	if (!CloseHandle(hThread))
		ErrorLogRet(false, "CloseHandle failed");

	if (!readProcessMemory(pProcessArgMem, pThreadInfo->arg, pThreadInfo->argSize))
		ErrorLogRet(false, "readProcessMemory for arg failed");

	if (!freeProcessMemory(pProcessCodeMem))
		ErrorLogRet(false, "freeProcessMemory for code failed");

	if (!freeProcessMemory(pProcessArgMem))
		ErrorLogRet(false, "freeProcessMemory for arg failed");

	return true; 
}

LPVOID DllInjector::allocProcessMemory(SIZE_T size) {
	return VirtualAllocEx(
		pi.hProcess,
		NULL,
		size,
		MEM_RESERVE | MEM_COMMIT,
		PAGE_EXECUTE_READWRITE);
}

bool DllInjector::freeProcessMemory(LPVOID lpAddress) {
	return VirtualFreeEx(
		pi.hProcess,
		lpAddress,
		0,
		MEM_RELEASE);
}

bool DllInjector::readProcessMemory(LPCVOID baseAddress, LPVOID buffer, SIZE_T size) {
	SIZE_T retSize = 0;
	if (!ReadProcessMemory(pi.hProcess, baseAddress, buffer, size, &retSize) || retSize != size)
		return false;

	return true;
}

bool DllInjector::writeProcessMemory(LPVOID baseAddress, LPVOID buffer, SIZE_T size) {
	SIZE_T retSize = 0;
	if (!WriteProcessMemory(pi.hProcess, baseAddress, buffer, size, &retSize) || retSize != size)
		return false;

	return true;
}
