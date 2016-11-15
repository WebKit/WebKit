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
#include "UserMediaPermissionRequestManagerProxy.h"

#include "APISecurityOrigin.h"
#include "APIUIClient.h"
#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <WebCore/MediaConstraintsImpl.h>
#include <WebCore/MockRealtimeMediaSourceCenter.h>
#include <WebCore/RealtimeMediaSource.h>
#include <WebCore/SecurityOriginData.h>

using namespace WebCore;

namespace WebKit {

UserMediaPermissionRequestManagerProxy::UserMediaPermissionRequestManagerProxy(WebPageProxy& page)
    : m_page(page)
{
}

void UserMediaPermissionRequestManagerProxy::invalidateRequests()
{
    for (auto& request : m_pendingUserMediaRequests.values())
        request->invalidate();
    m_pendingUserMediaRequests.clear();

    for (auto& request : m_pendingDeviceRequests.values())
        request->invalidate();
    m_pendingDeviceRequests.clear();

    m_pageSandboxExtensionsGranted.clear();
}

Ref<UserMediaPermissionRequestProxy> UserMediaPermissionRequestManagerProxy::createRequest(uint64_t userMediaID, const Vector<String>& audioDeviceUIDs, const Vector<String>& videoDeviceUIDs)
{
    Ref<UserMediaPermissionRequestProxy> request = UserMediaPermissionRequestProxy::create(*this, userMediaID, audioDeviceUIDs, videoDeviceUIDs);
    m_pendingUserMediaRequests.add(userMediaID, request.ptr());
    return request;
}

Ref<UserMediaPermissionRequestProxy> UserMediaPermissionRequestManagerProxy::createRequest(uint64_t userMediaID)
{
    Ref<UserMediaPermissionRequestProxy> request = UserMediaPermissionRequestProxy::create(*this, userMediaID, Vector<String>(), Vector<String>());
    m_pendingUserMediaRequests.add(userMediaID, request.ptr());
    return request;
}

#if ENABLE(MEDIA_STREAM)
static uint64_t toWebCore(UserMediaPermissionRequestProxy::UserMediaAccessDenialReason reason)
{
    switch (reason) {
    case UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::NoConstraints:
        return static_cast<uint64_t>(UserMediaRequest::MediaAccessDenialReason::NoConstraints);
        break;
    case UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::UserMediaDisabled:
        return static_cast<uint64_t>(UserMediaRequest::MediaAccessDenialReason::UserMediaDisabled);
        break;
    case UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::NoCaptureDevices:
        return static_cast<uint64_t>(UserMediaRequest::MediaAccessDenialReason::NoCaptureDevices);
        break;
    case UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::InvalidConstraint:
        return static_cast<uint64_t>(UserMediaRequest::MediaAccessDenialReason::InvalidConstraint);
        break;
    case UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::HardwareError:
        return static_cast<uint64_t>(UserMediaRequest::MediaAccessDenialReason::HardwareError);
        break;
    case UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::PermissionDenied:
        return static_cast<uint64_t>(UserMediaRequest::MediaAccessDenialReason::PermissionDenied);
        break;
    case UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::OtherFailure:
        return static_cast<uint64_t>(UserMediaRequest::MediaAccessDenialReason::OtherFailure);
        break;
    }
    
    ASSERT_NOT_REACHED();
    return static_cast<uint64_t>(UserMediaRequest::MediaAccessDenialReason::OtherFailure);
}
#endif

void UserMediaPermissionRequestManagerProxy::userMediaAccessWasDenied(uint64_t userMediaID, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason reason)
{
    if (!m_page.isValid())
        return;

    if (!m_pendingUserMediaRequests.take(userMediaID))
        return;

    denyRequest(userMediaID, reason, "");
}

void UserMediaPermissionRequestManagerProxy::denyRequest(uint64_t userMediaID, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason reason, const String& invalidConstraint)
{
    ASSERT(m_page.isValid());

#if ENABLE(MEDIA_STREAM)
    m_page.process().send(Messages::WebPage::UserMediaAccessWasDenied(userMediaID, toWebCore(reason), invalidConstraint), m_page.pageID());
#else
    UNUSED_PARAM(reason);
    UNUSED_PARAM(invalidConstraint);
#endif
}

void UserMediaPermissionRequestManagerProxy::userMediaAccessWasGranted(uint64_t userMediaID, const String& audioDeviceUID, const String& videoDeviceUID)
{
    ASSERT(!audioDeviceUID.isEmpty() || !videoDeviceUID.isEmpty());

    if (!m_page.isValid())
        return;

    if (!m_pendingUserMediaRequests.take(userMediaID))
        return;

#if ENABLE(MEDIA_STREAM)
    size_t extensionCount = 0;
    unsigned requiredExtensions = SandboxExtensionsGranted::None;
    if (!audioDeviceUID.isEmpty()) {
        requiredExtensions |= SandboxExtensionsGranted::Audio;
        extensionCount++;
    }
    if (!videoDeviceUID.isEmpty()) {
        requiredExtensions |= SandboxExtensionsGranted::Video;
        extensionCount++;
    }

    unsigned currentExtensions = m_pageSandboxExtensionsGranted.get(m_page.pageID());
    if (!(requiredExtensions & currentExtensions)) {
        ASSERT(extensionCount);
        m_pageSandboxExtensionsGranted.set(m_page.pageID(), requiredExtensions | currentExtensions);
        SandboxExtension::HandleArray handles;
        handles.allocate(extensionCount);
        if (!videoDeviceUID.isEmpty())
            SandboxExtension::createHandleForGenericExtension("com.apple.webkit.camera", handles[--extensionCount]);
        if (!audioDeviceUID.isEmpty())
            SandboxExtension::createHandleForGenericExtension("com.apple.webkit.microphone", handles[--extensionCount]);
        m_page.process().send(Messages::WebPage::GrantUserMediaDevicesSandboxExtension(handles), m_page.pageID());
    }

    m_page.process().send(Messages::WebPage::UserMediaAccessWasGranted(userMediaID, audioDeviceUID, videoDeviceUID), m_page.pageID());
#else
    UNUSED_PARAM(audioDeviceUID);
    UNUSED_PARAM(videoDeviceUID);
#endif
}

void UserMediaPermissionRequestManagerProxy::requestUserMediaPermissionForFrame(uint64_t userMediaID, uint64_t frameID, String userMediaDocumentOriginIdentifier, String topLevelDocumentOriginIdentifier, const WebCore::MediaConstraintsData& audioConstraintsData, const WebCore::MediaConstraintsData& videoConstraintsData)
{
#if ENABLE(MEDIA_STREAM)
    auto invalidHandler = [this, userMediaID](const String& invalidConstraint) {
        if (!m_page.isValid())
            return;

        denyRequest(userMediaID, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::InvalidConstraint, invalidConstraint);
    };

    auto validHandler = [this, userMediaID, frameID, userMediaDocumentOriginIdentifier, topLevelDocumentOriginIdentifier](const Vector<String>&& audioDeviceUIDs, const Vector<String>&& videoDeviceUIDs) {
        if (!m_page.isValid())
            return;

        if (videoDeviceUIDs.isEmpty() && audioDeviceUIDs.isEmpty()) {
            denyRequest(userMediaID, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::NoConstraints, "");
            return;
        }

        auto userMediaOrigin = API::SecurityOrigin::create(SecurityOriginData::fromDatabaseIdentifier(userMediaDocumentOriginIdentifier)->securityOrigin());
        auto topLevelOrigin = API::SecurityOrigin::create(SecurityOriginData::fromDatabaseIdentifier(topLevelDocumentOriginIdentifier)->securityOrigin());
        auto request = createRequest(userMediaID, audioDeviceUIDs, videoDeviceUIDs);

        if (!m_page.uiClient().decidePolicyForUserMediaPermissionRequest(m_page, *m_page.process().webFrame(frameID), *userMediaOrigin.get(), *topLevelOrigin.get(), request.get())) {
            m_pendingUserMediaRequests.take(userMediaID);
            request->deny(UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::UserMediaDisabled);
        }
    };

    auto audioConstraints = MediaConstraintsImpl::create(audioConstraintsData);
    auto videoConstraints = MediaConstraintsImpl::create(videoConstraintsData);

    syncWithWebCorePrefs();
    RealtimeMediaSourceCenter::singleton().validateRequestConstraints(validHandler, invalidHandler, audioConstraints, videoConstraints);
#else
    UNUSED_PARAM(userMediaID);
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(userMediaDocumentOriginIdentifier);
    UNUSED_PARAM(topLevelDocumentOriginIdentifier);
    UNUSED_PARAM(audioConstraintsData);
    UNUSED_PARAM(videoConstraintsData);
#endif
}

void UserMediaPermissionRequestManagerProxy::enumerateMediaDevicesForFrame(uint64_t userMediaID, uint64_t frameID, String userMediaDocumentOriginIdentifier, String topLevelDocumentOriginIdentifier)
{
#if ENABLE(MEDIA_STREAM)
    auto request = UserMediaPermissionCheckProxy::create(*this, userMediaID);
    m_pendingDeviceRequests.add(userMediaID, request.ptr());

    auto userMediaOrigin = API::SecurityOrigin::create(SecurityOriginData::fromDatabaseIdentifier(userMediaDocumentOriginIdentifier).valueOr(SecurityOriginData()).securityOrigin());
    auto topLevelOrigin = API::SecurityOrigin::create(SecurityOriginData::fromDatabaseIdentifier(topLevelDocumentOriginIdentifier).valueOr(SecurityOriginData()).securityOrigin());

    if (!m_page.uiClient().checkUserMediaPermissionForOrigin(m_page, *m_page.process().webFrame(frameID), *userMediaOrigin.get(), *topLevelOrigin.get(), request.get())) {
        m_pendingDeviceRequests.take(userMediaID);
        m_page.process().send(Messages::WebPage::DidCompleteMediaDeviceEnumeration(userMediaID, Vector<WebCore::CaptureDevice>(), emptyString(), false), m_page.pageID());
    }
#else
    UNUSED_PARAM(userMediaID);
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(userMediaDocumentOriginIdentifier);
    UNUSED_PARAM(topLevelDocumentOriginIdentifier);
#endif
}

void UserMediaPermissionRequestManagerProxy::didCompleteUserMediaPermissionCheck(uint64_t userMediaID, const String& deviceIdentifierHashSalt, bool originHasPersistentAccess)
{
    if (!m_page.isValid())
        return;

    if (!m_pendingDeviceRequests.take(userMediaID))
        return;

#if ENABLE(MEDIA_STREAM)
    syncWithWebCorePrefs();
    auto deviceInfo = RealtimeMediaSourceCenter::singleton().getMediaStreamDevices();
    m_page.process().send(Messages::WebPage::DidCompleteMediaDeviceEnumeration(userMediaID, deviceInfo, deviceIdentifierHashSalt, originHasPersistentAccess), m_page.pageID());
#else
    UNUSED_PARAM(deviceIdentifierHashSalt);
    UNUSED_PARAM(originHasPersistentAccess);
#endif
}

void UserMediaPermissionRequestManagerProxy::syncWithWebCorePrefs() const
{
#if ENABLE(MEDIA_STREAM)
    // Enable/disable the mock capture devices for the UI process as per the WebCore preferences. Note that
    // this is a noop if the preference hasn't changed since the last time this was called.
    bool mockDevicesEnabled = m_page.preferences().mockCaptureDevicesEnabled();
    WebCore::MockRealtimeMediaSourceCenter::setMockRealtimeMediaSourceCenterEnabled(mockDevicesEnabled);
#endif
}

} // namespace WebKit
