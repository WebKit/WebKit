/*-------------------------------------------------------------------------
 * drawElements Utility Library
 * ----------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Process abstraction.
 *//*--------------------------------------------------------------------*/

#include "deProcess.h"
#include "deMemory.h"
#include "deString.h"

#if (DE_OS == DE_OS_UNIX) || (DE_OS == DE_OS_OSX) || (DE_OS == DE_OS_IOS) || (DE_OS == DE_OS_ANDROID) || \
    (DE_OS == DE_OS_SYMBIAN) || (DE_OS == DE_OS_QNX)

#include "deCommandLine.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

typedef enum ProcessState_e
{
    PROCESSSTATE_NOT_STARTED = 0,
    PROCESSSTATE_RUNNING,
    PROCESSSTATE_FINISHED,

    PROCESSSTATE_LAST
} ProcessState;

struct deProcess_s
{
    ProcessState state;
    int exitCode;
    char *lastError;

    pid_t pid;
    deFile *standardIn;
    deFile *standardOut;
    deFile *standardErr;
};

static void die(int statusPipe, const char *message)
{
    size_t msgLen = strlen(message);
    int res       = 0;

    printf("Process launch failed: %s\n", message);
    res = (int)write(statusPipe, message, msgLen + 1);
    DE_UNREF(res); /* No need to check result. */
    exit(-1);
}

static void dieLastError(int statusPipe, const char *message)
{
    char msgBuf[256];
    int lastErr = errno;
    snprintf(msgBuf, sizeof(msgBuf), "%s, error %d: %s", message, lastErr, strerror(lastErr));
    die(statusPipe, msgBuf);
}

bool beginsWithPath(const char *fileName, const char *pathPrefix)
{
    size_t pathLen = strlen(pathPrefix);

    /* Strip trailing / */
    while (pathLen > 0 && pathPrefix[pathLen - 1] == '/')
        pathLen -= 1;

    return pathLen > 0 && memcmp(fileName, pathPrefix, pathLen) == 0 && fileName[pathLen] == '/';
}

static void stripLeadingPath(char *fileName, const char *pathPrefix)
{
    size_t pathLen     = strlen(pathPrefix);
    size_t fileNameLen = strlen(fileName);

    DE_ASSERT(beginsWithPath(fileName, pathPrefix));

    /* Strip trailing / */
    while (pathLen > 0 && pathPrefix[pathLen - 1] == '/')
        pathLen -= 1;

    DE_ASSERT(pathLen > 0);
    DE_ASSERT(fileName[pathLen] == '/');

    memmove(&fileName[0], &fileName[0] + pathLen + 1, fileNameLen - pathLen);
}

/* Doesn't return on success. */
static void execProcess(const char *commandLine, const char *workingDirectory, int statusPipe)
{
    deCommandLine *cmdLine = deCommandLine_parse(commandLine);
    char **argList         = cmdLine ? (char **)deCalloc(sizeof(char *) * ((size_t)cmdLine->numArgs + 1)) : NULL;

    if (!cmdLine || !argList)
        die(statusPipe, "Command line parsing failed (out of memory)");

    if (workingDirectory && chdir(workingDirectory) != 0)
        dieLastError(statusPipe, "chdir() failed");

    {
        int argNdx;
        for (argNdx = 0; argNdx < cmdLine->numArgs; argNdx++)
            argList[argNdx] = cmdLine->args[argNdx];
        argList[argNdx] = NULL; /* Terminate with 0. */
    }

    if (workingDirectory && beginsWithPath(argList[0], workingDirectory))
        stripLeadingPath(argList[0], workingDirectory);

    execv(argList[0], argList);

    /* Failed. */
    dieLastError(statusPipe, "execv() failed");
}

deProcess *deProcess_create(void)
{
    deProcess *process = (deProcess *)deCalloc(sizeof(deProcess));
    if (!process)
        return NULL;

    process->state = PROCESSSTATE_NOT_STARTED;

    return process;
}

static void deProcess_cleanupHandles(deProcess *process)
{
    if (process->standardIn)
        deFile_destroy(process->standardIn);

    if (process->standardOut)
        deFile_destroy(process->standardOut);

    if (process->standardErr)
        deFile_destroy(process->standardErr);

    process->pid         = 0;
    process->standardIn  = NULL;
    process->standardOut = NULL;
    process->standardErr = NULL;
}

void deProcess_destroy(deProcess *process)
{
    /* Never leave child processes running. Otherwise we'll have zombies. */
    if (deProcess_isRunning(process))
    {
        deProcess_kill(process);
        deProcess_waitForFinish(process);
    }

    deProcess_cleanupHandles(process);
    deFree(process->lastError);
    deFree(process);
}

