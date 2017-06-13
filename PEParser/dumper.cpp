#include "stdafx.h"
#include "dumper.h"
#include "dumpPDB.h"
#include "support.h"


matchInfo *getExpectedMatchInfoFromPE(struct PEInfo *peInfo);


Dumper::Dumper(_TCHAR *peFile, _TCHAR *pdbFile, char *outfile) {
	this->peFile = peFile;
	this->pdbFile = pdbFile;

	int err = fopen_s(&fstream, outfile, "w+");
	if (err) {
		InfoLog("Failed to create dumper: fopen error: %d", err);
		return;
	}

	map = new Mapper(peFile);
	ZeroMemory(&peInfo, sizeof(peInfo));

	if (!map->MapFile()) {
		InfoLog("Mapping file failed");
		return;
	}

	peInfo.pBase = (ULONG_PTR)map->GetMappingAddr();
	if (!parsePE()) {
		InfoLog("Error during PE parsing");
		return;
	}
}

Dumper::~Dumper() {
	map->~Mapper();
	fclose(fstream);
}

bool Dumper::dump() {
	if (!dumpSections()) 
		InfoLogRet("Failed dumpSections", false);

	if (!dumpExports()) 
		InfoLogRet("Failed dumpExports", false);

	if (!dumpImports())
		InfoLogRet("Failed dumpImports", false);

	if (!matchPDB())
		InfoLogRet("Failed matchPDB", false);

	if (!dumpRuntimeFunctions())
		InfoLogRet("Failed dumpRuntimeFunctions", false);

	return true;
}

//#################################################################################################
//
//										Section table dump
//
//#################################################################################################
bool Dumper::dumpSections() {
	peInfo.pSectionHeader = getPSectionHeader();

	const char head[] =
		"\n\n################################################################################\n"
		"#                                 Section Table                                 \n"
		"################################################################################\n";

	fprintf_s(fstream, head);

	for (int i = 0; i < peInfo.secNum; ++i) {
		dumpSection(peInfo.pSectionHeader + i);
	}

	InfoLog("Section table succesfully dumped!");
	return true;
}

void Dumper::dumpSection(PIMAGE_SECTION_HEADER pSectionHeader) {
	// Alignments
	int SFAL = 19, SFAR = 15;

	// Header
	char buf[1024];
	fprintf_s(fstream, "SECTION %s\tVirtSize: %08x\tVirtAddr: %08x\n",
		(pSectionHeader)->Name, (pSectionHeader)->Misc,
		(pSectionHeader)->VirtualAddress);

	// Fields
	fprintf_s(fstream, "\t%-*s%08x\t", SFAL, "raw data offset: ", (pSectionHeader)->PointerToRawData);
	fprintf_s(fstream, "%-*s%08x\n", SFAL, "raw data size: ", (pSectionHeader)->SizeOfRawData);
	fprintf_s(fstream, "\t%-*s%08x\t", SFAL, "relocation offset: ", (pSectionHeader)->PointerToRelocations);
	fprintf_s(fstream, "%-*s%08x\n", SFAL, "relocations: ", (pSectionHeader)->NumberOfRelocations);
	fprintf_s(fstream, "\t%-*s%08x\t", SFAL, "line # offset: ", (pSectionHeader)->PointerToLinenumbers);
	fprintf_s(fstream, "%-*s%08x\n", SFAL, "lines # : ", (pSectionHeader)->NumberOfLinenumbers);
	fprintf_s(fstream, "\t%-*s%08x\t\n\n", SFAL, "characteristics: ", (pSectionHeader)->Characteristics);
}


