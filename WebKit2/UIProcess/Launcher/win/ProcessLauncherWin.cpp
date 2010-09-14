/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ProcessLauncher.h"

#include "Connection.h"
#include "RunLoop.h"
#include <wtf/text/WTFString.h>

using namespace WebCore;

#if !defined(NDEBUG) && (!defined(DEBUG_INTERNAL) || defined(DEBUG_ALL))
const LPCWSTR webProcessName = L"WebKit2WebProcess_debug.exe";
#else
const LPCWSTR webProcessName = L"WebKit2WebProcess.exe";
#endif

namespace WebKit {

void ProcessLauncher::launchProcess()
{
    // First, create the server and client identifiers.
    HANDLE serverIdentifier, clientIdentifier;
    if (!CoreIPC::Connection::createServerAndClientIdentifiers(serverIdentifier, clientIdentifier)) {
        // FIXME: What should we do here?
        ASSERT_NOT_REACHED();
    }

    // Ensure that the child process inherits the client identifier.
    ::SetHandleInformation(clientIdentifier, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        
    Vector<UChar> commandLineVector;

    // FIXME: We would like to pass a full path to the .exe here.

    String commandLine(webProcessName);
    append(commandLineVector, commandLine);
    append(commandLineVector, " -mode webprocess");
    append(commandLineVector, " -clientIdentifier ");
    append(commandLineVector, String::number(reinterpret_cast<uintptr_t>(clientIdentifier)));
    commandLineVector.append('\0');

    STARTUPINFO startupInfo = { 0 };
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInformation = { 0 };
    BOOL result = ::CreateProcessW(0, commandLineVector.data(), 0, 0, true, 0, 0, 0, &startupInfo, &processInformation);

    // We can now close the client identifier handle.
    ::CloseHandle(clientIdentifier);

    if (!result) {
        // FIXME: What should we do here?
        DWORD error = ::GetLastError();
        ASSERT_NOT_REACHED();
    }

    // Don't leak the thread handle.
    ::CloseHandle(processInformation.hThread);

    // We've finished launching the process, message back to the run loop.
    RunLoop::main()->scheduleWork(WorkItem::create(this, &ProcessLauncher::didFinishLaunchingProcess, processInformation.hProcess, serverIdentifier));
}

void ProcessLauncher::terminateProcess()
{
    if (!m_processIdentifier)
        return;

    ::TerminateProcess(m_processIdentifier, 0);
}

} // namespace WebKit
