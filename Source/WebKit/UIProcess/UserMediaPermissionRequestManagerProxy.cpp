/*
 * Copyright (C) 2014 Igalia S.L.
 * Copyright (C) 2016-2025 Apple Inc. All rights reserved.
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

#include "APIPageConfiguration.h"
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
#include <WebCore/ContextDestructionObserverInlines.h>
#include <WebCore/MediaConstraintType.h>
#include <WebCore/MediaConstraints.h>
#include <WebCore/MockRealtimeMediaSourceCenter.h>
#include <WebCore/PermissionName.h>
#include <WebCore/PlatformMediaSessionManager.h>
#include <WebCore/RealtimeMediaSource.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/UserMediaRequest.h>
#include <algorithm>
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/LoggerHelper.h>
#include <wtf/Scope.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/WeakHashSet.h>

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
static WeakHashSet<UserMediaPermissionRequestManagerProxy>& proxies()
{
    static NeverDestroyed<WeakHashSet<UserMediaPermissionRequestManagerProxy>> set;
    return set;
}

void UserMediaPermissionRequestManagerProxy::forEach(NOESCAPE const WTF::Function<void(UserMediaPermissionRequestManagerProxy&)>& function)
{
    for (Ref proxy : proxies())
        function(proxy);
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

WTF_MAKE_TZONE_ALLOCATED_IMPL(UserMediaPermissionRequestManagerProxy);

Ref<UserMediaPermissionRequestManagerProxy> UserMediaPermissionRequestManagerProxy::create(WebPageProxy& page)
{
    return adoptRef(*new UserMediaPermissionRequestManagerProxy(page));
}

UserMediaPermissionRequestManagerProxy::UserMediaPermissionRequestManagerProxy(WebPageProxy& page)
    : m_page(page)
    , m_rejectionTimer(RunLoop::mainSingleton(), "UserMediaPermissionRequestManagerProxy::RejectionTimer"_s, this, &UserMediaPermissionRequestManagerProxy::rejectionTimerFired)
    , m_watchdogTimer(RunLoop::mainSingleton(), "UserMediaPermissionRequestManagerProxy::WatchdogTimer"_s, this, &UserMediaPermissionRequestManagerProxy::watchdogTimerFired)
#if !RELEASE_LOG_DISABLED
    , m_logger(page.logger())
    , m_logIdentifier(uniqueLogIdentifier())
#endif
{
#if ENABLE(MEDIA_STREAM)
    proxies().add(*this);
#endif
    syncWithWebCorePrefs();
}

UserMediaPermissionRequestManagerProxy::~UserMediaPermissionRequestManagerProxy()
{
    if (m_page)
        disconnectFromPage();

#if ENABLE(MEDIA_STREAM)
    proxies().remove(*this);
#endif
}

#if ENABLE(MEDIA_STREAM)
static void revokeSandboxExtensionsIfNeededForPage(WebPageProxy& page)
{
    page.forEachWebContentProcess([](auto& process, auto&&) {
        UserMediaProcessManager::singleton().revokeSandboxExtensionsIfNeeded(process);
    });
}
#endif

// FIXME: what happens when a process disconnects and gets put in to WebProcessCache?
void UserMediaPermissionRequestManagerProxy::disconnectFromPage()
{
    stopCapture();
#if ENABLE(MEDIA_STREAM)
    if (RefPtr page = m_page.get())
        revokeSandboxExtensionsIfNeededForPage(*page);
#endif
    m_page = nullptr;
}

WebPageProxy* UserMediaPermissionRequestManagerProxy::page() const
{
    return m_page.get();
}

void UserMediaPermissionRequestManagerProxy::invalidatePendingRequests()
{
    if (RefPtr currentUserMediaRequest = std::exchange(m_currentUserMediaRequest, nullptr))
        currentUserMediaRequest->invalidate();

    auto pendingUserMediaRequests = WTF::move(m_pendingUserMediaRequests);
    for (auto& request : pendingUserMediaRequests)
        request->invalidate();

    auto pregrantedRequests = WTF::move(m_pregrantedRequests);
    for (auto& request : pregrantedRequests)
        request->invalidate();

    m_pendingDeviceRequests.clear();
}

void UserMediaPermissionRequestManagerProxy::stopCapture()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    invalidatePendingRequests();
    if (RefPtr page = m_page.get())
        page->stopMediaCapture(MediaProducerMediaCaptureKind::EveryKind);
}

void UserMediaPermissionRequestManagerProxy::captureDevicesChanged()
{
#if ENABLE(MEDIA_STREAM)
    ALWAYS_LOG(LOGIDENTIFIER);
    RefPtr page = m_page.get();
    if (!page || !page->hasRunningProcess() || !page->mainFrame())
        return;

    Ref origin = WebCore::SecurityOrigin::create(page->mainFrame()->url());
    getUserMediaPermissionInfo(page->mainFrame()->frameID(), origin.get(), WTF::move(origin), [weakThis = WeakPtr { *this }](auto cameraState, auto microphoneState) {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->captureDevicesChanged(cameraState == PermissionState::Granted, microphoneState == PermissionState::Granted);
    });
#endif
}

#if ENABLE(MEDIA_STREAM)
void UserMediaPermissionRequestManagerProxy::captureDevicesChanged(bool hasCameraPersistentAccess, bool hasMicrophonePersistentAccess)
{
    if (!hasCameraPersistentAccess && !hasMicrophonePersistentAccess && m_grantedRequests.isEmpty())
        return;

    RefPtr page = m_page.get();
    if (!page || !page->hasRunningProcess())
        return;

    page->forEachWebContentProcess([](auto& process, auto pageID) {
        if (!process.hasConnection())
            return;
        process.send(Messages::WebPage::CaptureDevicesChanged(), pageID);
    });
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

void UserMediaPermissionRequestManagerProxy::denyRequest(UserMediaPermissionRequestProxy& request, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason reason, const String& message)
{
    denyRequest(request, reason, message, WebCore::MediaConstraintType::Unknown);
}

void UserMediaPermissionRequestManagerProxy::denyRequest(UserMediaPermissionRequestProxy& request, WebCore::MediaConstraintType invalidConstraint)
{
    denyRequest(request, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::InvalidConstraint, { }, invalidConstraint);
}

void UserMediaPermissionRequestManagerProxy::denyRequest(UserMediaPermissionRequestProxy& request, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason reason, const String& message, WebCore::MediaConstraintType invalidConstraint)
{
    RefPtr page = m_page.get();
    if (!page || !page->hasRunningProcess())
        return;

    ALWAYS_LOG(LOGIDENTIFIER, request.userMediaID() ? request.userMediaID()->toUInt64() : 0, ", reason: ", reason);

    bool shouldCacheResult = true;
#if ENABLE(WEB_ARCHIVE)
    shouldCacheResult = !page->didLoadWebArchive();
#endif

    if (reason == UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::PermissionDenied && shouldCacheResult)
        m_deniedRequests.append(DeniedRequest { request.mainFrameID(), request.userMediaDocumentSecurityOrigin(), request.topLevelDocumentSecurityOrigin(), request.requiresAudioCapture(), request.requiresVideoCapture(), request.requiresDisplayCapture() });

    if (auto callback = request.decisionCompletionHandler()) {
        callback(false);
        return;
    }

#if ENABLE(MEDIA_STREAM)
    if (auto userMediaID = request.userMediaID())
        page->sendToProcessContainingFrame(request.frameID(), Messages::WebPage::UserMediaAccessWasDenied(*userMediaID, toWebCore(reason), message, invalidConstraint));
#else
    UNUSED_PARAM(reason);
    UNUSED_PARAM(invalidConstraint);
#endif

    processNextUserMediaRequestIfNeeded();
}

void UserMediaPermissionRequestManagerProxy::grantRequest(UserMediaPermissionRequestProxy& request)
{
    RefPtr page = m_page.get();
    if (!page || !page->hasRunningProcess())
        return;

#if ENABLE(MEDIA_STREAM)
    ALWAYS_LOG(LOGIDENTIFIER, request.userMediaID() ? request.userMediaID()->toUInt64() : 0, ", video: ", request.videoDevice().label(), ", audio: ", request.audioDevice().label());

    if (auto callback = request.decisionCompletionHandler()) {
        page->willStartCapture(request, [callback = WTF::move(callback)]() mutable {
            callback(true);
        });
        bool shouldCacheResult = true;
#if ENABLE(WEB_ARCHIVE)
        shouldCacheResult = !page->didLoadWebArchive();
#endif
        if (shouldCacheResult)
            m_grantedRequests.append(request);
        if (request.requiresAudioCapture())
            m_grantedAudioFrames.add(request.frameID());
        if (request.requiresVideoCapture())
            m_grantedVideoFrames.add(request.frameID());
        return;
    }

    Ref userMediaDocumentSecurityOrigin = request.userMediaDocumentSecurityOrigin();
    Ref topLevelDocumentSecurityOrigin = request.topLevelDocumentSecurityOrigin();
    protect(page->websiteDataStore())->ensureProtectedDeviceIdHashSaltStorage()->deviceIdHashSaltForOrigin(userMediaDocumentSecurityOrigin, topLevelDocumentSecurityOrigin, [weakThis = WeakPtr { *this }, request = protect(request)](String&&) mutable {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->finishGrantingRequest(request);
    });
#else
    UNUSED_PARAM(request);
#endif
}

#if ENABLE(MEDIA_STREAM)

#if PLATFORM(COCOA)
static bool doesPageNeedTCCD(const WebPageProxy& page)
{
    Ref preferences = page.preferences();
    return !preferences->captureAudioInGPUProcessEnabled() || !preferences->captureVideoInGPUProcessEnabled();
}
#endif

void UserMediaPermissionRequestManagerProxy::finishGrantingRequest(UserMediaPermissionRequestProxy& request)
{
    ALWAYS_LOG(LOGIDENTIFIER, request.userMediaID() ? request.userMediaID()->toUInt64() : 0);
    updateStoredRequests(request);

    if (!UserMediaProcessManager::singleton().willCreateMediaStream(*this, request)) {
        denyRequest(request, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::OtherFailure, "Unable to extend sandbox."_s);
        return;
    }

    RefPtr page = m_page.get();
    if (!page) {
        denyRequest(request, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::OtherFailure, "No page."_s);
        return;
    }

    page->willStartCapture(request, [weakThis = WeakPtr { *this }, request = protect(request)]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        RefPtr page = protectedThis->m_page.get();
        if (!page)
            return;

        RefPtr frame = WebFrameProxy::webFrame(request->frameID());
        if (!frame)
            return;
        Ref process = frame->process();

        ++protectedThis->m_hasPendingCapture;

        Vector<SandboxExtension::Handle> handles;
#if PLATFORM(COCOA)
        if (!protectedThis->m_hasCreatedSandboxExtensionForTCCD.contains(process->coreProcessIdentifier()) && doesPageNeedTCCD(*page)) {
            handles = SandboxExtension::createHandlesForMachLookup({ "com.apple.tccd"_s }, process->auditToken(), SandboxExtension::MachBootstrapOptions::EnableMachBootstrap);
            protectedThis->m_hasCreatedSandboxExtensionForTCCD.add(process->coreProcessIdentifier());
        }
#endif

        CompletionHandler<void()> completionHandler = [weakThis = WTF::move(weakThis)] {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;
            if (!--protectedThis->m_hasPendingCapture) {
                if (RefPtr page = protectedThis->m_page.get())
                    revokeSandboxExtensionsIfNeededForPage(*page);
            }
        };

#if PLATFORM(COCOA)
        if (!request->requiresDisplayCapture()) {
            // We revalidate devices as devices may have changed since before prompting the user.
            auto validDevices = RealtimeMediaSourceCenter::singleton().validateRequestConstraintsAfterEnumeration(request->userRequest(), request->deviceIdentifierHashSalts());
            if (!!validDevices) {
                request->setEligibleAudioDevices(WTF::move(validDevices->audioDevices));
                request->setEligibleVideoDevices(WTF::move(validDevices->videoDevices));
            }
        }
#endif
        page->sendWithAsyncReplyToProcessContainingFrame(request->frameID(), Messages::WebPage::UserMediaAccessWasGranted { *request->userMediaID(), request->audioDevice(), request->videoDevice(), request->deviceIdentifierHashSalts(), WTF::move(handles) }, WTF::move(completionHandler));

        protectedThis->processNextUserMediaRequestIfNeeded();
    });
}

void UserMediaPermissionRequestManagerProxy::didCommitLoadForFrame(FrameIdentifier frameID)
{
    ALWAYS_LOG(LOGIDENTIFIER, frameID.toUInt64());
    m_frameEphemeralHashSalts.remove(frameID);
}

void UserMediaPermissionRequestManagerProxy::resetAccess(WebFrameProxy* frame)
{
    ALWAYS_LOG(LOGIDENTIFIER, frame ? frame->frameID().toUInt64() : 0);

    if (RefPtr currentUserMediaRequest = m_currentUserMediaRequest; currentUserMediaRequest && (!frame || m_currentUserMediaRequest->frameID() == frame->frameID())) {
        // Avoid starting pending requests after denying current request.
        auto pendingUserMediaRequests = std::exchange(m_pendingUserMediaRequests, { });
        currentUserMediaRequest->deny(UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::OtherFailure);
        m_pendingUserMediaRequests = std::exchange(pendingUserMediaRequests, { });
    }

    if (frame) {
        m_grantedRequests.removeAllMatching([frame](const auto& grantedRequest) {
            return grantedRequest->mainFrameID() == frame->frameID() && !grantedRequest->userMediaDocumentSecurityOrigin().isSameOriginAs(SecurityOrigin::create(frame->url()).get());
        });
        m_grantedAudioFrames.remove(frame->frameID());
        m_grantedVideoFrames.remove(frame->frameID());
        m_frameEphemeralHashSalts.remove(frame->frameID());
    } else {
        m_grantedRequests.clear();
        m_grantedAudioFrames.clear();
        m_grantedVideoFrames.clear();
        m_frameEphemeralHashSalts.clear();
    }
    m_pregrantedRequests.clear();
    m_deniedRequests.clear();
}

Seconds UserMediaPermissionRequestManagerProxy::inactiveMediaCaptureStreamDuration() const
{
    return m_lastCaptureTime ? MonotonicTime::now() - *m_lastCaptureTime : 0_s;
}

bool UserMediaPermissionRequestManagerProxy::hasGrantedRequest(std::optional<FrameIdentifier> frameID, const SecurityOrigin& userMediaDocumentOrigin, const SecurityOrigin& topLevelDocumentOrigin, bool needsAudio, bool needsVideo, bool isUserGesturePriviledged) const
{
    RefPtr page = m_page.get();
    if (!page || page->isMediaStreamCaptureMuted() || m_grantedRequests.isEmpty())
        return false;

    if (!isUserGesturePriviledged && inactiveMediaCaptureStreamDuration().minutes() > protect(page->preferences())->inactiveMediaCaptureStreamRepromptWithoutUserGestureIntervalInMinutes())
        return false;

#if ENABLE(WEB_ARCHIVE)
    if (page->didLoadWebArchive())
        return false;
#endif

    bool checkForAudio = needsAudio;
    bool checkForVideo = needsVideo;
    for (Ref grantedRequest : m_grantedRequests) {
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

        return true;
    }
    return false;
}

static bool isMatchingDeniedRequest(const UserMediaPermissionRequestProxy& request, const UserMediaPermissionRequestManagerProxy::DeniedRequest& deniedRequest)
{
    return deniedRequest.mainFrameID == request.mainFrameID()
        && protect(deniedRequest.userMediaDocumentOrigin)->isSameSchemeHostPort(request.userMediaDocumentSecurityOrigin())
        && protect(deniedRequest.topLevelDocumentOrigin)->isSameSchemeHostPort(request.topLevelDocumentSecurityOrigin());
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
        if (request.requiresAudioCapture())
            m_grantedAudioFrames.add(request.frameID());
        if (request.requiresVideoCapture())
            m_grantedVideoFrames.add(request.frameID());
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
    denyRequest(m_pendingRejections.takeFirst(), UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::PermissionDenied);
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

    return hasGrantedRequest(request.frameID(), request.userMediaDocumentSecurityOrigin(), request.topLevelDocumentSecurityOrigin(), requestingMicrophone, requestingCamera, request.isUserGesturePriviledged()) ? RequestAction::Grant : RequestAction::Prompt;
}
#endif

void UserMediaPermissionRequestManagerProxy::requestUserMediaPermissionForFrame(UserMediaRequestIdentifier userMediaID, FrameInfoData&& frameInfo, Ref<SecurityOrigin>&& userMediaDocumentOrigin, Ref<SecurityOrigin>&& topLevelDocumentOrigin, MediaStreamRequest&& userRequest)
{
#if ENABLE(MEDIA_STREAM)
    RefPtr page = m_page.get();
    if (!page || !page->hasRunningProcess())
        return;

    ALWAYS_LOG(LOGIDENTIFIER, userMediaID.toUInt64());

    Ref request = UserMediaPermissionRequestProxy::create(*this, userMediaID, page->mainFrame()->frameID(), WTF::move(frameInfo), WTF::move(userMediaDocumentOrigin), WTF::move(topLevelDocumentOrigin), { }, { }, WTF::move(userRequest));
    if (m_currentUserMediaRequest) {
        if (m_currentUserMediaRequest->requiresDisplayCapture() && request->requiresDisplayCapture()) {
            ALWAYS_LOG(LOGIDENTIFIER, "Cancelling pending getDisplayMedia request");
            RefPtr currentUserMediaRequest = std::exchange(m_currentUserMediaRequest, nullptr);
            currentUserMediaRequest->deny(UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::OtherFailure);
        } else {
            m_pendingUserMediaRequests.append(WTF::move(request));
            return;
        }
    }

    if (!UserMediaProcessManager::singleton().captureEnabled()) {
        ALWAYS_LOG(LOGIDENTIFIER, "capture disabled");
        m_pendingRejections.append(WTF::move(request));
        scheduleNextRejection();
        return;
    }

    startProcessingUserMediaPermissionRequest(WTF::move(request));
#else
    UNUSED_PARAM(userMediaID);
    UNUSED_PARAM(frameInfo);
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
    m_currentUserMediaRequest = WTF::move(request);

    Ref userMediaDocumentSecurityOrigin = m_currentUserMediaRequest->userMediaDocumentSecurityOrigin();
    Ref topLevelDocumentSecurityOrigin = m_currentUserMediaRequest->topLevelDocumentSecurityOrigin();
    getUserMediaPermissionInfo(m_currentUserMediaRequest->frameID(), WTF::move(userMediaDocumentSecurityOrigin), WTF::move(topLevelDocumentSecurityOrigin), [weakThis = WeakPtr { *this }, request = m_currentUserMediaRequest](auto cameraState, auto microphoneState) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        if (!request->isPending())
            return;

        Ref currentUserMediaRequest = *protectedThis->m_currentUserMediaRequest;
        if (currentUserMediaRequest->userRequest().type == MediaStreamRequest::Type::UserMedia && (((cameraState == PermissionState::Granted) && currentUserMediaRequest->userRequest().videoConstraints.isValid) || ((microphoneState == PermissionState::Granted) && currentUserMediaRequest->userRequest().audioConstraints.isValid)))
            currentUserMediaRequest->setHasPersistentAccess();

        protectedThis->processUserMediaPermissionRequest();
    });
}

String UserMediaPermissionRequestManagerProxy::ephemeralDeviceHashSaltForFrame(WebCore::FrameIdentifier frameIdentifier)
{
    auto iter = m_frameEphemeralHashSalts.find(frameIdentifier);
    if (iter != m_frameEphemeralHashSalts.end())
        return iter->value;

    static constexpr unsigned hashSaltSize { 48 };
    static constexpr unsigned randomDataSize { hashSaltSize / 16 };

    std::array<uint64_t, randomDataSize> randomData;
    cryptographicallyRandomValues(asWritableBytes(std::span<uint64_t> { randomData }));

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
    ALWAYS_LOG(LOGIDENTIFIER, m_currentUserMediaRequest->userMediaID() ? m_currentUserMediaRequest->userMediaID()->toUInt64() : 0, ", persistent access: ", m_currentUserMediaRequest->hasPersistentAccess());

    RefPtr page = m_page.get();
    if (!page)
        return;

    Ref userMediaDocumentSecurityOrigin = m_currentUserMediaRequest->userMediaDocumentSecurityOrigin();
    Ref topLevelDocumentSecurityOrigin = m_currentUserMediaRequest->topLevelDocumentSecurityOrigin();
    protect(page->websiteDataStore())->ensureProtectedDeviceIdHashSaltStorage()->deviceIdHashSaltForOrigin(userMediaDocumentSecurityOrigin, topLevelDocumentSecurityOrigin, [weakThis = WeakPtr { *this }, request = m_currentUserMediaRequest] (String&& deviceIDHashSalt) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        if (!request->isPending())
            return;

        WebCore::MediaDeviceHashSalts deviceHashSaltsForFrame = { deviceIDHashSalt, protectedThis->ephemeralDeviceHashSaltForFrame(request->frameID()) };

        WebCore::RealtimeMediaSourceCenter::ValidateHandler validateHandler = [protectedThis, request, deviceHashSaltsForFrame = deviceHashSaltsForFrame](auto&& result) mutable {
            if (!request->isPending())
                return;

            RefPtr page = protectedThis->m_page.get();
            if (!page || !page->hasRunningProcess() || !page->mainFrame())
                return;

            if (!result) {
                protectedThis->processUserMediaPermissionInvalidRequest(result.error());
                return;
            }

            auto validDevices = WTF::move(result).value();
            protectedThis->processUserMediaPermissionValidRequest(WTF::move(validDevices.audioDevices), WTF::move(validDevices.videoDevices), WTF::move(deviceHashSaltsForFrame));
        };

        protectedThis->syncWithWebCorePrefs();

        auto& realtimeMediaSourceCenter = RealtimeMediaSourceCenter::singleton();
        if (realtimeMediaSourceCenter.displayCaptureFactory().displayCaptureDeviceManager().requiresCaptureDevicesEnumeration() || !request->requiresDisplayCapture())
            protectedThis->validateUserMediaRequestConstraints(WTF::move(validateHandler), WTF::move(deviceHashSaltsForFrame));
        else
            validateHandler(WebCore::RealtimeMediaSourceCenter::ValidDevices { { }, { } });
    });
}

#if !USE(GLIB)
void UserMediaPermissionRequestManagerProxy::validateUserMediaRequestConstraints(WebCore::RealtimeMediaSourceCenter::ValidateHandler&& validateHandler, MediaDeviceHashSalts&& deviceIDHashSalts)
{
    RealtimeMediaSourceCenter::singleton().validateRequestConstraints(WTF::move(validateHandler), m_currentUserMediaRequest->userRequest(), WTF::move(deviceIDHashSalts));
}
#endif

void UserMediaPermissionRequestManagerProxy::processUserMediaPermissionInvalidRequest(MediaConstraintType invalidConstraint)
{
    ALWAYS_LOG(LOGIDENTIFIER, m_currentUserMediaRequest->userMediaID() ? m_currentUserMediaRequest->userMediaID()->toUInt64() : 0);
    bool filterConstraint = !m_currentUserMediaRequest->hasPersistentAccess() && !wasGrantedVideoOrAudioAccess(m_currentUserMediaRequest->frameID());

    denyRequest(protect(*m_currentUserMediaRequest), filterConstraint ? MediaConstraintType::Unknown : invalidConstraint);
}

void UserMediaPermissionRequestManagerProxy::processUserMediaPermissionValidRequest(Vector<CaptureDevice>&& audioDevices, Vector<CaptureDevice>&& videoDevices, WebCore::MediaDeviceHashSalts&& deviceIdentifierHashSalts)
{
    RefPtr currentUserMediaRequest = m_currentUserMediaRequest;
    ALWAYS_LOG(LOGIDENTIFIER, currentUserMediaRequest->userMediaID() ? currentUserMediaRequest->userMediaID()->toUInt64() : 0, ", video: ", videoDevices.size(), " audio: ", audioDevices.size());
    if (!currentUserMediaRequest->requiresDisplayCapture() && videoDevices.isEmpty() && audioDevices.isEmpty()) {
        denyRequest(*currentUserMediaRequest, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::NoConstraints);
        return;
    }

    currentUserMediaRequest->setDeviceIdentifierHashSalts(WTF::move(deviceIdentifierHashSalts));
    currentUserMediaRequest->setEligibleVideoDevices(WTF::move(videoDevices));
    currentUserMediaRequest->setEligibleAudioDevices(WTF::move(audioDevices));

    auto action = getRequestAction(*currentUserMediaRequest);
    ALWAYS_LOG(LOGIDENTIFIER, currentUserMediaRequest->userMediaID() ? currentUserMediaRequest->userMediaID()->toUInt64() : 0, ", action: ", action);

    if (action == RequestAction::Deny) {
        denyRequest(*currentUserMediaRequest, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::PermissionDenied);
        return;
    }

    RefPtr page = m_page.get();
    if (!page) {
        denyRequest(*currentUserMediaRequest, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::PermissionDenied);
        return;
    }

    if (action == RequestAction::Grant) {
        ASSERT(!currentUserMediaRequest->requiresDisplayCapture());

        if (page->isViewVisible())
            grantRequest(*currentUserMediaRequest);
        else
            m_pregrantedRequests.append(currentUserMediaRequest.releaseNonNull());

        return;
    }

    Ref preferences = page->preferences();
    if (preferences->mockCaptureDevicesEnabled() && currentUserMediaRequest->requiresDisplayCapture() && !m_currentUserMediaRequest->hasVideoDevice()) {
        auto displayDevices = WebCore::RealtimeMediaSourceCenter::singleton().displayCaptureFactory().displayCaptureDeviceManager().captureDevices();
        currentUserMediaRequest->setEligibleVideoDevices(WTF::move(displayDevices));
    }

    if (page->isControlledByAutomation()) {
        if (RefPtr automationSession = page->configuration().processPool().automationSession()) {
            ALWAYS_LOG(LOGIDENTIFIER, currentUserMediaRequest->userMediaID() ? currentUserMediaRequest->userMediaID()->toUInt64() : 0, ", page controlled by automation");
            if (automationSession->shouldAllowGetUserMediaForPage(*page))
                grantRequest(*currentUserMediaRequest);
            else
                denyRequest(*currentUserMediaRequest, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::PermissionDenied);
            return;
        }
    }

    if (preferences->mockCaptureDevicesEnabled() && !preferences->mockCaptureDevicesPromptEnabled()) {
        ALWAYS_LOG(LOGIDENTIFIER, currentUserMediaRequest->userMediaID() ? currentUserMediaRequest->userMediaID()->toUInt64() : 0, ", mock devices don't require prompt");
        grantRequest(*currentUserMediaRequest);
        return;
    }

    requestSystemValidation(*page, *currentUserMediaRequest, [weakThis = WeakPtr { *this }](bool isOK) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        if (!isOK) {
            protectedThis->denyRequest(protect(*protectedThis->m_currentUserMediaRequest), UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::PermissionDenied);
            return;
        }
        protectedThis->decidePolicyForUserMediaPermissionRequest();
    });
}

void UserMediaPermissionRequestManagerProxy::decidePolicyForUserMediaPermissionRequest()
{
    RefPtr currentUserMediaRequest = m_currentUserMediaRequest;
    if (!currentUserMediaRequest)
        return;

    // If page navigated, there is no need to call the page client for authorization.
    RefPtr webFrame = WebFrameProxy::webFrame(currentUserMediaRequest->frameID());
    RefPtr page = m_page.get();
    if (!webFrame || !page || !protocolHostAndPortAreEqual(URL(protect(page->pageLoadState())->activeURL()), currentUserMediaRequest->topLevelDocumentSecurityOrigin().data().toURL())) {
        denyRequest(*currentUserMediaRequest, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::NoConstraints);
        return;
    }

    // FIXME: Remove webFrame, userMediaOrigin and topLevelOrigin from this uiClient API call.
    Ref userMediaOrigin = API::SecurityOrigin::create(currentUserMediaRequest->userMediaDocumentSecurityOrigin());
    Ref topLevelOrigin = API::SecurityOrigin::create(currentUserMediaRequest->topLevelDocumentSecurityOrigin());
    page->uiClient().decidePolicyForUserMediaPermissionRequest(*page, *webFrame, WTF::move(userMediaOrigin), WTF::move(topLevelOrigin), *currentUserMediaRequest);
}

void UserMediaPermissionRequestManagerProxy::checkUserMediaPermissionForSpeechRecognition(WebCore::FrameIdentifier mainFrameIdentifier, FrameInfoData&& frameInfo, const WebCore::SecurityOrigin& requestingOrigin, const WebCore::SecurityOrigin& topOrigin, const WebCore::CaptureDevice& device, CompletionHandler<void(bool)>&& completionHandler)
{
    RefPtr page = m_page.get();
    RefPtr frame = WebFrameProxy::webFrame(frameInfo.frameID);
    if (!page || !frame || !protocolHostAndPortAreEqual(URL(protect(page->pageLoadState())->activeURL()), topOrigin.data().toURL())) {
        completionHandler(false);
        return;
    }

    // We use no UserMediaRequestIdentifier because this does not correspond to a UserMediaPermissionRequest in web process.
    // We create the RequestProxy only to check the media permission for speech.
    Ref request = UserMediaPermissionRequestProxy::create(*this, std::nullopt, mainFrameIdentifier, WTF::move(frameInfo), requestingOrigin.isolatedCopy(), topOrigin.isolatedCopy(), Vector<WebCore::CaptureDevice> { device }, { }, { }, WTF::move(completionHandler));

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

    Ref apiRequestingOrigin = API::SecurityOrigin::create(requestingOrigin);
    Ref apiTopOrigin = API::SecurityOrigin::create(topOrigin);
    page->uiClient().decidePolicyForUserMediaPermissionRequest(*page, *frame, WTF::move(apiRequestingOrigin), WTF::move(apiTopOrigin), request.get());
}

bool UserMediaPermissionRequestManagerProxy::shouldChangeDeniedToPromptForCamera(const ClientOrigin& origin) const
{
    RefPtr page = m_page.get();
    if (!page)
        return false;

    if (!protocolHostAndPortAreEqual(URL(protect(page->pageLoadState())->activeURL()), origin.topOrigin.toURL()))
        return true;

    return std::ranges::none_of(m_deniedRequests, [](auto& request) { return request.isVideoDenied; })
        && std::ranges::none_of(m_pregrantedRequests, [](auto& request) { return request->requiresVideoCapture(); })
        && std::ranges::none_of(m_grantedRequests, [](auto& request) { return request->requiresVideoCapture(); });
}

bool UserMediaPermissionRequestManagerProxy::shouldChangeDeniedToPromptForMicrophone(const ClientOrigin& origin) const
{
    RefPtr page = m_page.get();
    if (!page)
        return false;

    if (!protocolHostAndPortAreEqual(URL(protect(page->pageLoadState())->activeURL()), origin.topOrigin.toURL()))
        return true;

    return std::ranges::none_of(m_deniedRequests, [](auto& request) { return request.isAudioDenied; })
        && std::ranges::none_of(m_pregrantedRequests, [](auto& request) { return request->requiresAudioCapture(); })
        && std::ranges::none_of(m_grantedRequests, [](auto& request) { return request->requiresAudioCapture(); });
}

bool UserMediaPermissionRequestManagerProxy::shouldChangePromptToGrantForCamera(const ClientOrigin& origin) const
{
    return hasGrantedRequest(std::nullopt, origin.clientOrigin.securityOrigin().get(), origin.topOrigin.securityOrigin().get(), false, true, true);
}

bool UserMediaPermissionRequestManagerProxy::shouldChangePromptToGrantForMicrophone(const ClientOrigin& origin) const
{
    return hasGrantedRequest(std::nullopt, origin.clientOrigin.securityOrigin().get(), origin.topOrigin.securityOrigin().get(), true, false, true);
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

class UserMediaPermissionInfoGatherer : public RefCounted<UserMediaPermissionInfoGatherer> {
public:
    static Ref<UserMediaPermissionInfoGatherer> create(CompletionHandler<void(PermissionState, PermissionState)>&& handler) { return adoptRef(*new UserMediaPermissionInfoGatherer(WTF::move(handler))); }
    ~UserMediaPermissionInfoGatherer()
    {
        m_handler(m_cameraPermission, m_microphonePermission);
    }

    void setCameraPermission(PermissionState state) { m_cameraPermission = state; }
    void setMicrophonePermission(PermissionState state) { m_microphonePermission = state; }

private:
    explicit UserMediaPermissionInfoGatherer(CompletionHandler<void(PermissionState, PermissionState)>&& handler)
        : m_handler(WTF::move(handler))
    {
    }

    CompletionHandler<void(PermissionState, PermissionState)> m_handler;
    PermissionState m_cameraPermission { PermissionState::Prompt };
    PermissionState m_microphonePermission { PermissionState::Prompt };
};

void UserMediaPermissionRequestManagerProxy::getUserMediaPermissionInfo(FrameIdentifier frameID, Ref<SecurityOrigin>&& userMediaDocumentOrigin, Ref<SecurityOrigin>&& topLevelDocumentOrigin, CompletionHandler<void(PermissionState, PermissionState)>&& handler)
{
    RefPtr page = m_page.get();
    RefPtr webFrame = WebFrameProxy::webFrame(frameID);
    if (!page || !webFrame || !protocolHostAndPortAreEqual(URL(protect(page->pageLoadState())->activeURL()), topLevelDocumentOrigin->data().toURL())) {
        handler(PermissionState::Prompt, PermissionState::Prompt);
        return;
    }

    Ref gatherer = UserMediaPermissionInfoGatherer::create(WTF::move(handler));

    Ref topLevelOrigin = API::SecurityOrigin::create(topLevelDocumentOrigin.get());
    page->uiClient().queryPermission("camera"_s, topLevelOrigin, [gatherer](auto result) {
        if (result)
            gatherer->setCameraPermission(*result);
    });
    page->uiClient().queryPermission("microphone"_s, topLevelOrigin, [gatherer](auto result) {
        if (result)
            gatherer->setMicrophonePermission(*result);
    });
}

bool UserMediaPermissionRequestManagerProxy::wasGrantedVideoOrAudioAccess(FrameIdentifier frameID)
{
    return wasGrantedAudioAccess(frameID) || wasGrantedVideoAccess(frameID);
}

bool UserMediaPermissionRequestManagerProxy::wasGrantedAudioAccess(FrameIdentifier frameID)
{
    return m_grantedAudioFrames.contains(frameID);
}

bool UserMediaPermissionRequestManagerProxy::wasGrantedVideoAccess(FrameIdentifier frameID)
{
    return m_grantedVideoFrames.contains(frameID);
}

#if !USE(GLIB)
void UserMediaPermissionRequestManagerProxy::platformGetMediaStreamDevices(bool revealIdsAndLabels, CompletionHandler<void(Vector<CaptureDeviceWithCapabilities>&&)>&& completionHandler)
{
    RealtimeMediaSourceCenter::singleton().getMediaStreamDevices([revealIdsAndLabels, completionHandler = WTF::move(completionHandler)](auto&& devices) mutable {
        auto devicesWithCapabilities = WTF::compactMap(devices, [revealIdsAndLabels](auto& device) -> std::optional<CaptureDeviceWithCapabilities> {
            RealtimeMediaSourceCapabilities deviceCapabilities;

            if (revealIdsAndLabels && device.isInputDevice()) {
                auto capabilities = RealtimeMediaSourceCenter::singleton().getCapabilities(device);
                if (!capabilities)
                    return std::nullopt;

                if (revealIdsAndLabels)
                    deviceCapabilities = WTF::move(*capabilities);
            }

            return CaptureDeviceWithCapabilities { WTF::move(device), WTF::move(deviceCapabilities) };
        });

        completionHandler(WTF::move(devicesWithCapabilities));
    });
}
#endif

void UserMediaPermissionRequestManagerProxy::computeFilteredDeviceList(FrameIdentifier frameID, PermissionState cameraState, PermissionState microphoneState, CompletionHandler<void(Vector<CaptureDeviceWithCapabilities>&&)>&& completion)
{
    static const unsigned defaultMaximumCameraCount = 1;
    static const unsigned defaultMaximumMicrophoneCount = 1;

    bool revealIdsAndLabels = cameraState == PermissionState::Granted || microphoneState == PermissionState::Granted;
    RefPtr page = m_page.get();
    bool shoulExposeCaptureDevicesBasedOnPermission = page && !protect(page->preferences())->exposeCaptureDevicesAfterCaptureEnabled();
    platformGetMediaStreamDevices(revealIdsAndLabels || wasGrantedVideoAccess(frameID) || wasGrantedAudioAccess(frameID), [frameID, logIdentifier = LOGIDENTIFIER, weakThis = WeakPtr { *this }, cameraState, microphoneState, revealIdsAndLabels, shoulExposeCaptureDevicesBasedOnPermission, completion = WTF::move(completion)](auto&& devicesWithCapabilities) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis) {
            completion({ });
            return;
        }

        unsigned cameraCount = 0;
        unsigned microphoneCount = 0;
        unsigned speakerCount = 0;

        bool shouldRestrictCamera = !protectedThis->wasGrantedVideoAccess(frameID);
        bool shouldRestrictMicrophone = !protectedThis->wasGrantedAudioAccess(frameID);
        if (shoulExposeCaptureDevicesBasedOnPermission) {
            shouldRestrictCamera = cameraState == PermissionState::Denied || (!revealIdsAndLabels && !protectedThis->wasGrantedVideoAccess(frameID));
            shouldRestrictMicrophone = microphoneState == PermissionState::Denied || (!revealIdsAndLabels && !protectedThis->wasGrantedAudioAccess(frameID));
        }

        bool shouldRestrictSpeaker = !protectedThis->wasGrantedAudioAccess(frameID);
        Vector<CaptureDeviceWithCapabilities> filteredDevices;
        for (auto& deviceWithCapabilities : devicesWithCapabilities) {
            auto& device = deviceWithCapabilities.device;
            if (!device.enabled())
                continue;

            switch (device.type()) {
            case WebCore::CaptureDevice::DeviceType::Camera:
                cameraCount++;
                if (shouldRestrictCamera) {
                    if (cameraCount <= defaultMaximumCameraCount)
                        filteredDevices.append({ { { }, WebCore::CaptureDevice::DeviceType::Camera, { }, { } }, { } });
                    break;
                }
                filteredDevices.append(deviceWithCapabilities);
                break;
            case WebCore::CaptureDevice::DeviceType::Microphone:
                microphoneCount++;
                if (shouldRestrictMicrophone) {
                    if (microphoneCount <= defaultMaximumMicrophoneCount)
                        filteredDevices.append({ { { }, WebCore::CaptureDevice::DeviceType::Microphone, { }, { } }, { } });
                    break;
                }
                filteredDevices.append(deviceWithCapabilities);
                break;
            case WebCore::CaptureDevice::DeviceType::Speaker:
                if (shouldRestrictSpeaker)
                    break;
                speakerCount++;
                filteredDevices.append(deviceWithCapabilities);
                break;
            default:
                break;
            }
        }

        ALWAYS_LOG_WITH_THIS(protectedThis, logIdentifier, "exposing ", cameraCount, " camera(s) filtering = ", shouldRestrictCamera, ", ", microphoneCount, " microphone(s) filtering = ", shouldRestrictMicrophone, ", ", speakerCount, " speaker(s) filtering = ", shouldRestrictSpeaker);

#if RELEASE_LOG_DISABLED
        UNUSED_VARIABLE(speakerCount);
#endif

        completion(WTF::move(filteredDevices));
    });
}
#endif

void UserMediaPermissionRequestManagerProxy::enumerateMediaDevicesForFrame(FrameIdentifier frameID, Ref<SecurityOrigin>&& userMediaDocumentOrigin, Ref<SecurityOrigin>&& topLevelDocumentOrigin, CompletionHandler<void(const Vector<CaptureDeviceWithCapabilities>&, MediaDeviceHashSalts&&)>&& completionHandler)
{
#if ENABLE(MEDIA_STREAM)
    ALWAYS_LOG(LOGIDENTIFIER);

    auto callback = [weakThis = WeakPtr { *this }, frameID, userMediaDocumentOrigin, topLevelDocumentOrigin, completionHandler = WTF::move(completionHandler)](auto cameraState, auto microphoneState) mutable {
        auto callCompletionHandler = makeScopeExit([&completionHandler] {
            completionHandler({ }, { });
        });

        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        RefPtr page = protectedThis->m_page.get();
        if (!page || !page->hasRunningProcess())
            return;

        auto requestID = MediaDevicePermissionRequestIdentifier::generate();
        protectedThis->m_pendingDeviceRequests.add(requestID);

        callCompletionHandler.release();
        protect(page->websiteDataStore())->ensureProtectedDeviceIdHashSaltStorage()->deviceIdHashSaltForOrigin(userMediaDocumentOrigin, topLevelDocumentOrigin, [weakThis = WTF::move(weakThis), requestID, frameID, userMediaDocumentOrigin, topLevelDocumentOrigin, cameraState, microphoneState, completionHandler = WTF::move(completionHandler)](String&& deviceIDHashSalt) mutable {
            auto callCompletionHandler = makeScopeExit([&completionHandler] {
                completionHandler({ }, { });
            });

            RefPtr protectedThis = weakThis.get();
            if (!protectedThis || !protectedThis->m_pendingDeviceRequests.remove(requestID))
                return;

            RefPtr page = protectedThis->m_page.get();
            if (!page || !page->hasRunningProcess())
                return;

            protectedThis->syncWithWebCorePrefs();

            MediaDeviceHashSalts hashSaltsForRequest = { deviceIDHashSalt, protectedThis->ephemeralDeviceHashSaltForFrame(frameID) };

            callCompletionHandler.release();
            protectedThis->computeFilteredDeviceList(frameID, cameraState, microphoneState, [completionHandler = WTF::move(completionHandler), hashSaltsForRequest = WTF::move(hashSaltsForRequest)] (auto&& devices) mutable {
                completionHandler(devices, WTF::move(hashSaltsForRequest));
            });
        });
    };

    getUserMediaPermissionInfo(frameID, WTF::move(userMediaDocumentOrigin), WTF::move(topLevelDocumentOrigin), WTF::move(callback));
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
    if (m_mockDevicesEnabledOverride)
        return *m_mockDevicesEnabledOverride;
    RefPtr page = m_page.get();
    return page && protect(page->preferences())->mockCaptureDevicesEnabled();
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
    RefPtr page = m_page.get();
    if (!page)
        return;

#if ENABLE(MEDIA_STREAM)
    Ref preferences = page->preferences();
    // Enable/disable the mock capture devices for the UI process as per the WebCore preferences. Note that
    // this is a noop if the preference hasn't changed since the last time this was called.
    bool mockDevicesEnabled = m_mockDevicesEnabledOverride ? *m_mockDevicesEnabledOverride : preferences->mockCaptureDevicesEnabled();

#if ENABLE(GPU_PROCESS)
    if (preferences->captureAudioInGPUProcessEnabled() && preferences->useMicrophoneMuteStatusAPI())
        protect(page->legacyMainFrameProcess().processPool())->ensureProtectedGPUProcess()->enableMicrophoneMuteStatusAPI();

    if (preferences->captureAudioInGPUProcessEnabled() || preferences->captureVideoInGPUProcessEnabled())
        protect(page->legacyMainFrameProcess().processPool())->ensureProtectedGPUProcess()->setUseMockCaptureDevices(mockDevicesEnabled);
#endif

    if (MockRealtimeMediaSourceCenter::mockRealtimeMediaSourceCenterEnabled() == mockDevicesEnabled)
        return;
    MockRealtimeMediaSourceCenter::setMockRealtimeMediaSourceCenterEnabled(mockDevicesEnabled);
#endif
}

void UserMediaPermissionRequestManagerProxy::captureStateChanged(MediaProducerMediaStateFlags oldState, MediaProducerMediaStateFlags newState)
{
    RefPtr page = m_page.get();
    if (!page || !page->hasRunningProcess())
        return;

#if ENABLE(MEDIA_STREAM)
    if (!m_hasPendingCapture)
        revokeSandboxExtensionsIfNeededForPage(*page);

    if (m_captureState == (newState & activeCaptureMask))
        return;

    ALWAYS_LOG(LOGIDENTIFIER, "state was: ", m_captureState.toRaw(), ", is now: ", (newState & activeCaptureMask).toRaw());
    m_captureState = newState & activeCaptureMask;

    Seconds interval;
    if (m_captureState & activeCaptureMask) {
        interval = Seconds::fromHours(protect(page->preferences())->longRunningMediaCaptureStreamRepromptIntervalInHours());
        m_lastCaptureTime = { };
    } else {
        interval = Seconds::fromMinutes(protect(page->preferences())->inactiveMediaCaptureStreamRepromptIntervalInMinutes());
        m_lastCaptureTime = MonotonicTime::now();
    }

    if (interval == m_currentWatchdogInterval)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, "watchdog set to ", interval.value());
    m_currentWatchdogInterval = interval;
    m_watchdogTimer.startOneShot(m_currentWatchdogInterval);
#endif
}

void UserMediaPermissionRequestManagerProxy::viewIsBecomingVisible()
{
    auto pregrantedRequests = WTF::move(m_pregrantedRequests);
    for (auto& request : pregrantedRequests)
        grantRequest(request);
}

void UserMediaPermissionRequestManagerProxy::watchdogTimerFired()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_grantedRequests.clear();
    m_pregrantedRequests.clear();
    m_currentWatchdogInterval = 0_s;
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& UserMediaPermissionRequestManagerProxy::logChannel() const
{
    return JOIN_LOG_CHANNEL_WITH_PREFIX(LOG_CHANNEL_PREFIX, WebRTC);
}

const Logger& UserMediaPermissionRequestManagerProxy::logger() const
{
    return protect(*m_page)->logger();
}
#endif

String convertEnumerationToString(UserMediaPermissionRequestManagerProxy::RequestAction enumerationValue)
{
    static const std::array<NeverDestroyed<String>, 3> values = {
        MAKE_STATIC_STRING_IMPL("Deny"),
        MAKE_STATIC_STRING_IMPL("Grant"),
        MAKE_STATIC_STRING_IMPL("Prompt"),
    };
    static_assert(static_cast<size_t>(UserMediaPermissionRequestManagerProxy::RequestAction::Deny) == 0, "UserMediaPermissionRequestManagerProxy::RequestAction::Deny is not 0 as expected");
    static_assert(static_cast<size_t>(UserMediaPermissionRequestManagerProxy::RequestAction::Grant) == 1, "UserMediaPermissionRequestManagerProxy::RequestAction::Grant is not 1 as expected");
    static_assert(static_cast<size_t>(UserMediaPermissionRequestManagerProxy::RequestAction::Prompt) == 2, "UserMediaPermissionRequestManagerProxy::RequestAction::Prompt is not 2 as expected");
    return values[static_cast<size_t>(enumerationValue)];
}

} // namespace WebKit