const char *deProcess_getLastError(const deProcess *process)
{
    return process->lastError ? process->lastError : "No error";
}

int deProcess_getExitCode(const deProcess *process)
{
    return process->exitCode;
}

static bool deProcess_setError(deProcess *process, const char *error)
{
    if (process->lastError)
    {
        deFree(process->lastError);
        process->lastError = NULL;
    }

    process->lastError = deStrdup(error);
    return process->lastError != NULL;
}

static bool deProcess_setErrorFromErrno(deProcess *process, const char *message)
{
    char msgBuf[256];
    int lastErr = errno;
    snprintf(msgBuf, sizeof(msgBuf), "%s, error %d: %s", message, lastErr, strerror(lastErr));
    return deProcess_setError(process, message);
}

static void closePipe(int p[2])
{
    if (p[0] >= 0)
        close(p[0]);
    if (p[1] >= 0)
        close(p[1]);
}

bool deProcess_start(deProcess *process, const char *commandLine, const char *workingDirectory)
{
    pid_t pid         = 0;
    int pipeIn[2]     = {-1, -1};
    int pipeOut[2]    = {-1, -1};
    int pipeErr[2]    = {-1, -1};
    int statusPipe[2] = {-1, -1};

    if (process->state == PROCESSSTATE_RUNNING)
    {
        deProcess_setError(process, "Process already running");
        return false;
    }
    else if (process->state == PROCESSSTATE_FINISHED)
    {
        deProcess_cleanupHandles(process);
        process->state = PROCESSSTATE_NOT_STARTED;
    }

    if (pipe(pipeIn) < 0 || pipe(pipeOut) < 0 || pipe(pipeErr) < 0 || pipe(statusPipe) < 0)
    {
        deProcess_setErrorFromErrno(process, "pipe() failed");

        closePipe(pipeIn);
        closePipe(pipeOut);
        closePipe(pipeErr);
        closePipe(statusPipe);

        return false;
    }

    pid = fork();

    if (pid < 0)
    {
        deProcess_setErrorFromErrno(process, "fork() failed");

        closePipe(pipeIn);
        closePipe(pipeOut);
        closePipe(pipeErr);
        closePipe(statusPipe);

        return false;
    }

    if (pid == 0)
    {
        /* Child process. */

        /* Close unused endpoints. */
        close(pipeIn[1]);
        close(pipeOut[0]);
        close(pipeErr[0]);
        close(statusPipe[0]);

        /* Set status pipe to close on exec(). That way parent will know that exec() succeeded. */
        if (fcntl(statusPipe[1], F_SETFD, FD_CLOEXEC) != 0)
            dieLastError(statusPipe[1], "Failed to set FD_CLOEXEC");

        /* Map stdin. */
        if (pipeIn[0] != STDIN_FILENO && dup2(pipeIn[0], STDIN_FILENO) != STDIN_FILENO)
            dieLastError(statusPipe[1], "dup2() failed");
        close(pipeIn[0]);

        /* Stdout. */
        if (pipeOut[1] != STDOUT_FILENO && dup2(pipeOut[1], STDOUT_FILENO) != STDOUT_FILENO)
            dieLastError(statusPipe[1], "dup2() failed");
        close(pipeOut[1]);

        /* Stderr. */
        if (pipeErr[1] != STDERR_FILENO && dup2(pipeErr[1], STDERR_FILENO) != STDERR_FILENO)
            dieLastError(statusPipe[1], "dup2() failed");
        close(pipeErr[1]);

        /* Doesn't return. */
        execProcess(commandLine, workingDirectory, statusPipe[1]);
    }
    else
    {
        /* Parent process. */

        /* Check status. */
        {
            char errBuf[256];
            ssize_t result = 0;

            close(statusPipe[1]);
            while ((result = read(statusPipe[0], errBuf, 1)) == -1)
                if (errno != EAGAIN && errno != EINTR)
                    break;

            if (result > 0)
            {
                int procStatus = 0;

                /* Read full error msg. */
                int errPos = 1;
                while (errPos < DE_LENGTH_OF_ARRAY(errBuf))
                {
                    result = read(statusPipe[0], errBuf + errPos, 1);
                    if (result == -1)
                        break; /* Done. */

                    errPos += 1;
                }

                /* Make sure str is null-terminated. */
                errBuf[errPos] = 0;

                /* Close handles. */
                close(statusPipe[0]);
                closePipe(pipeIn);
                closePipe(pipeOut);
                closePipe(pipeErr);

                /* Run waitpid to clean up zombie. */
                waitpid(pid, &procStatus, 0);

                deProcess_setError(process, errBuf);

                return false;
            }

            /* Status pipe is not needed. */
            close(statusPipe[0]);
        }

        /* Set running state. */
        process->pid   = pid;
        process->state = PROCESSSTATE_RUNNING;

        /* Stdin, stdout. */
        close(pipeIn[0]);
        close(pipeOut[1]);
        close(pipeErr[1]);

        process->standardIn  = deFile_createFromHandle((uintptr_t)pipeIn[1]);
        process->standardOut = deFile_createFromHandle((uintptr_t)pipeOut[0]);
        process->standardErr = deFile_createFromHandle((uintptr_t)pipeErr[0]);

        if (!process->standardIn)
            close(pipeIn[1]);

        if (!process->standardOut)
            close(pipeOut[0]);

        if (!process->standardErr)
            close(pipeErr[0]);
    }

    return true;
}

