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
#include "ThreadLauncher.h"

#include "WebProcess.h"
#include <WebCore/RunLoop.h>
#include <runtime/InitializeThreading.h>
#include <wtf/MainThread.h>
#include <wtf/Threading.h>

using namespace WebCore;

namespace WebKit {

static void webThreadBody(void* context)
{
    HANDLE clientIdentifier = reinterpret_cast<HANDLE>(context);

    // Initialization
    JSC::initializeThreading();
    WTF::initializeMainThread();

    WebProcess::shared().initialize(clientIdentifier, RunLoop::current());
    RunLoop::run();
}

CoreIPC::Connection::Identifier ThreadLauncher::createWebThread()
{
    // First, create the server and client identifiers.
    HANDLE serverIdentifier, clientIdentifier;
    if (!CoreIPC::Connection::createServerAndClientIdentifiers(serverIdentifier, clientIdentifier)) {
        // FIXME: What should we do here?
        ASSERT_NOT_REACHED();
    }

    if (!createThread(webThreadBody, reinterpret_cast<void*>(clientIdentifier), "WebKit2: WebThread")) {
        ::CloseHandle(serverIdentifier);
        ::CloseHandle(clientIdentifier);
        return 0;
    }

    return serverIdentifier;
}

} // namespace WebKit