//#################################################################################################
//
//										Export table dump
//
//#################################################################################################
bool Dumper::dumpExports() {
	if (peInfo.exportRVA == NULL)
		InfoLogRet("There is no Export table", true);

	PIMAGE_EXPORT_DIRECTORY pExpDir = (PIMAGE_EXPORT_DIRECTORY)(peInfo.pBase + peInfo.exportRVA);

	PDWORD addr = (PDWORD)(peInfo.pBase + pExpDir->AddressOfFunctions);
	PDWORD name = (PDWORD)(peInfo.pBase + pExpDir->AddressOfNames);
	PWORD  ordn = (PWORD)(peInfo.pBase + pExpDir->AddressOfNameOrdinals);

	const char head[] =
		"\n\n################################################################################\n"
		"#                                  Export Table                                 \n"
		"################################################################################\n";
	char buf[1024];
	int ETFA = 17;
	fprintf_s(fstream, "%s%s: %s\n\n", head, "Name", (PCHAR)(pExpDir->Name + peInfo.pBase));

	//	First and second dump alignment
	int FA = 8, SA = 6;
	fprintf_s(fstream, "\t%-*s\t%-*s\t%s\n", FA, "Entry Pt", SA, "Ordn", "Name");

	for (int i = 0; i < pExpDir->NumberOfNames; ++i) {
		int idx = ordn[i] + pExpDir->Base;
		fprintf_s(fstream, "\t%-*08x\t%-*d\t%s\n", FA, addr[i] + peInfo.pBase,
			SA, idx, name[i] + (char *)peInfo.pBase);
	}

	InfoLog("Export table succesfully dumped!");
	return true;
}


//#################################################################################################
//
//										Import table dump
//
//#################################################################################################
bool Dumper::dumpImports() {
	if (peInfo.importRVA == NULL)
		InfoLogRet("There is no import table", true);

	PIMAGE_IMPORT_DESCRIPTOR pImpDir = (PIMAGE_IMPORT_DESCRIPTOR)(peInfo.pBase + peInfo.importRVA);

	const char head[] =
		"\n\n################################################################################\n"
		"#                                  Import Table                                 \n"
		"################################################################################\n";
	char buf[1024];
	int ETFA = 17;
	fprintf_s(fstream, head);

	while (pImpDir->Characteristics) {
		PCHAR name = (PCHAR)(pImpDir->Name + peInfo.pBase);
		DWORD chrc = pImpDir->Characteristics + peInfo.pBase;
		DWORD time = pImpDir->TimeDateStamp;
		DWORD fwdc = pImpDir->ForwarderChain;
		DWORD fthk = pImpDir->FirstThunk;
		fprintf_s(fstream, "\n\t%s\n\t  Hint/Name Table: %08x\n\t  TimeDataStamp: %08x\n"
			"\t  ForwarderChain: %08x\n\t  First thunk RVA: %08x\n",
			name, chrc, time, fwdc, fthk);

		PVOID pThunk = (PVOID)(pImpDir->OriginalFirstThunk + peInfo.pBase);
		if (!pImpDir->OriginalFirstThunk)
			pThunk = (PVOID)(pImpDir->FirstThunk + peInfo.pBase);

		if (!dumpImportedFuncs(pThunk))
			return false;

		pImpDir++;
	}

	InfoLog("Import table succesfully dumped!");
	return true;
}

bool Dumper::dumpImportedFuncs(PVOID pThunk) {
	WORD machine = peInfo.pPEHeader->FileHeader.Machine;

	if (machine == IMAGE_FILE_MACHINE_AMD64)
		return dumpImportedFuncsX64((PIMAGE_THUNK_DATA64)pThunk);
	else
		return dumpImportedFuncsX86((PIMAGE_THUNK_DATA32)pThunk);
}

bool Dumper::dumpImportedFuncsX86(PIMAGE_THUNK_DATA32 pThunk) {
	char buf[1024];
	int ALGN = 6;
	fprintf_s(fstream, "\t  %-*s%s\n", ALGN, "ordn", "name");

	while (pThunk->u1.AddressOfData) {
		if (pThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG32)
			fprintf_s(fstream, "\t  %lld\n", pThunk->u1.Ordinal & 0xFFFF);
		else {
			DWORD ordn = ((PIMAGE_IMPORT_BY_NAME)(pThunk->u1.AddressOfData + peInfo.pBase))->Hint;
			PCHAR name = ((PIMAGE_IMPORT_BY_NAME)(pThunk->u1.AddressOfData + peInfo.pBase))->Name;
			fprintf_s(fstream, "\t  %-*d%s\n", ALGN, ordn, name);
		}

		pThunk++;
	}

	return true;
}

