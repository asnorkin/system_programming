#include "stdafx.h"
#include "dumpPDB.h"

#pragma warning (disable : 4100)


DWORD g_dwMachineType = CV_CFL_80386;


// Tags returned by Dia
const wchar_t * const rgTags[] =
{
	L"(SymTagNull)",                     // SymTagNull
	L"Executable (Global)",              // SymTagExe
	L"Compiland",                        // SymTagCompiland
	L"CompilandDetails",                 // SymTagCompilandDetails
	L"CompilandEnv",                     // SymTagCompilandEnv
	L"Function",                         // SymTagFunction
	L"Block",                            // SymTagBlock
	L"Data",                             // SymTagData
	L"Annotation",                       // SymTagAnnotation
	L"Label",                            // SymTagLabel
	L"PublicSymbol",                     // SymTagPublicSymbol
	L"UserDefinedType",                  // SymTagUDT
	L"Enum",                             // SymTagEnum
	L"FunctionType",                     // SymTagFunctionType
	L"PointerType",                      // SymTagPointerType
	L"ArrayType",                        // SymTagArrayType
	L"BaseType",                         // SymTagBaseType
	L"Typedef",                          // SymTagTypedef
	L"BaseClass",                        // SymTagBaseClass
	L"Friend",                           // SymTagFriend
	L"FunctionArgType",                  // SymTagFunctionArgType
	L"FuncDebugStart",                   // SymTagFuncDebugStart
	L"FuncDebugEnd",                     // SymTagFuncDebugEnd
	L"UsingNamespace",                   // SymTagUsingNamespace
	L"VTableShape",                      // SymTagVTableShape
	L"VTable",                           // SymTagVTable
	L"Custom",                           // SymTagCustom
	L"Thunk",                            // SymTagThunk
	L"CustomType",                       // SymTagCustomType
	L"ManagedType",                      // SymTagManagedType
	L"Dimension",                        // SymTagDimension
	L"CallSite",                         // SymTagCallSite
	L"InlineSite",                       // SymTagInlineSite
	L"BaseInterface",                    // SymTagBaseInterface
	L"VectorType",                       // SymTagVectorType
	L"MatrixType",                       // SymTagMatrixType
	L"HLSLType",                         // SymTagHLSLType
	L"Caller",                           // SymTagCaller,
	L"Callee",                           // SymTagCallee,
	L"Export",                           // SymTagExport,
	L"HeapAllocationSite",               // SymTagHeapAllocationSite
	L"CoffGroup",                        // SymTagCoffGroup
};


PCHAR wchar2pchar(wchar_t *inpStr);


////////////////////////////////////////////////////////////
// Create an IDiaData source and open a PDB file
//
bool LoadDataFromPdb(
	const wchar_t    *szFilename,
	IDiaDataSource  **ppSource,
	IDiaSession     **ppSession,
	IDiaSymbol      **ppGlobal)
{
	wchar_t wszExt[MAX_PATH];
	wchar_t *wszSearchPath = L"SRV**\\\\symbols\\symbols"; // Alternate path to search for debug data
	DWORD dwMachType = 0;

	HRESULT hr = CoInitialize(NULL);

	// Obtain access to the provider

	hr = CoCreateInstance(__uuidof(DiaSource),
		NULL,
		CLSCTX_INPROC_SERVER,
		__uuidof(IDiaDataSource),
		(void **)ppSource);

	if (FAILED(hr)) {
		wprintf(L"CoCreateInstance failed - HRESULT = %08X\n", hr);

		return false;
	}

	_wsplitpath_s(szFilename, NULL, 0, NULL, 0, NULL, 0, wszExt, MAX_PATH);

	if (!_wcsicmp(wszExt, L".pdb")) {
		// Open and prepare a program database (.pdb) file as a debug data source

		hr = (*ppSource)->loadDataFromPdb(szFilename);

		if (FAILED(hr)) {
			wprintf(L"loadDataFromPdb failed - HRESULT = %08X\n", hr);

			return false;
		}
	}

	else {
		CCallback callback; // Receives callbacks from the DIA symbol locating procedure,
							// thus enabling a user interface to report on the progress of
							// the location attempt. The client application may optionally
							// provide a reference to its own implementation of this
							// virtual base class to the IDiaDataSource::loadDataForExe method.
		callback.AddRef();

		// Open and prepare the debug data associated with the executable

		hr = (*ppSource)->loadDataForExe(szFilename, wszSearchPath, &callback);

		if (FAILED(hr)) {
			wprintf(L"loadDataForExe failed - HRESULT = %08X\n", hr);

			return false;
		}
	}

	// Open a session for querying symbols

	hr = (*ppSource)->openSession(ppSession);

	if (FAILED(hr)) {
		wprintf(L"openSession failed - HRESULT = %08X\n", hr);

		return false;
	}

	// Retrieve a reference to the global scope

	hr = (*ppSession)->get_globalScope(ppGlobal);

	if (hr != S_OK) {
		wprintf(L"get_globalScope failed\n");

		return false;
	}

	// Set Machine type for getting correct register names

	if ((*ppGlobal)->get_machineType(&dwMachType) == S_OK) {
		switch (dwMachType) {
		case IMAGE_FILE_MACHINE_I386: g_dwMachineType = CV_CFL_80386; break;
		case IMAGE_FILE_MACHINE_IA64: g_dwMachineType = CV_CFL_IA64; break;
		case IMAGE_FILE_MACHINE_AMD64: g_dwMachineType = CV_CFL_AMD64; break;
		}
	}

	return true;
}


