#include "stdafx.h"
#include "import.h"
#include "peb.h"


ULONG_PTR changeFuncAddrByName(PCHAR name, ULONG_PTR newFunc, PVOID pOrigThunk, PVOID pThunk, ULONG_PTR pBase) {
	PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)pBase;
	PIMAGE_NT_HEADERS pPEHeader = (PIMAGE_NT_HEADERS)(pBase + pDosHeader->e_lfanew);
	WORD machine = pPEHeader->FileHeader.Machine;

	if (machine == IMAGE_FILE_MACHINE_AMD64)
		return changeFuncAddrByNameX64(name, newFunc, (PIMAGE_THUNK_DATA64)pOrigThunk, (PIMAGE_THUNK_DATA64)pThunk, pBase);
	else
		return changeFuncAddrByNameX86(name, newFunc, (PIMAGE_THUNK_DATA32)pOrigThunk, (PIMAGE_THUNK_DATA32)pThunk, pBase);

}


ULONG_PTR changeFuncAddrByNameX86(PCHAR name, ULONG_PTR newFunc, PIMAGE_THUNK_DATA32 pOrigThunk, PIMAGE_THUNK_DATA32 pThunk, ULONG_PTR pBase) {
	while (pOrigThunk->u1.AddressOfData) {
		if (pOrigThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG64)
			InfoLogRet(0, "Ordinal higher bit is set!");

		if (pThunk == NULL)
			InfoLogRet(NULL, "pThunk is NULL");

		if (pOrigThunk == NULL)
			InfoLogRet(NULL, "pOrigThunk is NULL");

		PCHAR curr_name = ((PIMAGE_IMPORT_BY_NAME)(pOrigThunk->u1.AddressOfData + pBase))->Name;
		if (!strcmp(curr_name, name)) {
			ULONG_PTR originalAddr = (ULONG_PTR)(pThunk->u1.Function);
			InfoLog("\n\n### The func is founded: %s | %p ###\n\n", curr_name, originalAddr);
			InfoLog("Trying to hook it!");

			DWORD oldProtect;
			VirtualProtect(&(pThunk->u1.Function), sizeof(newFunc), PAGE_EXECUTE_READWRITE, &oldProtect);
			LPVOID dst = memcpy(&(pThunk->u1.Function), &newFunc, sizeof(newFunc));
			VirtualProtect(&(pThunk->u1.Function), sizeof(newFunc), oldProtect, &oldProtect);

			return originalAddr;
		}

		pThunk++;
		pOrigThunk++;
	}

	return NULL;
}


ULONG_PTR changeFuncAddrByNameX64(PCHAR name, ULONG_PTR newFunc, PIMAGE_THUNK_DATA64 pOrigThunk, PIMAGE_THUNK_DATA64 pThunk, ULONG_PTR pBase) {
	while (pOrigThunk->u1.AddressOfData) {
		if (pOrigThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG64)
			InfoLogRet(0, "Ordinal higher bit is set!");

		if (pThunk == NULL)
			InfoLogRet(NULL, "pThunk is NULL");

		if (pOrigThunk == NULL)
			InfoLogRet(NULL, "pOrigThunk is NULL");

		PCHAR curr_name = ((PIMAGE_IMPORT_BY_NAME)(pOrigThunk->u1.AddressOfData + pBase))->Name;
		if (!strcmp(curr_name, name)) {
			ULONG_PTR originalAddr = (ULONG_PTR)(pThunk->u1.Function);
			InfoLog("\n\n### The func is founded: %s | %p ###\n\n", curr_name, originalAddr);
			InfoLog("Trying to hook it!");

			DWORD oldProtect;
			VirtualProtect(&(pThunk->u1.Function), sizeof(newFunc), PAGE_EXECUTE_READWRITE, &oldProtect);
			LPVOID dst = memcpy(&(pThunk->u1.Function), &newFunc, sizeof(newFunc));
			VirtualProtect(&(pThunk->u1.Function), sizeof(newFunc), oldProtect, &oldProtect);

			return originalAddr;
		}

		pThunk++;
		pOrigThunk++;
	}

	return NULL;
}


ULONG_PTR getImportRVA(ULONG_PTR pBase) {
	PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)pBase;
	PIMAGE_NT_HEADERS pPEHeader = (PIMAGE_NT_HEADERS)(pBase + pDosHeader->e_lfanew);
	PIMAGE_DATA_DIRECTORY pDD = getPDataDirectory(pPEHeader);
	return pDD[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
}

PIMAGE_DATA_DIRECTORY getPDataDirectory(PIMAGE_NT_HEADERS pPEHeader) {
	PIMAGE_DATA_DIRECTORY pDD;
	WORD machine = pPEHeader->FileHeader.Machine;

	if (machine == IMAGE_FILE_MACHINE_AMD64) {
		IMAGE_OPTIONAL_HEADER64 optHeader;
		memcpy(&optHeader, &(pPEHeader->OptionalHeader), sizeof(optHeader));
		pDD = optHeader.DataDirectory;
	}
	else {
		IMAGE_OPTIONAL_HEADER32 optHeader;
		memcpy(&optHeader, &(pPEHeader->OptionalHeader), sizeof(optHeader));
		pDD = optHeader.DataDirectory;
	}

	return pDD;
}


#if defined _M_IX86
ULONG_PTR get_pBase() {
	DWORD offset = 0x30;
	PPEB32 offPeb = __readfsword(offset);
	return pPeb->ImageBaseAddress;

}

#elif defined _M_X64
ULONG_PTR get_pBase() {
	DWORD offset = 0x60;
	PPEB64 pPeb = (PPEB64)__readgsqword(offset);
	return pPeb->ImageBaseAddress;
}

#else
ULONG_PTR get_pBase() {
	fprintf(dumpfile, "Error getting PEB: undefined architecture\n");
	exit(EXIT_FAILURE);
}

#endif

bool checkMZ(ULONG_PTR pBase) {
	PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)pBase;
	char firstSymbol = pDosHeader->e_magic & 0xFF,
		secondSymbol = (pDosHeader->e_magic >> 8) & 0xFF;

	if (firstSymbol == 'M' && secondSymbol == 'Z')
		InfoLogRet(true, "MZ signature succesfully checked!");

	InfoLogRet(false, "Error during MZ signature checking");
}