bool deProcess_isRunning(deProcess *process)
{
    if (process->state == PROCESSSTATE_RUNNING)
    {
        int status = 0;

        if (waitpid(process->pid, &status, WNOHANG) == 0)
            return true; /* No status available. */

        if (WIFEXITED(status) || WIFSIGNALED(status))
        {
            /* Child has finished. */
            process->state = PROCESSSTATE_FINISHED;
            return false;
        }
        else
            return true;
    }
    else
        return false;
}

bool deProcess_waitForFinish(deProcess *process)
{
    int status = 0;
    pid_t waitResult;

    if (process->state != PROCESSSTATE_RUNNING)
    {
        deProcess_setError(process, "Process is not running");
        return false;
    }

    /* \note [pyry] HACK, apparently needed by some versions of OS X. */
    while ((waitResult = waitpid(process->pid, &status, 0)) != process->pid)
        if (errno != ENOENT)
            break;

    if (waitResult != process->pid)
    {
        deProcess_setErrorFromErrno(process, "waitpid() failed");
        return false; /* waitpid() failed. */
    }

    if (!WIFEXITED(status) && !WIFSIGNALED(status))
    {
        deProcess_setErrorFromErrno(process, "waitpid() failed");
        return false; /* Something strange happened. */
    }

    process->exitCode = WEXITSTATUS(status);
    process->state    = PROCESSSTATE_FINISHED;
    return true;
}

static bool deProcess_sendSignal(deProcess *process, int sigNum)
{
    if (process->state != PROCESSSTATE_RUNNING)
    {
        deProcess_setError(process, "Process is not running");
        return false;
    }

    if (kill(process->pid, sigNum) == 0)
        return true;
    else
    {
        deProcess_setErrorFromErrno(process, "kill() failed");
        return false;
    }
}

bool deProcess_terminate(deProcess *process)
{
    return deProcess_sendSignal(process, SIGTERM);
}

bool deProcess_kill(deProcess *process)
{
    return deProcess_sendSignal(process, SIGKILL);
}

deFile *deProcess_getStdIn(deProcess *process)
{
    return process->standardIn;
}

deFile *deProcess_getStdOut(deProcess *process)
{
    return process->standardOut;
}

deFile *deProcess_getStdErr(deProcess *process)
{
    return process->standardErr;
}

bool deProcess_closeStdIn(deProcess *process)
{
    if (process->standardIn)
    {
        deFile_destroy(process->standardIn);
        process->standardIn = NULL;
        return true;
    }
    else
        return false;
}

bool deProcess_closeStdOut(deProcess *process)
{
    if (process->standardOut)
    {
        deFile_destroy(process->standardOut);
        process->standardOut = NULL;
        return true;
    }
    else
        return false;
}

bool deProcess_closeStdErr(deProcess *process)
{
    if (process->standardErr)
    {
        deFile_destroy(process->standardErr);
        process->standardErr = NULL;
        return true;
    }
    else
        return false;
}

#elif (DE_OS == DE_OS_WIN32)

#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <strsafe.h>

typedef enum ProcessState_e
{
    PROCESSSTATE_NOT_STARTED = 0,
    PROCESSSTATE_RUNNING,
    PROCESSSTATE_FINISHED,

    PROCESSSTATE_LAST
} ProcessState;

struct deProcess_s
{
    ProcessState state;
    char *lastError;
    int exitCode;

    PROCESS_INFORMATION procInfo;
    deFile *standardIn;
    deFile *standardOut;
    deFile *standardErr;
};

static bool deProcess_setError(deProcess *process, const char *error)
{
    if (process->lastError)
    {
        deFree(process->lastError);
        process->lastError = NULL;
    }

    process->lastError = deStrdup(error);
    return process->lastError != NULL;
}

