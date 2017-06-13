#include "stdafx.h"
#include "mapper.h"
#include "support.h"


Mapper::Mapper(_TCHAR *dumpfile) :
	hFile(0), hFileMapping(0), pImageBase(NULL)
{
	hFile  = CreateFile(dumpfile,
						GENERIC_READ,
						FILE_SHARE_READ,
						NULL,
						OPEN_EXISTING,
						0,
						NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		ErrorLog("Failed to open file");
		return;
	}

	hFileMapping	= CreateFileMapping(hFile,
										NULL,
										PAGE_READONLY | SEC_IMAGE,
										0,
										0,
										NULL);
	if (hFileMapping == INVALID_HANDLE_VALUE) {
		CloseHandle(hFile);
		ErrorLog("Failed to create mapping");
		return;
	}

	is_init = true;
	return;
}

Mapper::~Mapper() {
	if (!is_init) 
		return;
	
	if (!UnmapFile())
		ErrorLog("Error during Mapped destructing: UnmapFile failed");

	CloseHandle(hFileMapping);
	CloseHandle(hFile);
}

BOOL Mapper::MapFile(DWORD accessType,
					 DWORD offsetHigh,
					 DWORD offsetLow,
					 SIZE_T bytesNumber) {
	if (!is_init) {
		InfoLog("Trying to map file using not inialized mapper");
		return false;
	}

	if (is_mapped) {
		InfoLog("Trying to map file which already partily mapped");
		return false;
	}

	pImageBase  = MapViewOfFile(hFileMapping,
								accessType, 
								offsetHigh, 
								offsetLow, 
								bytesNumber);
	if (!pImageBase) {
		ErrorLog("Failed to map file using MapViewOfFile");
		return false;
	}

	InfoLog("File successfully mapped");
	is_mapped = true;
	return true;
}

BOOL Mapper::UnmapFile() {
	if (!is_mapped) {
		InfoLog("Trying to unmap file which is not mapped");
		return false;
	}

	if (!UnmapViewOfFile(pImageBase)) {
		ErrorLog("Failed to unmap file using UnmapViewOfFile");
		return false;
	}

	is_mapped = false;
	return true;
}

PVOID Mapper::GetMappingAddr() {
	return is_mapped ? pImageBase : NULL;
}
