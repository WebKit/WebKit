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

#pragma once

#include "UserMediaPermissionCheckProxy.h"
#include "UserMediaPermissionRequestProxy.h"
#include <WebCore/SecurityOrigin.h>
#include <WebCore/Timer.h>
#include <WebCore/UserMediaRequest.h>
#include <wtf/HashMap.h>
#include <wtf/Seconds.h>

namespace WebCore {
class CaptureDevice;
struct MediaConstraints;
class SecurityOrigin;
};

namespace WebKit {

class WebPageProxy;

class FrameAuthorizationState {
public:
    FrameAuthorizationState(Ref<WebCore::SecurityOrigin>&& userMediaDocumentSecurityOrigin, Ref<WebCore::SecurityOrigin>&& topLevelDocumentSecurityOrigin);

    bool hasPermissionToUseCaptureDevice(const String& deviceUID);
    void setHasPermissionToUseCaptureDevice(const String&, bool);

    void ensureSecurityOriginsAreEqual(WebCore::SecurityOrigin& userMediaDocumentSecurityOrigin, WebCore::SecurityOrigin& topLevelDocumentSecurityOrigin);

    void setDeviceIdentifierHashSalt(String&& hashSalt) { m_deviceIdentifierHashSalt = hashSalt; }
    const String& deviceIdentifierHashSalt() const { return m_deviceIdentifierHashSalt; }

private:
    Ref<WebCore::SecurityOrigin> m_userMediaDocumentSecurityOrigin;
    Ref<WebCore::SecurityOrigin> m_topLevelDocumentSecurityOrigin;
    Vector<String> m_authorizedDeviceUIDs;
    String m_deviceIdentifierHashSalt;
};

class UserMediaPermissionRequestManagerProxy {
public:
    explicit UserMediaPermissionRequestManagerProxy(WebPageProxy&);
    ~UserMediaPermissionRequestManagerProxy();

    WebPageProxy& page() const { return m_page; }

    void invalidatePendingRequests();

    void requestUserMediaPermissionForFrame(uint64_t userMediaID, uint64_t frameID, Ref<WebCore::SecurityOrigin>&&  userMediaDocumentOrigin, Ref<WebCore::SecurityOrigin>&& topLevelDocumentOrigin, const WebCore::MediaConstraints& audioConstraints, const WebCore::MediaConstraints& videoConstraints);

    void userMediaAccessWasGranted(uint64_t, const String& audioDeviceUID, const String& videoDeviceUID);
    void userMediaAccessWasDenied(uint64_t, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason);

    template <typename RequestType>
    FrameAuthorizationState& stateForRequest(RequestType&);

    void enumerateMediaDevicesForFrame(uint64_t userMediaID, uint64_t frameID, Ref<WebCore::SecurityOrigin>&& userMediaDocumentOrigin, Ref<WebCore::SecurityOrigin>&& topLevelDocumentOrigin);

    void stopCapture();
    void scheduleNextRejection();
    void rejectionTimerFired();
    void clearCachedState();

    void startedCaptureSession();
    void endedCaptureSession();

private:
    Ref<UserMediaPermissionRequestProxy> createRequest(uint64_t userMediaID, uint64_t frameID, Ref<WebCore::SecurityOrigin>&& userMediaDocumentOrigin, Ref<WebCore::SecurityOrigin>&& topLevelDocumentOrigin, Vector<String>&& audioDeviceUIDs, Vector<String>&& videoDeviceUIDs);
    void denyRequest(uint64_t userMediaID, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason, const String& invalidConstraint);

    void getUserMediaPermissionInfo(uint64_t userMediaID, uint64_t frameID, UserMediaPermissionCheckProxy::CompletionHandler&&, Ref<WebCore::SecurityOrigin>&& userMediaDocumentOrigin, Ref<WebCore::SecurityOrigin>&& topLevelDocumentOrigin);

    void syncWithWebCorePrefs() const;

    HashMap<uint64_t, RefPtr<UserMediaPermissionRequestProxy>> m_pendingUserMediaRequests;
    HashMap<uint64_t, Ref<UserMediaPermissionCheckProxy>> m_pendingDeviceRequests;
    HashMap<uint64_t, std::unique_ptr<FrameAuthorizationState>> m_frameStates;

    WebPageProxy& m_page;

    WebCore::Timer m_rejectionTimer;
    Vector<uint64_t> m_pendingRejections;
};

} // namespace WebKit

