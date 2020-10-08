/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#import "UserMediaPermissionRequestManagerProxy.h"

#import "SandboxUtilities.h"
#import "TCCSPI.h"
#import <wtf/SoftLinking.h>
#import <wtf/spi/darwin/SandboxSPI.h>

SOFT_LINK_PRIVATE_FRAMEWORK(TCC)
SOFT_LINK(TCC, TCCAccessPreflight, TCCAccessPreflightResult, (CFStringRef service, CFDictionaryRef options), (service, options))
SOFT_LINK(TCC, TCCAccessPreflightWithAuditToken, TCCAccessPreflightResult, (CFStringRef service, audit_token_t token, CFDictionaryRef options), (service, token, options))
SOFT_LINK_CONSTANT(TCC, kTCCServiceMicrophone, CFStringRef)
SOFT_LINK_CONSTANT(TCC, kTCCServiceCamera, CFStringRef)

namespace WebKit {

bool UserMediaPermissionRequestManagerProxy::permittedToCaptureAudio()
{
#if ENABLE(MEDIA_STREAM)

#if PLATFORM(MAC)
    static std::once_flag onceFlag;
    static bool entitled = true;
    std::call_once(onceFlag, [] {
        if (!currentProcessIsSandboxed())
            return;

        int result = sandbox_check(getpid(), "device-microphone", SANDBOX_FILTER_NONE);
        entitled = !result;
        if (result == -1)
            WTFLogAlways("Error checking 'device-microphone' sandbox access, errno=%ld", (long)errno);
    });
    if (!entitled)
        return false;
#endif // PLATFORM(MAC)

    static TCCAccessPreflightResult access = TCCAccessPreflight(getkTCCServiceMicrophone(), NULL);
    if (access == kTCCAccessPreflightGranted)
        return true;

    static bool isPermitted = dynamic_objc_cast<NSString>(NSBundle.mainBundle.infoDictionary[@"NSMicrophoneUsageDescription"]).length;
    return isPermitted;
#else
    return false;
#endif
}

bool UserMediaPermissionRequestManagerProxy::permittedToCaptureVideo()
{
#if ENABLE(MEDIA_STREAM)

#if PLATFORM(MAC)
    static std::once_flag onceFlag;
    static bool entitled = true;
    std::call_once(onceFlag, [] {
        if (!currentProcessIsSandboxed())
            return;

        int result = sandbox_check(getpid(), "device-camera", SANDBOX_FILTER_NONE);
        entitled = !result;
        if (result == -1)
            WTFLogAlways("Error checking 'device-camera' sandbox access, errno=%ld", (long)errno);
    });
    if (!entitled)
        return false;
#endif // PLATFORM(MAC)

    static TCCAccessPreflightResult access = TCCAccessPreflight(getkTCCServiceCamera(), NULL);
    if (access == kTCCAccessPreflightGranted)
        return true;

    static bool isPermitted = dynamic_objc_cast<NSString>(NSBundle.mainBundle.infoDictionary[@"NSCameraUsageDescription"]).length;
    return isPermitted;
#else
    return false;
#endif
}

} // namespace WebKit
