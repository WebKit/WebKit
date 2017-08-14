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
#include <WebCore/UserMediaRequest.h>

#if ENABLE(MEDIA_STREAM) && USE(AVFOUNDATION)
#include <WebCore/RealtimeMediaSourceCenterMac.h>
#endif

using namespace WebCore;

namespace WebKit {

UserMediaPermissionRequestManagerProxy::UserMediaPermissionRequestManagerProxy(WebPageProxy& page)
    : m_page(page)
    , m_rejectionTimer(RunLoop::main(), this, &UserMediaPermissionRequestManagerProxy::rejectionTimerFired)
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

Ref<UserMediaPermissionRequestProxy> UserMediaPermissionRequestManagerProxy::createRequest(uint64_t userMediaID, uint64_t mainFrameID, uint64_t frameID, Ref<WebCore::SecurityOrigin>&& userMediaDocumentOrigin, Ref<WebCore::SecurityOrigin>&& topLevelDocumentOrigin, Vector<String>&& audioDeviceUIDs, Vector<String>&& videoDeviceUIDs, String&& deviceIDHashSalt)
{
    auto request = UserMediaPermissionRequestProxy::create(*this, userMediaID, mainFrameID, frameID, WTFMove(userMediaDocumentOrigin), WTFMove(topLevelDocumentOrigin), WTFMove(audioDeviceUIDs), WTFMove(videoDeviceUIDs), WTFMove(deviceIDHashSalt));
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

    if (reason == UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::PermissionDenied)
        m_deniedRequests.append(DeniedRequest { request->mainFrameID(), request->userMediaDocumentSecurityOrigin(), request->topLevelDocumentSecurityOrigin(), request->requiresAudio(), request->requiresVideo() });

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

    grantAccess(userMediaID, audioDeviceUID, videoDeviceUID, request->deviceIdentifierHashSalt());
    m_grantedRequests.append(request.releaseNonNull());
#else
    UNUSED_PARAM(userMediaID);
    UNUSED_PARAM(audioDeviceUID);
    UNUSED_PARAM(videoDeviceUID);
#endif
}

#if ENABLE(MEDIA_STREAM)
void UserMediaPermissionRequestManagerProxy::resetAccess(uint64_t frameID)
{
    m_grantedRequests.removeAllMatching([frameID](const auto& grantedRequest) {
        return grantedRequest->mainFrameID() == frameID;
    });
    m_pregrantedRequests.clear();
    m_deniedRequests.clear();
}

const UserMediaPermissionRequestProxy* UserMediaPermissionRequestManagerProxy::searchForGrantedRequest(uint64_t frameID, const WebCore::SecurityOrigin& userMediaDocumentOrigin, const WebCore::SecurityOrigin& topLevelDocumentOrigin, bool needsAudio, bool needsVideo) const
{
    if (m_page.isMediaStreamCaptureMuted())
        return nullptr;

    bool checkForAudio = needsAudio;
    bool checkForVideo = needsVideo;
    for (const auto& grantedRequest : m_grantedRequests) {
        if (!grantedRequest->userMediaDocumentSecurityOrigin().isSameSchemeHostPort(userMediaDocumentOrigin))
            continue;
        if (!grantedRequest->topLevelDocumentSecurityOrigin().isSameSchemeHostPort(topLevelDocumentOrigin))
            continue;
        if (grantedRequest->frameID() != frameID)
            continue;

        if (!grantedRequest->videoDeviceUIDs().isEmpty())
            checkForVideo = false;

        if (!grantedRequest->audioDeviceUIDs().isEmpty())
            checkForAudio = false;

        if (checkForVideo || checkForAudio)
            continue;

        return grantedRequest.ptr();
    }
    return nullptr;
}

bool UserMediaPermissionRequestManagerProxy::wasRequestDenied(uint64_t mainFrameID, const WebCore::SecurityOrigin& userMediaDocumentOrigin, const WebCore::SecurityOrigin& topLevelDocumentOrigin, bool needsAudio, bool needsVideo)
{
    for (const auto& deniedRequest : m_deniedRequests) {
        if (!deniedRequest.userMediaDocumentOrigin->isSameSchemeHostPort(userMediaDocumentOrigin))
            continue;
        if (!deniedRequest.topLevelDocumentOrigin->isSameSchemeHostPort(topLevelDocumentOrigin))
            continue;
        if (deniedRequest.mainFrameID != mainFrameID)
            continue;
        if (deniedRequest.isAudioDenied && needsAudio)
            return true;
        if (deniedRequest.isVideoDenied && needsVideo)
            return true;
    }
    return false;
}

void UserMediaPermissionRequestManagerProxy::grantAccess(uint64_t userMediaID, const String& audioDeviceUID, const String& videoDeviceUID, const String& deviceIdentifierHashSalt)
{
    UserMediaProcessManager::singleton().willCreateMediaStream(*this, !audioDeviceUID.isEmpty(), !videoDeviceUID.isEmpty());
    m_page.process().send(Messages::WebPage::UserMediaAccessWasGranted(userMediaID, audioDeviceUID, videoDeviceUID, deviceIdentifierHashSalt), m_page.pageID());
}
#endif

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

static inline void allowRequest(UserMediaPermissionRequestProxy& request)
{
    request.allow(request.audioDeviceUIDs().isEmpty() ? String() : request.audioDeviceUIDs()[0], request.videoDeviceUIDs().isEmpty() ? String() : request.videoDeviceUIDs()[0]);
}

void UserMediaPermissionRequestManagerProxy::requestUserMediaPermissionForFrame(uint64_t userMediaID, uint64_t frameID, Ref<WebCore::SecurityOrigin>&& userMediaDocumentOrigin, Ref<WebCore::SecurityOrigin>&& topLevelDocumentOrigin, const WebCore::MediaConstraints& audioConstraints, const WebCore::MediaConstraints& videoConstraints)
{
#if ENABLE(MEDIA_STREAM)
    if (!UserMediaProcessManager::singleton().captureEnabled()) {
        m_pendingRejections.append(userMediaID);
        scheduleNextRejection();
        return;
    }

    WebCore::RealtimeMediaSourceCenter::InvalidConstraintsHandler invalidHandler = [this, userMediaID](const String& invalidConstraint) {
        if (!m_page.isValid())
            return;

        denyRequest(userMediaID, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::InvalidConstraint, invalidConstraint);
    };

    WebCore::RealtimeMediaSourceCenter::ValidConstraintsHandler validHandler = [this, userMediaID, frameID, userMediaDocumentOrigin = userMediaDocumentOrigin.copyRef(), topLevelDocumentOrigin = topLevelDocumentOrigin.copyRef()](Vector<String>&& audioDeviceUIDs, Vector<String>&& videoDeviceUIDs, String&& deviceIdentifierHashSalt) mutable {
        if (!m_page.isValid() || !m_page.mainFrame())
            return;

        if (videoDeviceUIDs.isEmpty() && audioDeviceUIDs.isEmpty()) {
            denyRequest(userMediaID, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::NoConstraints, emptyString());
            return;
        }

        if (wasRequestDenied(m_page.mainFrame()->frameID(), userMediaDocumentOrigin.get(), topLevelDocumentOrigin.get(), !audioDeviceUIDs.isEmpty(), !videoDeviceUIDs.isEmpty())) {
            denyRequest(userMediaID, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::PermissionDenied, emptyString());
            return;
        }

        auto* grantedRequest = searchForGrantedRequest(frameID, userMediaDocumentOrigin.get(), topLevelDocumentOrigin.get(), !audioDeviceUIDs.isEmpty(), !videoDeviceUIDs.isEmpty());
        if (grantedRequest) {
            if (m_page.isViewVisible())
            // We select the first available devices, but the current client API allows client to select which device to pick.
            // FIXME: Remove the possiblity for the client to do the device selection.
                grantAccess(userMediaID, audioDeviceUIDs.isEmpty() ? String() : audioDeviceUIDs[0], videoDeviceUIDs.isEmpty() ? String() : videoDeviceUIDs[0], grantedRequest->deviceIdentifierHashSalt());
            else
                m_pregrantedRequests.append(createRequest(userMediaID, m_page.mainFrame()->frameID(), frameID, WTFMove(userMediaDocumentOrigin), WTFMove(topLevelDocumentOrigin), WTFMove(audioDeviceUIDs), WTFMove(videoDeviceUIDs), String(grantedRequest->deviceIdentifierHashSalt())));
            return;
        }
        auto userMediaOrigin = API::SecurityOrigin::create(userMediaDocumentOrigin.get());
        auto topLevelOrigin = API::SecurityOrigin::create(topLevelDocumentOrigin.get());

        auto request = createRequest(userMediaID, m_page.mainFrame()->frameID(), frameID, WTFMove(userMediaDocumentOrigin), WTFMove(topLevelDocumentOrigin), WTFMove(audioDeviceUIDs), WTFMove(videoDeviceUIDs), WTFMove(deviceIdentifierHashSalt));

        if (m_page.preferences().mockCaptureDevicesEnabled() && !m_page.preferences().mockCaptureDevicesPromptEnabled()) {
            allowRequest(request);
            return;
        }

        if (!m_page.uiClient().decidePolicyForUserMediaPermissionRequest(m_page, *m_page.process().webFrame(frameID), WTFMove(userMediaOrigin), WTFMove(topLevelOrigin), request.get()))
            userMediaAccessWasDenied(userMediaID, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::UserMediaDisabled);
    };

    auto haveDeviceSaltHandler = [this, validHandler = WTFMove(validHandler), invalidHandler = WTFMove(invalidHandler), audioConstraints = WebCore::MediaConstraints(audioConstraints), videoConstraints = WebCore::MediaConstraints(videoConstraints)](uint64_t userMediaID, String&& deviceIdentifierHashSalt, bool originHasPersistentAccess) mutable {

        auto request = m_pendingDeviceRequests.take(userMediaID);
        if (!request)
            return;

        if (!m_page.isValid())
            return;
        
        audioConstraints.deviceIDHashSalt = deviceIdentifierHashSalt;
        videoConstraints.deviceIDHashSalt = deviceIdentifierHashSalt;

        syncWithWebCorePrefs();
        
        RealtimeMediaSourceCenter::singleton().validateRequestConstraints(WTFMove(validHandler), WTFMove(invalidHandler), audioConstraints, videoConstraints, WTFMove(deviceIdentifierHashSalt));
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
#endif
}

void UserMediaPermissionRequestManagerProxy::captureStateChanged(WebCore::MediaProducer::MediaStateFlags oldState, WebCore::MediaProducer::MediaStateFlags newState)
{
    if (!m_page.isValid())
        return;

#if ENABLE(MEDIA_STREAM)
    bool wasCapturingAudio = oldState & WebCore::MediaProducer::AudioCaptureMask;
    bool wasCapturingVideo = oldState & WebCore::MediaProducer::VideoCaptureMask;
    bool isCapturingAudio = newState & WebCore::MediaProducer::AudioCaptureMask;
    bool isCapturingVideo = newState & WebCore::MediaProducer::VideoCaptureMask;

    if ((wasCapturingAudio && !isCapturingAudio) || (wasCapturingVideo && !isCapturingVideo))
        UserMediaProcessManager::singleton().endedCaptureSession(*this);
    if ((!wasCapturingAudio && isCapturingAudio) || (!wasCapturingVideo && isCapturingVideo))
        UserMediaProcessManager::singleton().startedCaptureSession(*this);
#endif
}

void UserMediaPermissionRequestManagerProxy::processPregrantedRequests()
{
    for (auto& request : m_pregrantedRequests)
        allowRequest(request.get());
    m_pregrantedRequests.clear();
}

} // namespace WebKit
