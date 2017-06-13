#pragma once

#include <unordered_map>


extern DWORD g_dwMachineType;


typedef struct matchInfo {
	DWORD Signature;
	GUID GUID;
	DWORD Age;
	BYTE* PdbFileName;
} matchInfo;


bool LoadDataFromPdb(
	const wchar_t    *szFilename,
	IDiaDataSource  **ppSource,
	IDiaSession     **ppSession,
	IDiaSymbol      **ppGlobal);


//	Uses dia2 LoadAndValidateDataFromPdb func
bool LoadAndValidateDataFromPdb(
	const wchar_t    *szFilename,
	IDiaDataSource  **ppSource,
	IDiaSession     **ppSession,
	IDiaSymbol      **ppGlobal,
	matchInfo		*MI);


bool DumpAllRuntimeFuncsToDict(
	IDiaSymbol      *ppGlobal,
	std::unordered_map < ULONG_PTR, PCHAR > *dict
);


bool DumpAllPublicsToDict(
	IDiaSymbol      *pGlobal,
	std::unordered_map < ULONG_PTR, PCHAR > *dict
);


bool DumpAllGlobalsToDict(
	IDiaSymbol      *pGlobal,
	std::unordered_map < ULONG_PTR, PCHAR > *dict
);


void DumpPublicSymbolToDict(IDiaSymbol *pSymbol, std::unordered_map < ULONG_PTR, PCHAR > *dict);
void DumpGlobalSymbolToDict(IDiaSymbol *pSymbol, std::unordered_map < ULONG_PTR, PCHAR > *dict);


void Cleanup(IDiaSymbol* g_pGlobalSymbol, IDiaSession* g_pDiaSession);