bool Dumper::dumpImportedFuncsX64(PIMAGE_THUNK_DATA64 pThunk) {
	char buf[1024];
	int ALGN = 6;
	fprintf_s(fstream, "\t  %-*s%s\n", ALGN, "ordn", "name");

	while (pThunk->u1.AddressOfData) {
		if (pThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG64)
			fprintf_s(fstream, "\t  %lld\n", pThunk->u1.Ordinal & 0xFFFF);
		else {
			DWORD ordn = ((PIMAGE_IMPORT_BY_NAME)(pThunk->u1.AddressOfData + peInfo.pBase))->Hint;
			PCHAR name = ((PIMAGE_IMPORT_BY_NAME)(pThunk->u1.AddressOfData + peInfo.pBase))->Name;
			fprintf_s(fstream, "\t  %-*d%s\n", ALGN, ordn, name);
		}

		pThunk++;
	}

	return true;
}

//#################################################################################################
//
//										   PDB matching
//
//#################################################################################################
bool Dumper::matchPDB() {
	matchInfo *expectedMatchInfo = getExpectedMatchInfoFromPE(&peInfo);
	if (!expectedMatchInfo)
		InfoLogRet("getExpectedMatchInfoFromPE failed\n", true);

	if (!pdbFile && !(expectedMatchInfo->PdbFileName))
		InfoLogRet("No user PDB file and no matching PDB file in PE file", true);

	if (!pdbFile && !!(expectedMatchInfo->PdbFileName)) {
		pdbFile = (_TCHAR *)expectedMatchInfo->PdbFileName;
		InfoLog("No user PDF file but there is PE pdb file: %s. Trying to match it\n", pdbFile);
	}

	wchar_t *g_szFilename = pdbFile;
	IDiaDataSource *g_pDiaDataSource = NULL;
	IDiaSession *g_pDiaSession = NULL;
	IDiaSymbol *g_pGlobalSymbol = NULL;

	if (!LoadAndValidateDataFromPdb(
		g_szFilename,
		&g_pDiaDataSource,
		&g_pDiaSession,
		&g_pGlobalSymbol,
		expectedMatchInfo)) {
		Cleanup(g_pGlobalSymbol, g_pDiaSession);
		InfoLogRet("PDB matching failed", true);
	}

	InfoLog("PDB succesfully matched!");
	Cleanup(g_pGlobalSymbol, g_pDiaSession);
	return true;
}

matchInfo *getExpectedMatchInfoFromPE(struct PEInfo *peInfo) {
	if (peInfo->debugRVA == NULL)
		InfoLogRet("There is no debug directory", NULL);

	PIMAGE_DEBUG_DIRECTORY pDebugSection = (PIMAGE_DEBUG_DIRECTORY)(peInfo->pBase + peInfo->debugRVA);
	PIMAGE_DEBUG_DIRECTORY pDebugDir = pDebugSection;

	matchInfo *MI = (matchInfo *)calloc(1, sizeof(matchInfo));
	MI->Age = 0;
	MI->GUID = GUID_NULL;
	MI->Signature = 0;
	MI->PdbFileName = NULL;
	
	size_t DebugSectionSize = peInfo->pPEHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].Size;
	size_t DebugDirsNum = DebugSectionSize / sizeof(IMAGE_DEBUG_DIRECTORY);
	if (sizeof(IMAGE_DEBUG_DIRECTORY) * DebugDirsNum != DebugSectionSize)
		InfoLogRet("Debug section size isn't divisible to IMAGE_DEBUD_DIRECTORY size", NULL);

	for (int i = 0; i < DebugDirsNum; ++i) {
		if (pDebugDir->Type == IMAGE_DEBUG_TYPE_CODEVIEW)
			break;
		else if (i == DebugDirsNum - 1) {
			pDebugDir = NULL;
			break;
		}

		pDebugDir++;
	}

	if (pDebugDir == NULL)
		InfoLogRet("Codeview debug directory hasn't been found", NULL);

	ULONG_PTR cvInfo = (ULONG_PTR)(peInfo->pBase + pDebugDir->AddressOfRawData);

	// CV signature should be "NB10", but we check only first byte
	if ((((PCV_INFO_PDB20)cvInfo)->CvHeader.CvSignature & 0xFF) == 'N') {
		PCV_INFO_PDB20 cvInfo20 = (PCV_INFO_PDB20)cvInfo;
		MI->Age = cvInfo20->Age;
		MI->Signature = cvInfo20->Signature;
		MI->PdbFileName = cvInfo20->PdbFileName;

		InfoLog("PE expects the PDB 2.0 file with the following matching information:\n");
		InfoLog("\tAge: %d\n", MI->Age);
		InfoLog("\tSignature: %d\n", MI->Signature);
		InfoLog("\tPDB filename: %s\n", (char *)MI->PdbFileName);

		// CV signature should be "RSDS", but we check only first byte
	}
	else if ((((PCV_INFO_PDB70)cvInfo)->CvSignature & 0xFF) == 'R') {
		PCV_INFO_PDB70 cvInfo70 = (PCV_INFO_PDB70)cvInfo;
		MI->Age = cvInfo70->Age;
		MI->GUID = cvInfo70->Signature;
		MI->PdbFileName = cvInfo70->PdbFileName;

		InfoLog("PE expects the PDB 7.0 file with the following matching information:");
		InfoLog("\tAge: %d", MI->Age);
		InfoLog("\tGUID Signature: %08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
			MI->GUID.Data1, MI->GUID.Data2, MI->GUID.Data3,
			MI->GUID.Data4[0], MI->GUID.Data4[1], MI->GUID.Data4[2], MI->GUID.Data4[3],
			MI->GUID.Data4[4], MI->GUID.Data4[5], MI->GUID.Data4[6], MI->GUID.Data4[7]);

	}
	else
		InfoLogRet("Unknown format of Codeview debug directory\n", NULL);

	return MI;
}

