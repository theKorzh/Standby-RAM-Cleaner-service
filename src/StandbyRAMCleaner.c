/*
 * Standby RAM Cleaner Service
 * ---------------------------
 * Monitors free memory and purges the Standby List on Windows.
 *
 * Copyright (C) 2025 theKorzh
 *
 * This file is part of the Standby RAM Cleaner Service project.
 *
 * The program is licensed under the GNU General Public License version 3 (GPLv3).
 * You may use, copy, modify, and distribute this file under the terms of GPLv3:
 *   - https://www.gnu.org/licenses/gpl-3.0.html
 *
 * THIS SOFTWARE IS PROVIDED "AS IS", WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GPLv3
 * license for more details.
 */

#include <windows.h>
#include <winreg.h>
#include <tchar.h>
#include <stdio.h>
#include <winternl.h>
#include <strsafe.h>

#pragma comment(lib, "advapi32.lib")

#define REG_PATH TEXT("SOFTWARE\\MemoryCleaner")
#define REG_MINFREE TEXT("MinFreeMB")
#define REG_INTERVAL TEXT("CheckIntervalSec")
#define DEFAULT_MINFREE_MB 2048
#define DEFAULT_INTERVAL_SEC 10

#define SERVICE_NAME TEXT("StandbyRAMCleanerService")
#define SERVICE_DISPLAY_NAME TEXT("Standby RAM Cleaner Service")
#define SERVICE_DESCRIPTION TEXT("Monitors memory and purges Standby List when free memory is low.")

#define SystemPerformanceInformation 2
#define SystemMemoryListInformation 0x50
#define MemoryPurgeStandbyList 4

typedef struct _SYSTEM_PERFORMANCE_INFORMATION {
    LARGE_INTEGER IdleTime;
    LARGE_INTEGER ReadTransferCount;
    LARGE_INTEGER WriteTransferCount;
    LARGE_INTEGER OtherTransferCount;
    ULONG ReadOperationCount;
    ULONG WriteOperationCount;
    ULONG OtherOperationCount;
    ULONG AvailablePages;
    ULONG CommittedPages;
    ULONG CommitLimit;
    ULONG PeakCommitment;
    ULONG PageFaultCount;
    ULONG CopyOnWriteCount;
    ULONG TransitionCount;
    ULONG CacheTransitionCount;
    ULONG DemandZeroCount;
    ULONG PageReadCount;
    ULONG PageReadIoCount;
    ULONG CacheReadCount;
    ULONG CacheIoCount;
    ULONG DirtyPagesWriteCount;
    ULONG DirtyWriteIoCount;
    ULONG MappedPagesWriteCount;
    ULONG MappedWriteIoCount;
    ULONG PagedPoolPages;
    ULONG NonPagedPoolPages;
    ULONG PagedPoolAllocs;
    ULONG PagedPoolFrees;
    ULONG NonPagedPoolAllocs;
    ULONG NonPagedPoolFrees;
    ULONG FreePageCount;
    ULONG ZeroPageCount;
    ULONG ModifiedPageCount;
    ULONG ModifiedNoWritePageCount;
    ULONG BadPageCount;
    ULONG PageCountByPriority[8];
} SYSTEM_PERFORMANCE_INFORMATION;

typedef NTSTATUS(NTAPI* NtQuerySystemInformation_t)(
    ULONG SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
);

typedef NTSTATUS(NTAPI* NtSetSystemInformation_t)(
    ULONG SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength
);

static SERVICE_STATUS_HANDLE g_ServiceStatusHandle = NULL;
static BOOL g_Running = TRUE;

// --- Registry helper ---
DWORD GetRegistryDWORD(LPCTSTR name, DWORD defaultValue) {
    HKEY hKey;
    DWORD value = defaultValue, size = sizeof(DWORD);
    if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, REG_PATH, 0, NULL,
        REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegQueryValueEx(hKey, name, NULL, NULL, (LPBYTE)&value, &size);
        RegSetValueEx(hKey, name, 0, REG_DWORD, (const BYTE*)&value, sizeof(DWORD));
        RegCloseKey(hKey);
    }
    return value;
}

