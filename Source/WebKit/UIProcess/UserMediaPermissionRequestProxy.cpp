/*
 * Copyright (C) 2014 Igalia S.L.
 * Copyright (C) 2016-2022 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "UserMediaPermissionRequestProxy.h"

#include "MediaPermissionUtilities.h"
#include "UserMediaPermissionRequestManagerProxy.h"
#include <WebCore/CaptureDeviceManager.h>
#include <WebCore/RealtimeMediaSourceCenter.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SecurityOriginData.h>
#include <wtf/text/StringHash.h>

namespace WebKit {
using namespace WebCore;

#if !PLATFORM(COCOA)
Ref<UserMediaPermissionRequestProxy> UserMediaPermissionRequestProxy::create(UserMediaPermissionRequestManagerProxy& manager, WebCore::UserMediaRequestIdentifier userMediaID, WebCore::FrameIdentifier mainFrameID, WebCore::FrameIdentifier frameID, Ref<WebCore::SecurityOrigin>&& userMediaDocumentOrigin, Ref<WebCore::SecurityOrigin>&& topLevelDocumentOrigin, Vector<WebCore::CaptureDevice>&& audioDevices, Vector<WebCore::CaptureDevice>&& videoDevices, WebCore::MediaStreamRequest&& request, CompletionHandler<void(bool)>&& decisionCompletionHandler)
{
    return adoptRef(*new UserMediaPermissionRequestProxy(manager, userMediaID, mainFrameID, frameID, WTFMove(userMediaDocumentOrigin), WTFMove(topLevelDocumentOrigin), WTFMove(audioDevices), WTFMove(videoDevices), WTFMove(request), WTFMove(decisionCompletionHandler)));
}
#endif

UserMediaPermissionRequestProxy::UserMediaPermissionRequestProxy(UserMediaPermissionRequestManagerProxy& manager, UserMediaRequestIdentifier userMediaID, FrameIdentifier mainFrameID, FrameIdentifier frameID, Ref<WebCore::SecurityOrigin>&& userMediaDocumentOrigin, Ref<WebCore::SecurityOrigin>&& topLevelDocumentOrigin, Vector<WebCore::CaptureDevice>&& audioDevices, Vector<WebCore::CaptureDevice>&& videoDevices, WebCore::MediaStreamRequest&& request, CompletionHandler<void(bool)>&& decisionCompletionHandler)
    : m_manager(&manager)
    , m_userMediaID(userMediaID)
    , m_mainFrameID(mainFrameID)
    , m_frameID(frameID)
    , m_userMediaDocumentSecurityOrigin(WTFMove(userMediaDocumentOrigin))
    , m_topLevelDocumentSecurityOrigin(WTFMove(topLevelDocumentOrigin))
    , m_eligibleVideoDevices(WTFMove(videoDevices))
    , m_eligibleAudioDevices(WTFMove(audioDevices))
    , m_request(WTFMove(request))
    , m_decisionCompletionHandler(WTFMove(decisionCompletionHandler))
{
}

#if ENABLE(MEDIA_STREAM)
static inline void setDeviceAsFirst(Vector<CaptureDevice>& devices, const String& deviceID)
{
    size_t index = devices.findIf([&deviceID](const auto& device) {
        return device.persistentId() == deviceID;
    });
    ASSERT(index != notFound);

    if (index) {
        auto device = devices[index];
        ASSERT(device.enabled());

        devices.remove(index);
        devices.insert(0, WTFMove(device));
    }
}
#endif

void UserMediaPermissionRequestProxy::allow(const String& audioDeviceUID, const String& videoDeviceUID)
{
#if ENABLE(MEDIA_STREAM)
    if (!audioDeviceUID.isEmpty())
        setDeviceAsFirst(m_eligibleAudioDevices, audioDeviceUID);
    if (!videoDeviceUID.isEmpty())
        setDeviceAsFirst(m_eligibleVideoDevices, videoDeviceUID);
#else
    UNUSED_PARAM(audioDeviceUID);
    UNUSED_PARAM(videoDeviceUID);
#endif

    allow();
}

void UserMediaPermissionRequestProxy::allow()
{
    ASSERT(m_manager);
    if (!m_manager)
        return;

    m_manager->grantRequest(*this);
    invalidate();
}

void UserMediaPermissionRequestProxy::deny(UserMediaAccessDenialReason reason)
{
    if (!m_manager)
        return;

    m_manager->denyRequest(*this, reason);
    invalidate();
}

void UserMediaPermissionRequestProxy::invalidate()
{
    m_manager = nullptr;
    if (m_decisionCompletionHandler)
        m_decisionCompletionHandler(false);
}

Vector<String> UserMediaPermissionRequestProxy::videoDeviceUIDs() const
{
    return WTF::map(m_eligibleVideoDevices, [] (auto& device) {
        return device.persistentId();
    });
}

Vector<String> UserMediaPermissionRequestProxy::audioDeviceUIDs() const
{
    return WTF::map(m_eligibleAudioDevices, [] (auto& device) {
        return device.persistentId();
    });
}

String convertEnumerationToString(UserMediaPermissionRequestProxy::UserMediaAccessDenialReason enumerationValue)
{
    static const NeverDestroyed<String> values[] = {
        MAKE_STATIC_STRING_IMPL("NoConstraints"),
        MAKE_STATIC_STRING_IMPL("UserMediaDisabled"),
        MAKE_STATIC_STRING_IMPL("NoCaptureDevices"),
        MAKE_STATIC_STRING_IMPL("InvalidConstraint"),
        MAKE_STATIC_STRING_IMPL("HardwareError"),
        MAKE_STATIC_STRING_IMPL("PermissionDenied"),
        MAKE_STATIC_STRING_IMPL("OtherFailure"),
    };
    static_assert(static_cast<size_t>(UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::NoConstraints) == 0, "UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::NoConstraints is not 0 as expected");
    static_assert(static_cast<size_t>(UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::UserMediaDisabled) == 1, "UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::UserMediaDisabled is not 1 as expected");
    static_assert(static_cast<size_t>(UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::NoCaptureDevices) == 2, "UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::NoCaptureDevices is not 2 as expected");
    static_assert(static_cast<size_t>(UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::InvalidConstraint) == 3, "UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::InvalidConstraint is not 3 as expected");
    static_assert(static_cast<size_t>(UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::HardwareError) == 4, "UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::HardwareError is not 4 as expected");
    static_assert(static_cast<size_t>(UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::PermissionDenied) == 5, "UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::PermissionDenied is not 5 as expected");
    static_assert(static_cast<size_t>(UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::OtherFailure) == 6, "UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::OtherFailure is not 6 as expected");
    ASSERT(static_cast<size_t>(enumerationValue) < std::size(values));
    return values[static_cast<size_t>(enumerationValue)];
}

void UserMediaPermissionRequestProxy::promptForGetDisplayMedia(UserMediaDisplayCapturePromptType promptType)
{
#if ENABLE(MEDIA_STREAM) && PLATFORM(COCOA)
    ASSERT(m_manager && canPromptForGetDisplayMedia() && promptType != UserMediaDisplayCapturePromptType::UserChoose);
    if (!m_manager || !canPromptForGetDisplayMedia() || promptType != UserMediaDisplayCapturePromptType::UserChoose) {
        deny(UserMediaAccessDenialReason::PermissionDenied);
        return;
    }

    alertForPermission(m_manager->page(), MediaPermissionReason::ScreenCapture, topLevelDocumentSecurityOrigin().data(), [this, protectedThis = Ref { *this }](bool granted) {
        if (!granted)
            deny(UserMediaAccessDenialReason::PermissionDenied);
        else
            allow();
    });
#endif
}

void UserMediaPermissionRequestProxy::promptForGetUserMedia()
{
#if ENABLE(MEDIA_STREAM) && PLATFORM(COCOA)
    ASSERT(m_manager);
    if (!m_manager) {
        deny(UserMediaAccessDenialReason::PermissionDenied);
        return;
    }

    MediaPermissionReason reason = MediaPermissionReason::Camera;
    if (requiresAudioCapture())
        reason = requiresVideoCapture() ? MediaPermissionReason::CameraAndMicrophone : MediaPermissionReason::Microphone;

    alertForPermission(m_manager->page(), reason, topLevelDocumentSecurityOrigin().data(), [this, protectedThis = Ref { *this }](bool granted) {
        if (!granted)
            deny(UserMediaAccessDenialReason::PermissionDenied);
        else
            allow();
    });
#endif
}

void UserMediaPermissionRequestProxy::doDefaultAction()
{
#if ENABLE(MEDIA_STREAM) && PLATFORM(COCOA)
    if (requiresDisplayCapture()) {
        if (!canPromptForGetDisplayMedia()) {
            deny(UserMediaAccessDenialReason::PermissionDenied);
            return;
        }

        promptForGetDisplayMedia();
    } else
        promptForGetUserMedia();
#else
    deny();
#endif
}

bool UserMediaPermissionRequestProxy::canPromptForGetDisplayMedia()
{
#if ENABLE(MEDIA_STREAM) && PLATFORM(IOS)
    return true;
#else
    return false;
#endif
}

} // namespace WebKit
