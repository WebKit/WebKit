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

#include "WebProcessMain.h"

#include "RunLoop.h"
#include "WebProcess.h"
#include <runtime/InitializeThreading.h>
#include <WebCore/PlatformString.h>
#include <WebCore/StringHash.h>
#include <wtf/Threading.h>

using namespace WebCore;

namespace WebKit {

// FIXME: If the Mac port is going to need this we should consider moving it to Platform or Shared.
class CommandLine {
public:
    CommandLine() { }
    bool parse(LPTSTR commandLineString);

    String operator[](const String&) const;

private:
    HashMap<String, String> m_args;
};

bool CommandLine::parse(LPTSTR commandLineString)
{
    m_args.clear();

    // Check if this is an empty command line.
    if (!commandLineString || !commandLineString[0])
        return true;

    int argumentCount;
    LPWSTR* commandLineArgs = ::CommandLineToArgvW(commandLineString, &argumentCount);
    if (!commandLineArgs)
        return false;

    if (argumentCount % 2) {
        ::LocalFree(commandLineArgs);
        return false;
    }

    for (int i = 0; i < argumentCount ; i += 2) {
        LPWSTR key = commandLineArgs[i];

        if (key[0] != '-') {
            ::LocalFree(commandLineArgs);
            return false;
        }

        m_args.set(&key[1], commandLineArgs[i + 1]);
    }

    ::LocalFree(commandLineArgs);
    return true;
}

String CommandLine::operator[](const String& key) const
{
    return m_args.get(key);
}

int WebProcessMain(HINSTANCE hInstance, LPWSTR commandLineString)
{
    CommandLine commandLine;

    // FIXME: Should we return an error code here?
    if (!commandLine.parse(commandLineString))
        return 0;

    OleInitialize(0);

    JSC::initializeThreading();
    WTF::initializeMainThread();
    RunLoop::initializeMainRunLoop();

    const String& identifierString = commandLine["clientIdentifier"];

    // FIXME: Should we return an error code here?
    HANDLE clientIdentifier = reinterpret_cast<HANDLE>(identifierString.toUInt64Strict());
    if (!clientIdentifier)
        return 0;

    WebProcess::shared().initialize(clientIdentifier, RunLoop::main());
    RunLoop::run();

    return 0;
}

} // namespace WebKit