// --- Memory info ---
BOOL GetMemoryInfo(ULONGLONG* freeBytes, ULONGLONG* standbyBytes) {
    HMODULE ntdll = GetModuleHandle(TEXT("ntdll.dll"));
    NtQuerySystemInformation_t NtQuerySystemInformation =
        (NtQuerySystemInformation_t)GetProcAddress(ntdll, "NtQuerySystemInformation");
    if (!NtQuerySystemInformation) return FALSE;

    SYSTEM_PERFORMANCE_INFORMATION info;
    NTSTATUS status = NtQuerySystemInformation(SystemPerformanceInformation, &info, sizeof(info), NULL);
    if (status != 0) return FALSE;

    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    DWORD pageSize = sysInfo.dwPageSize;

    *freeBytes = ((ULONGLONG)info.FreePageCount + (ULONGLONG)info.ZeroPageCount) * pageSize;

    ULONGLONG standby = 0;
    for (int i = 0; i < 8; i++) standby += (ULONGLONG)info.PageCountByPriority[i];
    *standbyBytes = standby * pageSize;

    return TRUE;
}

// --- Purge standby ---
BOOL PurgeStandbyList() {
    HMODULE ntdll = GetModuleHandle(TEXT("ntdll.dll"));
    NtSetSystemInformation_t NtSetSystemInformation =
        (NtSetSystemInformation_t)GetProcAddress(ntdll, "NtSetSystemInformation");
    if (!NtSetSystemInformation) return FALSE;

    int param = MemoryPurgeStandbyList;
    NTSTATUS status = NtSetSystemInformation(SystemMemoryListInformation, &param, sizeof(param));
    return status == 0;
}

// --- Service functions ---
void WINAPI ServiceCtrlHandler(DWORD ctrlCode) {
    switch (ctrlCode) {
    case SERVICE_CONTROL_STOP:
        g_Running = FALSE;
        if (g_ServiceStatusHandle) {
            SERVICE_STATUS status = { 0 };
            status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
            status.dwCurrentState = SERVICE_STOP_PENDING;
            status.dwControlsAccepted = 0;
            SetServiceStatus(g_ServiceStatusHandle, &status);
        }
        break;
    default:
        break;
    }
}

void WINAPI ServiceMain(DWORD argc, LPTSTR* argv) {
    g_ServiceStatusHandle = RegisterServiceCtrlHandler(SERVICE_NAME, ServiceCtrlHandler);
    if (!g_ServiceStatusHandle) return;

    SERVICE_STATUS status = { 0 };
    status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    status.dwCurrentState = SERVICE_RUNNING;
    status.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    SetServiceStatus(g_ServiceStatusHandle, &status);

    while (g_Running) {
        DWORD minFreeMB = GetRegistryDWORD(REG_MINFREE, DEFAULT_MINFREE_MB);
        DWORD intervalSec = GetRegistryDWORD(REG_INTERVAL, DEFAULT_INTERVAL_SEC);

        ULONGLONG freeBytes, standbyBytes;
        if (GetMemoryInfo(&freeBytes, &standbyBytes)) {
            double freeMB = freeBytes / (1024.0 * 1024.0);
            double standbyMB = standbyBytes / (1024.0 * 1024.0);
            if (freeMB < minFreeMB) {
                PurgeStandbyList();
            }
        }

        Sleep(intervalSec * 1000);
    }

    status.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(g_ServiceStatusHandle, &status);
}

// --- Main ---
int _tmain(int argc, TCHAR* argv[]) {
    if (argc > 1) {
        if (_tcscmp(argv[1], TEXT("/install")) == 0) {
            SC_HANDLE scManager = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
            if (!scManager) return 1;
            TCHAR path[MAX_PATH];
            GetModuleFileName(NULL, path, MAX_PATH);
            SC_HANDLE sc = CreateService(scManager, SERVICE_NAME, SERVICE_DISPLAY_NAME,
                SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START,
                SERVICE_ERROR_NORMAL, path, NULL, NULL, NULL, NULL, NULL);
            if (sc) CloseServiceHandle(sc);
            CloseServiceHandle(scManager);
            return 0;
        }
        else if (_tcscmp(argv[1], TEXT("/uninstall")) == 0) {
            SC_HANDLE scManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
            if (!scManager) return 1;
            SC_HANDLE sc = OpenService(scManager, SERVICE_NAME, DELETE | SERVICE_STOP | SERVICE_QUERY_STATUS);
            if (sc) {
                SERVICE_STATUS ss;
                ControlService(sc, SERVICE_CONTROL_STOP, &ss);
                DeleteService(sc);
                CloseServiceHandle(sc);
            }
            CloseServiceHandle(scManager);
            return 0;
        }
    }

    SERVICE_TABLE_ENTRY ServiceTable[] = {
        { (LPTSTR)SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)ServiceMain },
        { NULL, NULL }
    };
    StartServiceCtrlDispatcher(ServiceTable);
    return 0;
}
