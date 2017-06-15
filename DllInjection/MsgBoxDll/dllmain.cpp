// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		MessageBox(NULL, TEXT("DLL_PROCESS_ATTACH"), TEXT("Hello!"), MB_OK | MB_SYSTEMMODAL);
		break;

	case DLL_THREAD_ATTACH:
		MessageBox(NULL, TEXT("DLL_THREAD_ATTACH"), TEXT("Hello!"), MB_OK | MB_SYSTEMMODAL);
		break;

	case DLL_THREAD_DETACH:
		MessageBox(NULL, TEXT("DLL_THREAD_DETACH"), TEXT("Hello!"), MB_OK | MB_SYSTEMMODAL);
		break;

	case DLL_PROCESS_DETACH:
		MessageBox(NULL, TEXT("DLL_PROCESS_DETACH"), TEXT("Hello!"), MB_OK | MB_SYSTEMMODAL);
		break;
	}
	return TRUE;
}

