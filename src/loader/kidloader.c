#include <stdio.h>
#include <windows.h>
#include <tlhelp32.h>

#define DLLNAME "libkiddll.dll"

int main(int argc, char** argv) {
    puts("Starting to listen for the game.");

    DWORD last_pid = ~0;

    while(1) {
        DWORD pid = 0;

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        PROCESSENTRY32W entry;
        entry.dwSize = sizeof(entry);

        if(!Process32FirstW(snapshot, &entry)) {
            puts("Failed to utilise toolhelp32 snapshot, please report.");
            puts("Press any key to exit...");
            getchar();
            return 1;
        }

        do {
            if(wcscmp(entry.szExeFile, L"HatinTimeGame.exe") == 0) {
                pid = entry.th32ProcessID;
                break;
            }
        }
        while(Process32NextW(snapshot, &entry));
        CloseHandle(snapshot);

        if(pid == 0 || pid == last_pid) {
            Sleep(10);
            continue;
        }

        last_pid = pid;

        HANDLE hat_handle = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
        if(hat_handle == INVALID_HANDLE_VALUE || hat_handle == NULL) {
            puts("Failed to get a process handle for the game, please report.");
            printf("Last error: 0x%016lX\n", GetLastError());
            puts("Press any key to exit...");
            getchar();
            return 3;
        }

        char dll_path[_MAX_PATH] = { 0 };
        GetFullPathName(DLLNAME, _MAX_PATH, dll_path, NULL);
        puts(dll_path);
        
        LPVOID addr = VirtualAllocEx(hat_handle, NULL, sizeof(dll_path), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        SIZE_T n;
        WriteProcessMemory(hat_handle, addr, dll_path, sizeof(dll_path), &n);

        LPVOID loadlib = GetProcAddress(GetModuleHandle("Kernel32.dll"), "LoadLibraryA");
        HANDLE rthread = CreateRemoteThread(hat_handle, NULL, 0, (LPTHREAD_START_ROUTINE)loadlib, addr, 0, NULL);

        WaitForSingleObject(rthread, INFINITE);

        DWORD exit_code;
        GetExitCodeThread(rthread, &exit_code);

        CloseHandle(rthread);
        VirtualFreeEx(hat_handle, addr, 0, MEM_RELEASE);
        CloseHandle(hat_handle);

        puts("Success, waiting for next time.");
    }
    
    return 0;
}
