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
#import "WebProcessInitialization.h"

#import "WebProcess.h"
#import "WebSystemInterface.h"
#import <WebCore/LocalizedStrings.h>
#import <WebCore/RunLoop.h>
#import <WebKitSystemInterface.h>
#import <runtime/InitializeThreading.h>
#import <wtf/MainThread.h>

using namespace WebCore;

namespace WebKit {

void initializeWebProcess(const WebProcessInitializationParameters& parameters)
{
    @autoreleasepool {
        InitWebCoreSystemInterface();
        JSC::initializeThreading();
        WTF::initializeMainThread();
        RunLoop::initializeMainRunLoop();

        if (!parameters.uiProcessName.isNull()) {
            NSString *applicationName = [NSString stringWithFormat:WEB_UI_STRING("%@ Web Content", "Visible name of the web process. The argument is the application name."), (NSString *)parameters.uiProcessName];
            WKSetVisibleApplicationName((CFStringRef)applicationName);
        }

        WebProcess& webProcess = WebProcess::shared();
        webProcess.initializeShim();
        webProcess.initializeSandbox(parameters.clientIdentifier);
        webProcess.initializeConnection(parameters.connectionIdentifier);
        
        WKAXRegisterRemoteApp();
    }
}

} // namespace WebKit
