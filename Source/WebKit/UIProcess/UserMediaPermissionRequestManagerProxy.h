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

#pragma once

#include "UserMediaPermissionCheckProxy.h"
#include "UserMediaPermissionRequestProxy.h"
#include <WebCore/MediaProducer.h>
#include <WebCore/SecurityOrigin.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Deque.h>
#include <wtf/HashMap.h>
#include <wtf/LoggerHelper.h>
#include <wtf/RunLoop.h>
#include <wtf/Seconds.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class CaptureDevice;
struct MediaConstraints;
struct MediaStreamRequest;
class SecurityOrigin;
};

namespace WebKit {

class WebPageProxy;

class UserMediaPermissionRequestManagerProxy
    : public CanMakeWeakPtr<UserMediaPermissionRequestManagerProxy>
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit UserMediaPermissionRequestManagerProxy(WebPageProxy&);
    ~UserMediaPermissionRequestManagerProxy();

    WebPageProxy& page() const { return m_page; }

#if ENABLE(MEDIA_STREAM)
    static void forEach(const WTF::Function<void(UserMediaPermissionRequestManagerProxy&)>&);
#endif

    void invalidatePendingRequests();

    void requestUserMediaPermissionForFrame(uint64_t userMediaID, WebCore::FrameIdentifier, Ref<WebCore::SecurityOrigin>&&  userMediaDocumentOrigin, Ref<WebCore::SecurityOrigin>&& topLevelDocumentOrigin, WebCore::MediaStreamRequest&&);

    void resetAccess(WebCore::FrameIdentifier mainFrameID);
    void viewIsBecomingVisible();

    void grantRequest(UserMediaPermissionRequestProxy&);
    void denyRequest(UserMediaPermissionRequestProxy&, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason, const String& invalidConstraint = { });

    void enumerateMediaDevicesForFrame(WebCore::FrameIdentifier, Ref<WebCore::SecurityOrigin>&& userMediaDocumentOrigin, Ref<WebCore::SecurityOrigin>&& topLevelDocumentOrigin, CompletionHandler<void(const Vector<WebCore::CaptureDevice>&, const String&)>&&);

    void stopCapture();
    void scheduleNextRejection();
    void rejectionTimerFired();
    void clearCachedState();
    void captureDevicesChanged();

    void captureStateChanged(WebCore::MediaProducer::MediaStateFlags oldState, WebCore::MediaProducer::MediaStateFlags newState);
    void syncWithWebCorePrefs() const;

    enum class RequestAction {
        Deny,
        Grant,
        Prompt
    };

    void setMockCaptureDevicesEnabledOverride(Optional<bool> enabled) { m_mockDevicesEnabledOverride = enabled; }

private:
#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final;
    const void* logIdentifier() const final { return m_logIdentifier; }
    const char* logClassName() const override { return "UserMediaPermissionRequestManagerProxy"; }
    WTFLogChannel& logChannel() const final;
#endif

    Ref<UserMediaPermissionRequestProxy> createPermissionRequest(uint64_t userMediaID, WebCore::FrameIdentifier mainFrameID, WebCore::FrameIdentifier, Ref<WebCore::SecurityOrigin>&& userMediaDocumentOrigin, Ref<WebCore::SecurityOrigin>&& topLevelDocumentOrigin, Vector<WebCore::CaptureDevice>&& audioDevices, Vector<WebCore::CaptureDevice>&& videoDevices, WebCore::MediaStreamRequest&&);
#if ENABLE(MEDIA_STREAM)
    void finishGrantingRequest(UserMediaPermissionRequestProxy&);

    const UserMediaPermissionRequestProxy* searchForGrantedRequest(WebCore::FrameIdentifier, const WebCore::SecurityOrigin& userMediaDocumentOrigin, const WebCore::SecurityOrigin& topLevelDocumentOrigin, bool needsAudio, bool needsVideo) const;
    bool wasRequestDenied(WebCore::FrameIdentifier mainFrameID, const WebCore::SecurityOrigin& userMediaDocumentOrigin, const WebCore::SecurityOrigin& topLevelDocumentOrigin, bool needsAudio, bool needsVideo, bool needsScreenCapture);

