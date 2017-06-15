#include "stdafx.h"
#include "hook.h"
#include "import.h"

#include <CommCtrl.h>
#include <stdlib.h>


bool hookFuncByName(PCHAR name, ULONG_PTR newFunc) {
	ULONG_PTR pBase = get_pBase();
	InfoLog("pBase: %p\n", pBase);
	if (!checkMZ(pBase))
		return false;

	ULONG_PTR importRVA = getImportRVA(pBase);
	if (importRVA == NULL)
		InfoLogRet(false, "importRVA is NULL");

	PIMAGE_IMPORT_DESCRIPTOR pImpDir = (PIMAGE_IMPORT_DESCRIPTOR)(pBase + importRVA);
	while (pImpDir->Characteristics) {
		PVOID pOrigThunk = (PVOID)(pImpDir->OriginalFirstThunk + pBase);
		if (!pImpDir->OriginalFirstThunk)
			InfoLog("Original first thunk is NULL");

		PVOID pThunk = (PVOID)(pImpDir->FirstThunk + pBase);
		if (!pThunk)
			InfoLog("pThunk in NULL");

		ULONG_PTR pHookedFunc = changeFuncAddrByName(name, newFunc, pOrigThunk, pThunk, pBase);
		if (pHookedFunc != NULL) {
			InfoLog("Original address of hooked func: %p\n", pHookedFunc);
			break;
		}

		pImpDir++;
	}

	return true;
}


bool get_hListView(HWND hWnd, HWND *p_hListView, bool *is_hListView_got) {
	WCHAR buf[TITLE_SIZE];
	if (!GetWindowText(hWnd, buf, sizeof(buf)))
		InfoLogRet(false, "GetWindowText failed in get_hListView");

	if (!wcscmp(buf, PROCESSES)) {
		InfoLog("Processes table is found!");
		(*is_hListView_got) = true;
		if (!EnumChildWindows(hWnd, EnumChildProc, (LPARAM)p_hListView))
			InfoLog("EnumChildWindows failed in get_hListView");
	}

	return true;
}


BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM lParam) {
	WCHAR buf[TITLE_SIZE];
	if (!GetWindowText(hwnd, buf, sizeof(buf)))
		InfoLogRet(false, "GetWindowText failed in EnumChilProc");

	if (!wcscmp(buf, PROCESSES)) {
		InfoLog("Processes ListView is found!");
		memcpy((PVOID)lParam, &hwnd, sizeof(hwnd));
		return false;
	}

	return true;
}


bool addAffinityColumn(HWND hWnd, int idx) {
	LVCOLUMN lvC;

	lvC.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	lvC.iSubItem = 7;
	lvC.pszText = L"Affinity";
	lvC.cx = 100;
	lvC.fmt = LVCFMT_LEFT;

	return ListView_InsertColumn(hWnd, idx, &lvC);
}


bool drawAffinityByPID(HWND hWnd, int pidColNum) {
	if (pidColNum == -1)
		InfoLogRet(false, "pidColNum is -1 in drawAffinityByPID");

	int itemCount = ListView_GetItemCount(hWnd);

	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	int cpuNum = sysinfo.dwNumberOfProcessors;

	WCHAR buf[BUFFSIZE] = {};
	WCHAR affBuf[MAX_CPU_NUM] = {};

	for (int iItem = 0; iItem < itemCount; ++iItem) {
		ListView_GetItemText(hWnd, iItem, pidColNum, buf, sizeof(buf));

		int pid = _wtoi(buf);

		if (!getAffinity(pid, affBuf, cpuNum))
			InfoLogRet(false, "getAffinity failed in drawAffinityByPID");

		int affinityCol = getListViewNumOfCols(hWnd) - 1;  // -1 because of affinity column itself
		ListView_SetItemText(hWnd, iItem, affinityCol, affBuf);
	}

	return true;
}


bool getAffinity(int pid, PWCHAR affBuf, int cpuNum) {
	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);

	DWORD_PTR processAffinityMask = 0, systemAffinityMask = 0;
	GetProcessAffinityMask(hProcess, &processAffinityMask, &systemAffinityMask);

	for (int i = 0; i < cpuNum; ++i) {
		int curr_bit = processAffinityMask & 0x1;
		affBuf[i] = curr_bit != 0 ? '+' : '-';
		processAffinityMask = processAffinityMask >> 1;
	}

	affBuf[cpuNum] = '\0';
	return true;
}


int getListViewNumOfCols(HWND hWnd) {
	HWND hWndHdr = ListView_GetHeader(hWnd);
	if (!hWndHdr)
		InfoLog("ListView_GetHeader returned NULL in getListViewNumOfCols");

	int numOfCols = Header_GetItemCount(hWndHdr);
	if (numOfCols == -1)
		InfoLog("Header_getItemCount failed in getListViewNumOfCols");

	return numOfCols;
}