//	Uses dia2 LoadAndValidateDataFromPdb func
bool LoadAndValidateDataFromPdb(
	const wchar_t    *szFilename,
	IDiaDataSource  **ppSource,
	IDiaSession     **ppSession,
	IDiaSymbol      **ppGlobal,
	matchInfo		*MI)
{
	wchar_t wszExt[MAX_PATH];
	wchar_t *wszSearchPath = L"SRV**\\\\symbols\\symbols"; // Alternate path to search for debug data
	DWORD dwMachType = 0;

	HRESULT hr = CoInitialize(NULL);

	// Obtain access to the provider

	hr = CoCreateInstance(__uuidof(DiaSource),
		NULL,
		CLSCTX_INPROC_SERVER,
		__uuidof(IDiaDataSource),
		(void **)ppSource);

	if (FAILED(hr)) {
		wprintf(L"CoCreateInstance failed - HRESULT = %08X\n", hr);

		return false;
	}

	_wsplitpath_s(szFilename, NULL, 0, NULL, 0, NULL, 0, wszExt, MAX_PATH);

	if (!_wcsicmp(wszExt, L".pdb")) {
		// Open and prepare a program database (.pdb) file as a debug data source

		hr = (*ppSource)->loadAndValidateDataFromPdb(szFilename, &(MI->GUID), MI->Signature, MI->Age);

		if (FAILED(hr)) {
			switch (hr)
			{
			case E_PDB_NOT_FOUND: printf("Failed to open the .pdb file, or the file has an invalid format.\n"); break;
			case E_PDB_FORMAT:    printf("Attempted to access a .pdb file with an obsolete format.\n"); break;

			case E_PDB_INVALID_AGE: printf("Failed to open the .pdb file, 'Age' does not match..\n"); break;
			case E_PDB_INVALID_SIG: printf("Failed to open the .pdb file, 'Signature' does not match.\n"); break;

			case E_INVALIDARG: printf("loadAndValidateDataFromPdb failed - Invalid parameter.\n"); break;
			case E_UNEXPECTED: printf("loadAndValidateDataFromPdb failed - The data source has already been prepared.\n"); break;

			default: wprintf(L"loadAndValidateDataFromPdb failed - HRESULT = %08X\n", hr);
			}

			return false;
		}
	}

	else {
		CCallback callback; // Receives callbacks from the DIA symbol locating procedure,
							// thus enabling a user interface to report on the progress of
							// the location attempt. The client application may optionally
							// provide a reference to its own implementation of this
							// virtual base class to the IDiaDataSource::loadDataForExe method.
		callback.AddRef();

		// Open and prepare the debug data associated with the executable

		hr = (*ppSource)->loadDataForExe(szFilename, wszSearchPath, &callback);

		if (FAILED(hr)) {
			wprintf(L"loadDataForExe failed - HRESULT = %08X\n", hr);

			return false;
		}
	}

	// Open a session for querying symbols

	hr = (*ppSource)->openSession(ppSession);

	if (FAILED(hr)) {
		wprintf(L"openSession failed - HRESULT = %08X\n", hr);

		return false;
	}

	// Retrieve a reference to the global scope

	hr = (*ppSession)->get_globalScope(ppGlobal);

	if (hr != S_OK) {
		wprintf(L"get_globalScope failed\n");

		return false;
	}

	// Set Machine type for getting correct register names

	if ((*ppGlobal)->get_machineType(&dwMachType) == S_OK) {
		switch (dwMachType) {
		case IMAGE_FILE_MACHINE_I386: g_dwMachineType = CV_CFL_80386; break;
		case IMAGE_FILE_MACHINE_IA64: g_dwMachineType = CV_CFL_IA64; break;
		case IMAGE_FILE_MACHINE_AMD64: g_dwMachineType = CV_CFL_AMD64; break;
		}
	}

	return true;
}

