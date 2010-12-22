#include <windows.h>
#include <assert.h>
#include <psapi.h>
#include <stdio.h>
#include <tchar.h>
#include <time.h>
#include "Shlwapi.h"

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "shlwapi.lib")

bool gSingleProcess = true;
int gQueryInterval = 5; // seconds
time_t gDuration = 0;   // seconds
LPTSTR gCommandLine;

HRESULT ProcessArgs(int argc, TCHAR *argv[]);
HRESULT PrintUsage();
void UseImage(void (functionForQueryType(HANDLE)));
void QueryContinuously(HANDLE hProcess);
time_t ElapsedTime(time_t startTime);
unsigned int OneQuery(HANDLE hProcess);
unsigned int OneQueryMP(HANDLE hProcess);

int __cdecl _tmain (int argc, TCHAR *argv[])
{
    HRESULT result = ProcessArgs(argc, argv);
    if (FAILED(result))
        return result;

    UseImage(QueryContinuously);
    return S_OK;
}

HRESULT ProcessArgs(int argc, TCHAR *argv[])
{
    LPTSTR argument;
    for( int count = 1; count < argc; count++ ) {
        argument = argv[count] ;
        if (wcsstr(argument, _T("-h")) ||
            wcsstr(argument, _T("--help")))
            return PrintUsage();
        else if (wcsstr(argument, _T("--exe"))) {
            gCommandLine = argv[++count];
            if (wcsstr(gCommandLine, _T("chrome.exe")))
                gSingleProcess = false;
        } else if (wcsstr(argument, _T("-i")) ||
            wcsstr(argument, _T("--interval"))) {
            gQueryInterval = _wtoi(argv[++count]);
            if (gQueryInterval < 1) {
                printf("ERROR: invalid interval\n");
                return E_INVALIDARG;
            }
        } else if (wcsstr(argument, _T("-d")) ||
            wcsstr(argument, _T("--duration"))) {
            gDuration = _wtoi(argv[++count]);
            if (gDuration < 1) {
                printf("ERROR: invalid duration\n");
                return E_INVALIDARG;
            }
        } else {
            _tprintf(_T("ERROR: unrecognized argument \"%s\"\n"), (LPCTSTR)argument);
            return PrintUsage();
        }
    }
    if (argc < 2 || !wcslen(gCommandLine) ) {
        printf("ERROR: executable path is required\n");
        return PrintUsage();
    }
    return S_OK;
}

HRESULT PrintUsage()
{
    printf("record-memory-win --exe EXE_PATH\n");
    printf("    Launch an executable and print the memory usage (in Private Bytes)\n");
    printf("    of the process.\n\n");
    printf("Usage:\n");
    printf("-h [--help]         : Print usage\n");
    printf("--exe arg           : Launch specified image.  Required\n");
    printf("-i [--interval] arg : Print memory usage every arg seconds.  Default: 5 seconds\n");
    printf("-d [--duration] arg : Run for up to arg seconds.  Default: no limit\n\n");
    printf("Examples:\n");
    printf("    record-memory-win --exe \"C:\\Program Files\\Safari\\Safari.exe\"\n");
    printf("    record-memory-win --exe Safari.exe -i 10 -d 7200\n");
    return E_FAIL;
}

void UseImage(void (functionForQueryType(HANDLE)))
{
    STARTUPINFO si = {0};
    si.cb = sizeof(STARTUPINFO);
    PROCESS_INFORMATION pi = {0};

    // Start the child process. 
    if(!CreateProcess( NULL,   // No module name (use command line)
        gCommandLine,        // Command line
        NULL,           // Process handle not inheritable
        NULL,           // Thread handle not inheritable
        FALSE,          // Set handle inheritance to FALSE
        0,              // No creation flags
        NULL,           // Use parent's environment block
        NULL,           // Use parent's starting directory 
        &si,            // Pointer to STARTUPINFO structure
        &pi ))          // Pointer to PROCESS_INFORMATION structure
        printf("CreateProcess failed (%d)\n", GetLastError());
    else {
        printf("Created process\n");
        functionForQueryType(pi.hProcess);
        // Close process and thread handles. 
        CloseHandle( pi.hProcess );
        CloseHandle( pi.hThread );
    }
}

void QueryContinuously(HANDLE hProcess)
{
    Sleep(2000); // give the process some time to launch
    bool pastDuration = false;
    time_t startTime = time(NULL);
    unsigned int memUsage = gSingleProcess ? OneQuery(hProcess) : OneQueryMP(hProcess);
    while(memUsage && !pastDuration) {
        printf( "%u\n", memUsage );
        Sleep(gQueryInterval*1000);
        memUsage = gSingleProcess ? OneQuery(hProcess) : OneQueryMP(hProcess);
        pastDuration = gDuration > 0 ? ElapsedTime(startTime) > gDuration : false;
    } 
}

// returns elapsed time in seconds
time_t ElapsedTime(time_t startTime)
{
    time_t currentTime = time(NULL);
    return currentTime - startTime;
}

// returns Commit Size (Private Bytes) in bytes
unsigned int OneQuery(HANDLE hProcess)
{
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (NULL == hProcess)
        return 0;
    if (GetProcessMemoryInfo(hProcess, (PPROCESS_MEMORY_COUNTERS)&pmc, sizeof(pmc)))
        return (unsigned)pmc.PrivateUsage;
    return 0;
}

// returns Commit Size (Private Bytes) in bytes for multi-process executables
unsigned int OneQueryMP(HANDLE hProcess)
{
    unsigned int memUsage = 0;
    TCHAR monitoredProcessName[MAX_PATH];
    GetProcessImageFileName(hProcess, monitoredProcessName, sizeof(monitoredProcessName)/sizeof(TCHAR));
    LPTSTR shortProcessName = PathFindFileName(monitoredProcessName);
    DWORD aProcesses[1024], cbNeeded, cProcesses;
    HANDLE hFoundProcess;
    if (!EnumProcesses(aProcesses, sizeof(aProcesses), &cbNeeded))
        return 0;

    // Calculate how many process identifiers were returned.
    cProcesses = cbNeeded / sizeof(DWORD);
    // find existing process
    for (unsigned int i = 0; i < cProcesses; i++)
        if (aProcesses[i] != 0) {
            DWORD retVal = 0;
            TCHAR foundProcessName[MAX_PATH];

            // Get a handle to the process.
            hFoundProcess = OpenProcess(PROCESS_QUERY_INFORMATION |
                                   PROCESS_VM_READ,
                                   FALSE, aProcesses[i]);

            // Get the process name.
            if (NULL != hFoundProcess) {
                HMODULE hMod;
                DWORD cbNeeded;

                if (EnumProcessModules(hFoundProcess, &hMod, sizeof(hMod), &cbNeeded)) {
                    GetModuleBaseName(hFoundProcess, hMod, foundProcessName, sizeof(foundProcessName)/sizeof(TCHAR));
                    if (wcsstr(foundProcessName, shortProcessName))
                        memUsage += OneQuery(hFoundProcess);
                }
            }
            CloseHandle(hFoundProcess);
        }
    return memUsage;
}
