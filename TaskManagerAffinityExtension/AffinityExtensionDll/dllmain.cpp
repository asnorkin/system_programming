// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"
#include "hook.h"
#include "hooked_funcs.h"


BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		if (!hookFuncByName(SET_WINDOW_LONG_PTR_W, (ULONG_PTR)HookedSetWindowLongPtrW))
			InfoLog("hookFuncByName failed in dllmain during hooking %s function", SET_WINDOW_LONG_PTR_W);

		if (!hookFuncByName(SEND_MESSAGE_W, (ULONG_PTR)HookedSendMessageW))
			InfoLog("hookFuncByName failed in dllmain durin hooking %s function", SEND_MESSAGE_W);

	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