//#################################################################################################
//
//										Runtime functions dump
//
//#################################################################################################
bool Dumper::dumpRuntimeFunctions() {
	if (!peInfo.exceptionRVA)
		InfoLogRet("There is no .pdata section", false);

	PIMAGE_IA64_RUNTIME_FUNCTION_ENTRY pPDataSection = (PIMAGE_IA64_RUNTIME_FUNCTION_ENTRY)(peInfo.pBase + peInfo.exceptionRVA);
	PIMAGE_IA64_RUNTIME_FUNCTION_ENTRY pRuntimeFuncEntry = pPDataSection;

	size_t pdataSectionSize = getPDataDirectory()[IMAGE_DIRECTORY_ENTRY_EXCEPTION].Size;
	size_t RuntimeFuncsNum = pdataSectionSize / sizeof(IMAGE_IA64_RUNTIME_FUNCTION_ENTRY);
	if (sizeof(IMAGE_IA64_RUNTIME_FUNCTION_ENTRY) * RuntimeFuncsNum != pdataSectionSize)
		InfoLogRet("pdata section size isn't divisible to IMAGE_IA64_RUNTIME_FUNCTION_ENTRY size", NULL);

	if (!parseRuntimeFuncs())
		InfoLog("PDB runtime functions parsing failed. Trying to get information only from PE");

	const char head[] =
	"\n\n################################################################################\n"
		"#                             Runtime functions                                 \n"
		"################################################################################\n";
	fprintf_s(fstream, head);
	
	for (int i = 0; i < RuntimeFuncsNum; ++i) {
		if (!dumpRuntimeFunction(pRuntimeFuncEntry, i + 1))
			return false;

		pRuntimeFuncEntry++;
	}

	InfoLog("Runtime function succesfully dumped!");
	return true;
}

bool Dumper::dumpRuntimeFunction(PIMAGE_IA64_RUNTIME_FUNCTION_ENTRY pRuntimeFuncEntry, int idx) {
	std::unordered_map <ULONG_PTR, PCHAR>::const_iterator 
		iter = runtimeFuncsDict.find((ULONG_PTR)(pRuntimeFuncEntry->BeginAddress));

	PCHAR name;
	if (iter != runtimeFuncsDict.end())
		name = iter->second;
	else
		name = "";

	char buf[1024];
	fprintf_s(fstream, "\t#%zd: %s\n\t\tBegin address: %p\n\t\tEnd address: %p\n", idx, name,
		(ULONG_PTR)(pRuntimeFuncEntry->BeginAddress), (ULONG_PTR)(pRuntimeFuncEntry->EndAddress));

	if (((pRuntimeFuncEntry->UnwindInfoAddress) & 1) == 0) {
		fprintf_s(fstream, "\t\tUnwind info address: %p\n", pRuntimeFuncEntry->UnwindInfoAddress);

		PUNWIND_INFO uInfo = (PUNWIND_INFO)(pRuntimeFuncEntry->UnwindInfoAddress + peInfo.pBase);
		if (!dumpUnwindInfo(uInfo))
			InfoLogRet("Runtime func dump failed: dumpUnwindInfo error", false);

	} else {
		fprintf_s(fstream, "\t\tUnwind data: %p\n\n", pRuntimeFuncEntry->UnwindData);
	}


	return true;
}


