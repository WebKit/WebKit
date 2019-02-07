/*
 * Copyright (C) 2014 Igalia S.L.
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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
#include "DeviceIdHashSaltStorage.h"
#include "UserMediaPermissionRequestManager.h"
#include "UserMediaProcessManager.h"
#include "WebAutomationSession.h"
#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include "WebProcess.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include "WebsiteDataStore.h"
#include <WebCore/MediaConstraints.h>
#include <WebCore/MockRealtimeMediaSourceCenter.h>
#include <WebCore/RealtimeMediaSource.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/UserMediaRequest.h>

namespace WebKit {
using namespace WebCore;

#if ENABLE(MEDIA_STREAM)
static const MediaProducer::MediaStateFlags activeCaptureMask = MediaProducer::HasActiveAudioCaptureDevice | MediaProducer::HasActiveVideoCaptureDevice;

static uint64_t generateRequestID()
{
    static uint64_t uniqueRequestID = 1;
    return uniqueRequestID++;
}
#endif

UserMediaPermissionRequestManagerProxy::UserMediaPermissionRequestManagerProxy(WebPageProxy& page)
    : m_page(page)
    , m_rejectionTimer(RunLoop::main(), this, &UserMediaPermissionRequestManagerProxy::rejectionTimerFired)
    , m_watchdogTimer(RunLoop::main(), this, &UserMediaPermissionRequestManagerProxy::watchdogTimerFired)
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

    m_pendingDeviceRequests.clear();
}

void UserMediaPermissionRequestManagerProxy::stopCapture()
{
    invalidatePendingRequests();
    m_page.stopMediaCapture();
}

void UserMediaPermissionRequestManagerProxy::captureDevicesChanged()
{
#if ENABLE(MEDIA_STREAM)
    if (!m_page.isValid() || !m_page.mainFrame())
        return;

    auto handler = [this](Optional<bool> originHasPersistentAccess) mutable {
        if (!originHasPersistentAccess || !m_page.isValid())
            return;

        if (m_grantedRequests.isEmpty() && !*originHasPersistentAccess)
            return;

        m_page.process().send(Messages::WebPage::CaptureDevicesChanged(), m_page.pageID());
    };

    auto origin = WebCore::SecurityOrigin::create(m_page.mainFrame()->url());
    getUserMediaPermissionInfo(m_page.mainFrame()->frameID(), origin.get(), WTFMove(origin), WTFMove(handler));
#endif
}

void UserMediaPermissionRequestManagerProxy::clearCachedState()
{
    invalidatePendingRequests();
}

#if ENABLE(MEDIA_STREAM)
static uint64_t toWebCore(UserMediaPermissionRequestProxy::UserMediaAccessDenialReason reason)
{
    switch (reason) {
    case UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::NoConstraints:
        return static_cast<uint64_t>(UserMediaRequest::MediaAccessDenialReason::NoConstraints);
    case UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::UserMediaDisabled:
        return static_cast<uint64_t>(UserMediaRequest::MediaAccessDenialReason::UserMediaDisabled);
    case UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::NoCaptureDevices:
        return static_cast<uint64_t>(UserMediaRequest::MediaAccessDenialReason::NoCaptureDevices);
    case UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::InvalidConstraint:
        return static_cast<uint64_t>(UserMediaRequest::MediaAccessDenialReason::InvalidConstraint);
    case UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::HardwareError:
        return static_cast<uint64_t>(UserMediaRequest::MediaAccessDenialReason::HardwareError);
    case UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::PermissionDenied:
        return static_cast<uint64_t>(UserMediaRequest::MediaAccessDenialReason::PermissionDenied);
    case UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::OtherFailure:
        return static_cast<uint64_t>(UserMediaRequest::MediaAccessDenialReason::OtherFailure);
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
        m_deniedRequests.append(DeniedRequest { request->mainFrameID(), request->userMediaDocumentSecurityOrigin(), request->topLevelDocumentSecurityOrigin(), request->requiresAudioCapture(), request->requiresVideoCapture(), request->requiresDisplayCapture() });

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

void UserMediaPermissionRequestManagerProxy::userMediaAccessWasGranted(uint64_t userMediaID, CaptureDevice&& audioDevice, CaptureDevice&& videoDevice)
{
    ASSERT(audioDevice || videoDevice);

    if (!m_page.isValid())
        return;

#if ENABLE(MEDIA_STREAM)
    auto request = m_pendingUserMediaRequests.take(userMediaID);
    if (!request)
        return;

    auto& userMediaDocumentSecurityOrigin = request->userMediaDocumentSecurityOrigin();
    auto& topLevelDocumentSecurityOrigin = request->topLevelDocumentSecurityOrigin();
    m_page.websiteDataStore().deviceIdHashSaltStorage().deviceIdHashSaltForOrigin(userMediaDocumentSecurityOrigin, topLevelDocumentSecurityOrigin, [this, weakThis = makeWeakPtr(*this), request = request.releaseNonNull()] (String&& deviceIDHashSalt) mutable {
        if (!weakThis)
            return;
        if (!grantAccess(request))
            return;

        m_grantedRequests.append(WTFMove(request));
        if (m_hasFilteredDeviceList)
            captureDevicesChanged();
        m_hasFilteredDeviceList = false;
    });
#else
    UNUSED_PARAM(userMediaID);
    UNUSED_PARAM(audioDevice);
    UNUSED_PARAM(videoDevice);
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
    m_hasFilteredDeviceList = false;
}

const UserMediaPermissionRequestProxy* UserMediaPermissionRequestManagerProxy::searchForGrantedRequest(uint64_t frameID, const SecurityOrigin& userMediaDocumentOrigin, const SecurityOrigin& topLevelDocumentOrigin, bool needsAudio, bool needsVideo) const
{
    if (m_page.isMediaStreamCaptureMuted())
        return nullptr;

    bool checkForAudio = needsAudio;
    bool checkForVideo = needsVideo;
    for (const auto& grantedRequest : m_grantedRequests) {
        if (grantedRequest->requiresDisplayCapture())
            continue;
        if (!grantedRequest->userMediaDocumentSecurityOrigin().isSameSchemeHostPort(userMediaDocumentOrigin))
            continue;
        if (!grantedRequest->topLevelDocumentSecurityOrigin().isSameSchemeHostPort(topLevelDocumentOrigin))
            continue;
        if (grantedRequest->frameID() != frameID)
            continue;

        if (grantedRequest->requiresVideoCapture())
            checkForVideo = false;

        if (grantedRequest->requiresAudioCapture())
            checkForAudio = false;

        if (checkForVideo || checkForAudio)
            continue;

        return grantedRequest.ptr();
    }
    return nullptr;
}

bool UserMediaPermissionRequestManagerProxy::wasRequestDenied(uint64_t mainFrameID, const SecurityOrigin& userMediaDocumentOrigin, const SecurityOrigin& topLevelDocumentOrigin, bool needsAudio, bool needsVideo, bool needsScreenCapture)
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
        if (deniedRequest.isScreenCaptureDenied && needsScreenCapture)
            return true;
    }
    return false;
}

bool UserMediaPermissionRequestManagerProxy::grantAccess(const UserMediaPermissionRequestProxy& request)
{
    if (!UserMediaProcessManager::singleton().willCreateMediaStream(*this, request.hasAudioDevice(), request.hasVideoDevice())) {
        denyRequest(request.userMediaID(), UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::OtherFailure, "Unable to extend sandbox.");
        return false;
    }

    m_page.process().send(Messages::WebPage::UserMediaAccessWasGranted(request.userMediaID(), request.audioDevice(), request.videoDevice(), request.deviceIdentifierHashSalt()), m_page.pageID());
    return true;
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

#if ENABLE(MEDIA_STREAM)
UserMediaPermissionRequestManagerProxy::RequestAction UserMediaPermissionRequestManagerProxy::getRequestAction(const UserMediaPermissionRequestProxy& request)
{
    bool requestingScreenCapture = request.requestType() == MediaStreamRequest::Type::DisplayMedia;
    bool requestingCamera = !requestingScreenCapture && request.hasVideoDevice();
    bool requestingMicrophone = request.hasAudioDevice();

    ASSERT(!(requestingScreenCapture && !request.hasVideoDevice()));
    ASSERT(!(requestingScreenCapture && requestingMicrophone));

    if (wasRequestDenied(request.frameID(), request.userMediaDocumentSecurityOrigin(), request.topLevelDocumentSecurityOrigin(), requestingMicrophone, requestingCamera, requestingScreenCapture))
        return RequestAction::Deny;

    if (request.requestType() == MediaStreamRequest::Type::DisplayMedia)
        return RequestAction::Prompt;

    return searchForGrantedRequest(request.frameID(), request.userMediaDocumentSecurityOrigin(), request.topLevelDocumentSecurityOrigin(), requestingMicrophone, requestingCamera) ? RequestAction::Grant : RequestAction::Prompt;
}
#endif

void UserMediaPermissionRequestManagerProxy::requestUserMediaPermissionForFrame(uint64_t userMediaID, uint64_t frameID, Ref<SecurityOrigin>&& userMediaDocumentOrigin, Ref<SecurityOrigin>&& topLevelDocumentOrigin, MediaStreamRequest&& userRequest)
{
#if ENABLE(MEDIA_STREAM)
    if (!UserMediaProcessManager::singleton().captureEnabled()) {
        m_pendingRejections.append(userMediaID);
        scheduleNextRejection();
        return;
    }

    if (!m_page.isValid())
        return;

    auto userMediaOrigin = API::SecurityOrigin::create(userMediaDocumentOrigin.get());
    auto topLevelOrigin = API::SecurityOrigin::create(topLevelDocumentOrigin.get());

    auto request = m_pendingUserMediaRequests.add(userMediaID, UserMediaPermissionRequestProxy::create(*this, userMediaID, m_page.mainFrame()->frameID(), frameID, WTFMove(userMediaDocumentOrigin), WTFMove(topLevelDocumentOrigin), { }, { }, WTFMove(userRequest))).iterator->value.copyRef();

    getUserMediaPermissionInfo(frameID, request->userMediaDocumentSecurityOrigin(), request->topLevelDocumentSecurityOrigin(), [this, request = request.releaseNonNull()](Optional<bool> hasPersistentAccess) mutable {
        if (!request->isPending())
            return;

        if (!hasPersistentAccess) {
            request->deny(UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::OtherFailure);
            return;
        }

        processUserMediaPermissionRequest(WTFMove(request), *hasPersistentAccess);
    });
}

void UserMediaPermissionRequestManagerProxy::processUserMediaPermissionRequest(Ref<UserMediaPermissionRequestProxy>&& request, bool hasPersistentAccess)
{
    if (hasPersistentAccess)
        request->setHasPersistentAccess();

    auto& userMediaDocumentSecurityOrigin = request->userMediaDocumentSecurityOrigin();
    auto& topLevelDocumentSecurityOrigin = request->topLevelDocumentSecurityOrigin();
    m_page.websiteDataStore().deviceIdHashSaltStorage().deviceIdHashSaltForOrigin(userMediaDocumentSecurityOrigin, topLevelDocumentSecurityOrigin, [this, request = WTFMove(request)] (String&& deviceIDHashSalt) mutable {
        if (!request->isPending())
            return;

        RealtimeMediaSourceCenter::InvalidConstraintsHandler invalidHandler = [this, request = request.copyRef()](const String& invalidConstraint) {
            if (!request->isPending())
                return;

            if (!m_page.isValid())
                return;

            processUserMediaPermissionInvalidRequest(request.get(), invalidConstraint);
        };

        auto validHandler = [this, request = request.copyRef()](Vector<CaptureDevice>&& audioDevices, Vector<CaptureDevice>&& videoDevices, String&& deviceIdentifierHashSalt) mutable {
            if (!request->isPending())
                return;

            if (!m_page.isValid() || !m_page.mainFrame())
                return;

            processUserMediaPermissionValidRequest(WTFMove(request), WTFMove(audioDevices), WTFMove(videoDevices), WTFMove(deviceIdentifierHashSalt));
        };

        syncWithWebCorePrefs();

        RealtimeMediaSourceCenter::singleton().validateRequestConstraints(WTFMove(validHandler), WTFMove(invalidHandler), request->userRequest(), WTFMove(deviceIDHashSalt));
    });
#else
    UNUSED_PARAM(userMediaID);
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(userMediaDocumentOrigin);
    UNUSED_PARAM(topLevelDocumentOrigin);
    UNUSED_PARAM(userRequest);
#endif
}

#if ENABLE(MEDIA_STREAM)
void UserMediaPermissionRequestManagerProxy::processUserMediaPermissionInvalidRequest(const UserMediaPermissionRequestProxy& request, const String& invalidConstraint)
{
    bool filterConstraint = !request.hasPersistentAccess() && !wasGrantedVideoOrAudioAccess(request.frameID(), request.userMediaDocumentSecurityOrigin(), request.topLevelDocumentSecurityOrigin());

    denyRequest(request.userMediaID(), UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::InvalidConstraint, filterConstraint ? String { } : invalidConstraint);
}

void UserMediaPermissionRequestManagerProxy::processUserMediaPermissionValidRequest(Ref<UserMediaPermissionRequestProxy>&& request, Vector<CaptureDevice>&& audioDevices, Vector<CaptureDevice>&& videoDevices, String&& deviceIdentifierHashSalt)
{
    if (videoDevices.isEmpty() && audioDevices.isEmpty()) {
        denyRequest(request->userMediaID(), UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::NoConstraints, emptyString());
        return;
    }

    request->setDeviceIdentifierHashSalt(WTFMove(deviceIdentifierHashSalt));
    request->setEligibleVideoDeviceUIDs(WTFMove(videoDevices));
    request->setEligibleAudioDeviceUIDs(WTFMove(audioDevices));

    auto action = getRequestAction(request);
    if (action == RequestAction::Deny) {
        denyRequest(request->userMediaID(), UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::PermissionDenied, emptyString());
        return;
    }

    if (action == RequestAction::Grant) {
        ASSERT(request->requestType() != MediaStreamRequest::Type::DisplayMedia);

        if (m_page.isViewVisible())
            grantAccess(request);
        else
            m_pregrantedRequests.append(WTFMove(request));

        return;
    }

    if (m_page.isControlledByAutomation()) {
        if (WebAutomationSession* automationSession = m_page.process().processPool().automationSession()) {
            if (automationSession->shouldAllowGetUserMediaForPage(m_page))
                request->allow();
            else
                userMediaAccessWasDenied(request->userMediaID(), UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::PermissionDenied);

            return;
        }
    }

    if (m_page.preferences().mockCaptureDevicesEnabled() && !m_page.preferences().mockCaptureDevicesPromptEnabled()) {
        request->allow();
        return;
    }

    // If page navigated, there is no need to call the page client for authorization.
    auto* webFrame = m_page.process().webFrame(request->frameID());

    if (!webFrame || !SecurityOrigin::createFromString(m_page.pageLoadState().activeURL())->isSameSchemeHostPort(request->topLevelDocumentSecurityOrigin())) {
        denyRequest(request->userMediaID(), UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::NoConstraints, emptyString());
        return;
    }

    // FIXME: Remove webFrame, userMediaOrigin and topLevelOrigin from this uiClient API call.
    auto userMediaOrigin = API::SecurityOrigin::create(request->userMediaDocumentSecurityOrigin());
    auto topLevelOrigin = API::SecurityOrigin::create(request->topLevelDocumentSecurityOrigin());
    m_page.uiClient().decidePolicyForUserMediaPermissionRequest(m_page, *webFrame, WTFMove(userMediaOrigin), WTFMove(topLevelOrigin), request);
}

void UserMediaPermissionRequestManagerProxy::getUserMediaPermissionInfo(uint64_t frameID, Ref<SecurityOrigin>&& userMediaDocumentOrigin, Ref<SecurityOrigin>&& topLevelDocumentOrigin, CompletionHandler<void(Optional<bool>)>&& handler)
{
    auto* webFrame = m_page.process().webFrame(frameID);
    if (!webFrame || !SecurityOrigin::createFromString(m_page.pageLoadState().activeURL())->isSameSchemeHostPort(topLevelDocumentOrigin.get())) {
        handler({ });
        return;
    }

    auto userMediaOrigin = API::SecurityOrigin::create(userMediaDocumentOrigin.get());
    auto topLevelOrigin = API::SecurityOrigin::create(topLevelDocumentOrigin.get());

    auto requestID = generateRequestID();
    m_pendingDeviceRequests.add(requestID);

    auto request = UserMediaPermissionCheckProxy::create(frameID, [this, weakThis = makeWeakPtr(*this), requestID, handler = WTFMove(handler)](Optional<bool> allowed) mutable {
        if (!weakThis || !m_pendingDeviceRequests.remove(requestID) || !allowed) {
            handler({ });
            return;
        }
        handler(*allowed);
    }, WTFMove(userMediaDocumentOrigin), WTFMove(topLevelDocumentOrigin));

    // FIXME: Remove webFrame, userMediaOrigin and topLevelOrigin from this uiClient API call.
    m_page.uiClient().checkUserMediaPermissionForOrigin(m_page, *webFrame, userMediaOrigin.get(), topLevelOrigin.get(), request.get());
}

bool UserMediaPermissionRequestManagerProxy::wasGrantedVideoOrAudioAccess(uint64_t frameID, const SecurityOrigin& userMediaDocumentOrigin, const SecurityOrigin& topLevelDocumentOrigin)
{
    for (const auto& grantedRequest : m_grantedRequests) {
        if (grantedRequest->requiresDisplayCapture())
            continue;
        if (!grantedRequest->userMediaDocumentSecurityOrigin().isSameSchemeHostPort(userMediaDocumentOrigin))
            continue;
        if (!grantedRequest->topLevelDocumentSecurityOrigin().isSameSchemeHostPort(topLevelDocumentOrigin))
            continue;
        if (grantedRequest->frameID() != frameID)
            continue;

        if (grantedRequest->requiresVideoCapture() || grantedRequest->requiresAudioCapture())
            return true;
    }

    return false;
}

Vector<CaptureDevice> UserMediaPermissionRequestManagerProxy::computeFilteredDeviceList(bool revealIdsAndLabels, const String& deviceIDHashSalt)
{
#if PLATFORM(IOS_FAMILY)
    static const int defaultMaximumCameraCount = 2;
#else
    static const int defaultMaximumCameraCount = 1;
#endif
    static const int defaultMaximumMicrophoneCount = 1;

    auto devices = RealtimeMediaSourceCenter::singleton().getMediaStreamDevices();
    int cameraCount = 0;
    int microphoneCount = 0;

    Vector<CaptureDevice> filteredDevices;
    for (const auto& device : devices) {
        if (!device.enabled() || (device.type() != WebCore::CaptureDevice::DeviceType::Camera && device.type() != WebCore::CaptureDevice::DeviceType::Microphone))
            continue;

        if (!revealIdsAndLabels) {
            if (device.type() == WebCore::CaptureDevice::DeviceType::Camera && ++cameraCount > defaultMaximumCameraCount)
                continue;
            if (device.type() == WebCore::CaptureDevice::DeviceType::Microphone && ++microphoneCount > defaultMaximumMicrophoneCount)
                continue;
        }

        auto label = emptyString();
        auto id = emptyString();
        auto groupId = emptyString();
        if (revealIdsAndLabels) {
            label = device.label();
            id = RealtimeMediaSourceCenter::singleton().hashStringWithSalt(device.persistentId(), deviceIDHashSalt);
            groupId = RealtimeMediaSourceCenter::singleton().hashStringWithSalt(device.groupId(), deviceIDHashSalt);
        }

        filteredDevices.append(CaptureDevice(id, device.type(), label, groupId));
    }

    m_hasFilteredDeviceList = !revealIdsAndLabels;
    return filteredDevices;
}
#endif

void UserMediaPermissionRequestManagerProxy::enumerateMediaDevicesForFrame(uint64_t userMediaID, uint64_t frameID, Ref<SecurityOrigin>&& userMediaDocumentOrigin, Ref<SecurityOrigin>&& topLevelDocumentOrigin)
{
#if ENABLE(MEDIA_STREAM)
    auto completionHandler = [this, userMediaID, frameID, userMediaDocumentOrigin = userMediaDocumentOrigin.copyRef(), topLevelDocumentOrigin = topLevelDocumentOrigin.copyRef()](Optional<bool> originHasPersistentAccess) mutable {
        if (!originHasPersistentAccess)
            return;

        if (!m_page.isValid())
            return;

        auto requestID = generateRequestID();
        m_pendingDeviceRequests.add(requestID);

        auto& requestOrigin = userMediaDocumentOrigin.get();
        auto& topOrigin = topLevelDocumentOrigin.get();
        m_page.websiteDataStore().deviceIdHashSaltStorage().deviceIdHashSaltForOrigin(requestOrigin, topOrigin, [this, weakThis = makeWeakPtr(*this), requestID, frameID, userMediaID, userMediaDocumentOrigin = WTFMove(userMediaDocumentOrigin), topLevelDocumentOrigin = WTFMove(topLevelDocumentOrigin), originHasPersistentAccess = *originHasPersistentAccess] (String&& deviceIDHashSalt) {
            if (!weakThis || !m_pendingDeviceRequests.remove(requestID))
                return;

            if (!m_page.isValid())
                return;

            syncWithWebCorePrefs();

            bool revealIdsAndLabels = originHasPersistentAccess || wasGrantedVideoOrAudioAccess(frameID, userMediaDocumentOrigin.get(), topLevelDocumentOrigin.get());

            m_page.process().send(Messages::WebPage::DidCompleteMediaDeviceEnumeration { userMediaID, computeFilteredDeviceList(revealIdsAndLabels, deviceIDHashSalt), deviceIDHashSalt, originHasPersistentAccess }, m_page.pageID());
        });
    };

    getUserMediaPermissionInfo(frameID, WTFMove(userMediaDocumentOrigin), WTFMove(topLevelDocumentOrigin), WTFMove(completionHandler));
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
    MockRealtimeMediaSourceCenter::setMockRealtimeMediaSourceCenterEnabled(mockDevicesEnabled);
#endif
}

void UserMediaPermissionRequestManagerProxy::captureStateChanged(MediaProducer::MediaStateFlags oldState, MediaProducer::MediaStateFlags newState)
{
    if (!m_page.isValid())
        return;

#if ENABLE(MEDIA_STREAM)
    bool wasCapturingAudio = oldState & MediaProducer::AudioCaptureMask;
    bool wasCapturingVideo = oldState & MediaProducer::VideoCaptureMask;
    bool isCapturingAudio = newState & MediaProducer::AudioCaptureMask;
    bool isCapturingVideo = newState & MediaProducer::VideoCaptureMask;

    if ((wasCapturingAudio && !isCapturingAudio) || (wasCapturingVideo && !isCapturingVideo))
        UserMediaProcessManager::singleton().endedCaptureSession(*this);
    if ((!wasCapturingAudio && isCapturingAudio) || (!wasCapturingVideo && isCapturingVideo))
        UserMediaProcessManager::singleton().startedCaptureSession(*this);

    if (m_captureState == (newState & activeCaptureMask))
        return;

    m_captureState = newState & activeCaptureMask;

    Seconds interval;
    if (m_captureState & activeCaptureMask)
        interval = Seconds::fromHours(m_page.preferences().longRunningMediaCaptureStreamRepromptIntervalInHours());
    else
        interval = Seconds::fromMinutes(m_page.preferences().inactiveMediaCaptureSteamRepromptIntervalInMinutes());

    if (interval == m_currentWatchdogInterval)
        return;

    m_currentWatchdogInterval = interval;
    m_watchdogTimer.startOneShot(m_currentWatchdogInterval);
#endif
}

void UserMediaPermissionRequestManagerProxy::viewIsBecomingVisible()
{
    for (auto& request : m_pregrantedRequests)
        request->allow();
    m_pregrantedRequests.clear();
}

void UserMediaPermissionRequestManagerProxy::watchdogTimerFired()
{
    m_grantedRequests.clear();
    m_pregrantedRequests.clear();
    m_currentWatchdogInterval = 0_s;
    m_hasFilteredDeviceList = false;
}

} // namespace WebKit