    using PermissionInfo = UserMediaPermissionCheckProxy::PermissionInfo;
    void getUserMediaPermissionInfo(WebCore::FrameIdentifier, Ref<WebCore::SecurityOrigin>&& userMediaDocumentOrigin, Ref<WebCore::SecurityOrigin>&& topLevelDocumentOrigin, CompletionHandler<void(PermissionInfo)>&&);
    void captureDevicesChanged(PermissionInfo);

    RequestAction getRequestAction(const UserMediaPermissionRequestProxy&);

    bool wasGrantedVideoOrAudioAccess(WebCore::FrameIdentifier, const WebCore::SecurityOrigin& userMediaDocumentOrigin, const WebCore::SecurityOrigin& topLevelDocumentOrigin);

    Vector<WebCore::CaptureDevice> computeFilteredDeviceList(bool revealIdsAndLabels, const String& deviceIDHashSalt);

    void processUserMediaPermissionRequest();
    void processUserMediaPermissionInvalidRequest(const String& invalidConstraint);
    void processUserMediaPermissionValidRequest(Vector<WebCore::CaptureDevice>&& audioDevices, Vector<WebCore::CaptureDevice>&& videoDevices, String&& deviceIdentifierHashSalt);
    void startProcessingUserMediaPermissionRequest(Ref<UserMediaPermissionRequestProxy>&&);
#endif

    void watchdogTimerFired();

    void processNextUserMediaRequestIfNeeded();

    RefPtr<UserMediaPermissionRequestProxy> m_currentUserMediaRequest;
    Deque<Ref<UserMediaPermissionRequestProxy>> m_pendingUserMediaRequests;
    HashSet<uint64_t> m_pendingDeviceRequests;

    WebPageProxy& m_page;

    RunLoop::Timer<UserMediaPermissionRequestManagerProxy> m_rejectionTimer;
    Deque<Ref<UserMediaPermissionRequestProxy>> m_pendingRejections;

    Vector<Ref<UserMediaPermissionRequestProxy>> m_pregrantedRequests;
    Vector<Ref<UserMediaPermissionRequestProxy>> m_grantedRequests;

    struct DeniedRequest {
        WebCore::FrameIdentifier mainFrameID;
        Ref<WebCore::SecurityOrigin> userMediaDocumentOrigin;
        Ref<WebCore::SecurityOrigin> topLevelDocumentOrigin;
        bool isAudioDenied;
        bool isVideoDenied;
        bool isScreenCaptureDenied;
    };
    Vector<DeniedRequest> m_deniedRequests;

    WebCore::MediaProducer::MediaStateFlags m_captureState { WebCore::MediaProducer::IsNotPlaying };
    RunLoop::Timer<UserMediaPermissionRequestManagerProxy> m_watchdogTimer;
    Seconds m_currentWatchdogInterval;
#if !RELEASE_LOG_DISABLED
    Ref<const Logger> m_logger;
    const void* m_logIdentifier;
#endif
    bool m_hasFilteredDeviceList { false };
    uint64_t m_hasPendingCapture { 0 };
    Optional<bool> m_mockDevicesEnabledOverride;
};

String convertEnumerationToString(UserMediaPermissionRequestManagerProxy::RequestAction);

} // namespace WebKit

#if ENABLE(MEDIA_STREAM)
namespace WTF {

template<typename Type>
struct LogArgument;

template <>
struct LogArgument<WebKit::UserMediaPermissionRequestManagerProxy::RequestAction> {
    static String toString(const WebKit::UserMediaPermissionRequestManagerProxy::RequestAction type)
    {
        return convertEnumerationToString(type);
    }
};

}; // namespace WTF
#endif