bool Dumper::dumpUnwindInfo(PUNWIND_INFO uInfo) {
	fprintf_s(fstream, "\t\tVersion: %d\n", uInfo->Version);

	if (uInfo->Version == 1)
		return dumpUnwindInfoV1(uInfo);
	else
		return dumpUnwindInfoV2(uInfo);
}

bool Dumper::dumpUnwindInfoV1(PUNWIND_INFO uInfo) {
	fprintf_s(fstream, "\t\tFlags: %s %s %s\n",
		uInfo->Flags & UNW_FLAG_EHANDLER ? "UNW_FLAG_EHANDLER" : "",
		uInfo->Flags & UNW_FLAG_UHANDLER ? "UNW_FLAG_UHANDLER" : "",
		uInfo->Flags & UNW_FLAG_CHAININFO ? "UNW_FLAG_CHAININFO" : "");

	PVariable pUInfoVar = (PVariable)(&(uInfo->UnwindCode[(uInfo->CountOfCodes + 1) & ~1]));
	if (uInfo->Flags & UNW_FLAG_EHANDLER) {
		fprintf_s(fstream, "\t\tAddress of exception handler: %p\n",
			(ULONG_PTR)(pUInfoVar->ExceptionHandlerInfo.pExceptionHandler));

		std::unordered_map <ULONG_PTR, PCHAR>::const_iterator
			it = runtimeFuncsDict.find((ULONG_PTR)(pUInfoVar->ExceptionHandlerInfo.pExceptionHandler));
		if (it != runtimeFuncsDict.end())
			fprintf_s(fstream, "\t\tName of exception handler: %s\n", it->second);
	}

	if (uInfo->Flags & UNW_FLAG_UHANDLER) {
		fprintf_s(fstream, "\t\tAddress of termination handler: %p\n",
			(ULONG_PTR)(pUInfoVar->ExceptionHandlerInfo.pExceptionHandler));

		std::unordered_map <ULONG_PTR, PCHAR>::const_iterator
			it = runtimeFuncsDict.find((ULONG_PTR)(pUInfoVar->ExceptionHandlerInfo.pExceptionHandler));
		if (it != runtimeFuncsDict.end())
			fprintf_s(fstream, "\t\tName of termination handler: %s\n", it->second);
	}

	fprintf_s(fstream, "\t\tProlog size: %d\n", uInfo->SizeOfProlog);
	fprintf_s(fstream, "\t\tUnwind codes count: %d\n", uInfo->CountOfCodes);
	fprintf_s(fstream, "\t\tFrame register: %d\n", uInfo->FrameRegister);
	fprintf_s(fstream, "\t\tFrame offset: %d\n", uInfo->FrameOffset);

	for (int i = 0; i < uInfo->CountOfCodes; ++i) {
		fprintf(fstream, "\t\tUnwind code %d:\n", i);

		if (!dumpUnwindCode(uInfo->UnwindCode[i]))
			return false;
	}

	return true;
}

bool Dumper::dumpUnwindInfoV2(PUNWIND_INFO uInfo) {
	// This structure of UnwindInfo is not documented on MSDN
	// https://www.aladdin-rd.ru/company/pressroom/articles/45402/

	return true;
}

