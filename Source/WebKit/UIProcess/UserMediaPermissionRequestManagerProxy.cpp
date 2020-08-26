/*
 * Copyright (C) 2014 Igalia S.L.
 * Copyright (C) 2016-2020 Apple Inc. All rights reserved.
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
#include "Logging.h"
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
#include <wtf/Scope.h>

#if ENABLE(GPU_PROCESS)
#include "GPUProcessMessages.h"
#include "GPUProcessProxy.h"
#endif

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

#if ENABLE(MEDIA_STREAM)
static HashSet<UserMediaPermissionRequestManagerProxy*>& proxies()
{
    static NeverDestroyed<HashSet<UserMediaPermissionRequestManagerProxy*>> set;
    return set;
}

void UserMediaPermissionRequestManagerProxy::forEach(const WTF::Function<void(UserMediaPermissionRequestManagerProxy&)>& function)
{
    for (auto* proxy : proxies())
        function(*proxy);
}
#endif

UserMediaPermissionRequestManagerProxy::UserMediaPermissionRequestManagerProxy(WebPageProxy& page)
    : m_page(page)
    , m_rejectionTimer(RunLoop::main(), this, &UserMediaPermissionRequestManagerProxy::rejectionTimerFired)
    , m_watchdogTimer(RunLoop::main(), this, &UserMediaPermissionRequestManagerProxy::watchdogTimerFired)
#if !RELEASE_LOG_DISABLED
    , m_logger(page.logger())
    , m_logIdentifier(uniqueLogIdentifier())
#endif
{
#if ENABLE(MEDIA_STREAM)
    proxies().add(this);
#endif
    syncWithWebCorePrefs();
}

UserMediaPermissionRequestManagerProxy::~UserMediaPermissionRequestManagerProxy()
{
    m_page.send(Messages::WebPage::StopMediaCapture { });
#if ENABLE(MEDIA_STREAM)
    UserMediaProcessManager::singleton().revokeSandboxExtensionsIfNeeded(page().process());
    proxies().remove(this);
#endif
    invalidatePendingRequests();
}

void UserMediaPermissionRequestManagerProxy::invalidatePendingRequests()
{
    if (m_currentUserMediaRequest) {
        m_currentUserMediaRequest->invalidate();
        m_currentUserMediaRequest = nullptr;
    }

    auto pendingUserMediaRequests = WTFMove(m_pendingUserMediaRequests);
    for (auto& request : pendingUserMediaRequests)
        request->invalidate();

    auto pregrantedRequests = WTFMove(m_pregrantedRequests);
    for (auto& request : pregrantedRequests)
        request->invalidate();

    m_pendingDeviceRequests.clear();
}

void UserMediaPermissionRequestManagerProxy::stopCapture()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    invalidatePendingRequests();
    m_page.stopMediaCapture();
}

void UserMediaPermissionRequestManagerProxy::captureDevicesChanged()
{
#if ENABLE(MEDIA_STREAM)
    ALWAYS_LOG(LOGIDENTIFIER);
    if (!m_page.hasRunningProcess() || !m_page.mainFrame())
        return;

    auto origin = WebCore::SecurityOrigin::create(m_page.mainFrame()->url());
    getUserMediaPermissionInfo(m_page.mainFrame()->frameID(), origin.get(), WTFMove(origin), [this](PermissionInfo permissionInfo) {
        captureDevicesChanged(permissionInfo);
    });
#endif
}

#if ENABLE(MEDIA_STREAM)
void UserMediaPermissionRequestManagerProxy::captureDevicesChanged(PermissionInfo permissionInfo)
{
    switch (permissionInfo) {
    case PermissionInfo::Error:
        return;
    case PermissionInfo::Unknown:
        if (m_grantedRequests.isEmpty())
            return;
        break;
    case PermissionInfo::Granted:
        break;
    }
    if (!m_page.hasRunningProcess())
        return;

    m_page.send(Messages::WebPage::CaptureDevicesChanged());
}
#endif

void UserMediaPermissionRequestManagerProxy::clearCachedState()
{
    ALWAYS_LOG(LOGIDENTIFIER);
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

void UserMediaPermissionRequestManagerProxy::denyRequest(UserMediaPermissionRequestProxy& request, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason reason, const String& invalidConstraint)
{
    if (!m_page.hasRunningProcess())
        return;

    ALWAYS_LOG(LOGIDENTIFIER, request.userMediaID(), ", reason: ", reason);

    if (reason == UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::PermissionDenied)
        m_deniedRequests.append(DeniedRequest { request.mainFrameID(), request.userMediaDocumentSecurityOrigin(), request.topLevelDocumentSecurityOrigin(), request.requiresAudioCapture(), request.requiresVideoCapture(), request.requiresDisplayCapture() });

#if ENABLE(MEDIA_STREAM)
    m_page.send(Messages::WebPage::UserMediaAccessWasDenied(request.userMediaID(), toWebCore(reason), invalidConstraint));
#else
    UNUSED_PARAM(reason);
    UNUSED_PARAM(invalidConstraint);
#endif

    processNextUserMediaRequestIfNeeded();
}

void UserMediaPermissionRequestManagerProxy::grantRequest(UserMediaPermissionRequestProxy& request)
{
    if (!m_page.hasRunningProcess())
        return;

#if ENABLE(MEDIA_STREAM)
    ALWAYS_LOG(LOGIDENTIFIER, request.userMediaID(), ", video: ", request.videoDevice().label(), ", audio: ", request.audioDevice().label());

    auto& userMediaDocumentSecurityOrigin = request.userMediaDocumentSecurityOrigin();
    auto& topLevelDocumentSecurityOrigin = request.topLevelDocumentSecurityOrigin();
    m_page.websiteDataStore().deviceIdHashSaltStorage().deviceIdHashSaltForOrigin(userMediaDocumentSecurityOrigin, topLevelDocumentSecurityOrigin, [this, weakThis = makeWeakPtr(*this), request = makeRef(request)](String&&) mutable {
        if (!weakThis)
            return;
        finishGrantingRequest(request);
    });
#else
    UNUSED_PARAM(request);
#endif
}

#if ENABLE(MEDIA_STREAM)
void UserMediaPermissionRequestManagerProxy::finishGrantingRequest(UserMediaPermissionRequestProxy& request)
{
    ALWAYS_LOG(LOGIDENTIFIER, request.userMediaID());
    if (!UserMediaProcessManager::singleton().willCreateMediaStream(*this, request.hasAudioDevice(), request.hasVideoDevice())) {
        denyRequest(request, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::OtherFailure, "Unable to extend sandbox.");
        return;
    }

    m_page.willStartCapture(request, [this, weakThis = makeWeakPtr(this), strongRequest = makeRef(request)]() mutable {
        if (!weakThis)
            return;

        auto& request = strongRequest.get();

        if (request.requestType() == MediaStreamRequest::Type::UserMedia)
            m_grantedRequests.append(makeRef(request));

        // FIXME: m_hasFilteredDeviceList will trigger ondevicechange events for various documents from different origins.
        if (m_hasFilteredDeviceList)
            captureDevicesChanged(PermissionInfo::Granted);
        m_hasFilteredDeviceList = false;

        ++m_hasPendingCapture;

        SandboxExtension::Handle handle;
#if PLATFORM(COCOA)
        if (!m_hasCreatedSandboxExtensionForTCCD) {
            SandboxExtension::createHandleForMachLookup("com.apple.tccd"_s, m_page.process().connection()->getAuditToken(), handle);
            m_hasCreatedSandboxExtensionForTCCD = true;
        }
#endif

        m_page.sendWithAsyncReply(Messages::WebPage::UserMediaAccessWasGranted { request.userMediaID(), request.audioDevice(), request.videoDevice(), request.deviceIdentifierHashSalt(), handle }, [this, weakThis = WTFMove(weakThis)] {
            if (!weakThis)
                return;
            if (!--m_hasPendingCapture)
                UserMediaProcessManager::singleton().revokeSandboxExtensionsIfNeeded(page().process());
        });

        processNextUserMediaRequestIfNeeded();
    });
}

void UserMediaPermissionRequestManagerProxy::resetAccess(Optional<FrameIdentifier> frameID)
{
    if (frameID) {
        ALWAYS_LOG(LOGIDENTIFIER, frameID ? frameID->loggingString() : String { });
        m_grantedRequests.removeAllMatching([frameID](const auto& grantedRequest) {
            return grantedRequest->mainFrameID() == frameID;
        });
    } else
        m_grantedRequests.clear();
    m_pregrantedRequests.clear();
    m_deniedRequests.clear();
    m_hasFilteredDeviceList = false;
}

const UserMediaPermissionRequestProxy* UserMediaPermissionRequestManagerProxy::searchForGrantedRequest(FrameIdentifier frameID, const SecurityOrigin& userMediaDocumentOrigin, const SecurityOrigin& topLevelDocumentOrigin, bool needsAudio, bool needsVideo) const
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

bool UserMediaPermissionRequestManagerProxy::wasRequestDenied(FrameIdentifier mainFrameID, const SecurityOrigin& userMediaDocumentOrigin, const SecurityOrigin& topLevelDocumentOrigin, bool needsAudio, bool needsVideo, bool needsScreenCapture)
{
    for (const auto& deniedRequest : m_deniedRequests) {
        if (!deniedRequest.userMediaDocumentOrigin->isSameSchemeHostPort(userMediaDocumentOrigin))
            continue;
        if (!deniedRequest.topLevelDocumentOrigin->isSameSchemeHostPort(topLevelDocumentOrigin))
            continue;
        if (deniedRequest.mainFrameID != mainFrameID)
            continue;

        if (deniedRequest.isScreenCaptureDenied && needsScreenCapture)
            return true;

        // In case we asked for both audio and video, maybe the callback would have returned true for just audio or just video.
        if (deniedRequest.isAudioDenied && deniedRequest.isVideoDenied) {
            if (needsAudio && needsVideo)
                return true;
            continue;
        }

        if (deniedRequest.isAudioDenied && needsAudio)
            return true;
        if (deniedRequest.isVideoDenied && needsVideo)
            return true;
    }
    return false;
}

#endif

void UserMediaPermissionRequestManagerProxy::rejectionTimerFired()
{
    denyRequest(m_pendingRejections.takeFirst(), UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::PermissionDenied, emptyString());
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

    if (!request.isUserGesturePriviledged() && wasRequestDenied(request.frameID(), request.userMediaDocumentSecurityOrigin(), request.topLevelDocumentSecurityOrigin(), requestingMicrophone, requestingCamera, requestingScreenCapture))
        return RequestAction::Deny;

    if (request.requestType() == MediaStreamRequest::Type::DisplayMedia)
        return RequestAction::Prompt;

    return searchForGrantedRequest(request.frameID(), request.userMediaDocumentSecurityOrigin(), request.topLevelDocumentSecurityOrigin(), requestingMicrophone, requestingCamera) ? RequestAction::Grant : RequestAction::Prompt;
}
#endif

void UserMediaPermissionRequestManagerProxy::requestUserMediaPermissionForFrame(uint64_t userMediaID, FrameIdentifier frameID, Ref<SecurityOrigin>&& userMediaDocumentOrigin, Ref<SecurityOrigin>&& topLevelDocumentOrigin, MediaStreamRequest&& userRequest)
{
#if ENABLE(MEDIA_STREAM)
    if (!m_page.hasRunningProcess())
        return;

    ALWAYS_LOG(LOGIDENTIFIER, userMediaID);

    auto request = UserMediaPermissionRequestProxy::create(*this, userMediaID, m_page.mainFrame()->frameID(), frameID, WTFMove(userMediaDocumentOrigin), WTFMove(topLevelDocumentOrigin), { }, { }, WTFMove(userRequest));
    if (m_currentUserMediaRequest) {
        m_pendingUserMediaRequests.append(WTFMove(request));
        return;
    }

    if (!UserMediaProcessManager::singleton().captureEnabled()) {
        ALWAYS_LOG(LOGIDENTIFIER, "capture disabled");
        m_pendingRejections.append(WTFMove(request));
        scheduleNextRejection();
        return;
    }

    startProcessingUserMediaPermissionRequest(WTFMove(request));
#else
    UNUSED_PARAM(userMediaID);
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(userMediaDocumentOrigin);
    UNUSED_PARAM(topLevelDocumentOrigin);
    UNUSED_PARAM(userRequest);
#endif
}

void UserMediaPermissionRequestManagerProxy::processNextUserMediaRequestIfNeeded()
{
#if ENABLE(MEDIA_STREAM)
    if (m_pendingUserMediaRequests.isEmpty()) {
        m_currentUserMediaRequest = nullptr;
        return;
    }
    startProcessingUserMediaPermissionRequest(m_pendingUserMediaRequests.takeFirst());
#endif
}

#if ENABLE(MEDIA_STREAM)
void UserMediaPermissionRequestManagerProxy::startProcessingUserMediaPermissionRequest(Ref<UserMediaPermissionRequestProxy>&& request)
{
    m_currentUserMediaRequest = WTFMove(request);

    auto& userMediaDocumentSecurityOrigin = m_currentUserMediaRequest->userMediaDocumentSecurityOrigin();
    auto& topLevelDocumentSecurityOrigin = m_currentUserMediaRequest->topLevelDocumentSecurityOrigin();
    getUserMediaPermissionInfo(m_currentUserMediaRequest->frameID(), userMediaDocumentSecurityOrigin, topLevelDocumentSecurityOrigin, [this, request = m_currentUserMediaRequest](auto permissionInfo) mutable {
        if (!request->isPending())
            return;

        switch (permissionInfo) {
        case PermissionInfo::Error:
            this->denyRequest(*m_currentUserMediaRequest, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::OtherFailure);
            return;
        case PermissionInfo::Unknown:
            break;
        case PermissionInfo::Granted:
            m_currentUserMediaRequest->setHasPersistentAccess();
            break;
        }
        this->processUserMediaPermissionRequest();
    });
}

void UserMediaPermissionRequestManagerProxy::processUserMediaPermissionRequest()
{
    ALWAYS_LOG(LOGIDENTIFIER, m_currentUserMediaRequest->userMediaID(), ", persistent access: ", m_currentUserMediaRequest->hasPersistentAccess());

    auto& userMediaDocumentSecurityOrigin = m_currentUserMediaRequest->userMediaDocumentSecurityOrigin();
    auto& topLevelDocumentSecurityOrigin = m_currentUserMediaRequest->topLevelDocumentSecurityOrigin();
    m_page.websiteDataStore().deviceIdHashSaltStorage().deviceIdHashSaltForOrigin(userMediaDocumentSecurityOrigin, topLevelDocumentSecurityOrigin, [this, request = m_currentUserMediaRequest] (String&& deviceIDHashSalt) mutable {
        if (!request->isPending())
            return;

        RealtimeMediaSourceCenter::InvalidConstraintsHandler invalidHandler = [this, request](const String& invalidConstraint) {
            if (!request->isPending())
                return;

            if (!m_page.hasRunningProcess())
                return;

            processUserMediaPermissionInvalidRequest(invalidConstraint);
        };

        auto validHandler = [this, request](Vector<CaptureDevice>&& audioDevices, Vector<CaptureDevice>&& videoDevices, String&& deviceIdentifierHashSalt) mutable {
            if (!request->isPending())
                return;

            if (!m_page.hasRunningProcess() || !m_page.mainFrame())
                return;

            processUserMediaPermissionValidRequest(WTFMove(audioDevices), WTFMove(videoDevices), WTFMove(deviceIdentifierHashSalt));
        };

        syncWithWebCorePrefs();

        RealtimeMediaSourceCenter::singleton().validateRequestConstraints(WTFMove(validHandler), WTFMove(invalidHandler), m_currentUserMediaRequest->userRequest(), WTFMove(deviceIDHashSalt));
    });
}
#endif

#if ENABLE(MEDIA_STREAM)
void UserMediaPermissionRequestManagerProxy::processUserMediaPermissionInvalidRequest(const String& invalidConstraint)
{
    ALWAYS_LOG(LOGIDENTIFIER, m_currentUserMediaRequest->userMediaID());
    bool filterConstraint = !m_currentUserMediaRequest->hasPersistentAccess() && !wasGrantedVideoOrAudioAccess(m_currentUserMediaRequest->frameID(), m_currentUserMediaRequest->userMediaDocumentSecurityOrigin(), m_currentUserMediaRequest->topLevelDocumentSecurityOrigin());

    denyRequest(*m_currentUserMediaRequest, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::InvalidConstraint, filterConstraint ? String { } : invalidConstraint);
}

void UserMediaPermissionRequestManagerProxy::processUserMediaPermissionValidRequest(Vector<CaptureDevice>&& audioDevices, Vector<CaptureDevice>&& videoDevices, String&& deviceIdentifierHashSalt)
{
    ALWAYS_LOG(LOGIDENTIFIER, m_currentUserMediaRequest->userMediaID(), ", video: ", videoDevices.size(), " audio: ", audioDevices.size());
    if (videoDevices.isEmpty() && audioDevices.isEmpty()) {
        denyRequest(*m_currentUserMediaRequest, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::NoConstraints, emptyString());
        return;
    }

    m_currentUserMediaRequest->setDeviceIdentifierHashSalt(WTFMove(deviceIdentifierHashSalt));
    m_currentUserMediaRequest->setEligibleVideoDeviceUIDs(WTFMove(videoDevices));
    m_currentUserMediaRequest->setEligibleAudioDeviceUIDs(WTFMove(audioDevices));

    auto action = getRequestAction(*m_currentUserMediaRequest);
    ALWAYS_LOG(LOGIDENTIFIER, m_currentUserMediaRequest->userMediaID(), ", action: ", action);

    if (action == RequestAction::Deny) {
        denyRequest(*m_currentUserMediaRequest, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::PermissionDenied, emptyString());
        return;
    }

    if (action == RequestAction::Grant) {
        ASSERT(m_currentUserMediaRequest->requestType() != MediaStreamRequest::Type::DisplayMedia);

        if (m_page.isViewVisible())
            grantRequest(*m_currentUserMediaRequest);
        else
            m_pregrantedRequests.append(m_currentUserMediaRequest.releaseNonNull());

        return;
    }

    if (m_page.isControlledByAutomation()) {
        if (WebAutomationSession* automationSession = m_page.process().processPool().automationSession()) {
            ALWAYS_LOG(LOGIDENTIFIER, m_currentUserMediaRequest->userMediaID(), ", page controlled by automation");
            if (automationSession->shouldAllowGetUserMediaForPage(m_page))
                grantRequest(*m_currentUserMediaRequest);
            else
                denyRequest(*m_currentUserMediaRequest, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::PermissionDenied);
            return;
        }
    }

    if (m_page.preferences().mockCaptureDevicesEnabled() && !m_page.preferences().mockCaptureDevicesPromptEnabled()) {
        ALWAYS_LOG(LOGIDENTIFIER, m_currentUserMediaRequest->userMediaID(), ", mock devices don't require prompt");
        grantRequest(*m_currentUserMediaRequest);
        return;
    }

    // If page navigated, there is no need to call the page client for authorization.
    auto* webFrame = m_page.process().webFrame(m_currentUserMediaRequest->frameID());

    if (!webFrame || !SecurityOrigin::createFromString(m_page.pageLoadState().activeURL())->isSameSchemeHostPort(m_currentUserMediaRequest->topLevelDocumentSecurityOrigin())) {
        denyRequest(*m_currentUserMediaRequest, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::NoConstraints, emptyString());
        return;
    }

    // FIXME: Remove webFrame, userMediaOrigin and topLevelOrigin from this uiClient API call.
    auto userMediaOrigin = API::SecurityOrigin::create(m_currentUserMediaRequest->userMediaDocumentSecurityOrigin());
    auto topLevelOrigin = API::SecurityOrigin::create(m_currentUserMediaRequest->topLevelDocumentSecurityOrigin());
    m_page.uiClient().decidePolicyForUserMediaPermissionRequest(m_page, *webFrame, WTFMove(userMediaOrigin), WTFMove(topLevelOrigin), *m_currentUserMediaRequest);
}

void UserMediaPermissionRequestManagerProxy::getUserMediaPermissionInfo(FrameIdentifier frameID, Ref<SecurityOrigin>&& userMediaDocumentOrigin, Ref<SecurityOrigin>&& topLevelDocumentOrigin, CompletionHandler<void(PermissionInfo)>&& handler)
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

    auto request = UserMediaPermissionCheckProxy::create(frameID, [this, weakThis = makeWeakPtr(*this), requestID, handler = WTFMove(handler)](auto permissionInfo) mutable {
        if (!weakThis || !m_pendingDeviceRequests.remove(requestID))
            permissionInfo = PermissionInfo::Error;
        handler(permissionInfo);
    }, WTFMove(userMediaDocumentOrigin), WTFMove(topLevelDocumentOrigin));

    // FIXME: Remove webFrame, userMediaOrigin and topLevelOrigin from this uiClient API call.
    m_page.uiClient().checkUserMediaPermissionForOrigin(m_page, *webFrame, userMediaOrigin.get(), topLevelOrigin.get(), request.get());
}

bool UserMediaPermissionRequestManagerProxy::wasGrantedVideoOrAudioAccess(FrameIdentifier frameID, const SecurityOrigin& userMediaDocumentOrigin, const SecurityOrigin& topLevelDocumentOrigin)
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

static inline bool haveMicrophoneDevice(const Vector<WebCore::CaptureDevice>& devices, const String& deviceID)
{
    return std::any_of(devices.begin(), devices.end(), [&deviceID](auto& device) {
        return device.persistentId() == deviceID && device.type() == CaptureDevice::DeviceType::Microphone;
    });
}

Vector<CaptureDevice> UserMediaPermissionRequestManagerProxy::computeFilteredDeviceList(bool revealIdsAndLabels, const String& deviceIDHashSalt)
{
    static const int defaultMaximumCameraCount = 1;
    static const int defaultMaximumMicrophoneCount = 1;

    auto devices = RealtimeMediaSourceCenter::singleton().getMediaStreamDevices();
    int cameraCount = 0;
    int microphoneCount = 0;

    Vector<CaptureDevice> filteredDevices;
    for (const auto& device : devices) {
        if (!device.enabled() || (device.type() != WebCore::CaptureDevice::DeviceType::Camera && device.type() != WebCore::CaptureDevice::DeviceType::Microphone && device.type() != WebCore::CaptureDevice::DeviceType::Speaker))
            continue;

        if (!revealIdsAndLabels) {
            if (device.type() == WebCore::CaptureDevice::DeviceType::Camera && ++cameraCount > defaultMaximumCameraCount)
                continue;
            if (device.type() == WebCore::CaptureDevice::DeviceType::Microphone && ++microphoneCount > defaultMaximumMicrophoneCount)
                continue;
            if (device.type() != WebCore::CaptureDevice::DeviceType::Camera && device.type() != WebCore::CaptureDevice::DeviceType::Microphone)
                continue;
        } else {
            // We only expose speakers tied to a microphone for the moment.
            if (device.type() == WebCore::CaptureDevice::DeviceType::Speaker && !haveMicrophoneDevice(devices, device.groupId()))
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

    ALWAYS_LOG(LOGIDENTIFIER, filteredDevices.size(), " devices revealed");
    return filteredDevices;
}
#endif

void UserMediaPermissionRequestManagerProxy::enumerateMediaDevicesForFrame(FrameIdentifier frameID, Ref<SecurityOrigin>&& userMediaDocumentOrigin, Ref<SecurityOrigin>&& topLevelDocumentOrigin, CompletionHandler<void(const Vector<CaptureDevice>&, const String&)>&& completionHandler)
{
#if ENABLE(MEDIA_STREAM)
    ALWAYS_LOG(LOGIDENTIFIER);

    auto callback = [this, frameID, userMediaDocumentOrigin, topLevelDocumentOrigin, completionHandler = WTFMove(completionHandler)](PermissionInfo permissionInfo) mutable {
        auto callCompletionHandler = makeScopeExit([&completionHandler] {
            completionHandler({ }, { });
        });

        bool originHasPersistentAccess;
        switch (permissionInfo) {
        case PermissionInfo::Error:
            return;
        case PermissionInfo::Unknown:
            originHasPersistentAccess = false;
            break;
        case PermissionInfo::Granted:
            originHasPersistentAccess = true;
            break;
        }

        if (!m_page.hasRunningProcess())
            return;

        auto requestID = generateRequestID();
        m_pendingDeviceRequests.add(requestID);

        auto& requestOrigin = userMediaDocumentOrigin.get();
        auto& topOrigin = topLevelDocumentOrigin.get();

        callCompletionHandler.release();
        m_page.websiteDataStore().deviceIdHashSaltStorage().deviceIdHashSaltForOrigin(requestOrigin, topOrigin, [this, weakThis = makeWeakPtr(*this), requestID, frameID, userMediaDocumentOrigin = WTFMove(userMediaDocumentOrigin), topLevelDocumentOrigin = WTFMove(topLevelDocumentOrigin), originHasPersistentAccess, completionHandler = WTFMove(completionHandler)](String&& deviceIDHashSalt) mutable {
            auto callCompletionHandler = makeScopeExit([&completionHandler] {
                completionHandler({ }, { });
            });

            if (!weakThis || !m_pendingDeviceRequests.remove(requestID))
                return;

            if (!m_page.hasRunningProcess())
                return;

            syncWithWebCorePrefs();

            bool revealIdsAndLabels = originHasPersistentAccess || wasGrantedVideoOrAudioAccess(frameID, userMediaDocumentOrigin.get(), topLevelDocumentOrigin.get());

            callCompletionHandler.release();
            completionHandler(computeFilteredDeviceList(revealIdsAndLabels, deviceIDHashSalt), deviceIDHashSalt);
        });
    };

    getUserMediaPermissionInfo(frameID, WTFMove(userMediaDocumentOrigin), WTFMove(topLevelDocumentOrigin), WTFMove(callback));
#else
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(userMediaDocumentOrigin);
    UNUSED_PARAM(topLevelDocumentOrigin);
    completionHandler({ }, { });
#endif
}

void UserMediaPermissionRequestManagerProxy::setMockCaptureDevicesEnabledOverride(Optional<bool> enabled)
{
    m_mockDevicesEnabledOverride = enabled;
    syncWithWebCorePrefs();
}

void UserMediaPermissionRequestManagerProxy::syncWithWebCorePrefs() const
{
#if ENABLE(MEDIA_STREAM)
    // Enable/disable the mock capture devices for the UI process as per the WebCore preferences. Note that
    // this is a noop if the preference hasn't changed since the last time this was called.
    bool mockDevicesEnabled = m_mockDevicesEnabledOverride ? *m_mockDevicesEnabledOverride : m_page.preferences().mockCaptureDevicesEnabled();

#if ENABLE(GPU_PROCESS)
    if (m_page.preferences().captureAudioInGPUProcessEnabled() || m_page.preferences().captureVideoInGPUProcessEnabled())
        GPUProcessProxy::singleton().setUseMockCaptureDevices(mockDevicesEnabled);
#endif

    if (MockRealtimeMediaSourceCenter::mockRealtimeMediaSourceCenterEnabled() == mockDevicesEnabled)
        return;
    MockRealtimeMediaSourceCenter::setMockRealtimeMediaSourceCenterEnabled(mockDevicesEnabled);
#endif
}

void UserMediaPermissionRequestManagerProxy::captureStateChanged(MediaProducer::MediaStateFlags oldState, MediaProducer::MediaStateFlags newState)
{
    if (!m_page.hasRunningProcess())
        return;

#if ENABLE(MEDIA_STREAM)
    if (!m_hasPendingCapture)
        UserMediaProcessManager::singleton().revokeSandboxExtensionsIfNeeded(page().process());

    if (m_captureState == (newState & activeCaptureMask))
        return;

    ALWAYS_LOG(LOGIDENTIFIER, "state was: ", m_captureState, ", is now: ", newState & activeCaptureMask);
    m_captureState = newState & activeCaptureMask;

    Seconds interval;
    if (m_captureState & activeCaptureMask)
        interval = Seconds::fromHours(m_page.preferences().longRunningMediaCaptureStreamRepromptIntervalInHours());
    else
        interval = Seconds::fromMinutes(m_page.preferences().inactiveMediaCaptureSteamRepromptIntervalInMinutes());

    if (interval == m_currentWatchdogInterval)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, "watchdog set to ", interval.value());
    m_currentWatchdogInterval = interval;
    m_watchdogTimer.startOneShot(m_currentWatchdogInterval);
#endif
}

void UserMediaPermissionRequestManagerProxy::viewIsBecomingVisible()
{
    auto pregrantedRequests = WTFMove(m_pregrantedRequests);
    for (auto& request : pregrantedRequests)
        grantRequest(request);
}

void UserMediaPermissionRequestManagerProxy::watchdogTimerFired()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_grantedRequests.clear();
    m_pregrantedRequests.clear();
    m_currentWatchdogInterval = 0_s;
    m_hasFilteredDeviceList = false;
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& UserMediaPermissionRequestManagerProxy::logChannel() const
{
    return WebKit2LogWebRTC;
}

const Logger& UserMediaPermissionRequestManagerProxy::logger() const
{
    return m_page.logger();
}
#endif

String convertEnumerationToString(UserMediaPermissionRequestManagerProxy::RequestAction enumerationValue)
{
    static const NeverDestroyed<String> values[] = {
        MAKE_STATIC_STRING_IMPL("Deny"),
        MAKE_STATIC_STRING_IMPL("Grant"),
        MAKE_STATIC_STRING_IMPL("Prompt"),
    };
    static_assert(static_cast<size_t>(UserMediaPermissionRequestManagerProxy::RequestAction::Deny) == 0, "UserMediaPermissionRequestManagerProxy::RequestAction::Deny is not 0 as expected");
    static_assert(static_cast<size_t>(UserMediaPermissionRequestManagerProxy::RequestAction::Grant) == 1, "UserMediaPermissionRequestManagerProxy::RequestAction::Grant is not 1 as expected");
    static_assert(static_cast<size_t>(UserMediaPermissionRequestManagerProxy::RequestAction::Prompt) == 2, "UserMediaPermissionRequestManagerProxy::RequestAction::Prompt is not 2 as expected");
    ASSERT(static_cast<size_t>(enumerationValue) < WTF_ARRAY_LENGTH(values));
    return values[static_cast<size_t>(enumerationValue)];
}

} // namespace WebKit
