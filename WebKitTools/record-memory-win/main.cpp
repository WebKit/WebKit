#include <assert.h>
#include <atlstr.h>
#include <psapi.h>
#include <stdio.h>
#include <tchar.h>
#include <time.h>
#include <windows.h>

#pragma comment(lib, "psapi.lib")

bool gSingleProcess = true;
int gQueryInterval = 5; // seconds
time_t gDuration = 0;   // seconds
CString gCommandLine;

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
    CString argument;
    for( int count = 1; count < argc; count++ ) {
        argument = argv[count];

        if ((argument.Find(_T("-h")) != -1) ||
            (argument.Find(_T("--help")) != -1))
            return PrintUsage();
        else if (argument.Find(_T("--exe")) != -1) {
            gCommandLine = argv[++count];
            if (gCommandLine.Find(_T("chrome.exe")) != -1)
                gSingleProcess = false;
        } else if ((argument.Find(_T("-i")) != -1) ||
            (argument.Find(_T("--interval")) != -1)) {
            gQueryInterval = _wtoi(argv[++count]);
            if (gQueryInterval < 1) {
                printf("ERROR: invalid interval\n");
                return E_INVALIDARG;
            }
        } else if ((argument.Find(_T("-d")) != -1) ||
            (argument.Find(_T("--duration")) != -1)) {
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
    if (argc < 2 || gCommandLine.IsEmpty()) {
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

    LPWSTR commandLine = gCommandLine.GetBuffer(MAX_PATH);  // WHAT'sTHIS? 

    // Start the child process. 
    if(!CreateProcess( NULL,   // No module name (use command line)
        commandLine,        // Command line
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
    ::Sleep(2000); // give the process some time to launch
    bool pastDuration = false;
    time_t startTime = time(NULL);
    unsigned int memUsage = gSingleProcess ? OneQuery(hProcess) : OneQueryMP(hProcess);
    while(memUsage && !pastDuration) {
        printf( "%u\n", memUsage );
        ::Sleep(gQueryInterval*1000);
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
    if (::GetProcessMemoryInfo(hProcess, (PPROCESS_MEMORY_COUNTERS)&pmc, sizeof(pmc)))
        return (unsigned)pmc.PrivateUsage;
    return 0;
}

// returns Commit Size (Private Bytes) in bytes for multi-process executables
unsigned int OneQueryMP(HANDLE hProcess)
{
    unsigned int memUsage = 0;
    CString processName;
    LPTSTR szProcessName = processName.GetBuffer(MAX_PATH);
    ::GetProcessImageFileName(hProcess, szProcessName, processName.GetAllocLength());
    processName.ReleaseBuffer();
    CString shortProcessName = ::PathFindFileName(processName);
    DWORD aProcesses[1024], cbNeeded, cProcesses;
    HANDLE hSpawnedProcess;
    if (!EnumProcesses(aProcesses, sizeof(aProcesses), &cbNeeded))
        return 0;

    // Calculate how many process identifiers were returned.
    cProcesses = cbNeeded / sizeof(DWORD);
    // find existing process
    for (unsigned int i = 0; i < cProcesses; i++)
        if (aProcesses[i] != 0) {
            DWORD retVal = 0;
            TCHAR szProcessName[MAX_PATH];

            // Get a handle to the process.
            hSpawnedProcess = OpenProcess(PROCESS_QUERY_INFORMATION |
                                   PROCESS_VM_READ,
                                   FALSE, aProcesses[i]);

            // Get the process name.
            if (NULL != hSpawnedProcess) {
                HMODULE hMod;
                DWORD cbNeeded;

                if (EnumProcessModules(hSpawnedProcess, &hMod, sizeof(hMod), &cbNeeded)) {
                    GetModuleBaseName(hSpawnedProcess, hMod, szProcessName, sizeof(szProcessName)/sizeof(TCHAR));
                    CString processName(szProcessName);
                    if (processName.Find(shortProcessName) != -1)
                        memUsage += OneQuery(hSpawnedProcess);
                }
            }
            CloseHandle(hSpawnedProcess);
        }
    return memUsage;
}
