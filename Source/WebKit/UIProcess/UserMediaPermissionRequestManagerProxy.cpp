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

    for (auto& request : m_pendingDeviceRequests.values())
        request->invalidate();
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

    auto requestID = generateRequestID();
    auto handler = [this, requestID](bool originHasPersistentAccess) mutable {

        auto pendingRequest = m_pendingDeviceRequests.take(requestID);
        if (!pendingRequest || !m_page.isValid())
            return;

        if (m_grantedRequests.isEmpty() && !originHasPersistentAccess)
            return;

        m_page.process().send(Messages::WebPage::CaptureDevicesChanged(), m_page.pageID());
    };

    auto origin = WebCore::SecurityOrigin::create(m_page.mainFrame()->url());
    getUserMediaPermissionInfo(requestID, m_page.mainFrame()->frameID(), WTFMove(handler), origin.get(), WTFMove(origin));
#endif
}

void UserMediaPermissionRequestManagerProxy::clearCachedState()
{
    invalidatePendingRequests();
}

Ref<UserMediaPermissionRequestProxy> UserMediaPermissionRequestManagerProxy::createPermissionRequest(uint64_t userMediaID, uint64_t mainFrameID, uint64_t frameID, Ref<SecurityOrigin>&& userMediaDocumentOrigin, Ref<SecurityOrigin>&& topLevelDocumentOrigin, Vector<CaptureDevice>&& audioDevices, Vector<CaptureDevice>&& videoDevices, MediaStreamRequest&& request)
{
    auto permissionRequest = UserMediaPermissionRequestProxy::create(*this, userMediaID, mainFrameID, frameID, WTFMove(userMediaDocumentOrigin), WTFMove(topLevelDocumentOrigin), WTFMove(audioDevices), WTFMove(videoDevices), WTFMove(request));
    m_pendingUserMediaRequests.add(userMediaID, permissionRequest.ptr());
    return permissionRequest;
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

    if (!m_page.isValid() || !m_page.websiteDataStore().deviceIdHashSaltStorage())
        return;

#if ENABLE(MEDIA_STREAM)
    auto request = m_pendingUserMediaRequests.take(userMediaID);
    if (!request)
        return;

    m_page.websiteDataStore().deviceIdHashSaltStorage()->deviceIdHashSaltForOrigin(request->userMediaDocumentSecurityOrigin(), request->topLevelDocumentSecurityOrigin(), [this, weakThis = makeWeakPtr(*this), userMediaID, audioDevice = WTFMove(audioDevice), videoDevice = WTFMove(videoDevice), localRequest = request.copyRef()] (String&& deviceIDHashSalt) mutable {
        if (!weakThis)
            return;
        if (grantAccess(userMediaID, WTFMove(audioDevice), WTFMove(videoDevice), WTFMove(deviceIDHashSalt))) {
            m_grantedRequests.append(localRequest.releaseNonNull());
            if (m_hasFilteredDeviceList)
                captureDevicesChanged();
            m_hasFilteredDeviceList = false;
        }
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

bool UserMediaPermissionRequestManagerProxy::grantAccess(uint64_t userMediaID, const CaptureDevice audioDevice, const CaptureDevice videoDevice, const String& deviceIdentifierHashSalt)
{
    if (!UserMediaProcessManager::singleton().willCreateMediaStream(*this, !!audioDevice, !!videoDevice)) {
        denyRequest(userMediaID, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::OtherFailure, "Unable to extend sandbox.");
        return false;
    }

    m_page.process().send(Messages::WebPage::UserMediaAccessWasGranted(userMediaID, audioDevice, videoDevice, deviceIdentifierHashSalt), m_page.pageID());
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
UserMediaPermissionRequestManagerProxy::RequestAction UserMediaPermissionRequestManagerProxy::getRequestAction(uint64_t frameID, SecurityOrigin& userMediaDocumentOrigin, SecurityOrigin& topLevelDocumentOrigin, const MediaStreamRequest& userRequest, Vector<CaptureDevice>& audioDevices, Vector<CaptureDevice>& videoDevices)
{
    bool requestingScreenCapture = userRequest.type == MediaStreamRequest::Type::DisplayMedia;
    ASSERT(!(requestingScreenCapture && videoDevices.isEmpty()));
    ASSERT(!(requestingScreenCapture && !audioDevices.isEmpty()));
    bool requestingCamera = !requestingScreenCapture && !videoDevices.isEmpty();
    bool requestingMicrophone = !audioDevices.isEmpty();

    if (wasRequestDenied(frameID, userMediaDocumentOrigin, topLevelDocumentOrigin, requestingMicrophone, requestingCamera, requestingScreenCapture))
        return RequestAction::Deny;

    if (userRequest.type == MediaStreamRequest::Type::DisplayMedia)
        return RequestAction::Prompt;

    return searchForGrantedRequest(frameID, userMediaDocumentOrigin, topLevelDocumentOrigin, requestingMicrophone, requestingCamera) ? RequestAction::Grant : RequestAction::Prompt;
}
#endif

void UserMediaPermissionRequestManagerProxy::requestUserMediaPermissionForFrame(uint64_t userMediaID, uint64_t frameID, Ref<SecurityOrigin>&& userMediaDocumentOrigin, Ref<SecurityOrigin>&& topLevelDocumentOrigin, const MediaStreamRequest& userRequest)
{
#if ENABLE(MEDIA_STREAM)
    if (!UserMediaProcessManager::singleton().captureEnabled()) {
        m_pendingRejections.append(userMediaID);
        scheduleNextRejection();
        return;
    }

    RealtimeMediaSourceCenter::InvalidConstraintsHandler invalidHandler = [this, userMediaID](const String& invalidConstraint) {
        if (!m_page.isValid())
            return;

        denyRequest(userMediaID, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::InvalidConstraint, invalidConstraint);
    };

    auto validHandler = [this, userMediaID, frameID, userMediaDocumentOrigin = userMediaDocumentOrigin.copyRef(), topLevelDocumentOrigin = topLevelDocumentOrigin.copyRef(), localUserRequest = userRequest](Vector<CaptureDevice>&& audioDevices, Vector<CaptureDevice>&& videoDevices, String&& deviceIdentifierHashSalt) mutable {
        if (!m_page.isValid() || !m_page.mainFrame())
            return;

        if (videoDevices.isEmpty() && audioDevices.isEmpty()) {
            denyRequest(userMediaID, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::NoConstraints, emptyString());
            return;
        }

        auto action = getRequestAction(m_page.mainFrame()->frameID(), userMediaDocumentOrigin.get(), topLevelDocumentOrigin.get(), localUserRequest, audioDevices, videoDevices);
        if (action == RequestAction::Deny) {
            denyRequest(userMediaID, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::PermissionDenied, emptyString());
            return;
        }

        if (action == RequestAction::Grant) {
            ASSERT(localUserRequest.type != MediaStreamRequest::Type::DisplayMedia);

            if (m_page.isViewVisible()) {
                // We select the first available devices, but the current client API allows client to select which device to pick.
                // FIXME: Remove the possiblity for the client to do the device selection.
                auto audioDevice = !audioDevices.isEmpty() ? audioDevices[0] : CaptureDevice();
                auto videoDevice = !videoDevices.isEmpty() ? videoDevices[0] : CaptureDevice();
                grantAccess(userMediaID, WTFMove(audioDevice), WTFMove(videoDevice), WTFMove(deviceIdentifierHashSalt));
            } else
                m_pregrantedRequests.append(createPermissionRequest(userMediaID, m_page.mainFrame()->frameID(), frameID, WTFMove(userMediaDocumentOrigin), WTFMove(topLevelDocumentOrigin), WTFMove(audioDevices), WTFMove(videoDevices), WTFMove(localUserRequest)));

            return;
        }

        auto userMediaOrigin = API::SecurityOrigin::create(userMediaDocumentOrigin.get());
        auto topLevelOrigin = API::SecurityOrigin::create(topLevelDocumentOrigin.get());
        auto pendingRequest = createPermissionRequest(userMediaID, m_page.mainFrame()->frameID(), frameID, WTFMove(userMediaDocumentOrigin), WTFMove(topLevelDocumentOrigin), WTFMove(audioDevices), WTFMove(videoDevices), WTFMove(localUserRequest));

        if (m_page.isControlledByAutomation()) {
            if (WebAutomationSession* automationSession = m_page.process().processPool().automationSession()) {
                if (automationSession->shouldAllowGetUserMediaForPage(m_page))
                    pendingRequest->allow();
                else
                    userMediaAccessWasDenied(userMediaID, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::PermissionDenied);

                return;
            }
        }

        if (m_page.preferences().mockCaptureDevicesEnabled() && !m_page.preferences().mockCaptureDevicesPromptEnabled()) {
            pendingRequest->allow();
            return;
        }

        if (!m_page.uiClient().decidePolicyForUserMediaPermissionRequest(m_page, *m_page.process().webFrame(frameID), WTFMove(userMediaOrigin), WTFMove(topLevelOrigin), pendingRequest.get()))
            userMediaAccessWasDenied(userMediaID, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::UserMediaDisabled);
    };

    auto requestID = generateRequestID();
    auto havePermissionInfoHandler = [this, requestID, validHandler = WTFMove(validHandler), invalidHandler = WTFMove(invalidHandler), localUserRequest = userRequest](bool originHasPersistentAccess) mutable {

        auto pendingRequest = m_pendingDeviceRequests.take(requestID);
        if (!pendingRequest)
            return;

        if (!m_page.isValid() || !m_page.websiteDataStore().deviceIdHashSaltStorage())
            return;

        syncWithWebCorePrefs();

        m_page.websiteDataStore().deviceIdHashSaltStorage()->deviceIdHashSaltForOrigin(pendingRequest.value()->userMediaDocumentSecurityOrigin(), pendingRequest.value()->topLevelDocumentSecurityOrigin(), [validHandler = WTFMove(validHandler), invalidHandler = WTFMove(invalidHandler), localUserRequest = localUserRequest] (String&& deviceIDHashSalt) mutable {
            RealtimeMediaSourceCenter::singleton().validateRequestConstraints(WTFMove(validHandler), WTFMove(invalidHandler), WTFMove(localUserRequest), WTFMove(deviceIDHashSalt));
        });
    };

    getUserMediaPermissionInfo(requestID, frameID, WTFMove(havePermissionInfoHandler), WTFMove(userMediaDocumentOrigin), WTFMove(topLevelDocumentOrigin));
#else
    UNUSED_PARAM(userMediaID);
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(userMediaDocumentOrigin);
    UNUSED_PARAM(topLevelDocumentOrigin);
    UNUSED_PARAM(userRequest);
#endif
}

#if ENABLE(MEDIA_STREAM)
void UserMediaPermissionRequestManagerProxy::getUserMediaPermissionInfo(uint64_t requestID, uint64_t frameID, UserMediaPermissionCheckProxy::CompletionHandler&& handler, Ref<SecurityOrigin>&& userMediaDocumentOrigin, Ref<SecurityOrigin>&& topLevelDocumentOrigin)
{
    if (!m_page.websiteDataStore().deviceIdHashSaltStorage()) {
        handler(false);
        return;
    }

    auto userMediaOrigin = API::SecurityOrigin::create(userMediaDocumentOrigin.get());
    auto topLevelOrigin = API::SecurityOrigin::create(topLevelDocumentOrigin.get());
    auto request = UserMediaPermissionCheckProxy::create(frameID, WTFMove(handler), WTFMove(userMediaDocumentOrigin), WTFMove(topLevelDocumentOrigin));

    m_pendingDeviceRequests.add(requestID, request.copyRef());
    if (!m_page.uiClient().checkUserMediaPermissionForOrigin(m_page, *m_page.process().webFrame(frameID), userMediaOrigin.get(), topLevelOrigin.get(), request.get())) {
        m_pendingDeviceRequests.take(requestID);
        handler(false);
    }
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
#endif

void UserMediaPermissionRequestManagerProxy::enumerateMediaDevicesForFrame(uint64_t userMediaID, uint64_t frameID, Ref<SecurityOrigin>&& userMediaDocumentOrigin, Ref<SecurityOrigin>&& topLevelDocumentOrigin)
{
#if ENABLE(MEDIA_STREAM)

#if PLATFORM(IOS_FAMILY)
    static const int defaultMaximumCameraCount = 2;
#else
    static const int defaultMaximumCameraCount = 1;
#endif
    static const int defaultMaximumMicrophoneCount = 1;

    auto requestID = generateRequestID();
    auto completionHandler = [this, requestID, userMediaID, requestOrigin = userMediaDocumentOrigin.copyRef(), topOrigin = topLevelDocumentOrigin.copyRef()](bool originHasPersistentAccess) {
        m_page.websiteDataStore().deviceIdHashSaltStorage()->deviceIdHashSaltForOrigin(requestOrigin.get(), topOrigin.get(), [this, weakThis = makeWeakPtr(*this), requestID, userMediaID, &originHasPersistentAccess] (String&& deviceIDHashSalt) {
            if (!weakThis)
                return;
            auto pendingRequest = m_pendingDeviceRequests.take(requestID);
            if (!pendingRequest)
                return;

            if (!m_page.isValid() || !m_page.websiteDataStore().deviceIdHashSaltStorage())
                return;

            syncWithWebCorePrefs();

            auto devices = RealtimeMediaSourceCenter::singleton().getMediaStreamDevices();
            auto& request = *pendingRequest;
            bool revealIdsAndLabels = originHasPersistentAccess || wasGrantedVideoOrAudioAccess(request->frameID(), request->userMediaDocumentSecurityOrigin(), request->topLevelDocumentSecurityOrigin());
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

            m_page.process().send(Messages::WebPage::DidCompleteMediaDeviceEnumeration(userMediaID, WTFMove(filteredDevices), WTFMove(deviceIDHashSalt), originHasPersistentAccess), m_page.pageID());
        });
    };

    getUserMediaPermissionInfo(requestID, frameID, WTFMove(completionHandler), WTFMove(userMediaDocumentOrigin), WTFMove(topLevelDocumentOrigin));
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
