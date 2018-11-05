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
#include <wtf/HashMap.h>
#include <wtf/RunLoop.h>
#include <wtf/Seconds.h>

namespace WebCore {
class CaptureDevice;
struct MediaConstraints;
struct MediaStreamRequest;
class SecurityOrigin;
};

namespace WebKit {

class WebPageProxy;

class UserMediaPermissionRequestManagerProxy {
public:
    explicit UserMediaPermissionRequestManagerProxy(WebPageProxy&);
    ~UserMediaPermissionRequestManagerProxy();

    WebPageProxy& page() const { return m_page; }

    void invalidatePendingRequests();

    void requestUserMediaPermissionForFrame(uint64_t userMediaID, uint64_t frameID, Ref<WebCore::SecurityOrigin>&&  userMediaDocumentOrigin, Ref<WebCore::SecurityOrigin>&& topLevelDocumentOrigin, const WebCore::MediaStreamRequest&);

    void resetAccess(uint64_t mainFrameID);
    void viewIsBecomingVisible();

    void userMediaAccessWasGranted(uint64_t, WebCore::CaptureDevice&& audioDevice, WebCore::CaptureDevice&& videoDevice);
    void userMediaAccessWasDenied(uint64_t, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason);

    void enumerateMediaDevicesForFrame(uint64_t userMediaID, uint64_t frameID, Ref<WebCore::SecurityOrigin>&& userMediaDocumentOrigin, Ref<WebCore::SecurityOrigin>&& topLevelDocumentOrigin);

    void stopCapture();
    void scheduleNextRejection();
    void rejectionTimerFired();
    void clearCachedState();
    void captureDevicesChanged();

    void captureStateChanged(WebCore::MediaProducer::MediaStateFlags oldState, WebCore::MediaProducer::MediaStateFlags newState);
    void syncWithWebCorePrefs() const;

private:
    Ref<UserMediaPermissionRequestProxy> createPermissionRequest(uint64_t userMediaID, uint64_t mainFrameID, uint64_t frameID, Ref<WebCore::SecurityOrigin>&& userMediaDocumentOrigin, Ref<WebCore::SecurityOrigin>&& topLevelDocumentOrigin, Vector<WebCore::CaptureDevice>&& audioDevices, Vector<WebCore::CaptureDevice>&& videoDevices, String&&, WebCore::MediaStreamRequest&&);
    void denyRequest(uint64_t userMediaID, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason, const String& invalidConstraint);
#if ENABLE(MEDIA_STREAM)
    bool grantAccess(uint64_t userMediaID, const WebCore::CaptureDevice audioDevice, const WebCore::CaptureDevice videoDevice, const String& deviceIdentifierHashSalt);

    const UserMediaPermissionRequestProxy* searchForGrantedRequest(uint64_t frameID, const WebCore::SecurityOrigin& userMediaDocumentOrigin, const WebCore::SecurityOrigin& topLevelDocumentOrigin, bool needsAudio, bool needsVideo) const;
    bool wasRequestDenied(uint64_t mainFrameID, const WebCore::SecurityOrigin& userMediaDocumentOrigin, const WebCore::SecurityOrigin& topLevelDocumentOrigin, bool needsAudio, bool needsVideo, bool needsScreenCapture);

    void getUserMediaPermissionInfo(uint64_t userMediaID, uint64_t frameID, UserMediaPermissionCheckProxy::CompletionHandler&&, Ref<WebCore::SecurityOrigin>&& userMediaDocumentOrigin, Ref<WebCore::SecurityOrigin>&& topLevelDocumentOrigin);

    enum class RequestAction {
        Deny,
        Grant,
        Prompt        
    };
    RequestAction getRequestAction(uint64_t frameID, WebCore::SecurityOrigin& userMediaDocumentOrigin, WebCore::SecurityOrigin& topLevelDocumentOrigin, const WebCore::MediaStreamRequest&, Vector<WebCore::CaptureDevice>& audioDevices, Vector<WebCore::CaptureDevice>& videoDevices);
#endif

    void watchdogTimerFired();

    HashMap<uint64_t, RefPtr<UserMediaPermissionRequestProxy>> m_pendingUserMediaRequests;
    HashMap<uint64_t, Ref<UserMediaPermissionCheckProxy>> m_pendingDeviceRequests;

    WebPageProxy& m_page;

    RunLoop::Timer<UserMediaPermissionRequestManagerProxy> m_rejectionTimer;
    Vector<uint64_t> m_pendingRejections;

    Vector<Ref<UserMediaPermissionRequestProxy>> m_pregrantedRequests;
    Vector<Ref<UserMediaPermissionRequestProxy>> m_grantedRequests;

    struct DeniedRequest {
        uint64_t mainFrameID;
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
};

} // namespace WebKit

