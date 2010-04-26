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

#include "WebProcessLauncher.h"

#include "Connection.h"
#include "RunLoop.h"
#include "WebProcess.h"
#include <WebCore/PlatformString.h>
#include <runtime/InitializeThreading.h>
#include <string>
#include <wtf/PassRefPtr.h>
#include <wtf/Threading.h>

using namespace WebCore;

namespace WebKit {

// FIXME: We need to use a better connection identifier.
static const wchar_t* serverName = L"UIProcess";

static void* webThreadBody(void*)
{
    // Initialization
    JSC::initializeThreading();
    WTF::initializeMainThread();

    WebProcess::shared().initialize(serverName, RunLoop::current());
    RunLoop::run();

    return 0;
}

ProcessInfo launchWebProcess(CoreIPC::Connection::Client* client, ProcessModel model)
{
    ProcessInfo info = { 0, 0 };

    info.connection = CoreIPC::Connection::createServerConnection(serverName, client, RunLoop::main());
    info.connection->open();

    switch (model) {
        case ProcessModelSecondaryThread: {
            // FIXME: Pass the handle.
            if (createThread(webThreadBody, 0, "WebKit2: WebThread")) {
                info.processIdentifier = ::GetCurrentProcess();
                return info;
            }
            break;
        }
        case ProcessModelSecondaryProcess: {
            // FIXME: We would like to pass a full path to the .exe here.
            String commandLine(L"WebKit2WebProcess_debug.exe");

            STARTUPINFO startupInfo = { 0 };
            startupInfo.cb = sizeof(startupInfo);
            PROCESS_INFORMATION processInformation = { 0 };
            BOOL result = ::CreateProcessW(0, (LPWSTR)commandLine.charactersWithNullTermination(),
                                           0, 0, false, 0, 0, 0, &startupInfo, &processInformation);
            if (!result) {
                // FIXME: What should we do here?
                DWORD error = ::GetLastError();
                ASSERT_NOT_REACHED();
            }

            // Don't leak the thread handle.
            ::CloseHandle(processInformation.hThread);

            info.processIdentifier = processInformation.hProcess;
        }
    }

    return info;
}

} // namespace WebKit