static bool deProcess_setErrorFromWin32(deProcess *process, const char *msg)
{
    DWORD error = GetLastError();
    LPSTR msgBuf;
    char errBuf[256];

#if defined(UNICODE)
#error Unicode not supported.
#endif

    if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                      error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&msgBuf, 0, NULL) > 0)
    {
        snprintf(errBuf, sizeof(errBuf), "%s, error %lu: %s", msg, error, msgBuf);
        LocalFree(msgBuf);
        return deProcess_setError(process, errBuf);
    }
    else
    {
        /* Failed to get error str. */
        snprintf(errBuf, sizeof(errBuf), "%s, error %lu", msg, error);
        return deProcess_setError(process, errBuf);
    }
}

deProcess *deProcess_create(void)
{
    deProcess *process = (deProcess *)deCalloc(sizeof(deProcess));
    if (!process)
        return NULL;

    process->state = PROCESSSTATE_NOT_STARTED;

    return process;
}

void deProcess_cleanupHandles(deProcess *process)
{
    DE_ASSERT(!deProcess_isRunning(process));

    if (process->standardErr)
        deFile_destroy(process->standardErr);

    if (process->standardOut)
        deFile_destroy(process->standardOut);

    if (process->standardIn)
        deFile_destroy(process->standardIn);

    if (process->procInfo.hProcess)
        CloseHandle(process->procInfo.hProcess);

    if (process->procInfo.hThread)
        CloseHandle(process->procInfo.hThread);

    process->standardErr       = NULL;
    process->standardOut       = NULL;
    process->standardIn        = NULL;
    process->procInfo.hProcess = NULL;
    process->procInfo.hThread  = NULL;
}

void deProcess_destroy(deProcess *process)
{
    if (deProcess_isRunning(process))
    {
        deProcess_kill(process);
        deProcess_waitForFinish(process);
    }

    deProcess_cleanupHandles(process);
    deFree(process->lastError);
    deFree(process);
}

const char *deProcess_getLastError(const deProcess *process)
{
    return process->lastError ? process->lastError : "No error";
}

int deProcess_getExitCode(const deProcess *process)
{
    return process->exitCode;
}

bool deProcess_start(deProcess *process, const char *commandLine, const char *workingDirectory)
{
    SECURITY_ATTRIBUTES securityAttr;
    STARTUPINFO startInfo;

    /* Pipes. */
    HANDLE stdInRead   = NULL;
    HANDLE stdInWrite  = NULL;
    HANDLE stdOutRead  = NULL;
    HANDLE stdOutWrite = NULL;
    HANDLE stdErrRead  = NULL;
    HANDLE stdErrWrite = NULL;

    if (process->state == PROCESSSTATE_RUNNING)
    {
        deProcess_setError(process, "Process already running");
        return false;
    }
    else if (process->state == PROCESSSTATE_FINISHED)
    {
        /* Process finished, clean up old cruft. */
        deProcess_cleanupHandles(process);
        process->state = PROCESSSTATE_NOT_STARTED;
    }

    deMemset(&startInfo, 0, sizeof(startInfo));
    deMemset(&securityAttr, 0, sizeof(securityAttr));

    /* Security attributes for inheriting handle. */
    securityAttr.nLength              = sizeof(SECURITY_ATTRIBUTES);
    securityAttr.bInheritHandle       = TRUE;
    securityAttr.lpSecurityDescriptor = NULL;

    /* Create pipes. \todo [2011-10-03 pyry] Clean up handles on error! */
    if (!CreatePipe(&stdInRead, &stdInWrite, &securityAttr, 0) ||
        !SetHandleInformation(stdInWrite, HANDLE_FLAG_INHERIT, 0))
    {
        deProcess_setErrorFromWin32(process, "CreatePipe() failed");
        CloseHandle(stdInRead);
        CloseHandle(stdInWrite);
        return false;
    }

    if (!CreatePipe(&stdOutRead, &stdOutWrite, &securityAttr, 0) ||
        !SetHandleInformation(stdOutRead, HANDLE_FLAG_INHERIT, 0))
    {
        deProcess_setErrorFromWin32(process, "CreatePipe() failed");
        CloseHandle(stdInRead);
        CloseHandle(stdInWrite);
        CloseHandle(stdOutRead);
        CloseHandle(stdOutWrite);
        return false;
    }

    if (!CreatePipe(&stdErrRead, &stdErrWrite, &securityAttr, 0) ||
        !SetHandleInformation(stdErrRead, HANDLE_FLAG_INHERIT, 0))
    {
        deProcess_setErrorFromWin32(process, "CreatePipe() failed");
        CloseHandle(stdInRead);
        CloseHandle(stdInWrite);
        CloseHandle(stdOutRead);
        CloseHandle(stdOutWrite);
        CloseHandle(stdErrRead);
        CloseHandle(stdErrWrite);
        return false;
    }

    /* Setup startup info. */
    startInfo.cb         = sizeof(startInfo);
    startInfo.hStdError  = stdErrWrite;
    startInfo.hStdOutput = stdOutWrite;
    startInfo.hStdInput  = stdInRead;
    startInfo.dwFlags |= STARTF_USESTDHANDLES;

    if (!CreateProcess(NULL, (LPTSTR)commandLine, NULL, NULL, TRUE /* inherit handles */, 0, NULL, workingDirectory,
                       &startInfo, &process->procInfo))
    {
        /* Store error info. */
        deProcess_setErrorFromWin32(process, "CreateProcess() failed");

        /* Close all handles. */
        CloseHandle(stdInRead);
        CloseHandle(stdInWrite);
        CloseHandle(stdOutRead);
        CloseHandle(stdOutWrite);
        CloseHandle(stdErrRead);
        CloseHandle(stdErrWrite);

        return false;
    }

    process->state = PROCESSSTATE_RUNNING;

    /* Close our ends of handles.*/
    CloseHandle(stdErrWrite);
    CloseHandle(stdOutWrite);
    CloseHandle(stdInRead);

    /* Construct stdio file objects \note May fail, not detected. */
    process->standardIn  = deFile_createFromHandle((uintptr_t)stdInWrite);
    process->standardOut = deFile_createFromHandle((uintptr_t)stdOutRead);
    process->standardErr = deFile_createFromHandle((uintptr_t)stdErrRead);

    return true;
}

