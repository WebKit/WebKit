/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <wtf/WTFProcess.h>

#include <stdlib.h>

#if OS(UNIX)
#include <unistd.h>
#endif

#if OS(WINDOWS)
#include <windows.h>
#endif

namespace WTF {

void exitProcess(int status)
{
#if OS(WINDOWS)
    // Windows do not have "main thread" concept. So, shutdown of the main thread does not mean immediate process shutdown.
    // As a result, if there is running thread, it sometimes cause deadlock.
    //
    // > If one of the terminated threads in the process holds a lock and the DLL detach code in one of the loaded DLLs
    // > attempts to acquire the same lock, then calling ExitProcess results in a deadlock. In contrast, if a process
    // > terminates by calling TerminateProcess, the DLLs that the process is attached to are not notified of the process
    // > termination. Therefore, if you do not know the state of all threads in your process, it is better to call TerminateProcess
    // > than ExitProcess. Note that returning from the main function of an application results in a call to ExitProcess.
    // https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-exitprocess
    //
    // Always use TerminateProcess since framework does not know the status of the other threads.
    TerminateProcess(GetCurrentProcess(), status);

    // The code can reach here only when very buggy anti-virus software hooks are integrated into the system. To make it safe, in that case,
    // let the process crash explicitly.
    CRASH();
#else
    exit(status);
#endif
}

void terminateProcess(int status)
{
#if OS(WINDOWS)
    // On Windows, exitProcess and terminateProcess do the same thing due to its more complicated main thread handling.
    // See comment in exitProcess.
    exitProcess(status);
#else
    _exit(status);
#endif
}

}