bool DumpAllRuntimeFuncsToDict(
	IDiaSymbol      *ppGlobal,
	std::unordered_map < ULONG_PTR, PCHAR > *dict
)
{
	if (!DumpAllPublicsToDict(ppGlobal, dict))
		InfoLogRet("DumpAllPublicsToDict failed", false);

	if (!DumpAllGlobalsToDict(ppGlobal, dict))
		InfoLogRet("DumpAllGlobalsToDict failed", false);

	return true;
}

bool DumpAllPublicsToDict(
	IDiaSymbol      *pGlobal,
	std::unordered_map < ULONG_PTR, PCHAR > *dict
)
{
	// Retrieve all the public symbols

	IDiaEnumSymbols *pEnumSymbols;

	if (FAILED(pGlobal->findChildren(SymTagPublicSymbol, NULL, nsNone, &pEnumSymbols))) {
		return false;
	}

	IDiaSymbol *pSymbol;
	ULONG celt = 0;

	while (SUCCEEDED(pEnumSymbols->Next(1, &pSymbol, &celt)) && (celt == 1)) {
		DumpPublicSymbolToDict(pSymbol, dict);

		pSymbol->Release();
	}

	pEnumSymbols->Release();

	return true;
}

bool DumpAllGlobalsToDict(
	IDiaSymbol      *pGlobal,
	std::unordered_map < ULONG_PTR, PCHAR > *dict
)
{
	IDiaEnumSymbols *pEnumSymbols;
	IDiaSymbol *pSymbol;
	enum SymTagEnum dwSymTags[] = { SymTagFunction };

	ULONG celt = 0;

	for (size_t i = 0; i < _countof(dwSymTags); i++, pEnumSymbols = NULL) {
		if (SUCCEEDED(pGlobal->findChildren(dwSymTags[i], NULL, nsNone, &pEnumSymbols))) {
			while (SUCCEEDED(pEnumSymbols->Next(1, &pSymbol, &celt)) && (celt == 1)) {
				DumpGlobalSymbolToDict(pSymbol, dict);

				pSymbol->Release();
			}

			pEnumSymbols->Release();
		}

		else {
			wprintf(L"ERROR - DumpAllGlobals() returned no symbols\n");

			return false;
		}
	}

	return true;
}