bool Dumper::dumpUnwindCode(UNWIND_CODE uCode) {
	fprintf_s(fstream, "\t\t\tPrologue offset: %d\n", uCode.Prologue.CodeOffset);

	BYTE opcode = uCode.Prologue.CodeOffset;
	fprintf_s(fstream, "\t\t\tOpcode: ");
	switch (opcode) {
	case UWOP_PUSH_NONVOL: fprintf(fstream, "UWOP_PUSH_NONVOL(%d)\n", opcode); break;
	case UWOP_ALLOC_LARGE: fprintf(fstream, "UWOP_ALLOC_LARGE(%d)\n", opcode); break;
	case UWOP_ALLOC_SMALL: fprintf(fstream, "UWOP_ALLOC_SMALL(%d)\n", opcode); break;
	case UWOP_SET_FPREG: fprintf(fstream, "UWOP_SET_FPREG(%d)\n", opcode); break;
	case UWOP_SAVE_NONVOL: fprintf(fstream, "UWOP_SAVE_NONVOL(%d)\n", opcode); break;
	case UWOP_SAVE_NONVOL_FAR: fprintf(fstream, "UWOP_SAVE_NONVOL_FAR(%d)\n", opcode); break;
	case UWOP_SAVE_XMM128: fprintf(fstream, "UWOP_SAVE_XMM128(%d)\n", opcode); break;
	case UWOP_SAVE_XMM128_FAR: fprintf(fstream, "UWOP_SAVE_XMM128_FAR(%d)\n", opcode); break;
	case UWOP_PUSH_MACHFRAME: fprintf(fstream, "UWOP_PUSH_MACHFRAME(%d)\n", opcode); break;

	default: fprintf(fstream, "Unknown opcode(%d)\n", opcode);
	}

	fprintf(fstream, "\t\t\tOperation info: %d\n", uCode.Prologue.OpInfo);

	return true;
}

bool Dumper::parseRuntimeFuncs() {
	wchar_t *g_szFilename = pdbFile;
	IDiaDataSource *g_pDiaDataSource = NULL;
	IDiaSession *g_pDiaSession = NULL;
	IDiaSymbol *g_pGlobalSymbol = NULL;

	if (!LoadDataFromPdb(g_szFilename, &g_pDiaDataSource, &g_pDiaSession, &g_pGlobalSymbol))
		InfoLogRet("LoadDataFromPdb failed", false);

	if (!DumpAllRuntimeFuncsToDict(g_pGlobalSymbol, &runtimeFuncsDict))
		InfoLogRet("DumpAllRuntimeFuncsToDict failed", false);

	return true;
}

//#################################################################################################
//
//											PE parsing
//
//#################################################################################################
bool Dumper::parsePE() {
	peInfo.pPEHeader = getPPEHeader();
	if (!peInfo.pPEHeader)
		InfoLogRet("PE parsing failed: getting pPEHeader", false);

	peInfo.secNum = peInfo.pPEHeader->FileHeader.NumberOfSections;
	peInfo.secAlign = peInfo.pPEHeader->OptionalHeader.SectionAlignment;
	peInfo.pSectionHeader = getPSectionHeader();
	if (!peInfo.pSectionHeader)
		InfoLogRet("PE parsing failed: getting pSectionHeader", false);

	peInfo.exportRVA = getExportRVA();
	peInfo.importRVA = getImportRVA();
	peInfo.debugRVA = getDebugRVA();
	peInfo.exceptionRVA = getExceptionRVA();

	return true;
}

ULONG_PTR Dumper::getExceptionRVA() {
	PIMAGE_DATA_DIRECTORY pDD = getPDataDirectory();
	return pDD[IMAGE_DIRECTORY_ENTRY_EXCEPTION].VirtualAddress;
}

ULONG_PTR Dumper::getDebugRVA() {
	PIMAGE_DATA_DIRECTORY pDD = getPDataDirectory();
	return pDD[IMAGE_DIRECTORY_ENTRY_DEBUG].VirtualAddress;
}

