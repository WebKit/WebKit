/*
 * Copyright (C) 2012, 2014 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS)

#import "ChildProcess.h"

#import "SandboxInitializationParameters.h"
#import "WebKitSystemInterface.h"
#import <WebCore/FileSystem.h>
#import <WebCore/SystemVersion.h>
#import <mach/mach.h>
#import <mach/task.h>
#import <pwd.h>
#import <stdlib.h>
#import <sysexits.h>

#import <WebCore/FloatingPointEnvironment.h> 

#ifndef ENABLE_MANUAL_SANDBOXING
#define ENABLE_MANUAL_SANDBOXING 0
#endif

#if ENABLE_MANUAL_SANDBOXING

// We have to #undef __APPLE_API_PRIVATE to prevent sandbox.h from looking for a header file that does not exist (<rdar://problem/9679211>).
#undef __APPLE_API_PRIVATE
#import <sandbox.h>

#define SANDBOX_NAMED_EXTERNAL 0x0003
extern "C" int sandbox_init_with_parameters(const char *profile, uint64_t flags, const char *const parameters[], char **errorbuf);

#endif

using namespace WebCore;

namespace WebKit {

void ChildProcess::setApplicationIsDaemon()
{
}

void ChildProcess::platformInitialize()
{
    FloatingPointEnvironment& floatingPointEnvironment = FloatingPointEnvironment::shared(); 
    floatingPointEnvironment.enableDenormalSupport(); 
    floatingPointEnvironment.saveMainThreadEnvironment(); 
    [[NSFileManager defaultManager] changeCurrentDirectoryPath:[[NSBundle mainBundle] bundlePath]];
}

void ChildProcess::initializeSandbox(const ChildProcessInitializationParameters& parameters, SandboxInitializationParameters& sandboxParameters)
{
#if ENABLE_MANUAL_SANDBOXING
    NSBundle *webkit2Bundle = [NSBundle bundleForClass:NSClassFromString(@"WKView")];
    String defaultProfilePath = [webkit2Bundle pathForResource:[[NSBundle mainBundle] bundleIdentifier] ofType:@"sb"];
    if (sandboxParameters.systemDirectorySuffix().isNull()) {
        String defaultSystemDirectorySuffix = String([[NSBundle mainBundle] bundleIdentifier]) + "+" + parameters.clientIdentifier;
        sandboxParameters.setSystemDirectorySuffix(defaultSystemDirectorySuffix);
    }

    String sandboxImportPath = "/usr/local/share/sandbox/imports";
    sandboxParameters.addPathParameter("IMPORT_DIR", fileSystemRepresentation(sandboxImportPath).data());

    switch (sandboxParameters.mode()) {
    case SandboxInitializationParameters::UseDefaultSandboxProfilePath:
    case SandboxInitializationParameters::UseOverrideSandboxProfilePath: {
        String sandboxProfilePath = sandboxParameters.mode() == SandboxInitializationParameters::UseDefaultSandboxProfilePath ? defaultProfilePath : sandboxParameters.overrideSandboxProfilePath();
        if (!sandboxProfilePath.isEmpty()) {
            CString profilePath = fileSystemRepresentation(sandboxProfilePath);
            char* errorBuf;
            if (sandbox_init_with_parameters(profilePath.data(), SANDBOX_NAMED_EXTERNAL, sandboxParameters.namedParameterArray(), &errorBuf)) {
                WTFLogAlways("%s: Couldn't initialize sandbox profile [%s], error '%s'\n", getprogname(), profilePath.data(), errorBuf);
                for (size_t i = 0, count = sandboxParameters.count(); i != count; ++i)
                    WTFLogAlways("%s=%s\n", sandboxParameters.name(i), sandboxParameters.value(i));
                exit(EX_NOPERM);
            }
        }

        break;
    }
    case SandboxInitializationParameters::UseSandboxProfile: {
        char* errorBuf;
        if (sandbox_init_with_parameters(sandboxParameters.sandboxProfile().utf8().data(), 0, sandboxParameters.namedParameterArray(), &errorBuf)) {
            WTFLogAlways("%s: Couldn't initialize sandbox profile, error '%s'\n", getprogname(), errorBuf);
            for (size_t i = 0, count = sandboxParameters.count(); i != count; ++i)
                WTFLogAlways("%s=%s\n", sandboxParameters.name(i), sandboxParameters.value(i));
            exit(EX_NOPERM);
        }

        break;
    }
    }
#else
    UNUSED_PARAM(parameters);
    UNUSED_PARAM(sandboxParameters);
    RELEASE_ASSERT_NOT_REACHED();
#endif
}

void ChildProcess::setQOS(int, int)
{

}

} // namespace WebKit

#endif
