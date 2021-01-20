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

namespace WebKit {

void AuxiliaryProcess::platformInitialize()
{
    FloatingPointEnvironment& floatingPointEnvironment = FloatingPointEnvironment::singleton(); 
    floatingPointEnvironment.enableDenormalSupport(); 
    floatingPointEnvironment.saveMainThreadEnvironment(); 
    [[NSFileManager defaultManager] changeCurrentDirectoryPath:[[NSBundle mainBundle] bundlePath]];
}

void AuxiliaryProcess::initializeSandbox(const AuxiliaryProcessInitializationParameters&, SandboxInitializationParameters&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

void AuxiliaryProcess::setQOS(int, int)
{

}

} // namespace WebKit

#endif
