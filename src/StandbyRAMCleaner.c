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
#include <strsafe.h>
#include <winternl.h>

#define SERVICE_NAME TEXT("StandbyRAMCleanerService")
#define SERVICE_DISPLAY_NAME TEXT("Standby RAM Cleaner Service")

// Define missing constants for Standby List purge
#ifndef MemoryPurgeStandbyList
#define MemoryPurgeStandbyList 4
#endif

#ifndef SystemMemoryListInformation
#define SystemMemoryListInformation 0x50
#endif

#ifndef STATUS_INFO_LENGTH_MISMATCH
#define STATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xC0000004)
#endif

typedef LONG NTSTATUS;
//typedef NTSTATUS(WINAPI* NtQuerySystemInformation_t)(SYSTEM_INFORMATION_CLASS, PVOID, ULONG, PULONG);
//typedef NTSTATUS(WINAPI* NtSetSystemInformation_t)(SYSTEM_INFORMATION_CLASS, PVOID, ULONG);

typedef struct _SYSTEM_MEMORY_LIST_INFORMATION {
    ULONGLONG ZeroPageCount;
    ULONGLONG FreePageCount;
    ULONGLONG ModifiedPageCount;
    ULONGLONG ModifiedNoWritePageCount;
    ULONGLONG BadPageCount;
    ULONGLONG PageCountByPriority[8];
    ULONGLONG RepurposedPagesByPriority[8];
    ULONGLONG StandbyRepurposedByPriority[8];
} SYSTEM_MEMORY_LIST_INFORMATION;

typedef NTSTATUS(WINAPI* NtQuerySystemInformation_t)(
    SYSTEM_INFORMATION_CLASS SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
);

typedef NTSTATUS(WINAPI* NtSetSystemInformation_t)(
    SYSTEM_INFORMATION_CLASS SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength
);

#define REG_PATH TEXT("SOFTWARE\\MemoryCleaner")
#define REG_MINFREE TEXT("MinFreeMB")
#define REG_INTERVAL TEXT("CheckIntervalSec")
#define DEFAULT_MINFREE_MB 2048
#define DEFAULT_INTERVAL_SEC 10

SERVICE_STATUS ServiceStatus;
SERVICE_STATUS_HANDLE hStatus;
volatile int running = 1;

void LogEvent(LPCTSTR message, WORD type) {
    HANDLE hEvent = RegisterEventSource(NULL, SERVICE_NAME);
    if (hEvent) {
        ReportEvent(hEvent, type, 0, 0, NULL, 1, 0, &message, NULL);
        DeregisterEventSource(hEvent);
    }
}

DWORD GetRegistryDword(HKEY hKey, LPCTSTR valueName, DWORD defaultValue) {
    DWORD data = defaultValue;
    DWORD dataSize = sizeof(DWORD);
    if (RegGetValue(hKey, NULL, valueName, RRF_RT_REG_DWORD, NULL, &data, &dataSize) != ERROR_SUCCESS) {
        RegSetValueEx(hKey, valueName, 0, REG_DWORD, (const BYTE*)&defaultValue, sizeof(DWORD));
        data = defaultValue;
    }
    return data;
}

void EnsureRegistryValues(DWORD* minFreeMB, DWORD* intervalSec) {
    HKEY hKey;
    if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, REG_PATH, 0, NULL, 0, KEY_READ | KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        *minFreeMB = GetRegistryDword(hKey, REG_MINFREE, DEFAULT_MINFREE_MB);
        *intervalSec = GetRegistryDword(hKey, REG_INTERVAL, DEFAULT_INTERVAL_SEC);
        RegCloseKey(hKey);
    } else {
        *minFreeMB = DEFAULT_MINFREE_MB;
        *intervalSec = DEFAULT_INTERVAL_SEC;
    }
}

void PurgeStandby(NtSetSystemInformation_t NtSetSystemInformation, DWORD freeMB) {
    int param = MemoryPurgeStandbyList;
    NTSTATUS status = NtSetSystemInformation(SystemMemoryListInformation, &param, sizeof(param));
    TCHAR msg[256];
    if (status == 0) {
        StringCchPrintf(msg, 256, TEXT("Standby purged. Effective Free: %lu MB"), freeMB);
        LogEvent(msg, EVENTLOG_INFORMATION_TYPE);
    } else {
        StringCchPrintf(msg, 256, TEXT("Failed to purge standby. Effective Free: %lu MB"), freeMB);
        LogEvent(msg, EVENTLOG_ERROR_TYPE);
    }
}

BOOL CheckMemoryPrivileges(NtSetSystemInformation_t NtSetSystemInformation) {
    int param = MemoryPurgeStandbyList;
    NTSTATUS status = NtSetSystemInformation(SystemMemoryListInformation, &param, sizeof(param));
    if (status == 0) return TRUE;
    LogEvent(TEXT("Service does not have sufficient privileges to purge Standby List."), EVENTLOG_ERROR_TYPE);
    return FALSE;
}

