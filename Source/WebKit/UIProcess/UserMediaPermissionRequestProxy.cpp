/*
 * Copyright (C) 2014 Igalia S.L.
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

UserMediaPermissionRequestProxy::UserMediaPermissionRequestProxy(UserMediaPermissionRequestManagerProxy& manager, uint64_t userMediaID, uint64_t mainFrameID, uint64_t frameID, Ref<WebCore::SecurityOrigin>&& userMediaDocumentOrigin, Ref<WebCore::SecurityOrigin>&& topLevelDocumentOrigin, Vector<WebCore::CaptureDevice>&& audioDevices, Vector<WebCore::CaptureDevice>&& videoDevices, WebCore::MediaStreamRequest&& request)
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

void UserMediaPermissionRequestProxy::allow(const String& audioDeviceUID, const String& videoDeviceUID)
{
    ASSERT(m_manager);
    if (!m_manager)
        return;

#if ENABLE(MEDIA_STREAM)
    CaptureDevice audioDevice;
    if (!audioDeviceUID.isEmpty()) {
        size_t index = m_eligibleAudioDevices.findMatching([&](const auto& device) {
            return device.persistentId() == audioDeviceUID;
        });
        ASSERT(index != notFound);

        if (index != notFound)
            audioDevice = m_eligibleAudioDevices[index];

        ASSERT(audioDevice.enabled());
    }

    CaptureDevice videoDevice;
    if (!videoDeviceUID.isEmpty()) {
        size_t index = m_eligibleVideoDevices.findMatching([&](const auto& device) {
            return device.persistentId() == videoDeviceUID;
        });
        ASSERT(index != notFound);

        if (index != notFound)
            videoDevice = m_eligibleVideoDevices[index];

        ASSERT(videoDevice.enabled());
    }

    m_manager->userMediaAccessWasGranted(m_userMediaID, WTFMove(audioDevice), WTFMove(videoDevice));
#else
    UNUSED_PARAM(audioDeviceUID);
    UNUSED_PARAM(videoDeviceUID);
#endif

    invalidate();
}

void UserMediaPermissionRequestProxy::allow(WebCore::CaptureDevice&& audioDevice, WebCore::CaptureDevice&& videoDevice)
{
    ASSERT(m_manager);
    if (!m_manager)
        return;

    m_manager->userMediaAccessWasGranted(m_userMediaID, WTFMove(audioDevice), WTFMove(videoDevice));
    invalidate();
}

void UserMediaPermissionRequestProxy::allow()
{
    ASSERT(m_manager);
    if (!m_manager)
        return;

    auto audioDevice = !m_eligibleAudioDevices.isEmpty() ? m_eligibleAudioDevices[0] : CaptureDevice();
    auto videoDevice = !m_eligibleVideoDevices.isEmpty() ? m_eligibleVideoDevices[0] : CaptureDevice();

    m_manager->userMediaAccessWasGranted(m_userMediaID, WTFMove(audioDevice), WTFMove(videoDevice));
    invalidate();
}

void UserMediaPermissionRequestProxy::deny(UserMediaAccessDenialReason reason)
{
    if (!m_manager)
        return;

    m_manager->userMediaAccessWasDenied(m_userMediaID, reason);
    invalidate();
}

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

} // namespace WebKit
