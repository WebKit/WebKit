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
#import "AuxiliaryProcess.h"

#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)

#import "SandboxInitializationParameters.h"
#import "XPCServiceEntryPoint.h"
#import <WebCore/FloatingPointEnvironment.h>
#import <WebCore/SystemVersion.h>
#import <mach/mach.h>
#import <mach/task.h>
#import <pwd.h>
#import <stdlib.h>
#import <sysexits.h>
#import <wtf/FileSystem.h>

#if ENABLE(MANUAL_SANDBOXING)
#import <wtf/spi/darwin/SandboxSPI.h>
#endif

namespace WebKit {

void AuxiliaryProcess::platformInitialize()
{
    FloatingPointEnvironment& floatingPointEnvironment = FloatingPointEnvironment::singleton(); 
    floatingPointEnvironment.enableDenormalSupport(); 
    floatingPointEnvironment.saveMainThreadEnvironment(); 
    [[NSFileManager defaultManager] changeCurrentDirectoryPath:[[NSBundle mainBundle] bundlePath]];
}

void AuxiliaryProcess::initializeSandbox(const AuxiliaryProcessInitializationParameters& parameters, SandboxInitializationParameters& sandboxParameters)
{
#if ENABLE(MANUAL_SANDBOXING)
    NSBundle *webkit2Bundle = [NSBundle bundleForClass:NSClassFromString(@"WKWebView")];
    String defaultProfilePath = [webkit2Bundle pathForResource:[[NSBundle mainBundle] bundleIdentifier] ofType:@"sb"];
    if (sandboxParameters.userDirectorySuffix().isNull()) {
        String defaultUserDirectorySuffix = makeString(String([[NSBundle mainBundle] bundleIdentifier]), '+', parameters.clientIdentifier);
        sandboxParameters.setUserDirectorySuffix(defaultUserDirectorySuffix);
    }

#if !PLATFORM(MACCATALYST)
    String sandboxImportPath = "/usr/local/share/sandbox/imports";
    sandboxParameters.addPathParameter("IMPORT_DIR", FileSystem::fileSystemRepresentation(sandboxImportPath).data());
#endif

    switch (sandboxParameters.mode()) {
    case SandboxInitializationParameters::UseDefaultSandboxProfilePath:
    case SandboxInitializationParameters::UseOverrideSandboxProfilePath: {
        String sandboxProfilePath = sandboxParameters.mode() == SandboxInitializationParameters::UseDefaultSandboxProfilePath ? defaultProfilePath : sandboxParameters.overrideSandboxProfilePath();
        if (!sandboxProfilePath.isEmpty()) {
            CString profilePath = FileSystem::fileSystemRepresentation(sandboxProfilePath);
            char* errorBuf;
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            if (sandbox_init_with_parameters(profilePath.data(), SANDBOX_NAMED_EXTERNAL, sandboxParameters.namedParameterArray(), &errorBuf)) {
                ALLOW_DEPRECATED_DECLARATIONS_END
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
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        if (sandbox_init_with_parameters(sandboxParameters.sandboxProfile().utf8().data(), 0, sandboxParameters.namedParameterArray(), &errorBuf)) {
            ALLOW_DEPRECATED_DECLARATIONS_END
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

void AuxiliaryProcess::setQOS(int, int)
{

}

} // namespace WebKit

#endif