ULONG_PTR Dumper::getImportRVA() {
	PIMAGE_DATA_DIRECTORY pDD = getPDataDirectory();
	return pDD[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
}

ULONG_PTR Dumper::getExportRVA() {
	PIMAGE_DATA_DIRECTORY pDD = getPDataDirectory();
	return pDD[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
}

PIMAGE_DATA_DIRECTORY Dumper::getPDataDirectory() {
	PIMAGE_DATA_DIRECTORY pDD;
	WORD machine = peInfo.pPEHeader->FileHeader.Machine;

	if (machine == IMAGE_FILE_MACHINE_AMD64) {
		IMAGE_OPTIONAL_HEADER64 optHeader;
		memcpy(&optHeader, &(peInfo.pPEHeader->OptionalHeader), sizeof(optHeader));
		pDD = optHeader.DataDirectory;
	} else {
		IMAGE_OPTIONAL_HEADER32 optHeader;
		memcpy(&optHeader, &(peInfo.pPEHeader->OptionalHeader), sizeof(optHeader));
		pDD = optHeader.DataDirectory;
	}

	return pDD;
}

PIMAGE_SECTION_HEADER Dumper::getPSectionHeader() {
	PIMAGE_NT_HEADERS pPEHeader = peInfo.pPEHeader;

	return (PIMAGE_SECTION_HEADER)(((ULONG_PTR)pPEHeader) +
									sizeof(pPEHeader->Signature) + 
									sizeof(IMAGE_FILE_HEADER) + 
									pPEHeader->FileHeader.SizeOfOptionalHeader);
}

PIMAGE_NT_HEADERS Dumper::getPPEHeader() {
	PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)peInfo.pBase;
	if (!checkMZ(pDosHeader)) 
		InfoLogRet("getPPEHeader failed: MZ check error", NULL);
		
	PIMAGE_NT_HEADERS pPEHeader = (PIMAGE_NT_HEADERS)(peInfo.pBase + pDosHeader->e_lfanew);
	_tprintf(_T("PE HEADER: signature=%c%c%x%x machine=%s \n"),
		pPEHeader->Signature & 0xFF,
		(pPEHeader->Signature >> 8) & 0xFF,
		(pPEHeader->Signature >> 16) & 0xFF,
		(pPEHeader->Signature >> 24) & 0xFF,
		pPEHeader->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64 ? _T("x64") :
		pPEHeader->FileHeader.Machine == IMAGE_FILE_MACHINE_I386  ? _T("x86") :
																	_T("UNKNOWN"));

	if (!checkPESignature(pPEHeader->Signature)) 
		InfoLog("getPPEHeader failed: PE Signature check error", NULL);

	return pPEHeader;
}

bool Dumper::checkPESignature(DWORD signature) {
	char firstSymbol = signature & 0xFF,
		secondSymbol = (signature >> 8) & 0xFF,
		 thirdSymbol = (signature >> 16) & 0xFF,
		fourthSymbol = (signature >> 24) & 0xFF;

	if (firstSymbol  != 'P' || 
		secondSymbol != 'E' ||
		thirdSymbol  != 0   || 
		fourthSymbol != 0)
		return false;

	return true;
}

bool Dumper::checkMZ(PIMAGE_DOS_HEADER pDosHeader) {
	char firstSymbol = pDosHeader->e_magic & 0xFF,
		secondSymbol = (pDosHeader->e_magic >> 8) & 0xFF;

	_tprintf(_T("DOS HEADER: %c%c 0x%x\n"),
			firstSymbol, secondSymbol, pDosHeader->e_lfanew);
	
	if (firstSymbol != 'M' || secondSymbol != 'Z') 
		return false;
	
	return true;
}

int Dumper::defSection(ULONG_PTR rva) {
	for (int i = 0; i < peInfo.secNum; ++i) {
		IMAGE_SECTION_HEADER currSection = *(peInfo.pSectionHeader + i);
		ULONG_PTR start = (ULONG_PTR)currSection.VirtualAddress;
		//ALIGN_UP(currSection.Misc.VirtualSize, peInfo.secAlign);
		ULONG_PTR end = start + (ULONG_PTR)currSection.Misc.VirtualSize;

		if (rva >= start && rva < end)
			return i;
	}

	return -1;
}

ULONG_PTR Dumper::rvaToOffset(ULONG_PTR rva) {
	int idxSection = defSection(rva);
	if (idxSection == -1)
		return 0;

	IMAGE_SECTION_HEADER currSection = *(peInfo.pSectionHeader + idxSection);
	return rva - currSection.VirtualAddress + currSection.PointerToRawData + peInfo.pBase;
}
