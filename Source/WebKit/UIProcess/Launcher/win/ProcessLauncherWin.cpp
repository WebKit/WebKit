/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
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

#include "config.h"
#include "ProcessLauncher.h"

#include "Connection.h"
#include "IPCUtilities.h"
#include <WTF/RunLoop.h>
#include <shlwapi.h>
#include <wtf/text/StringBuilder.h>

namespace WebKit {

static LPCWSTR processName(ProcessLauncher::ProcessType processType)
{
    switch (processType) {
    case ProcessLauncher::ProcessType::Web:
        return L"WebKitWebProcess.exe";
    case ProcessLauncher::ProcessType::Network:
        return L"WebKitNetworkProcess.exe";
#if ENABLE(GPU_PROCESS)
    case ProcessLauncher::ProcessType::GPU:
        return L"WebKitGPUProcess.exe";
#endif
    }
    return L"WebKitWebProcess.exe";
}

void ProcessLauncher::launchProcess()
{
    // First, create the server and client identifiers.
    HANDLE serverIdentifier, clientIdentifier;
    if (!IPC::createServerAndClientIdentifiers(serverIdentifier, clientIdentifier)) {
        // FIXME: What should we do here?
        ASSERT_NOT_REACHED();
    }

    // Ensure that the child process inherits the client identifier.
    ::SetHandleInformation(clientIdentifier, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);

    // To get the full file path to WebKit2WebProcess.exe, we fild the location of WebKit2.dll,
    // remove the last path component.
    HMODULE webKitModule = ::GetModuleHandle(L"WebKit2.dll");
    ASSERT(webKitModule);
    if (!webKitModule)
        return;

    WCHAR pathStr[MAX_PATH];
    if (!::GetModuleFileName(webKitModule, pathStr, std::size(pathStr)))
        return;

    ::PathRemoveFileSpec(pathStr);
    if (!::PathAppend(pathStr, processName(m_launchOptions.processType)))
        return;

    StringBuilder commandLineBuilder;
    commandLineBuilder.append("\"");
    commandLineBuilder.append(String(pathStr));
    commandLineBuilder.append("\"");
    commandLineBuilder.append(" -type ");
    commandLineBuilder.append(String::number(static_cast<int>(m_launchOptions.processType)));
    commandLineBuilder.append(" -processIdentifier ");
    commandLineBuilder.append(String::number(m_launchOptions.processIdentifier.toUInt64()));
    commandLineBuilder.append(" -clientIdentifier ");
    commandLineBuilder.append(String::number(reinterpret_cast<uintptr_t>(clientIdentifier)));
    if (m_client->shouldConfigureJSCForTesting())
        commandLineBuilder.append(" -configure-jsc-for-testing");
    if (!m_client->isJITEnabled())
        commandLineBuilder.append(" -disable-jit");
    commandLineBuilder.append('\0');

    auto commandLine = commandLineBuilder.toString().wideCharacters();

    STARTUPINFO startupInfo { };
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESHOWWINDOW;
    startupInfo.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION processInformation { };
    BOOL result = ::CreateProcess(0, commandLine.data(), 0, 0, true, 0, 0, 0, &startupInfo, &processInformation);

    // We can now close the client identifier handle.
    ::CloseHandle(clientIdentifier);

    if (!result) {
        // FIXME: What should we do here?
        ASSERT_NOT_REACHED();
    }

    // Don't leak the thread handle.
    ::CloseHandle(processInformation.hThread);

    // We've finished launching the process, message back to the run loop.
    RefPtr<ProcessLauncher> protectedThis(this);
    m_hProcess = processInformation.hProcess;
    WTF::ProcessID pid = processInformation.dwProcessId;

    RunLoop::main().dispatch([protectedThis, pid, serverIdentifier] {
        protectedThis->didFinishLaunchingProcess(pid, IPC::Connection::Identifier { serverIdentifier });
    });
}

void ProcessLauncher::terminateProcess()
{
    if (!m_hProcess.isValid())
        return;

    ::TerminateProcess(m_hProcess.get(), 0);
}

void ProcessLauncher::platformInvalidate()
{
    m_hProcess.clear();
}

} // namespace WebKit
