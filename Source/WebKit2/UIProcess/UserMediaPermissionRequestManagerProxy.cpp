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
#include "UserMediaProcessManager.h"
#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <WebCore/MediaConstraints.h>
#include <WebCore/MockRealtimeMediaSourceCenter.h>
#include <WebCore/RealtimeMediaSource.h>
#include <WebCore/SecurityOriginData.h>

#if ENABLE(MEDIA_STREAM) && USE(AVFOUNDATION)
#include <WebCore/RealtimeMediaSourceCenterMac.h>
#endif

using namespace WebCore;

namespace WebKit {

FrameAuthorizationState::FrameAuthorizationState(Ref<SecurityOrigin>&& userMediaDocumentSecurityOrigin, Ref<SecurityOrigin>&& topLevelDocumentSecurityOrigin)
    : m_userMediaDocumentSecurityOrigin(WTFMove(userMediaDocumentSecurityOrigin))
    , m_topLevelDocumentSecurityOrigin(WTFMove(topLevelDocumentSecurityOrigin))
{
}

bool FrameAuthorizationState::hasPermissionToUseCaptureDevice(const String& deviceUID)
{
    return m_authorizedDeviceUIDs.find(deviceUID) != notFound;
}

void FrameAuthorizationState::setHasPermissionToUseCaptureDevice(const String& deviceUID, bool hasPermission)
{
    if (deviceUID.isEmpty())
        return;

    size_t index = m_authorizedDeviceUIDs.find(deviceUID);
    if (hasPermission == (index != notFound))
        return;

    if (hasPermission)
        m_authorizedDeviceUIDs.append(deviceUID);
    else
        m_authorizedDeviceUIDs.remove(index);
}

void FrameAuthorizationState::ensureSecurityOriginsAreEqual(WebCore::SecurityOrigin& userMediaDocumentSecurityOrigin, WebCore::SecurityOrigin& topLevelDocumentSecurityOrigin)
{
    if (m_userMediaDocumentSecurityOrigin->equal(&userMediaDocumentSecurityOrigin) && m_topLevelDocumentSecurityOrigin->equal(&topLevelDocumentSecurityOrigin))
        return;

    m_userMediaDocumentSecurityOrigin = userMediaDocumentSecurityOrigin;
    m_topLevelDocumentSecurityOrigin = topLevelDocumentSecurityOrigin;
    m_authorizedDeviceUIDs.clear();
    m_deviceIdentifierHashSalt = emptyString();
}

template <typename RequestType>
FrameAuthorizationState& UserMediaPermissionRequestManagerProxy::stateForRequest(RequestType& request)
{
    auto& state = m_frameStates.add(request.frameID(), nullptr).iterator->value;
    if (state) {
        state->ensureSecurityOriginsAreEqual(request.userMediaDocumentSecurityOrigin(), request.topLevelDocumentSecurityOrigin());
        return *state;
    }

    state = std::make_unique<FrameAuthorizationState>(request.userMediaDocumentSecurityOrigin(), request.topLevelDocumentSecurityOrigin());
    return *state;
}

UserMediaPermissionRequestManagerProxy::UserMediaPermissionRequestManagerProxy(WebPageProxy& page)
    : m_page(page)
    , m_rejectionTimer(*this, &UserMediaPermissionRequestManagerProxy::rejectionTimerFired)
{
#if ENABLE(MEDIA_STREAM)
    UserMediaProcessManager::singleton().addUserMediaPermissionRequestManagerProxy(*this);
#endif
}

UserMediaPermissionRequestManagerProxy::~UserMediaPermissionRequestManagerProxy()
{
#if ENABLE(MEDIA_STREAM)
    UserMediaProcessManager::singleton().removeUserMediaPermissionRequestManagerProxy(*this);
#endif
    invalidatePendingRequests();
}

void UserMediaPermissionRequestManagerProxy::invalidatePendingRequests()
{
    for (auto& request : m_pendingUserMediaRequests.values())
        request->invalidate();
    m_pendingUserMediaRequests.clear();

    for (auto& request : m_pendingDeviceRequests.values())
        request->invalidate();
    m_pendingDeviceRequests.clear();

    m_frameStates.clear();
}

void UserMediaPermissionRequestManagerProxy::stopCapture()
{
    invalidatePendingRequests();
    m_page.stopMediaCapture();
}

void UserMediaPermissionRequestManagerProxy::clearCachedState()
{
    invalidatePendingRequests();
}

Ref<UserMediaPermissionRequestProxy> UserMediaPermissionRequestManagerProxy::createRequest(uint64_t userMediaID, uint64_t frameID, Ref<WebCore::SecurityOrigin>&& userMediaDocumentOrigin, Ref<WebCore::SecurityOrigin>&& topLevelDocumentOrigin, Vector<String>&& audioDeviceUIDs, Vector<String>&& videoDeviceUIDs)
{
    auto request = UserMediaPermissionRequestProxy::create(*this, userMediaID, frameID, WTFMove(userMediaDocumentOrigin), WTFMove(topLevelDocumentOrigin), WTFMove(audioDeviceUIDs), WTFMove(videoDeviceUIDs));
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

    auto request = m_pendingUserMediaRequests.take(userMediaID);
    if (!request)
        return;

    auto& frameState = stateForRequest(*request);
    for (const auto& deviceUID : request->videoDeviceUIDs())
        frameState.setHasPermissionToUseCaptureDevice(deviceUID, false);
    for (const auto& deviceUID : request->audioDeviceUIDs())
        frameState.setHasPermissionToUseCaptureDevice(deviceUID, false);

    denyRequest(userMediaID, reason, emptyString());
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

#if ENABLE(MEDIA_STREAM)
    auto request = m_pendingUserMediaRequests.take(userMediaID);
    if (!request)
        return;

    auto& frameState = stateForRequest(*request);
    if (!audioDeviceUID.isEmpty())
        frameState.setHasPermissionToUseCaptureDevice(audioDeviceUID, true);
    if (!videoDeviceUID.isEmpty())
        frameState.setHasPermissionToUseCaptureDevice(videoDeviceUID, true);

    UserMediaProcessManager::singleton().willCreateMediaStream(*this, !audioDeviceUID.isEmpty(), !videoDeviceUID.isEmpty());

    m_page.process().send(Messages::WebPage::UserMediaAccessWasGranted(userMediaID, audioDeviceUID, videoDeviceUID, frameState.deviceIdentifierHashSalt()), m_page.pageID());
#else
    UNUSED_PARAM(userMediaID);
    UNUSED_PARAM(audioDeviceUID);
    UNUSED_PARAM(videoDeviceUID);
#endif
}

void UserMediaPermissionRequestManagerProxy::rejectionTimerFired()
{
    uint64_t userMediaID = m_pendingRejections[0];
    m_pendingRejections.remove(0);

    denyRequest(userMediaID, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::PermissionDenied, emptyString());
    if (!m_pendingRejections.isEmpty())
        scheduleNextRejection();
}

void UserMediaPermissionRequestManagerProxy::scheduleNextRejection()
{
    const double mimimumDelayBeforeReplying = .25;
    if (!m_rejectionTimer.isActive())
        m_rejectionTimer.startOneShot(Seconds(mimimumDelayBeforeReplying + randomNumber()));
}

void UserMediaPermissionRequestManagerProxy::requestUserMediaPermissionForFrame(uint64_t userMediaID, uint64_t frameID, Ref<WebCore::SecurityOrigin>&& userMediaDocumentOrigin, Ref<WebCore::SecurityOrigin>&& topLevelDocumentOrigin, const WebCore::MediaConstraints& audioConstraints, const WebCore::MediaConstraints& videoConstraints)
{
#if ENABLE(MEDIA_STREAM)
    WebCore::RealtimeMediaSourceCenter::InvalidConstraintsHandler invalidHandler = [this, userMediaID](const String& invalidConstraint) {
        if (!m_page.isValid())
            return;

        denyRequest(userMediaID, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::InvalidConstraint, invalidConstraint);
    };

    WebCore::RealtimeMediaSourceCenter::ValidConstraintsHandler validHandler = [this, userMediaID, frameID, userMediaDocumentOrigin = userMediaDocumentOrigin.copyRef(), topLevelDocumentOrigin = topLevelDocumentOrigin.copyRef()](Vector<String>&& audioDeviceUIDs, Vector<String>&& videoDeviceUIDs) mutable {
        if (!m_page.isValid())
            return;

        if (videoDeviceUIDs.isEmpty() && audioDeviceUIDs.isEmpty()) {
            denyRequest(userMediaID, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::NoConstraints, emptyString());
            return;
        }

        bool authorizedForAudio = false;
        bool authorizedForVideo = false;
        
        auto userMediaOrigin = API::SecurityOrigin::create(userMediaDocumentOrigin.get());
        auto topLevelOrigin = API::SecurityOrigin::create(topLevelDocumentOrigin.get());
        
        auto request = createRequest(userMediaID, frameID, WTFMove(userMediaDocumentOrigin), WTFMove(topLevelDocumentOrigin), WTFMove(audioDeviceUIDs), WTFMove(videoDeviceUIDs));

        auto& frameState = stateForRequest(request.get());
        for (const auto& deviceUID : request->audioDeviceUIDs()) {
            if (frameState.hasPermissionToUseCaptureDevice(deviceUID)) {
                authorizedForAudio = true;
                break;
            }
        }
        for (const auto& deviceUID : request->videoDeviceUIDs()) {
            if (frameState.hasPermissionToUseCaptureDevice(deviceUID)) {
                authorizedForVideo = true;
                break;
            }
        }

        if (authorizedForAudio == !request->audioDeviceUIDs().isEmpty() && authorizedForVideo == !request->videoDeviceUIDs().isEmpty()) {
            userMediaAccessWasGranted(userMediaID, authorizedForAudio ? request->audioDeviceUIDs()[0] : emptyString(), authorizedForVideo ? request->videoDeviceUIDs()[0] : emptyString());
            return;
        }

        if (!m_page.uiClient().decidePolicyForUserMediaPermissionRequest(m_page, *m_page.process().webFrame(frameID), WTFMove(userMediaOrigin), WTFMove(topLevelOrigin), request.get()))
            userMediaAccessWasDenied(userMediaID, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::UserMediaDisabled);

    };

    if (!UserMediaProcessManager::singleton().captureEnabled()) {
        m_pendingRejections.append(userMediaID);
        scheduleNextRejection();
        return;
    }

    auto haveDeviceSaltHandler = [this, userMediaID, validHandler = WTFMove(validHandler), invalidHandler = WTFMove(invalidHandler), audioConstraints = WebCore::MediaConstraints(audioConstraints), videoConstraints = WebCore::MediaConstraints(videoConstraints)](uint64_t userMediaID, String&& deviceIdentifierHashSalt, bool originHasPersistentAccess) mutable {

        auto request = m_pendingDeviceRequests.take(userMediaID);
        if (!request)
            return;

        if (!m_page.isValid())
            return;
        
        audioConstraints.deviceIDHashSalt = deviceIdentifierHashSalt;
        videoConstraints.deviceIDHashSalt = deviceIdentifierHashSalt;

        auto& frameState = stateForRequest(request.value().get());
        frameState.setDeviceIdentifierHashSalt(WTFMove(deviceIdentifierHashSalt));
        
        syncWithWebCorePrefs();
        
        RealtimeMediaSourceCenter::singleton().validateRequestConstraints(WTFMove(validHandler), WTFMove(invalidHandler), audioConstraints, videoConstraints);
    };

    getUserMediaPermissionInfo(userMediaID, frameID, WTFMove(haveDeviceSaltHandler), WTFMove(userMediaDocumentOrigin), WTFMove(topLevelDocumentOrigin));
#else
    UNUSED_PARAM(userMediaID);
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(userMediaDocumentOrigin);
    UNUSED_PARAM(topLevelDocumentOrigin);
    UNUSED_PARAM(audioConstraints);
    UNUSED_PARAM(videoConstraints);
#endif
}

#if ENABLE(MEDIA_STREAM)
void UserMediaPermissionRequestManagerProxy::getUserMediaPermissionInfo(uint64_t userMediaID, uint64_t frameID, UserMediaPermissionCheckProxy::CompletionHandler&& handler, Ref<WebCore::SecurityOrigin>&& userMediaDocumentOrigin, Ref<WebCore::SecurityOrigin>&& topLevelDocumentOrigin)
{
    auto userMediaOrigin = API::SecurityOrigin::create(userMediaDocumentOrigin.get());
    auto topLevelOrigin = API::SecurityOrigin::create(topLevelDocumentOrigin.get());

    auto request = UserMediaPermissionCheckProxy::create(userMediaID, frameID, WTFMove(handler), WTFMove(userMediaDocumentOrigin), WTFMove(topLevelDocumentOrigin));
    m_pendingDeviceRequests.add(userMediaID, request.copyRef());

    if (!m_page.uiClient().checkUserMediaPermissionForOrigin(m_page, *m_page.process().webFrame(frameID), userMediaOrigin.get(), topLevelOrigin.get(), request.get()))
        request->completionHandler()(userMediaID, String(), false);
} 
#endif

void UserMediaPermissionRequestManagerProxy::enumerateMediaDevicesForFrame(uint64_t userMediaID, uint64_t frameID, Ref<WebCore::SecurityOrigin>&& userMediaDocumentOrigin, Ref<SecurityOrigin>&& topLevelDocumentOrigin)
{
#if ENABLE(MEDIA_STREAM)
    auto completionHandler = [this](uint64_t userMediaID, String&& deviceIdentifierHashSalt, bool originHasPersistentAccess) {
        auto request = m_pendingDeviceRequests.take(userMediaID);
        if (!request)
            return;

        if (!m_page.isValid())
            return;

        auto& frameState = stateForRequest(request.value().get());
        frameState.setDeviceIdentifierHashSalt(String(deviceIdentifierHashSalt));

        syncWithWebCorePrefs();
        auto deviceInfo = RealtimeMediaSourceCenter::singleton().getMediaStreamDevices();
        m_page.process().send(Messages::WebPage::DidCompleteMediaDeviceEnumeration(userMediaID, deviceInfo, deviceIdentifierHashSalt, originHasPersistentAccess), m_page.pageID());
    };

    getUserMediaPermissionInfo(userMediaID, frameID, WTFMove(completionHandler), WTFMove(userMediaDocumentOrigin), WTFMove(topLevelDocumentOrigin));
#else
    UNUSED_PARAM(userMediaID);
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(userMediaDocumentOrigin);
    UNUSED_PARAM(topLevelDocumentOrigin);
#endif
}

void UserMediaPermissionRequestManagerProxy::syncWithWebCorePrefs() const
{
#if ENABLE(MEDIA_STREAM)
    // Enable/disable the mock capture devices for the UI process as per the WebCore preferences. Note that
    // this is a noop if the preference hasn't changed since the last time this was called.
    bool mockDevicesEnabled = m_page.preferences().mockCaptureDevicesEnabled();
    WebCore::MockRealtimeMediaSourceCenter::setMockRealtimeMediaSourceCenterEnabled(mockDevicesEnabled);

#if USE(AVFOUNDATION)
    bool useAVFoundationAudioCapture = m_page.preferences().useAVFoundationAudioCapture();
    WebCore::RealtimeMediaSourceCenterMac::singleton().setUseAVFoundationAudioCapture(useAVFoundationAudioCapture);
#endif
#endif
}

void UserMediaPermissionRequestManagerProxy::startedCaptureSession()
{
    if (!m_page.isValid())
        return;

#if ENABLE(MEDIA_STREAM)
    UserMediaProcessManager::singleton().startedCaptureSession(*this);
#endif
}

void UserMediaPermissionRequestManagerProxy::endedCaptureSession()
{
    if (!m_page.isValid())
        return;

#if ENABLE(MEDIA_STREAM)
    UserMediaProcessManager::singleton().endedCaptureSession(*this);
#endif
}

} // namespace WebKit
