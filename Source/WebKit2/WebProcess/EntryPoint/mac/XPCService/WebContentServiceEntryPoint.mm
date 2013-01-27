/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#import "config.h"

#if HAVE(XPC)

#import "EnvironmentUtilities.h"
#import "WKBase.h"
#import "WebKit2Initialize.h"
#import "WebProcess.h"
#import <WebCore/RunLoop.h>
#import <stdio.h>
#import <stdlib.h>
#import <xpc/xpc.h>

using namespace WebCore;
using namespace WebKit;

extern "C" WK_EXPORT void initializeWebContentService(const char* clientIdentifier, xpc_connection_t connection, mach_port_t serverPort, const char* uiProcessName);

void initializeWebContentService(const char* clientIdentifier, xpc_connection_t connection, mach_port_t serverPort, const char* uiProcessName)
{
    // Remove the WebProcess shim from the DYLD_INSERT_LIBRARIES environment variable so any processes spawned by
    // the WebProcess don't try to insert the shim and crash.
    EnvironmentUtilities::stripValuesEndingWithString("DYLD_INSERT_LIBRARIES", "/SecItemShim.dylib");

    RunLoop::setUseApplicationRunLoopOnMainRunLoop();
    InitializeWebKit2();

    ChildProcessInitializationParameters parameters;
    parameters.uiProcessName = uiProcessName;
    parameters.clientIdentifier = clientIdentifier;
    parameters.connectionIdentifier = CoreIPC::Connection::Identifier(serverPort, connection);

    WebProcess::shared().initialize(parameters);
}

#endif // HAVE(XPC)
