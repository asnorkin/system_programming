// pe.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "support.h"
#include "dumper.h"


#define DUMPFILE "dump.txt"


void PrintUsage(const _TCHAR *program) {
	_tprintf(_T("Usage:\n"));
	_tprintf(_T("\t%s <pe file path> [<pdb file path>]\n"), program);
}


int _tmain(int argc, _TCHAR *argv[])
{
	_TCHAR *peFile = NULL, *pdbFile = NULL;

	if (argc == 2) {
		peFile = argv[1];
	} else if (argc == 3) {
		peFile = argv[1];
		pdbFile = argv[2];
	} else {
		PrintUsage(argv[0]);
		return -1;
	}

	Dumper dumper = Dumper(peFile, pdbFile, DUMPFILE);
	dumper.dump();

    return 0;
}

