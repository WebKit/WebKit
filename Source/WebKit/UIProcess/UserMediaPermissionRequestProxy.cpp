/*
 * Copyright (C) 2014 Igalia S.L.
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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

#include "UserMediaPermissionRequestManagerProxy.h"
#include <WebCore/CaptureDeviceManager.h>
#include <WebCore/RealtimeMediaSourceCenter.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SecurityOriginData.h>
#include <wtf/text/StringHash.h>

namespace WebKit {
using namespace WebCore;

UserMediaPermissionRequestProxy::UserMediaPermissionRequestProxy(UserMediaPermissionRequestManagerProxy& manager, uint64_t userMediaID, FrameIdentifier mainFrameID, FrameIdentifier frameID, Ref<WebCore::SecurityOrigin>&& userMediaDocumentOrigin, Ref<WebCore::SecurityOrigin>&& topLevelDocumentOrigin, Vector<WebCore::CaptureDevice>&& audioDevices, Vector<WebCore::CaptureDevice>&& videoDevices, WebCore::MediaStreamRequest&& request)
    : m_manager(&manager)
    , m_userMediaID(userMediaID)
    , m_mainFrameID(mainFrameID)
    , m_frameID(frameID)
    , m_userMediaDocumentSecurityOrigin(WTFMove(userMediaDocumentOrigin))
    , m_topLevelDocumentSecurityOrigin(WTFMove(topLevelDocumentOrigin))
    , m_eligibleVideoDevices(WTFMove(videoDevices))
    , m_eligibleAudioDevices(WTFMove(audioDevices))
    , m_request(WTFMove(request))
{
}

#if ENABLE(MEDIA_STREAM)
static inline void setDeviceAsFirst(Vector<CaptureDevice>& devices, const String& deviceID)
{
    size_t index = devices.findMatching([&deviceID](const auto& device) {
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

#if !PLATFORM(COCOA)
void UserMediaPermissionRequestProxy::doDefaultAction()
{
    deny();
}
#endif

void UserMediaPermissionRequestProxy::invalidate()
{
    m_manager = nullptr;
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
    ASSERT(static_cast<size_t>(enumerationValue) < WTF_ARRAY_LENGTH(values));
    return values[static_cast<size_t>(enumerationValue)];
}

} // namespace WebKit
