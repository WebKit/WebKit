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

Ref<UserMediaPermissionRequestProxy> UserMediaPermissionRequestProxy::create(UserMediaPermissionRequestManagerProxy& manager, std::optional<UserMediaRequestIdentifier> userMediaID, FrameIdentifier mainFrameID, FrameInfoData&& frameInfo, Ref<SecurityOrigin>&& userMediaDocumentOrigin, Ref<SecurityOrigin>&& topLevelDocumentOrigin, Vector<CaptureDevice>&& audioDevices, Vector<CaptureDevice>&& videoDevices, MediaStreamRequest&& request, CompletionHandler<void(bool)>&& decisionCompletionHandler)
{
    return adoptRef(*new UserMediaPermissionRequestProxyMac(manager, userMediaID, mainFrameID, WTF::move(frameInfo), WTF::move(userMediaDocumentOrigin), WTF::move(topLevelDocumentOrigin), WTF::move(audioDevices), WTF::move(videoDevices), WTF::move(request), WTF::move(decisionCompletionHandler)));
}

UserMediaPermissionRequestProxyMac::UserMediaPermissionRequestProxyMac(UserMediaPermissionRequestManagerProxy& manager, std::optional<UserMediaRequestIdentifier> userMediaID, FrameIdentifier mainFrameID, FrameInfoData&& frameInfo, Ref<SecurityOrigin>&& userMediaDocumentOrigin, Ref<SecurityOrigin>&& topLevelDocumentOrigin, Vector<CaptureDevice>&& audioDevices, Vector<CaptureDevice>&& videoDevices, MediaStreamRequest&& request, CompletionHandler<void(bool)>&& decisionCompletionHandler)
    : UserMediaPermissionRequestProxy(manager, userMediaID, mainFrameID, WTF::move(frameInfo), WTF::move(userMediaDocumentOrigin), WTF::move(topLevelDocumentOrigin), WTF::move(audioDevices), WTF::move(videoDevices), WTF::move(request), WTF::move(decisionCompletionHandler))
{
}

UserMediaPermissionRequestProxyMac::~UserMediaPermissionRequestProxyMac()
{
}

void UserMediaPermissionRequestProxyMac::invalidate()
{
#if ENABLE(MEDIA_STREAM)
    if (m_hasPendingGetDisplayMediaPrompt) {
        if (RefPtr page = protect(manager())->page())
            DisplayCaptureSessionManager::singleton().cancelGetDisplayMediaPrompt(*page);
        m_hasPendingGetDisplayMediaPrompt = false;
    }
#endif
    UserMediaPermissionRequestProxy::invalidate();
}

void UserMediaPermissionRequestProxyMac::promptForGetDisplayMedia(UserMediaDisplayCapturePromptType promptType)
{
#if ENABLE(MEDIA_STREAM)
    if (!manager())
        return;

    RefPtr page = protect(manager())->page();
    if (!page)
        return;

    m_hasPendingGetDisplayMediaPrompt = true;
    DisplayCaptureSessionManager::singleton().promptForGetDisplayMedia(promptType, *page, topLevelDocumentSecurityOrigin().data(), [protectedThis = Ref { *this }](std::optional<CaptureDevice> device) mutable {

        protectedThis->m_hasPendingGetDisplayMediaPrompt = false;

        if (!device) {
            protectedThis->deny(UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::PermissionDenied);
            return;
        }

        protectedThis->setEligibleVideoDevices({ device.value() });
        protectedThis->allow(String(), device.value().persistentId());
    });
#else
    ASSERT_NOT_REACHED();
#endif
}

bool UserMediaPermissionRequestProxyMac::canRequestDisplayCapturePermission()
{
#if ENABLE(MEDIA_STREAM)

    if (!DisplayCaptureSessionManager::singleton().overrideCanRequestDisplayCapturePermissionForTesting())
        return false;

    return DisplayCaptureSessionManager::singleton().canRequestDisplayCapturePermission();
#else
    return false;
#endif
}

} // namespace WebKit
