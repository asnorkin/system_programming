// DllInjector.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "DllInjector.h"

int main()
{
	printf("!");
	DllInjector injector = DllInjector(PROGRAM_PATH);
	if (!injector.inject(DLL_PATH))
		ErrorLogRet(-1, "Injection failed");

	return 0;
}



