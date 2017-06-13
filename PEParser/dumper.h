#pragma once

#include "mapper.h"
#include <unordered_map>

struct PEInfo {
	ULONG_PTR pBase;
	PIMAGE_NT_HEADERS pPEHeader;
	PIMAGE_SECTION_HEADER pSectionHeader;

	DWORD secNum;
	DWORD secAlign;

	ULONG_PTR exportRVA;
	ULONG_PTR importRVA;
	ULONG_PTR debugRVA;
	ULONG_PTR exceptionRVA;
};


class Dumper
{
public:
	Dumper(_TCHAR *peFile, _TCHAR *pdbFile, char *outfile);
	~Dumper();
	bool dump();

private:
	_TCHAR *peFile;
	_TCHAR *pdbFile;

	FILE *fstream;

	Mapper *map;

	struct PEInfo peInfo;
	std::unordered_map < ULONG_PTR, PCHAR > runtimeFuncsDict;


	bool dumpSections();
	void dumpSection(PIMAGE_SECTION_HEADER pSectionHeader);

	bool dumpExports();

	bool dumpImports();
	bool dumpImportedFuncs(PVOID pThunk);
	bool dumpImportedFuncsX86(PIMAGE_THUNK_DATA32 pThunk);
	bool dumpImportedFuncsX64(PIMAGE_THUNK_DATA64 pThunk);

	bool matchPDB();

	bool dumpRuntimeFunctions();
	bool dumpRuntimeFunction(PIMAGE_IA64_RUNTIME_FUNCTION_ENTRY pPDataSection, int idx);
	bool dumpUnwindInfo(PUNWIND_INFO uInfo);
	bool dumpUnwindInfoV1(PUNWIND_INFO uInfo);
	bool dumpUnwindInfoV2(PUNWIND_INFO uInfo);
	bool dumpUnwindCode(UNWIND_CODE uCode);

	bool parsePE();

	bool parseRuntimeFuncs();

	PIMAGE_DATA_DIRECTORY getPDataDirectory();
	PIMAGE_SECTION_HEADER getPSectionHeader();
	PIMAGE_NT_HEADERS getPPEHeader();

	ULONG_PTR getExportRVA();
	ULONG_PTR getImportRVA();
	ULONG_PTR getDebugRVA();
	ULONG_PTR getExceptionRVA();

	int defSection(ULONG_PTR rva);
	ULONG_PTR rvaToOffset(ULONG_PTR rva);

	bool checkMZ(PIMAGE_DOS_HEADER pDosHeader);
	bool checkPESignature(DWORD signature);
};