void CheckMemoryLoop(NtQuerySystemInformation_t NtQuerySystemInformation,
                     NtSetSystemInformation_t NtSetSystemInformation) {
    
	SYSTEM_INFO si;
    GetSystemInfo(&si);
	DWORD pageSize = si.dwPageSize; 

    while (running) {
        SYSTEM_MEMORY_LIST_INFORMATION memInfo = {0};
        NTSTATUS status = NtQuerySystemInformation(SystemMemoryListInformation,
                                                   &memInfo, sizeof(memInfo), NULL);
        if (status == 0) { // STATUS_SUCCESS
            ULONGLONG effectiveFreePages = memInfo.FreePageCount + memInfo.ZeroPageCount;
            DWORD freeMB = (DWORD)((effectiveFreePages * pageSize) / (1024 * 1024));

            DWORD minFreeMB, intervalSec;
            EnsureRegistryValues(&minFreeMB, &intervalSec);

            // Подсчёт Standby (для логирования)
            ULONGLONG standbyBytes = 0;
            for (int i = 0; i < 8; i++) standbyBytes += memInfo.PageCountByPriority[i] * pageSize;
            double xfreeMB = effectiveFreePages * pageSize / (1024.0 * 1024.0);
            double xstandbyMB = standbyBytes / (1024.0 * 1024.0);

            /*TCHAR msg[256];
            StringCchPrintf(msg, 256, TEXT("Free: %.0f MB, Standby: %.0f MB, Threshold: %lu MB"), xfreeMB, xstandbyMB, minFreeMB);
            LogEvent(msg, EVENTLOG_INFORMATION_TYPE);*/

            if (freeMB < minFreeMB) {
                PurgeStandby(NtSetSystemInformation, freeMB);
            }

            Sleep(intervalSec * 1000);
        } else {
            TCHAR msg[128];
            StringCchPrintf(msg, 128, TEXT("NtQuerySystemInformation failed. Status=0x%X"), status);
            LogEvent(msg, EVENTLOG_ERROR_TYPE);
            Sleep(5000);
        }
    }
}

void WINAPI ServiceCtrlHandler(DWORD CtrlCode) {
    switch (CtrlCode) {
    case SERVICE_CONTROL_STOP:
        ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        running = 0;
        SetServiceStatus(hStatus, &ServiceStatus);
        break;
    default:
        break;
    }
}

void WINAPI ServiceMain(DWORD argc, LPTSTR* argv) {
    ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    ServiceStatus.dwWin32ExitCode = 0;
    ServiceStatus.dwCheckPoint = 0;
    ServiceStatus.dwWaitHint = 0;

    hStatus = RegisterServiceCtrlHandler(SERVICE_NAME, ServiceCtrlHandler);
    SetServiceStatus(hStatus, &ServiceStatus);

    LogEvent(TEXT("Standby RAM Cleaner Service started."), EVENTLOG_INFORMATION_TYPE);

    ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(hStatus, &ServiceStatus);

    NtQuerySystemInformation_t NtQuerySystemInformation = (NtQuerySystemInformation_t)GetProcAddress(GetModuleHandle(TEXT("ntdll.dll")), "NtQuerySystemInformation");
    NtSetSystemInformation_t NtSetSystemInformation = (NtSetSystemInformation_t)GetProcAddress(GetModuleHandle(TEXT("ntdll.dll")), "NtSetSystemInformation");

    if (!NtQuerySystemInformation || !NtSetSystemInformation) {
        LogEvent(TEXT("Failed to locate NtQuerySystemInformation/NtSetSystemInformation."), EVENTLOG_ERROR_TYPE);
        ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        SetServiceStatus(hStatus, &ServiceStatus);
        return;
    }

    if (!CheckMemoryPrivileges(NtSetSystemInformation)) {
        ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        SetServiceStatus(hStatus, &ServiceStatus);
        return;
    }

    CheckMemoryLoop(NtQuerySystemInformation, NtSetSystemInformation);

    ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(hStatus, &ServiceStatus);
    LogEvent(TEXT("Standby RAM Cleaner Service stopped."), EVENTLOG_INFORMATION_TYPE);
}

void InstallService(LPCTSTR exePath) {
    SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!hSCManager) return;

    SC_HANDLE hService = OpenService(hSCManager, SERVICE_NAME, SERVICE_QUERY_STATUS);
    if (hService) {
        CloseServiceHandle(hService);
        CloseServiceHandle(hSCManager);
        return;
    }

    hService = CreateService(
        hSCManager,
        SERVICE_NAME,
        SERVICE_DISPLAY_NAME,
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,
        SERVICE_ERROR_NORMAL,
        exePath,
        NULL, NULL, NULL, NULL, NULL
    );

    if (hService) {
        StartService(hService, 0, NULL);
        LogEvent(TEXT("Standby RAM Cleaner Service installed and started."), EVENTLOG_INFORMATION_TYPE);
        CloseServiceHandle(hService);
    }

    CloseServiceHandle(hSCManager);
}

void UninstallService() {
    SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
    if (!hSCManager) return;

    SC_HANDLE hService = OpenService(hSCManager, SERVICE_NAME, DELETE | SERVICE_STOP | SERVICE_QUERY_STATUS);
    if (hService) {
        SERVICE_STATUS ss;
        ControlService(hService, SERVICE_CONTROL_STOP, &ss);
        DeleteService(hService);
        LogEvent(TEXT("Standby RAM Cleaner Service stopped and uninstalled."), EVENTLOG_INFORMATION_TYPE);
        CloseServiceHandle(hService);
    }

    CloseServiceHandle(hSCManager);
}

int main(int argc, char* argv[]) {
    TCHAR exePath[MAX_PATH];
    GetModuleFileName(NULL, exePath, MAX_PATH);

    if (argc > 1) {
        if (_stricmp(argv[1], "/install") == 0) {
            InstallService(exePath);
            return 0;
        }
        if (_stricmp(argv[1], "/uninstall") == 0) {
            UninstallService();
            return 0;
        }
    }

    SERVICE_TABLE_ENTRY ServiceTable[] = {
        { SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)ServiceMain },
        { NULL, NULL }
    };
    StartServiceCtrlDispatcher(ServiceTable);
    return 0;
}
