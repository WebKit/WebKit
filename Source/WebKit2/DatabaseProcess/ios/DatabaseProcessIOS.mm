/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS) && ENABLE(DATABASE_PROCESS)

#import "DatabaseProcess.h"

#import "SandboxInitializationParameters.h"
#import <WebCore/FileSystem.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/NotImplemented.h>
#import <WebKitSystemInterface.h>

using namespace WebCore;

#define ENABLE_MANUAL_DATABASE_SANDBOXING 0

namespace WebKit {

void DatabaseProcess::initializeProcess(const ChildProcessInitializationParameters&)
{
}

void DatabaseProcess::initializeProcessName(const ChildProcessInitializationParameters& parameters)
{
}

void DatabaseProcess::initializeSandbox(const ChildProcessInitializationParameters& parameters, SandboxInitializationParameters& sandboxParameters)
{
#if ENABLE_MANUAL_DATABASE_SANDBOXING
    // Need to override the default, because service has a different bundle ID.
    NSBundle *webkit2Bundle = [NSBundle bundleForClass:NSClassFromString(@"WKView")];
    sandboxParameters.setOverrideSandboxProfilePath([webkit2Bundle pathForResource:@"com.apple.WebKit.Databases" ofType:@"sb"]);
    ChildProcess::initializeSandbox(parameters, sandboxParameters);
#endif
}

} // namespace WebKit

#endif // PLATFORM(IOS) && ENABLE(DATABASE_PROCESS)
