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
#include <WebCore/MediaConstraintType.h>
#include <WebCore/MediaConstraints.h>
#include <WebCore/MockRealtimeMediaSourceCenter.h>
#include <WebCore/PermissionName.h>
#include <WebCore/PlatformMediaSessionManager.h>
#include <WebCore/RealtimeMediaSource.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/UserMediaRequest.h>
#include <wtf/CryptographicallyRandomNumber.h>
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

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

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

void UserMediaPermissionRequestManagerProxy::forEach(const WTF::Function<void(UserMediaPermissionRequestManagerProxy&)>& function)
{
    for (auto& proxy : proxies())
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
    , m_rejectionTimer(RunLoop::protectedMain(), this, &UserMediaPermissionRequestManagerProxy::rejectionTimerFired)
    , m_watchdogTimer(RunLoop::protectedMain(), this, &UserMediaPermissionRequestManagerProxy::watchdogTimerFired)
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

void UserMediaPermissionRequestManagerProxy::disconnectFromPage()
{
    stopCapture();
#if ENABLE(MEDIA_STREAM)
    if (RefPtr page = m_page.get())
        UserMediaProcessManager::singleton().revokeSandboxExtensionsIfNeeded(page->protectedLegacyMainFrameProcess());
#endif
    m_page = nullptr;
}

WebPageProxy* UserMediaPermissionRequestManagerProxy::page() const
{
    return m_page.get();
}

RefPtr<WebPageProxy> UserMediaPermissionRequestManagerProxy::protectedPage() const
{
    return m_page.get();
}

void UserMediaPermissionRequestManagerProxy::invalidatePendingRequests()
{
    if (RefPtr currentUserMediaRequest = std::exchange(m_currentUserMediaRequest, nullptr))
        currentUserMediaRequest->invalidate();

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
    getUserMediaPermissionInfo(page->mainFrame()->frameID(), origin.get(), WTFMove(origin), [weakThis = WeakPtr { *this }](PermissionInfo permissionInfo) {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->captureDevicesChanged(permissionInfo);
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
    RefPtr page = m_page.get();
    if (!page || !page->hasRunningProcess())
        return;

    page->legacyMainFrameProcess().send(Messages::WebPage::CaptureDevicesChanged(), page->webPageIDInMainFrameProcess());
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

    if (reason == UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::PermissionDenied)
        m_deniedRequests.append(DeniedRequest { request.mainFrameID(), request.userMediaDocumentSecurityOrigin(), request.topLevelDocumentSecurityOrigin(), request.requiresAudioCapture(), request.requiresVideoCapture(), request.requiresDisplayCapture() });

    if (auto callback = request.decisionCompletionHandler()) {
        callback(false);
        return;
    }

#if ENABLE(MEDIA_STREAM)
    if (auto userMediaID = request.userMediaID())
        page->legacyMainFrameProcess().send(Messages::WebPage::UserMediaAccessWasDenied(*userMediaID, toWebCore(reason), message, invalidConstraint), page->webPageIDInMainFrameProcess());
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
        page->willStartCapture(request, [callback = WTFMove(callback)]() mutable {
            callback(true);
        });
        m_grantedRequests.append(request);
        m_grantedFrames.add(request.frameID());
        return;
    }

    Ref userMediaDocumentSecurityOrigin = request.userMediaDocumentSecurityOrigin();
    Ref topLevelDocumentSecurityOrigin = request.topLevelDocumentSecurityOrigin();
    page->websiteDataStore().ensureProtectedDeviceIdHashSaltStorage()->deviceIdHashSaltForOrigin(userMediaDocumentSecurityOrigin, topLevelDocumentSecurityOrigin, [weakThis = WeakPtr { *this }, request = Ref { request }](String&&) mutable {
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
    return (!page.preferences().captureAudioInGPUProcessEnabled() && !page.preferences().captureAudioInUIProcessEnabled()) || !page.preferences().captureVideoInGPUProcessEnabled();
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

    page->willStartCapture(request, [this, weakThis = WeakPtr { *this }, request = Ref { request }]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        RefPtr page = m_page.get();
        if (!page)
            return;

        // FIXME: m_hasFilteredDeviceList will trigger ondevicechange events for various documents from different origins.
        if (m_hasFilteredDeviceList)
            captureDevicesChanged(PermissionInfo::Granted);
        m_hasFilteredDeviceList = false;

        ++m_hasPendingCapture;

        Vector<SandboxExtension::Handle> handles;
#if PLATFORM(COCOA)
        if (!m_hasCreatedSandboxExtensionForTCCD && doesPageNeedTCCD(*page)) {
            handles = SandboxExtension::createHandlesForMachLookup({ "com.apple.tccd"_s }, page->legacyMainFrameProcess().auditToken(), SandboxExtension::MachBootstrapOptions::EnableMachBootstrap);
            m_hasCreatedSandboxExtensionForTCCD = true;
        }
#endif

        page->legacyMainFrameProcess().sendWithAsyncReply(Messages::WebPage::UserMediaAccessWasGranted { *request->userMediaID(), request->audioDevice(), request->videoDevice(), request->deviceIdentifierHashSalts(), WTFMove(handles) }, [this, weakThis = WTFMove(weakThis)] {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;
            if (!--m_hasPendingCapture) {
                if (RefPtr page = m_page.get())
                    UserMediaProcessManager::singleton().revokeSandboxExtensionsIfNeeded(page->protectedLegacyMainFrameProcess());
            }
        }, page->webPageIDInMainFrameProcess());

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

    if (RefPtr currentUserMediaRequest = m_currentUserMediaRequest; currentUserMediaRequest && (!frameID || m_currentUserMediaRequest->frameID() == *frameID)) {
        // Avoid starting pending requests after denying current request.
        auto pendingUserMediaRequests = std::exchange(m_pendingUserMediaRequests, { });
        currentUserMediaRequest->deny(UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::OtherFailure);
        m_pendingUserMediaRequests = std::exchange(pendingUserMediaRequests, { });
    }

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
}

const UserMediaPermissionRequestProxy* UserMediaPermissionRequestManagerProxy::searchForGrantedRequest(std::optional<FrameIdentifier> frameID, const SecurityOrigin& userMediaDocumentOrigin, const SecurityOrigin& topLevelDocumentOrigin, bool needsAudio, bool needsVideo) const
{
    RefPtr page = m_page.get();
    if (!page || page->isMediaStreamCaptureMuted())
        return nullptr;

    bool checkForAudio = needsAudio;
    bool checkForVideo = needsVideo;
    for (Ref grantedRequest : m_grantedRequests) {
        if (grantedRequest->requiresDisplayCapture())
            continue;
        if (!grantedRequest->protectedUserMediaDocumentSecurityOrigin()->isSameSchemeHostPort(userMediaDocumentOrigin))
            continue;
        if (!grantedRequest->protectedTopLevelDocumentSecurityOrigin()->isSameSchemeHostPort(topLevelDocumentOrigin))
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
        && Ref { deniedRequest.userMediaDocumentOrigin }->isSameSchemeHostPort(request.protectedUserMediaDocumentSecurityOrigin())
        && Ref { deniedRequest.topLevelDocumentOrigin }->isSameSchemeHostPort(request.protectedTopLevelDocumentSecurityOrigin());
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

    return searchForGrantedRequest(request.frameID(), request.protectedUserMediaDocumentSecurityOrigin(), request.protectedTopLevelDocumentSecurityOrigin(), requestingMicrophone, requestingCamera) ? RequestAction::Grant : RequestAction::Prompt;
}
#endif

void UserMediaPermissionRequestManagerProxy::requestUserMediaPermissionForFrame(UserMediaRequestIdentifier userMediaID, FrameIdentifier frameID, Ref<SecurityOrigin>&& userMediaDocumentOrigin, Ref<SecurityOrigin>&& topLevelDocumentOrigin, MediaStreamRequest&& userRequest)
{
#if ENABLE(MEDIA_STREAM)
    RefPtr page = m_page.get();
    if (!page || !page->hasRunningProcess())
        return;

    ALWAYS_LOG(LOGIDENTIFIER, userMediaID.toUInt64());

    Ref request = UserMediaPermissionRequestProxy::create(*this, userMediaID, page->mainFrame()->frameID(), frameID, WTFMove(userMediaDocumentOrigin), WTFMove(topLevelDocumentOrigin), { }, { }, WTFMove(userRequest));
    if (m_currentUserMediaRequest) {
        if (m_currentUserMediaRequest->requiresDisplayCapture() && request->requiresDisplayCapture()) {
            ALWAYS_LOG(LOGIDENTIFIER, "Cancelling pending getDisplayMedia request");
            std::exchange(m_currentUserMediaRequest, nullptr)->deny(UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::OtherFailure);
        } else {
            m_pendingUserMediaRequests.append(WTFMove(request));
            return;
        }
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

    Ref userMediaDocumentSecurityOrigin = m_currentUserMediaRequest->userMediaDocumentSecurityOrigin();
    Ref topLevelDocumentSecurityOrigin = m_currentUserMediaRequest->topLevelDocumentSecurityOrigin();
    getUserMediaPermissionInfo(m_currentUserMediaRequest->frameID(), WTFMove(userMediaDocumentSecurityOrigin), WTFMove(topLevelDocumentSecurityOrigin), [this, weakThis = WeakPtr { *this }, request = m_currentUserMediaRequest](auto permissionInfo) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        if (!request->isPending())
            return;

        switch (permissionInfo) {
        case PermissionInfo::Error:
            this->denyRequest(Ref { *m_currentUserMediaRequest }, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::OtherFailure);
            return;
        case PermissionInfo::Unknown:
            break;
        case PermissionInfo::Granted:
            Ref { *m_currentUserMediaRequest }->setHasPersistentAccess();
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
    page->websiteDataStore().ensureProtectedDeviceIdHashSaltStorage()->deviceIdHashSaltForOrigin(userMediaDocumentSecurityOrigin, topLevelDocumentSecurityOrigin, [this, weakThis = WeakPtr { *this }, request = m_currentUserMediaRequest] (String&& deviceIDHashSalt) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        if (!request->isPending())
            return;

        RealtimeMediaSourceCenter::InvalidConstraintsHandler invalidHandler = [this, request](auto invalidConstraint) {
            if (!request->isPending())
                return;

            RefPtr page = m_page.get();
            if (!page || !page->hasRunningProcess())
                return;

            processUserMediaPermissionInvalidRequest(invalidConstraint);
        };

        WebCore::MediaDeviceHashSalts deviceHashSaltsForFrame = { deviceIDHashSalt, ephemeralDeviceHashSaltForFrame(request->frameID()) };

        auto validHandler = [this, request, deviceHashSaltsForFrame = deviceHashSaltsForFrame](Vector<CaptureDevice>&& audioDevices, Vector<CaptureDevice>&& videoDevices) mutable {
            if (!request->isPending())
                return;

            RefPtr page = m_page.get();
            if (!page || !page->hasRunningProcess() || !page->mainFrame())
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

void UserMediaPermissionRequestManagerProxy::processUserMediaPermissionInvalidRequest(MediaConstraintType invalidConstraint)
{
    ALWAYS_LOG(LOGIDENTIFIER, m_currentUserMediaRequest->userMediaID() ? m_currentUserMediaRequest->userMediaID()->toUInt64() : 0);
    bool filterConstraint = !m_currentUserMediaRequest->hasPersistentAccess() && !wasGrantedVideoOrAudioAccess(m_currentUserMediaRequest->frameID());

    denyRequest(Ref { *m_currentUserMediaRequest }, filterConstraint ? MediaConstraintType::Unknown : invalidConstraint);
}

void UserMediaPermissionRequestManagerProxy::processUserMediaPermissionValidRequest(Vector<CaptureDevice>&& audioDevices, Vector<CaptureDevice>&& videoDevices, WebCore::MediaDeviceHashSalts&& deviceIdentifierHashSalts)
{
    RefPtr currentUserMediaRequest = m_currentUserMediaRequest;
    ALWAYS_LOG(LOGIDENTIFIER, currentUserMediaRequest->userMediaID() ? currentUserMediaRequest->userMediaID()->toUInt64() : 0, ", video: ", videoDevices.size(), " audio: ", audioDevices.size());
    if (!currentUserMediaRequest->requiresDisplayCapture() && videoDevices.isEmpty() && audioDevices.isEmpty()) {
        denyRequest(*currentUserMediaRequest, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::NoConstraints);
        return;
    }

    currentUserMediaRequest->setDeviceIdentifierHashSalts(WTFMove(deviceIdentifierHashSalts));
    currentUserMediaRequest->setEligibleVideoDeviceUIDs(WTFMove(videoDevices));
    currentUserMediaRequest->setEligibleAudioDeviceUIDs(WTFMove(audioDevices));

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

    if (page->preferences().mockCaptureDevicesEnabled() && currentUserMediaRequest->requiresDisplayCapture() && !m_currentUserMediaRequest->hasVideoDevice()) {
        auto displayDevices = WebCore::RealtimeMediaSourceCenter::singleton().displayCaptureFactory().displayCaptureDeviceManager().captureDevices();
        currentUserMediaRequest->setEligibleVideoDeviceUIDs(WTFMove(displayDevices));
    }

    if (page->isControlledByAutomation()) {
        if (WebAutomationSession* automationSession = page->configuration().processPool().automationSession()) {
            ALWAYS_LOG(LOGIDENTIFIER, currentUserMediaRequest->userMediaID() ? currentUserMediaRequest->userMediaID()->toUInt64() : 0, ", page controlled by automation");
            if (automationSession->shouldAllowGetUserMediaForPage(*page))
                grantRequest(*currentUserMediaRequest);
            else
                denyRequest(*currentUserMediaRequest, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::PermissionDenied);
            return;
        }
    }

    if (page->preferences().mockCaptureDevicesEnabled() && !page->preferences().mockCaptureDevicesPromptEnabled()) {
        ALWAYS_LOG(LOGIDENTIFIER, currentUserMediaRequest->userMediaID() ? currentUserMediaRequest->userMediaID()->toUInt64() : 0, ", mock devices don't require prompt");
        grantRequest(*currentUserMediaRequest);
        return;
    }

    requestSystemValidation(*page, *currentUserMediaRequest, [weakThis = WeakPtr { *this }](bool isOK) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        if (!isOK) {
            protectedThis->denyRequest(Ref { *protectedThis->m_currentUserMediaRequest }, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::PermissionDenied);
            return;
        }
        protectedThis->decidePolicyForUserMediaPermissionRequest();
    });
}

void UserMediaPermissionRequestManagerProxy::decidePolicyForUserMediaPermissionRequest()
{
    if (!m_currentUserMediaRequest)
        return;

    // If page navigated, there is no need to call the page client for authorization.
    RefPtr webFrame = WebFrameProxy::webFrame(m_currentUserMediaRequest->frameID());
    RefPtr page = m_page.get();
    if (!webFrame || !page || !protocolHostAndPortAreEqual(URL(page->pageLoadState().activeURL()), m_currentUserMediaRequest->topLevelDocumentSecurityOrigin().data().toURL())) {
        denyRequest(Ref { *m_currentUserMediaRequest }, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::NoConstraints);
        return;
    }

    // FIXME: Remove webFrame, userMediaOrigin and topLevelOrigin from this uiClient API call.
    Ref userMediaOrigin = API::SecurityOrigin::create(m_currentUserMediaRequest->protectedUserMediaDocumentSecurityOrigin());
    Ref topLevelOrigin = API::SecurityOrigin::create(m_currentUserMediaRequest->protectedTopLevelDocumentSecurityOrigin());
    page->uiClient().decidePolicyForUserMediaPermissionRequest(*page, *webFrame, WTFMove(userMediaOrigin), WTFMove(topLevelOrigin), *m_currentUserMediaRequest);
}

void UserMediaPermissionRequestManagerProxy::checkUserMediaPermissionForSpeechRecognition(WebCore::FrameIdentifier frameIdentifier, const WebCore::SecurityOrigin& requestingOrigin, const WebCore::SecurityOrigin& topOrigin, const WebCore::CaptureDevice& device, CompletionHandler<void(bool)>&& completionHandler)
{
    RefPtr page = m_page.get();
    RefPtr frame = WebFrameProxy::webFrame(frameIdentifier);
    if (!page || !frame || !protocolHostAndPortAreEqual(URL(page->pageLoadState().activeURL()), topOrigin.data().toURL())) {
        completionHandler(false);
        return;
    }

    // We use no UserMediaRequestIdentifier because this does not correspond to a UserMediaPermissionRequest in web process.
    // We create the RequestProxy only to check the media permission for speech.
    Ref request = UserMediaPermissionRequestProxy::create(*this, std::nullopt, frameIdentifier, frameIdentifier, requestingOrigin.isolatedCopy(), topOrigin.isolatedCopy(), Vector<WebCore::CaptureDevice> { device }, { }, { }, WTFMove(completionHandler));

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
    page->uiClient().decidePolicyForUserMediaPermissionRequest(*page, *frame, WTFMove(apiRequestingOrigin), WTFMove(apiTopOrigin), request.get());
}

bool UserMediaPermissionRequestManagerProxy::shouldChangeDeniedToPromptForCamera(const ClientOrigin& origin) const
{
    RefPtr page = m_page.get();
    if (!page)
        return false;

    if (!protocolHostAndPortAreEqual(URL(page->pageLoadState().activeURL()), origin.topOrigin.toURL()))
        return true;

    return !anyOf(m_deniedRequests, [](auto& request) { return request.isVideoDenied; })
        && !anyOf(m_pregrantedRequests, [](auto& request) { return request->requiresVideoCapture(); })
        && !anyOf(m_grantedRequests, [](auto& request) { return request->requiresVideoCapture(); });
}

bool UserMediaPermissionRequestManagerProxy::shouldChangeDeniedToPromptForMicrophone(const ClientOrigin& origin) const
{
    RefPtr page = m_page.get();
    if (!page)
        return false;

    if (!protocolHostAndPortAreEqual(URL(page->pageLoadState().activeURL()), origin.topOrigin.toURL()))
        return true;

    return !anyOf(m_deniedRequests, [](auto& request) { return request.isAudioDenied; })
        && !anyOf(m_pregrantedRequests, [](auto& request) { return request->requiresAudioCapture(); })
        && !anyOf(m_grantedRequests, [](auto& request) { return request->requiresAudioCapture(); });
}

bool UserMediaPermissionRequestManagerProxy::shouldChangePromptToGrantForCamera(const ClientOrigin& origin) const
{
    return searchForGrantedRequest(std::nullopt, origin.clientOrigin.securityOrigin().get(), origin.topOrigin.securityOrigin().get(), false, true);
}

bool UserMediaPermissionRequestManagerProxy::shouldChangePromptToGrantForMicrophone(const ClientOrigin& origin) const
{
    return searchForGrantedRequest(std::nullopt, origin.clientOrigin.securityOrigin().get(), origin.topOrigin.securityOrigin().get(), true, false);
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
    RefPtr page = m_page.get();
    RefPtr webFrame = WebFrameProxy::webFrame(frameID);
    if (!page || !webFrame || !protocolHostAndPortAreEqual(URL(page->pageLoadState().activeURL()), topLevelDocumentOrigin->data().toURL())) {
        handler({ });
        return;
    }

    Ref userMediaOrigin = API::SecurityOrigin::create(userMediaDocumentOrigin.get());
    Ref topLevelOrigin = API::SecurityOrigin::create(topLevelDocumentOrigin.get());

    auto requestID = MediaDevicePermissionRequestIdentifier::generate();
    m_pendingDeviceRequests.add(requestID);

    Ref request = UserMediaPermissionCheckProxy::create(frameID, [this, weakThis = WeakPtr { *this }, requestID, handler = WTFMove(handler)](auto permissionInfo) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || !m_pendingDeviceRequests.remove(requestID))
            permissionInfo = PermissionInfo::Error;
        handler(permissionInfo);
    }, WTFMove(userMediaDocumentOrigin), WTFMove(topLevelDocumentOrigin));

    // FIXME: Remove webFrame, userMediaOrigin and topLevelOrigin from this uiClient API call.
    page->uiClient().checkUserMediaPermissionForOrigin(*page, *webFrame, userMediaOrigin.get(), topLevelOrigin.get(), request.get());
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
        auto devicesWithCapabilities = WTF::compactMap(devices, [revealIdsAndLabels](auto& device) -> std::optional<CaptureDeviceWithCapabilities> {
            RealtimeMediaSourceCapabilities deviceCapabilities;

            if (revealIdsAndLabels && device.isInputDevice()) {
                auto capabilities = RealtimeMediaSourceCenter::singleton().getCapabilities(device);
                if (!capabilities)
                    return std::nullopt;

                if (revealIdsAndLabels)
                    deviceCapabilities = WTFMove(*capabilities);
            }

            return CaptureDeviceWithCapabilities { WTFMove(device), WTFMove(deviceCapabilities) };
        });

        completionHandler(WTFMove(devicesWithCapabilities));
    });
}
#endif

void UserMediaPermissionRequestManagerProxy::computeFilteredDeviceList(bool revealIdsAndLabels, CompletionHandler<void(Vector<CaptureDeviceWithCapabilities>&&)>&& completion)
{
    static const unsigned defaultMaximumCameraCount = 1;
    static const unsigned defaultMaximumMicrophoneCount = 1;

    platformGetMediaStreamDevices(revealIdsAndLabels, [logIdentifier = LOGIDENTIFIER, this, weakThis = WeakPtr { *this }, revealIdsAndLabels, completion = WTFMove(completion)](auto&& devicesWithCapabilities) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis) {
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

        RefPtr page = m_page.get();
        if (!page || !page->hasRunningProcess())
            return;

        auto requestID = MediaDevicePermissionRequestIdentifier::generate();
        m_pendingDeviceRequests.add(requestID);

        callCompletionHandler.release();
        page->websiteDataStore().ensureProtectedDeviceIdHashSaltStorage()->deviceIdHashSaltForOrigin(userMediaDocumentOrigin, topLevelDocumentOrigin, [this, weakThis = WeakPtr { *this }, requestID, frameID, userMediaDocumentOrigin, topLevelDocumentOrigin, originHasPersistentAccess, completionHandler = WTFMove(completionHandler)](String&& deviceIDHashSalt) mutable {
            auto callCompletionHandler = makeScopeExit([&completionHandler] {
                completionHandler({ }, { });
            });

            RefPtr protectedThis = weakThis.get();
            if (!protectedThis || !m_pendingDeviceRequests.remove(requestID))
                return;

            RefPtr page = m_page.get();
            if (!page || !page->hasRunningProcess())
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
    return m_mockDevicesEnabledOverride ? *m_mockDevicesEnabledOverride : m_page && m_page->preferences().mockCaptureDevicesEnabled();
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
    // Enable/disable the mock capture devices for the UI process as per the WebCore preferences. Note that
    // this is a noop if the preference hasn't changed since the last time this was called.
    bool mockDevicesEnabled = m_mockDevicesEnabledOverride ? *m_mockDevicesEnabledOverride : page->preferences().mockCaptureDevicesEnabled();

#if ENABLE(GPU_PROCESS)
    if (page->preferences().captureAudioInGPUProcessEnabled() || page->preferences().captureVideoInGPUProcessEnabled())
        page->legacyMainFrameProcess().protectedProcessPool()->ensureProtectedGPUProcess()->setUseMockCaptureDevices(mockDevicesEnabled);
#endif

#if HAVE(SC_CONTENT_SHARING_PICKER)
    auto useSharingPicker = page->preferences().useSCContentSharingPicker();

#if ENABLE(GPU_PROCESS)
    if (useSharingPicker)
        page->legacyMainFrameProcess().protectedProcessPool()->ensureProtectedGPUProcess()->setUseSCContentSharingPicker(useSharingPicker);
    if (bool useMicrophoneMuteStatusAPI = page->preferences().useMicrophoneMuteStatusAPI())
        page->legacyMainFrameProcess().protectedProcessPool()->ensureProtectedGPUProcess()->enableMicrophoneMuteStatusAPI();
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
    RefPtr page = m_page.get();
    if (!page || !page->hasRunningProcess())
        return;

#if ENABLE(MEDIA_STREAM)
    if (!m_hasPendingCapture)
        UserMediaProcessManager::singleton().revokeSandboxExtensionsIfNeeded(page->protectedLegacyMainFrameProcess());

    if (m_captureState == (newState & activeCaptureMask))
        return;

    ALWAYS_LOG(LOGIDENTIFIER, "state was: ", m_captureState.toRaw(), ", is now: ", (newState & activeCaptureMask).toRaw());
    m_captureState = newState & activeCaptureMask;

    Seconds interval;
    if (m_captureState & activeCaptureMask)
        interval = Seconds::fromHours(page->preferences().longRunningMediaCaptureStreamRepromptIntervalInHours());
    else
        interval = Seconds::fromMinutes(page->preferences().inactiveMediaCaptureStreamRepromptIntervalInMinutes());

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
    return m_page->logger();
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

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
