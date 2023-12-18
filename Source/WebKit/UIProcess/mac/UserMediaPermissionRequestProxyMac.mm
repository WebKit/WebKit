/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "UserMediaPermissionRequestProxyMac.h"

#import "DisplayCaptureSessionManager.h"
#import "UserMediaPermissionRequestManagerProxy.h"
#import "WebPreferences.h"

namespace WebKit {
using namespace WebCore;

Ref<UserMediaPermissionRequestProxy> UserMediaPermissionRequestProxy::create(UserMediaPermissionRequestManagerProxy& manager, UserMediaRequestIdentifier userMediaID, FrameIdentifier mainFrameID, FrameIdentifier frameID, Ref<SecurityOrigin>&& userMediaDocumentOrigin, Ref<SecurityOrigin>&& topLevelDocumentOrigin, Vector<CaptureDevice>&& audioDevices, Vector<CaptureDevice>&& videoDevices, MediaStreamRequest&& request, CompletionHandler<void(bool)>&& decisionCompletionHandler)
{
    return adoptRef(*new UserMediaPermissionRequestProxyMac(manager, userMediaID, mainFrameID, frameID, WTFMove(userMediaDocumentOrigin), WTFMove(topLevelDocumentOrigin), WTFMove(audioDevices), WTFMove(videoDevices), WTFMove(request), WTFMove(decisionCompletionHandler)));
}

UserMediaPermissionRequestProxyMac::UserMediaPermissionRequestProxyMac(UserMediaPermissionRequestManagerProxy& manager, UserMediaRequestIdentifier userMediaID, FrameIdentifier mainFrameID, FrameIdentifier frameID, Ref<SecurityOrigin>&& userMediaDocumentOrigin, Ref<SecurityOrigin>&& topLevelDocumentOrigin, Vector<CaptureDevice>&& audioDevices, Vector<CaptureDevice>&& videoDevices, MediaStreamRequest&& request, CompletionHandler<void(bool)>&& decisionCompletionHandler)
    : UserMediaPermissionRequestProxy(manager, userMediaID, mainFrameID, frameID, WTFMove(userMediaDocumentOrigin), WTFMove(topLevelDocumentOrigin), WTFMove(audioDevices), WTFMove(videoDevices), WTFMove(request), WTFMove(decisionCompletionHandler))
{
}

UserMediaPermissionRequestProxyMac::~UserMediaPermissionRequestProxyMac()
{
}

void UserMediaPermissionRequestProxyMac::promptForGetDisplayMedia(UserMediaDisplayCapturePromptType promptType)
{
#if ENABLE(MEDIA_STREAM)
    if (!manager())
        return;

    DisplayCaptureSessionManager::singleton().promptForGetDisplayMedia(promptType, Ref { manager()->page() }, topLevelDocumentSecurityOrigin().data(), [protectedThis = Ref { *this }](std::optional<CaptureDevice> device) mutable {

        if (!device) {
            protectedThis->deny(UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::PermissionDenied);
            return;
        }

        protectedThis->setEligibleVideoDeviceUIDs({ device.value() });
        protectedThis->allow(String(), device.value().persistentId());
    });
#else
    ASSERT_NOT_REACHED();
#endif
}

bool UserMediaPermissionRequestProxyMac::canRequestDisplayCapturePermission()
{
#if ENABLE(MEDIA_STREAM)
    auto overridePreference = DisplayCaptureSessionManager::singleton().overrideCanRequestDisplayCapturePermissionForTesting();
    if (!manager() || (!overridePreference && manager()->page().preferences().requireUAGetDisplayMediaPrompt()))
        return false;

    return DisplayCaptureSessionManager::singleton().canRequestDisplayCapturePermission();
#else
    return false;
#endif
}

} // namespace WebKit
