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
#import <pal/spi/ios/MobileGestaltSPI.h>
#import <pwd.h>
#import <stdlib.h>
#import <sysexits.h>
#import <wtf/FileSystem.h>

namespace WebKit {

void AuxiliaryProcess::initializeSandbox(const AuxiliaryProcessInitializationParameters&, SandboxInitializationParameters&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

void AuxiliaryProcess::setQOS(int, int)
{

}

void AuxiliaryProcess::populateMobileGestaltCache(std::optional<SandboxExtension::Handle>&& mobileGestaltExtensionHandle)
{
    if (!mobileGestaltExtensionHandle)
        return;

    if (auto extension = SandboxExtension::create(WTF::move(*mobileGestaltExtensionHandle))) {
        bool ok = extension->consume();
        ASSERT_UNUSED(ok, ok);
        // If we have an extension handle for MobileGestalt, it means the MobileGestalt cache is invalid.
        // In this case, we perform a set of MobileGestalt queries while having access to the daemon,
        // which will populate the MobileGestalt in-memory cache with correct values.
        // The set of queries below was determined by finding all possible queries that have cachable
        // values, and would reach out to the daemon for the answer. That way, the in-memory cache
        // should be identical to a valid MobileGestalt cache after having queried all of these values.
        MGGetFloat32Answer(kMGQMainScreenScale, 0);
        MGGetSInt32Answer(kMGQMainScreenPitch, 0);
        MGGetSInt32Answer(kMGQMainScreenClass, MGScreenClassPad2);
        MGGetBoolAnswer(kMGQAppleInternalInstallCapability);
        MGGetBoolAnswer(kMGQiPadCapability);
        auto deviceName = adoptCF(MGCopyAnswer(kMGQDeviceName, nullptr));
        MGGetSInt32Answer(kMGQDeviceClassNumber, MGDeviceClassInvalid);
        MGGetBoolAnswer(kMGQHasExtendedColorDisplay);
        MGGetFloat32Answer(kMGQDeviceCornerRadius, 0);
        MGGetBoolAnswer(kMGQSupportsForceTouch);

        auto answer = adoptCF(MGCopyAnswer(kMGQBluetoothCapability, nullptr));
        answer = adoptCF(MGCopyAnswer(kMGQDeviceProximityCapability, nullptr));
        answer = adoptCF(MGCopyAnswer(kMGQDeviceSupportsARKit, nullptr));
        answer = adoptCF(MGCopyAnswer(kMGQTimeSyncCapability, nullptr));
        answer = adoptCF(MGCopyAnswer(kMGQWAPICapability, nullptr));
        answer = adoptCF(MGCopyAnswer(kMGQMainDisplayRotation, nullptr));

        ok = extension->revoke();
        ASSERT_UNUSED(ok, ok);
    }
}

} // namespace WebKit

#endif
