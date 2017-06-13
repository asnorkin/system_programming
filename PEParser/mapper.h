#pragma once

#include "Windows.h"

class Mapper {
public:
	Mapper(_TCHAR *dumpfile);
	~Mapper();

	BOOL MapFile(DWORD accessType = FILE_MAP_READ,
				 DWORD offsetHigh = 0,
				 DWORD offsetLow = 0,
				 SIZE_T bytesNumber = 0);
	BOOL UnmapFile();
	PVOID GetMappingAddr();

private:
	HANDLE hFile;
	HANDLE hFileMapping;
	PVOID  pImageBase;

	BOOL   is_init;
	BOOL   is_mapped;
};