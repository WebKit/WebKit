/*
 * Copyright (C) 2010, 2012 Apple Inc. All rights reserved.
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
#import "SharedWorkerProcess.h"

#if ENABLE(SHARED_WORKER_PROCESS)

#import "SharedWorkerProcessProxyMessages.h"
#import "SharedWorkerProcessCreationParameters.h"
#import <WebCore/LocalizedStrings.h>
#import <WebKitSystemInterface.h>
#import <dlfcn.h>
#import <objc/runtime.h>
#import <sysexits.h>
#import <wtf/HashSet.h>

// We have to #undef __APPLE_API_PRIVATE to prevent sandbox.h from looking for a header file that does not exist (<rdar://problem/9679211>). 
#undef __APPLE_API_PRIVATE
#import <sandbox.h>

#define SANDBOX_NAMED_EXTERNAL 0x0003
extern "C" int sandbox_init_with_parameters(const char *profile, uint64_t flags, const char *const parameters[], char **errorbuf);

namespace WebKit {

static void initializeSandbox()
{
    NSBundle *webkit2Bundle = [NSBundle bundleForClass:NSClassFromString(@"WKView")];
    const char* profilePath = [[webkit2Bundle pathForResource:@"com.apple.WebKit.SharedWorkerProcess" ofType:@"sb"] fileSystemRepresentation];

    char* errorBuf;
    if (sandbox_init_with_parameters(profilePath, SANDBOX_NAMED_EXTERNAL, 0, &errorBuf)) {
        WTFLogAlways("SharedWorkerProcess: couldn't initialize sandbox profile [%s] error '%s'\n", profilePath, errorBuf);
        exit(EX_NOPERM);
    }
}

void SharedWorkerProcess::platformInitializeSharedWorkerProcess(const SharedWorkerProcessCreationParameters& parameters)
{
    NSString *applicationName = [NSString stringWithFormat:WEB_UI_STRING("Shared Web Worker (%@ Internet plug-in)",
        "visible name of the plug-in host process. The argument is the application name."),
        (NSString *)parameters.parentProcessName];
    
    WKSetVisibleApplicationName((CFStringRef)applicationName);

    WebKit::initializeSandbox();
}

} // namespace WebKit

#endif // ENABLE(SHARED_WORKER_PROCESS)
