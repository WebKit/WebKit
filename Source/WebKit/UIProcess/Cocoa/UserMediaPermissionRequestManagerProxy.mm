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

#import "MediaPermissionUtilities.h"
#import "SandboxUtilities.h"
#import "TCCSPI.h"
#import "WebPageProxy.h"
#import "WebPreferences.h"
#import <WebCore/RuntimeApplicationChecks.h>
#import <wtf/BlockPtr.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>

#import <pal/cocoa/AVFoundationSoftLink.h>
#import <wtf/spi/darwin/SandboxSPI.h>

namespace WebKit {

bool UserMediaPermissionRequestManagerProxy::permittedToCaptureAudio()
{
#if ENABLE(MEDIA_STREAM)
    return checkSandboxRequirementForType(MediaPermissionType::Audio) && checkUsageDescriptionStringForType(MediaPermissionType::Audio);
#else
    return false;
#endif
}

bool UserMediaPermissionRequestManagerProxy::permittedToCaptureVideo()
{
#if ENABLE(MEDIA_STREAM)
    return checkSandboxRequirementForType(MediaPermissionType::Video) && checkUsageDescriptionStringForType(MediaPermissionType::Video);
#else
    return false;
#endif
}

#if ENABLE(MEDIA_STREAM)
void UserMediaPermissionRequestManagerProxy::requestSystemValidation(const WebPageProxy& page, UserMediaPermissionRequestProxy& request, CompletionHandler<void(bool)>&& callback)
{
    if (page.preferences().mockCaptureDevicesEnabled()) {
        callback(true);
        return;
    }

    // FIXME: Add TCC entitlement check for screensharing.
    auto audioStatus = request.requiresAudioCapture() ? checkAVCaptureAccessForType(MediaPermissionType::Audio) : MediaPermissionResult::Granted;
    if (audioStatus == MediaPermissionResult::Denied) {
        callback(false);
        return;
    }

    auto videoStatus = request.requiresVideoCapture() ? checkAVCaptureAccessForType(MediaPermissionType::Video) : MediaPermissionResult::Granted;
    if (videoStatus == MediaPermissionResult::Denied) {
        callback(false);
        return;
    }

    if (audioStatus == MediaPermissionResult::Unknown) {
        requestAVCaptureAccessForType(MediaPermissionType::Audio, [videoStatus, completionHandler = WTFMove(callback)](bool authorized) mutable {
            if (videoStatus == MediaPermissionResult::Granted) {
                completionHandler(authorized);
                return;
            }
                
            requestAVCaptureAccessForType(MediaPermissionType::Video, WTFMove(completionHandler));
        });
        return;
    }

    if (videoStatus == MediaPermissionResult::Unknown) {
        requestAVCaptureAccessForType(MediaPermissionType::Video, WTFMove(callback));
        return;
    }

    callback(true);
}
#endif // ENABLE(MEDIA_STREAM)

} // namespace WebKit
