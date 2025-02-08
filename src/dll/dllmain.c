#include <windows.h>
#include "main.h"

BOOL APIENTRY DllMain(HMODULE hinstDLL, DWORD Reason, LPVOID LPV) {
    static FILE* outf = NULL;
    static HANDLE thread = NULL;

    switch(Reason) {
        case DLL_PROCESS_ATTACH: {
            if(outf == NULL) {
                fopen_s(&outf, "tas-kid-log.txt", "w");
            }

            LOG("Attaching to process...\n");

            if(thread == NULL) {
                thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)main_thread, outf, 0, NULL);
            }
            break;
        }
        case DLL_PROCESS_DETACH: {
            if(outf) {
                LOG("Detaching from process...\n");
                fclose(outf);
                WSACleanup();
            }
            break;
        }
    }
    return TRUE;
}