////////////////////////////////////////////////////////////
// Print a public symbol info: name, VA, RVA, SEG:OFF
//
void DumpPublicSymbolToDict(IDiaSymbol *pSymbol, std::unordered_map < ULONG_PTR, PCHAR > *dict)
{
	DWORD dwSymTag;
	DWORD dwRVA;
	DWORD dwSeg;
	DWORD dwOff;
	BSTR bstrName;

	if (pSymbol->get_symTag(&dwSymTag) != S_OK) {
		return;
	}

	if (pSymbol->get_relativeVirtualAddress(&dwRVA) != S_OK) {
		dwRVA = 0xFFFFFFFF;
	}

	pSymbol->get_addressSection(&dwSeg);
	pSymbol->get_addressOffset(&dwOff);

	ULONG_PTR key;
	PCHAR value;

	if (dwSymTag == SymTagThunk) {
		if (pSymbol->get_name(&bstrName) == S_OK) {
			key = dwRVA;
			value = wchar2pchar(bstrName);
			dict->insert({ key, value });

			SysFreeString(bstrName);
		}

		else {
			if (pSymbol->get_targetRelativeVirtualAddress(&dwRVA) != S_OK) {
				dwRVA = 0xFFFFFFFF;
			}

			pSymbol->get_targetSection(&dwSeg);
			pSymbol->get_targetOffset(&dwOff);

			key = dwRVA;
			char value[1024];
			sprintf_s(value, "target -> [%08X][%04X:%08X]\n", dwRVA, dwSeg, dwOff);
			dict->insert({ key, value });
		}
	}

	else {
		// must be a function or a data symbol

		BSTR bstrUndname;

		if (pSymbol->get_name(&bstrName) == S_OK) {
			if (pSymbol->get_undecoratedName(&bstrUndname) == S_OK) {
				key = dwRVA;
				value = wchar2pchar(bstrUndname);
				dict->insert({ key, value });

				SysFreeString(bstrUndname);
			}

			else {
				key = dwRVA;
				value = wchar2pchar(bstrName);
				dict->insert({ key, value });
			}

			SysFreeString(bstrName);
		}
	}
}

////////////////////////////////////////////////////////////
// Print a global symbol info: name, VA, RVA, SEG:OFF
//
void DumpGlobalSymbolToDict(IDiaSymbol *pSymbol, std::unordered_map < ULONG_PTR, PCHAR > *dict)
{
	DWORD dwSymTag;
	DWORD dwRVA;
	DWORD dwSeg;
	DWORD dwOff;

	ULONG_PTR key;
	PCHAR value;

	if (pSymbol->get_symTag(&dwSymTag) != S_OK) {
		return;
	}

	if (pSymbol->get_relativeVirtualAddress(&dwRVA) != S_OK) {
		dwRVA = 0xFFFFFFFF;
	}

	pSymbol->get_addressSection(&dwSeg);
	pSymbol->get_addressOffset(&dwOff);

	if (dwSymTag == SymTagThunk) {
		BSTR bstrName;

		if (pSymbol->get_name(&bstrName) == S_OK) {
			key = dwRVA;
			value = wchar2pchar(bstrName);
			dict->insert({ key, value });

			SysFreeString(bstrName);
		}

		else {
			if (pSymbol->get_targetRelativeVirtualAddress(&dwRVA) != S_OK) {
				dwRVA = 0xFFFFFFFF;
			}

			pSymbol->get_targetSection(&dwSeg);
			pSymbol->get_targetOffset(&dwOff);
			
			key = dwRVA;
			char value[1024];
			sprintf_s(value, "target -> [%08X][%04X:%08X]\n", dwRVA, dwSeg, dwOff);
			dict->insert({ key, value });
		}
	}

	else {
		BSTR bstrName;
		BSTR bstrUndname;

		if (pSymbol->get_name(&bstrName) == S_OK) {
			if (pSymbol->get_undecoratedName(&bstrUndname) == S_OK) {
				key = dwRVA;
				value = wchar2pchar(bstrUndname);
				dict->insert({ key, value });

				SysFreeString(bstrUndname);
			}

			else {
				key = dwRVA;
				value = wchar2pchar(bstrName);
				dict->insert({ key, value });
			}

			SysFreeString(bstrName);
		}
	}
}


void Cleanup(IDiaSymbol* g_pGlobalSymbol, IDiaSession* g_pDiaSession)
{
	if (g_pGlobalSymbol) {
		g_pGlobalSymbol->Release();
		g_pGlobalSymbol = NULL;
	}

	if (g_pDiaSession) {
		g_pDiaSession->Release();
		g_pDiaSession = NULL;
	}

	CoUninitialize();
}

PCHAR wchar2pchar(wchar_t *inpStr) {
	size_t inpSize = wcslen(inpStr) + 1;
	size_t convertedChars = 0;

	const size_t outSize = inpSize * 2;
	char *outStr = new char[outSize];
	
	wcstombs_s(&convertedChars, outStr, outSize, inpStr, _TRUNCATE);
	return outStr;
}