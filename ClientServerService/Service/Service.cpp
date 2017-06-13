// Service.cpp : Defines the entry point for the console application.
//
#include "stdafx.h"
#include "svccontrol.h"
#include "svcconfig.h"

#define HELP_CMD TEXT("--help")
#define INSTALL_CMD TEXT("install")
#define START_CMD TEXT("start")
#define STOP_CMD TEXT("stop")
#define DELETE_CMD TEXT("delete")

#define PORT_NUMBER "27015"

void __cdecl initSvcForSCM();
VOID SvcInstall();

void printUsage() {
	printf("Usage: Service.exe <command>\n");
	printf("\t<command> could be one of the following:\n");
	printf("\t  install	-- install service\n");
	printf("\t  start -- start service\n");
	printf("\t  stop -- stop service\n");
	printf("\t  delete -- delete service\n");
}

TCHAR szSvcName[80] = L"svcControlTerminal";
TCHAR szCommand[10];

int _tmain(int argc, _TCHAR* argv[]) {
	if (argc == 1) {
		initSvcForSCM();
		return 0;
	}

	if (lstrcmpi(argv[1], HELP_CMD) == 0) {
		printUsage();
		return 0;
	}

	if (lstrcmpi(argv[1], INSTALL_CMD) == 0) 
		SvcInstall();
	else if (argc == 2) {
		StringCchCopy(szCommand, 10, argv[1]);
		if (lstrcmpi(argv[1], START_CMD) == 0) 
			DoStartSvc();
		else if (lstrcmpi(argv[1], STOP_CMD) == 0) 
			DoStopSvc();
		else if (lstrcmpi(argv[1], DELETE_CMD) == 0)
			DoDeleteSvc();
		else {
			_tprintf(TEXT("Invalid rule (%s)\n\n"), argv[1]);
			printUsage();
		}
	}

	return 0;
}