bool deProcess_isRunning(deProcess *process)
{
    if (process->state == PROCESSSTATE_RUNNING)
    {
        int exitCode;
        BOOL result = GetExitCodeProcess(process->procInfo.hProcess, (LPDWORD)&exitCode);

        if (result != TRUE)
        {
            deProcess_setErrorFromWin32(process, "GetExitCodeProcess() failed");
            return false;
        }

        if (exitCode == STILL_ACTIVE)
            return true;
        else
        {
            /* Done. */
            process->exitCode = exitCode;
            process->state    = PROCESSSTATE_FINISHED;
            return false;
        }
    }
    else
        return false;
}

bool deProcess_waitForFinish(deProcess *process)
{
    if (process->state == PROCESSSTATE_RUNNING)
    {
        if (WaitForSingleObject(process->procInfo.hProcess, INFINITE) != WAIT_OBJECT_0)
        {
            deProcess_setErrorFromWin32(process, "WaitForSingleObject() failed");
            return false;
        }
        return !deProcess_isRunning(process);
    }
    else
    {
        deProcess_setError(process, "Process is not running");
        return false;
    }
}

static bool stopProcess(deProcess *process, bool kill)
{
    if (process->state == PROCESSSTATE_RUNNING)
    {
        if (!TerminateProcess(process->procInfo.hProcess, kill ? -1 : 0))
        {
            deProcess_setErrorFromWin32(process, "TerminateProcess() failed");
            return false;
        }
        else
            return true;
    }
    else
    {
        deProcess_setError(process, "Process is not running");
        return false;
    }
}

bool deProcess_terminate(deProcess *process)
{
    return stopProcess(process, false);
}

bool deProcess_kill(deProcess *process)
{
    return stopProcess(process, true);
}

deFile *deProcess_getStdIn(deProcess *process)
{
    return process->standardIn;
}

deFile *deProcess_getStdOut(deProcess *process)
{
    return process->standardOut;
}

deFile *deProcess_getStdErr(deProcess *process)
{
    return process->standardErr;
}

bool deProcess_closeStdIn(deProcess *process)
{
    if (process->standardIn)
    {
        deFile_destroy(process->standardIn);
        process->standardIn = NULL;
        return true;
    }
    else
        return false;
}

bool deProcess_closeStdOut(deProcess *process)
{
    if (process->standardOut)
    {
        deFile_destroy(process->standardOut);
        process->standardOut = NULL;
        return true;
    }
    else
        return false;
}

bool deProcess_closeStdErr(deProcess *process)
{
    if (process->standardErr)
    {
        deFile_destroy(process->standardErr);
        process->standardErr = NULL;
        return true;
    }
    else
        return false;
}

#else
#error Implement deProcess for your OS.
#endif
