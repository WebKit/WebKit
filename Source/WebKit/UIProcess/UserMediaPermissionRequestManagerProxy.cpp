/*
 * Copyright (C) 2014 Igalia S.L.
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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
#include "MediaPermissionUtilities.h"
#include "MessageSenderInlines.h"
#include "PageLoadState.h"
#include "UserMediaPermissionRequestManager.h"
#include "UserMediaProcessManager.h"
#include "WebAutomationSession.h"
#include "WebFrameProxy.h"
#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include "WebProcess.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include "WebsiteDataStore.h"
#include <WebCore/CaptureDeviceWithCapabilities.h>
#include <WebCore/MediaConstraints.h>
#include <WebCore/MockRealtimeMediaSourceCenter.h>
#include <WebCore/PermissionName.h>
#include <WebCore/PlatformMediaSessionManager.h>
#include <WebCore/RealtimeMediaSource.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/UserMediaRequest.h>
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/Scope.h>

#if ENABLE(GPU_PROCESS)
#include "GPUProcessMessages.h"
#include "GPUProcessProxy.h"
#endif

#if HAVE(SCREEN_CAPTURE_KIT)
#include <WebCore/ScreenCaptureKitCaptureSource.h>
#endif

namespace WebKit {
using namespace WebCore;

#if ENABLE(MEDIA_STREAM)
static const MediaProducerMediaStateFlags activeCaptureMask { MediaProducerMediaState::HasActiveAudioCaptureDevice, MediaProducerMediaState::HasActiveVideoCaptureDevice };
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

#if !PLATFORM(COCOA)
bool UserMediaPermissionRequestManagerProxy::permittedToCaptureAudio()
{
    return true;
}

bool UserMediaPermissionRequestManagerProxy::permittedToCaptureVideo()
{
    return true;
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
    m_page.sendWithAsyncReply(Messages::WebPage::StopMediaCapture { MediaProducerMediaCaptureKind::EveryKind }, [] { });
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

#if ENABLE(MEDIA_STREAM)
    RealtimeMediaSourceCenter::singleton().audioCaptureFactory().removeExtensiveObserver(*this);
#endif
}

void UserMediaPermissionRequestManagerProxy::stopCapture()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    invalidatePendingRequests();
    m_page.stopMediaCapture(MediaProducerMediaCaptureKind::EveryKind);
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
        return static_cast<uint64_t>(MediaAccessDenialReason::NoConstraints);
    case UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::UserMediaDisabled:
        return static_cast<uint64_t>(MediaAccessDenialReason::UserMediaDisabled);
    case UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::NoCaptureDevices:
        return static_cast<uint64_t>(MediaAccessDenialReason::NoCaptureDevices);
    case UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::InvalidConstraint:
        return static_cast<uint64_t>(MediaAccessDenialReason::InvalidConstraint);
    case UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::HardwareError:
        return static_cast<uint64_t>(MediaAccessDenialReason::HardwareError);
    case UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::PermissionDenied:
        return static_cast<uint64_t>(MediaAccessDenialReason::PermissionDenied);
    case UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::OtherFailure:
        return static_cast<uint64_t>(MediaAccessDenialReason::OtherFailure);
    }

    ASSERT_NOT_REACHED();
    return static_cast<uint64_t>(MediaAccessDenialReason::OtherFailure);
}
#endif

void UserMediaPermissionRequestManagerProxy::denyRequest(UserMediaPermissionRequestProxy& request, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason reason, const String& invalidConstraint)
{
    if (!m_page.hasRunningProcess())
        return;

    ALWAYS_LOG(LOGIDENTIFIER, request.userMediaID().toUInt64(), ", reason: ", reason);

    if (reason == UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::PermissionDenied)
        m_deniedRequests.append(DeniedRequest { request.mainFrameID(), request.userMediaDocumentSecurityOrigin(), request.topLevelDocumentSecurityOrigin(), request.requiresAudioCapture(), request.requiresVideoCapture(), request.requiresDisplayCapture() });

    if (auto callback = request.decisionCompletionHandler()) {
        callback(false);
        return;
    }

#if ENABLE(MEDIA_STREAM)
    if (m_pregrantedRequests.isEmpty() && request.userRequest().audioConstraints.isValid)
        RealtimeMediaSourceCenter::singleton().audioCaptureFactory().removeExtensiveObserver(*this);

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
    ALWAYS_LOG(LOGIDENTIFIER, request.userMediaID().toUInt64(), ", video: ", request.videoDevice().label(), ", audio: ", request.audioDevice().label());

    if (auto callback = request.decisionCompletionHandler()) {
        m_page.willStartCapture(request, [callback = WTFMove(callback)]() mutable {
            callback(true);
        });
        m_grantedRequests.append(request);
        m_grantedFrames.add(request.frameID());
        return;
    }

    auto& userMediaDocumentSecurityOrigin = request.userMediaDocumentSecurityOrigin();
    auto& topLevelDocumentSecurityOrigin = request.topLevelDocumentSecurityOrigin();
    m_page.websiteDataStore().deviceIdHashSaltStorage().deviceIdHashSaltForOrigin(userMediaDocumentSecurityOrigin, topLevelDocumentSecurityOrigin, [this, weakThis = WeakPtr { *this }, request = Ref { request }](String&&) mutable {
        if (!weakThis)
            return;
        finishGrantingRequest(request);
    });
#else
    UNUSED_PARAM(request);
#endif
}

#if ENABLE(MEDIA_STREAM)

#if PLATFORM(COCOA)
static bool doesPageNeedTCCD(const WebPageProxy& page)
{
    return (!page.preferences().captureAudioInGPUProcessEnabled() && !page.preferences().captureAudioInUIProcessEnabled()) || !page.preferences().captureVideoInGPUProcessEnabled();
}
#endif

void UserMediaPermissionRequestManagerProxy::finishGrantingRequest(UserMediaPermissionRequestProxy& request)
{
    ALWAYS_LOG(LOGIDENTIFIER, request.userMediaID().toUInt64());
    updateStoredRequests(request);

    if (!UserMediaProcessManager::singleton().willCreateMediaStream(*this, request)) {
        denyRequest(request, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::OtherFailure, "Unable to extend sandbox."_s);
        return;
    }

    m_page.willStartCapture(request, [this, weakThis = WeakPtr { *this }, strongRequest = Ref { request }]() mutable {
        if (!weakThis)
            return;

        // FIXME: m_hasFilteredDeviceList will trigger ondevicechange events for various documents from different origins.
        if (m_hasFilteredDeviceList)
            captureDevicesChanged(PermissionInfo::Granted);
        m_hasFilteredDeviceList = false;

        ++m_hasPendingCapture;

        Vector<SandboxExtension::Handle> handles;
#if PLATFORM(COCOA)
        if (!m_hasCreatedSandboxExtensionForTCCD && doesPageNeedTCCD(m_page)) {
            handles = SandboxExtension::createHandlesForMachLookup({ "com.apple.tccd"_s }, m_page.process().auditToken(), SandboxExtension::MachBootstrapOptions::EnableMachBootstrap);
            m_hasCreatedSandboxExtensionForTCCD = true;
        }
#endif

        auto& request = strongRequest.get();
        m_page.sendWithAsyncReply(Messages::WebPage::UserMediaAccessWasGranted { request.userMediaID(), request.audioDevice(), request.videoDevice(), request.deviceIdentifierHashSalts(), handles }, [this, weakThis = WTFMove(weakThis)] {
            if (!weakThis)
                return;
            if (!--m_hasPendingCapture)
                UserMediaProcessManager::singleton().revokeSandboxExtensionsIfNeeded(page().process());
        });

        processNextUserMediaRequestIfNeeded();
    });
}

void UserMediaPermissionRequestManagerProxy::didCommitLoadForFrame(FrameIdentifier frameID)
{
    ALWAYS_LOG(LOGIDENTIFIER, frameID.object().toUInt64());
    m_frameEphemeralHashSalts.remove(frameID);
}

void UserMediaPermissionRequestManagerProxy::resetAccess(std::optional<FrameIdentifier> frameID)
{
    ALWAYS_LOG(LOGIDENTIFIER, frameID ? frameID->object().toUInt64() : 0);

    if (frameID) {
        m_grantedRequests.removeAllMatching([frameID](const auto& grantedRequest) {
            return grantedRequest->mainFrameID() == frameID;
        });
        m_grantedFrames.remove(*frameID);
        m_frameEphemeralHashSalts.remove(*frameID);
    } else {
        m_grantedRequests.clear();
        m_grantedFrames.clear();
        m_frameEphemeralHashSalts.clear();
    }
    m_pregrantedRequests.clear();
    m_deniedRequests.clear();
    m_hasFilteredDeviceList = false;
#if ENABLE(MEDIA_STREAM)
    RealtimeMediaSourceCenter::singleton().audioCaptureFactory().removeExtensiveObserver(*this);
#endif
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
        if (frameID && grantedRequest->frameID() != frameID)
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

static bool isMatchingDeniedRequest(const UserMediaPermissionRequestProxy& request, const UserMediaPermissionRequestManagerProxy::DeniedRequest& deniedRequest)
{
    return deniedRequest.mainFrameID == request.mainFrameID()
        && deniedRequest.userMediaDocumentOrigin->isSameSchemeHostPort(request.userMediaDocumentSecurityOrigin())
        && deniedRequest.topLevelDocumentOrigin->isSameSchemeHostPort(request.topLevelDocumentSecurityOrigin());
}

bool UserMediaPermissionRequestManagerProxy::wasRequestDenied(const UserMediaPermissionRequestProxy& request, bool needsAudio, bool needsVideo, bool needsScreenCapture)
{
    for (const auto& deniedRequest : m_deniedRequests) {
        if (!isMatchingDeniedRequest(request, deniedRequest))
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

void UserMediaPermissionRequestManagerProxy::updateStoredRequests(UserMediaPermissionRequestProxy& request)
{
    if (request.requestType() == MediaStreamRequest::Type::UserMedia) {
        m_grantedRequests.append(request);
        m_grantedFrames.add(request.frameID());
    }

    m_deniedRequests.removeAllMatching([&request](auto& deniedRequest) {
        if (!isMatchingDeniedRequest(request, deniedRequest))
            return false;

        if (deniedRequest.isAudioDenied && request.requiresAudioCapture())
            return true;
        if (deniedRequest.isVideoDenied && request.requiresVideoCapture())
            return true;
        if (deniedRequest.isScreenCaptureDenied && request.requiresDisplayCapture())
            return true;

        return false;
    });
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
        m_rejectionTimer.startOneShot(Seconds(mimimumDelayBeforeReplying + cryptographicallyRandomUnitInterval()));
}

#if ENABLE(MEDIA_STREAM)
UserMediaPermissionRequestManagerProxy::RequestAction UserMediaPermissionRequestManagerProxy::getRequestAction(const UserMediaPermissionRequestProxy& request)
{
    bool requestingScreenCapture = request.requestType() == MediaStreamRequest::Type::DisplayMedia || request.requestType() == MediaStreamRequest::Type::DisplayMediaWithAudio;
    bool requestingCamera = !requestingScreenCapture && request.hasVideoDevice();
    bool requestingMicrophone = request.hasAudioDevice();

    if (!request.isUserGesturePriviledged() && wasRequestDenied(request, requestingMicrophone, requestingCamera, requestingScreenCapture))
        return RequestAction::Deny;

    if (requestingScreenCapture)
        return RequestAction::Prompt;

    return searchForGrantedRequest(request.frameID(), request.userMediaDocumentSecurityOrigin(), request.topLevelDocumentSecurityOrigin(), requestingMicrophone, requestingCamera) ? RequestAction::Grant : RequestAction::Prompt;
}
#endif

void UserMediaPermissionRequestManagerProxy::requestUserMediaPermissionForFrame(UserMediaRequestIdentifier userMediaID, FrameIdentifier frameID, Ref<SecurityOrigin>&& userMediaDocumentOrigin, Ref<SecurityOrigin>&& topLevelDocumentOrigin, MediaStreamRequest&& userRequest)
{
#if ENABLE(MEDIA_STREAM)
    if (!m_page.hasRunningProcess())
        return;

    ALWAYS_LOG(LOGIDENTIFIER, userMediaID.toUInt64());

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
    if (request->userRequest().audioConstraints.isValid)
        RealtimeMediaSourceCenter::singleton().audioCaptureFactory().addExtensiveObserver(*this);

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

String UserMediaPermissionRequestManagerProxy::ephemeralDeviceHashSaltForFrame(WebCore::FrameIdentifier frameIdentifier)
{
    auto iter = m_frameEphemeralHashSalts.find(frameIdentifier);
    if (iter != m_frameEphemeralHashSalts.end())
        return iter->value;

    static constexpr unsigned hashSaltSize { 48 };
    static constexpr unsigned randomDataSize { hashSaltSize / 16 };

    uint64_t randomData[randomDataSize];
    cryptographicallyRandomValues(reinterpret_cast<unsigned char*>(randomData), sizeof(randomData));

    StringBuilder builder;
    builder.reserveCapacity(hashSaltSize);
    for (unsigned i = 0; i < randomDataSize; i++)
        builder.append(hex(randomData[i]));

    auto hashSaltForFrame = builder.toString();
    auto firstAddResult = m_frameEphemeralHashSalts.add(frameIdentifier, hashSaltForFrame);
    RELEASE_ASSERT(firstAddResult.isNewEntry);

    return hashSaltForFrame;
}

void UserMediaPermissionRequestManagerProxy::processUserMediaPermissionRequest()
{
    ALWAYS_LOG(LOGIDENTIFIER, m_currentUserMediaRequest->userMediaID().toUInt64(), ", persistent access: ", m_currentUserMediaRequest->hasPersistentAccess());

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

        WebCore::MediaDeviceHashSalts deviceHashSaltsForFrame = { deviceIDHashSalt, ephemeralDeviceHashSaltForFrame(request->frameID()) };

        auto validHandler = [this, request, deviceHashSaltsForFrame = deviceHashSaltsForFrame](Vector<CaptureDevice>&& audioDevices, Vector<CaptureDevice>&& videoDevices) mutable {
            if (!request->isPending())
                return;

            if (!m_page.hasRunningProcess() || !m_page.mainFrame())
                return;

            processUserMediaPermissionValidRequest(WTFMove(audioDevices), WTFMove(videoDevices), WTFMove(deviceHashSaltsForFrame));
        };

        syncWithWebCorePrefs();

        auto& realtimeMediaSourceCenter = RealtimeMediaSourceCenter::singleton();
        if (realtimeMediaSourceCenter.displayCaptureFactory().displayCaptureDeviceManager().requiresCaptureDevicesEnumeration() || !request->requiresDisplayCapture())
            platformValidateUserMediaRequestConstraints(WTFMove(validHandler), WTFMove(invalidHandler), WTFMove(deviceHashSaltsForFrame));
        else
            validHandler({ }, { });
    });
}

#if !USE(GLIB)
void UserMediaPermissionRequestManagerProxy::platformValidateUserMediaRequestConstraints(WebCore::RealtimeMediaSourceCenter::ValidConstraintsHandler&& validHandler, RealtimeMediaSourceCenter::InvalidConstraintsHandler&& invalidHandler, MediaDeviceHashSalts&& deviceIDHashSalts)
{
    RealtimeMediaSourceCenter::singleton().validateRequestConstraints(WTFMove(validHandler), WTFMove(invalidHandler), m_currentUserMediaRequest->userRequest(), WTFMove(deviceIDHashSalts));
}
#endif

void UserMediaPermissionRequestManagerProxy::processUserMediaPermissionInvalidRequest(const String& invalidConstraint)
{
    ALWAYS_LOG(LOGIDENTIFIER, m_currentUserMediaRequest->userMediaID().toUInt64());
    bool filterConstraint = !m_currentUserMediaRequest->hasPersistentAccess() && !wasGrantedVideoOrAudioAccess(m_currentUserMediaRequest->frameID());

    denyRequest(*m_currentUserMediaRequest, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::InvalidConstraint, filterConstraint ? String { } : invalidConstraint);
}

void UserMediaPermissionRequestManagerProxy::processUserMediaPermissionValidRequest(Vector<CaptureDevice>&& audioDevices, Vector<CaptureDevice>&& videoDevices, WebCore::MediaDeviceHashSalts&& deviceIdentifierHashSalts)
{
    ALWAYS_LOG(LOGIDENTIFIER, m_currentUserMediaRequest->userMediaID().toUInt64(), ", video: ", videoDevices.size(), " audio: ", audioDevices.size());
    if (!m_currentUserMediaRequest->requiresDisplayCapture() && videoDevices.isEmpty() && audioDevices.isEmpty()) {
        denyRequest(*m_currentUserMediaRequest, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::NoConstraints, emptyString());
        return;
    }

    m_currentUserMediaRequest->setDeviceIdentifierHashSalts(WTFMove(deviceIdentifierHashSalts));
    m_currentUserMediaRequest->setEligibleVideoDeviceUIDs(WTFMove(videoDevices));
    m_currentUserMediaRequest->setEligibleAudioDeviceUIDs(WTFMove(audioDevices));

    auto action = getRequestAction(*m_currentUserMediaRequest);
    ALWAYS_LOG(LOGIDENTIFIER, m_currentUserMediaRequest->userMediaID().toUInt64(), ", action: ", action);

    if (action == RequestAction::Deny) {
        denyRequest(*m_currentUserMediaRequest, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::PermissionDenied, emptyString());
        return;
    }

    if (action == RequestAction::Grant) {
        ASSERT(!m_currentUserMediaRequest->requiresDisplayCapture());

        if (m_page.isViewVisible())
            grantRequest(*m_currentUserMediaRequest);
        else
            m_pregrantedRequests.append(m_currentUserMediaRequest.releaseNonNull());

        return;
    }

    if (m_page.preferences().mockCaptureDevicesEnabled() && m_currentUserMediaRequest->requiresDisplayCapture() && !m_currentUserMediaRequest->hasVideoDevice()) {
        auto displayDevices = WebCore::RealtimeMediaSourceCenter::singleton().displayCaptureFactory().displayCaptureDeviceManager().captureDevices();
        m_currentUserMediaRequest->setEligibleVideoDeviceUIDs(WTFMove(displayDevices));
    }

    if (m_page.isControlledByAutomation()) {
        if (WebAutomationSession* automationSession = m_page.process().processPool().automationSession()) {
            ALWAYS_LOG(LOGIDENTIFIER, m_currentUserMediaRequest->userMediaID().toUInt64(), ", page controlled by automation");
            if (automationSession->shouldAllowGetUserMediaForPage(m_page))
                grantRequest(*m_currentUserMediaRequest);
            else
                denyRequest(*m_currentUserMediaRequest, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::PermissionDenied);
            return;
        }
    }

    if (m_page.preferences().mockCaptureDevicesEnabled() && !m_page.preferences().mockCaptureDevicesPromptEnabled()) {
        ALWAYS_LOG(LOGIDENTIFIER, m_currentUserMediaRequest->userMediaID().toUInt64(), ", mock devices don't require prompt");
        grantRequest(*m_currentUserMediaRequest);
        return;
    }

    requestSystemValidation(m_page, *m_currentUserMediaRequest, [weakThis = WeakPtr { *this }](bool isOK) {
        if (!weakThis)
            return;

        if (!isOK) {
            weakThis->denyRequest(*weakThis->m_currentUserMediaRequest, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::PermissionDenied);
            return;
        }
        weakThis->decidePolicyForUserMediaPermissionRequest();
    });
}

void UserMediaPermissionRequestManagerProxy::decidePolicyForUserMediaPermissionRequest()
{
    if (!m_currentUserMediaRequest)
        return;

    // If page navigated, there is no need to call the page client for authorization.
    auto* webFrame = WebFrameProxy::webFrame(m_currentUserMediaRequest->frameID());

    if (!webFrame || !protocolHostAndPortAreEqual(URL(m_page.pageLoadState().activeURL()), m_currentUserMediaRequest->topLevelDocumentSecurityOrigin().data().toURL())) {
        denyRequest(*m_currentUserMediaRequest, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::NoConstraints, emptyString());
        return;
    }

    // FIXME: Remove webFrame, userMediaOrigin and topLevelOrigin from this uiClient API call.
    auto userMediaOrigin = API::SecurityOrigin::create(m_currentUserMediaRequest->userMediaDocumentSecurityOrigin());
    auto topLevelOrigin = API::SecurityOrigin::create(m_currentUserMediaRequest->topLevelDocumentSecurityOrigin());
    m_page.uiClient().decidePolicyForUserMediaPermissionRequest(m_page, *webFrame, WTFMove(userMediaOrigin), WTFMove(topLevelOrigin), *m_currentUserMediaRequest);
}

void UserMediaPermissionRequestManagerProxy::checkUserMediaPermissionForSpeechRecognition(WebCore::FrameIdentifier frameIdentifier, const WebCore::SecurityOrigin& requestingOrigin, const WebCore::SecurityOrigin& topOrigin, const WebCore::CaptureDevice& device, CompletionHandler<void(bool)>&& completionHandler)
{
    auto* frame = WebFrameProxy::webFrame(frameIdentifier);
    if (!frame || !protocolHostAndPortAreEqual(URL(m_page.pageLoadState().activeURL()), topOrigin.data().toURL())) {
        completionHandler(false);
        return;
    }

    // We use UserMediaRequestIdentifierType of 0 because this does not correspond to a UserMediaPermissionRequest in web process.
    // We create the RequestProxy only to check the media permission for speech.
    auto request = UserMediaPermissionRequestProxy::create(*this, ObjectIdentifier<WebCore::UserMediaRequestIdentifierType>(0), frameIdentifier, frameIdentifier, requestingOrigin.isolatedCopy(), topOrigin.isolatedCopy(), Vector<WebCore::CaptureDevice> { device }, { }, { }, WTFMove(completionHandler));

    // FIXME: Use switch on action.
    auto action = getRequestAction(request.get());
    if (action == RequestAction::Deny) {
        request->decisionCompletionHandler()(false);
        return;
    }
    
    if (action == RequestAction::Grant) {
        request->decisionCompletionHandler()(true);
        return;
    }

    auto apiRequestingOrigin = API::SecurityOrigin::create(requestingOrigin);
    auto apiTopOrigin = API::SecurityOrigin::create(topOrigin);
    m_page.uiClient().decidePolicyForUserMediaPermissionRequest(m_page, *frame, WTFMove(apiRequestingOrigin), WTFMove(apiTopOrigin), request.get());
}

bool UserMediaPermissionRequestManagerProxy::shouldChangeDeniedToPromptForCamera(const ClientOrigin& origin) const
{
    if (!protocolHostAndPortAreEqual(URL(m_page.pageLoadState().activeURL()), origin.topOrigin.toURL()))
        return true;

    return !anyOf(m_deniedRequests, [](auto& request) { return request.isVideoDenied; })
        && !anyOf(m_pregrantedRequests, [](auto& request) { return request->requiresVideoCapture(); })
        && !anyOf(m_grantedRequests, [](auto& request) { return request->requiresVideoCapture(); });
}

bool UserMediaPermissionRequestManagerProxy::shouldChangeDeniedToPromptForMicrophone(const ClientOrigin& origin) const
{
    if (!protocolHostAndPortAreEqual(URL(m_page.pageLoadState().activeURL()), origin.topOrigin.toURL()))
        return true;

    return !anyOf(m_deniedRequests, [](auto& request) { return request.isAudioDenied; })
        && !anyOf(m_pregrantedRequests, [](auto& request) { return request->requiresAudioCapture(); })
        && !anyOf(m_grantedRequests, [](auto& request) { return request->requiresAudioCapture(); });
}

bool UserMediaPermissionRequestManagerProxy::shouldChangePromptToGrantForCamera(const ClientOrigin& origin) const
{
    return searchForGrantedRequest({ }, origin.clientOrigin.securityOrigin().get(), origin.topOrigin.securityOrigin().get(), false, true);
}

bool UserMediaPermissionRequestManagerProxy::shouldChangePromptToGrantForMicrophone(const ClientOrigin& origin) const
{
    return searchForGrantedRequest({ }, origin.clientOrigin.securityOrigin().get(), origin.topOrigin.securityOrigin().get(), true, false);
}

void UserMediaPermissionRequestManagerProxy::clearUserMediaPermissionRequestHistory(WebCore::PermissionName permissionName)
{
    m_deniedRequests.removeAllMatching([permissionName](auto& request) {
        return (request.isAudioDenied && permissionName == WebCore::PermissionName::Microphone) || (request.isVideoDenied && permissionName == WebCore::PermissionName::Camera);
    });
    m_grantedRequests.removeAllMatching([permissionName](auto& request) {
        return (request->requiresAudioCapture() && permissionName == WebCore::PermissionName::Microphone) || (request->requiresVideoCapture() && permissionName == WebCore::PermissionName::Camera);
    });
}

#if !PLATFORM(COCOA)
void UserMediaPermissionRequestManagerProxy::requestSystemValidation(const WebPageProxy&, UserMediaPermissionRequestProxy&, CompletionHandler<void(bool)>&& callback)
{
    callback(true);
}
#endif

void UserMediaPermissionRequestManagerProxy::getUserMediaPermissionInfo(FrameIdentifier frameID, Ref<SecurityOrigin>&& userMediaDocumentOrigin, Ref<SecurityOrigin>&& topLevelDocumentOrigin, CompletionHandler<void(PermissionInfo)>&& handler)
{
    auto* webFrame = WebFrameProxy::webFrame(frameID);
    if (!webFrame || !protocolHostAndPortAreEqual(URL(m_page.pageLoadState().activeURL()), topLevelDocumentOrigin->data().toURL())) {
        handler({ });
        return;
    }

    auto userMediaOrigin = API::SecurityOrigin::create(userMediaDocumentOrigin.get());
    auto topLevelOrigin = API::SecurityOrigin::create(topLevelDocumentOrigin.get());

    auto requestID = MediaDevicePermissionRequestIdentifier::generate();
    m_pendingDeviceRequests.add(requestID);

    auto request = UserMediaPermissionCheckProxy::create(frameID, [this, weakThis = WeakPtr { *this }, requestID, handler = WTFMove(handler)](auto permissionInfo) mutable {
        if (!weakThis || !m_pendingDeviceRequests.remove(requestID))
            permissionInfo = PermissionInfo::Error;
        handler(permissionInfo);
    }, WTFMove(userMediaDocumentOrigin), WTFMove(topLevelDocumentOrigin));

    // FIXME: Remove webFrame, userMediaOrigin and topLevelOrigin from this uiClient API call.
    m_page.uiClient().checkUserMediaPermissionForOrigin(m_page, *webFrame, userMediaOrigin.get(), topLevelOrigin.get(), request.get());
}

bool UserMediaPermissionRequestManagerProxy::wasGrantedVideoOrAudioAccess(FrameIdentifier frameID)
{
    return m_grantedFrames.contains(frameID);
}

static inline bool haveMicrophoneDevice(const Vector<CaptureDeviceWithCapabilities>& devices, const String& deviceID)
{
    return std::any_of(devices.begin(), devices.end(), [&deviceID](auto& deviceWithCapabilities) {
        auto& device = deviceWithCapabilities.device;
        return device.persistentId() == deviceID && device.type() == CaptureDevice::DeviceType::Microphone;
    });
}

#if !USE(GLIB)
void UserMediaPermissionRequestManagerProxy::platformGetMediaStreamDevices(bool revealIdsAndLabels, CompletionHandler<void(Vector<CaptureDeviceWithCapabilities>&&)>&& completionHandler)
{
    RealtimeMediaSourceCenter::singleton().getMediaStreamDevices([revealIdsAndLabels, completionHandler = WTFMove(completionHandler)](auto&& devices) mutable {
        Vector<CaptureDeviceWithCapabilities> devicesWithCapabilities;

        devicesWithCapabilities.reserveInitialCapacity(devices.size());
        for (auto& device : devices) {
            RealtimeMediaSourceCapabilities deviceCapabilities;

            if (revealIdsAndLabels && device.isInputDevice()) {
                auto capabilities = RealtimeMediaSourceCenter::singleton().getCapabilities(device);
                if (!capabilities)
                    continue;

                if (revealIdsAndLabels)
                    deviceCapabilities = WTFMove(*capabilities);
            }

            devicesWithCapabilities.uncheckedAppend({ WTFMove(device), WTFMove(deviceCapabilities) });
        }

        completionHandler(WTFMove(devicesWithCapabilities));
    });
}
#endif

void UserMediaPermissionRequestManagerProxy::computeFilteredDeviceList(bool revealIdsAndLabels, CompletionHandler<void(Vector<CaptureDeviceWithCapabilities>&&)>&& completion)
{
    static const unsigned defaultMaximumCameraCount = 1;
    static const unsigned defaultMaximumMicrophoneCount = 1;

    platformGetMediaStreamDevices(revealIdsAndLabels, [logIdentifier = LOGIDENTIFIER, this, weakThis = WeakPtr { *this }, revealIdsAndLabels, completion = WTFMove(completion)](auto&& devicesWithCapabilities) mutable {

        if (!weakThis) {
            completion({ });
            return;
        }

        unsigned cameraCount = 0;
        unsigned microphoneCount = 0;

        bool hasCamera = false;
        bool hasMicrophone = false;

        Vector<CaptureDeviceWithCapabilities> filteredDevices;
        for (auto& deviceWithCapabilities : devicesWithCapabilities) {
            auto& device = deviceWithCapabilities.device;
            if (!device.enabled() || (device.type() != WebCore::CaptureDevice::DeviceType::Camera && device.type() != WebCore::CaptureDevice::DeviceType::Microphone && device.type() != WebCore::CaptureDevice::DeviceType::Speaker))
                continue;
            hasCamera |= device.type() == WebCore::CaptureDevice::DeviceType::Camera;
            hasMicrophone |= device.type() == WebCore::CaptureDevice::DeviceType::Microphone;
            if (!revealIdsAndLabels) {
                if (device.type() == WebCore::CaptureDevice::DeviceType::Camera && ++cameraCount > defaultMaximumCameraCount)
                    continue;
                if (device.type() == WebCore::CaptureDevice::DeviceType::Microphone && ++microphoneCount > defaultMaximumMicrophoneCount)
                    continue;
                if (device.type() != WebCore::CaptureDevice::DeviceType::Camera && device.type() != WebCore::CaptureDevice::DeviceType::Microphone)
                    continue;
            } else {
                // We only expose speakers tied to a microphone for the moment.
                if (device.type() == WebCore::CaptureDevice::DeviceType::Speaker && !haveMicrophoneDevice(devicesWithCapabilities, device.groupId()))
                    continue;
            }

            if (revealIdsAndLabels)
                filteredDevices.append(deviceWithCapabilities);
            else
                filteredDevices.append({ { { }, device.type(), { }, { } }, { } });
        }

        m_hasFilteredDeviceList = !revealIdsAndLabels;
        ALWAYS_LOG(logIdentifier, filteredDevices.size(), " devices revealed, has filtering = ", !revealIdsAndLabels, " has camera = ", hasCamera, ", has microphone = ", hasMicrophone, " ");

        completion(WTFMove(filteredDevices));
    });
}
#endif

void UserMediaPermissionRequestManagerProxy::enumerateMediaDevicesForFrame(FrameIdentifier frameID, Ref<SecurityOrigin>&& userMediaDocumentOrigin, Ref<SecurityOrigin>&& topLevelDocumentOrigin, CompletionHandler<void(const Vector<CaptureDeviceWithCapabilities>&, MediaDeviceHashSalts&&)>&& completionHandler)
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

        auto requestID = MediaDevicePermissionRequestIdentifier::generate();
        m_pendingDeviceRequests.add(requestID);

        auto& requestOrigin = userMediaDocumentOrigin.get();
        auto& topOrigin = topLevelDocumentOrigin.get();

        callCompletionHandler.release();
        m_page.websiteDataStore().deviceIdHashSaltStorage().deviceIdHashSaltForOrigin(requestOrigin, topOrigin, [this, weakThis = WeakPtr { *this }, requestID, frameID, userMediaDocumentOrigin = WTFMove(userMediaDocumentOrigin), topLevelDocumentOrigin = WTFMove(topLevelDocumentOrigin), originHasPersistentAccess, completionHandler = WTFMove(completionHandler)](String&& deviceIDHashSalt) mutable {
            auto callCompletionHandler = makeScopeExit([&completionHandler] {
                completionHandler({ }, { });
            });

            if (!weakThis || !m_pendingDeviceRequests.remove(requestID))
                return;

            if (!m_page.hasRunningProcess())
                return;

            syncWithWebCorePrefs();

            MediaDeviceHashSalts hashSaltsForRequest = { deviceIDHashSalt, ephemeralDeviceHashSaltForFrame(frameID) };
            bool revealIdsAndLabels = originHasPersistentAccess || wasGrantedVideoOrAudioAccess(frameID);

            callCompletionHandler.release();
            computeFilteredDeviceList(revealIdsAndLabels, [completionHandler = WTFMove(completionHandler), hashSaltsForRequest = WTFMove(hashSaltsForRequest)] (auto&& devices) mutable {
                completionHandler(devices, WTFMove(hashSaltsForRequest));
            });
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

void UserMediaPermissionRequestManagerProxy::setMockCaptureDevicesEnabledOverride(std::optional<bool> enabled)
{
    m_mockDevicesEnabledOverride = enabled;
    syncWithWebCorePrefs();
}

bool UserMediaPermissionRequestManagerProxy::mockCaptureDevicesEnabled() const
{
    return m_mockDevicesEnabledOverride ? *m_mockDevicesEnabledOverride : m_page.preferences().mockCaptureDevicesEnabled();
}

bool UserMediaPermissionRequestManagerProxy::canAudioCaptureSucceed() const
{
    return mockCaptureDevicesEnabled() || permittedToCaptureAudio();
}

bool UserMediaPermissionRequestManagerProxy::canVideoCaptureSucceed() const
{
    return mockCaptureDevicesEnabled() || permittedToCaptureVideo();
}

void UserMediaPermissionRequestManagerProxy::syncWithWebCorePrefs() const
{
#if ENABLE(MEDIA_STREAM)
    // Enable/disable the mock capture devices for the UI process as per the WebCore preferences. Note that
    // this is a noop if the preference hasn't changed since the last time this was called.
    bool mockDevicesEnabled = m_mockDevicesEnabledOverride ? *m_mockDevicesEnabledOverride : m_page.preferences().mockCaptureDevicesEnabled();

#if ENABLE(GPU_PROCESS)
    if (m_page.preferences().captureAudioInGPUProcessEnabled() || m_page.preferences().captureVideoInGPUProcessEnabled())
        m_page.process().processPool().ensureGPUProcess().setUseMockCaptureDevices(mockDevicesEnabled);
#endif

#if HAVE(SC_CONTENT_SHARING_PICKER)
    auto useSharingPicker = m_page.preferences().useSCContentSharingPicker();

#if ENABLE(GPU_PROCESS)
    if (useSharingPicker)
        m_page.process().processPool().ensureGPUProcess().setUseSCContentSharingPicker(useSharingPicker);
#endif

    PlatformMediaSessionManager::setUseSCContentSharingPicker(useSharingPicker);
#endif

    if (MockRealtimeMediaSourceCenter::mockRealtimeMediaSourceCenterEnabled() == mockDevicesEnabled)
        return;
    MockRealtimeMediaSourceCenter::setMockRealtimeMediaSourceCenterEnabled(mockDevicesEnabled);
#endif
}

void UserMediaPermissionRequestManagerProxy::captureStateChanged(MediaProducerMediaStateFlags oldState, MediaProducerMediaStateFlags newState)
{
    if (!m_page.hasRunningProcess())
        return;

#if ENABLE(MEDIA_STREAM)
    if (!m_hasPendingCapture)
        UserMediaProcessManager::singleton().revokeSandboxExtensionsIfNeeded(page().process());

    if (m_captureState == (newState & activeCaptureMask))
        return;

    ALWAYS_LOG(LOGIDENTIFIER, "state was: ", m_captureState.toRaw(), ", is now: ", (newState & activeCaptureMask).toRaw());
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
    return JOIN_LOG_CHANNEL_WITH_PREFIX(LOG_CHANNEL_PREFIX, WebRTC);
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
    ASSERT(static_cast<size_t>(enumerationValue) < std::size(values));
    return values[static_cast<size_t>(enumerationValue)];
}

} // namespace WebKit
