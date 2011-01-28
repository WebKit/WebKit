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

#include "config.h"
#include "WebProcessMain.h"

#include "CommandLine.h"
#include "RunLoop.h"
#include "WebProcess.h"
#include <WebCore/SoftLinking.h>
#include <runtime/InitializeThreading.h>
#include <wtf/Threading.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

#if USE(SAFARI_THEME)
#ifdef DEBUG_ALL
SOFT_LINK_DEBUG_LIBRARY(SafariTheme)
#else
SOFT_LINK_LIBRARY(SafariTheme)
#endif

SOFT_LINK(SafariTheme, STInitialize, void, APIENTRY, (), ())

static void initializeSafariTheme()
{
    static bool didInitializeSafariTheme;
    if (!didInitializeSafariTheme) {
        if (SafariThemeLibrary())
            STInitialize();
        didInitializeSafariTheme = true;
    }
}
#endif // USE(SAFARI_THEME)

int WebProcessMain(const CommandLine& commandLine)
{
    ::OleInitialize(0);

#if USE(SAFARI_THEME)
    initializeSafariTheme();
#endif

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
