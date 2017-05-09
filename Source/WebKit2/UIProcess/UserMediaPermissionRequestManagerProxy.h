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
struct MediaConstraintsData;
class SecurityOrigin;
};

namespace WebKit {

class WebPageProxy;

class FrameAuthorizationState {
public:
    explicit FrameAuthorizationState(WebCore::SecurityOrigin* userMediaDocumentSecurityOrigin, WebCore::SecurityOrigin* topLevelDocumentSecurityOrigin);

    bool hasPermissionToUseCaptureDevice(const String& deviceUID);
    void setHasPermissionToUseCaptureDevice(const String&, bool);

    void ensureSecurityOriginsAreEqual(WebCore::SecurityOrigin* userMediaDocumentSecurityOrigin, WebCore::SecurityOrigin* topLevelDocumentSecurityOrigin);

    void setDeviceIdentifierHashSalt(const String& hashSalt) { m_deviceIdentifierHashSalt = hashSalt; }
    String deviceIdentifierHashSalt() const { return m_deviceIdentifierHashSalt; }

    bool hasPersistentAccess() const { return m_hasPersistentAccess; }
    void setHasPersistentAccess(bool value) { m_hasPersistentAccess = value; }

private:
    RefPtr<WebCore::SecurityOrigin> m_userMediaDocumentSecurityOrigin;
    RefPtr<WebCore::SecurityOrigin> m_topLevelDocumentSecurityOrigin;
    Vector<String> m_authorizedDeviceUIDs;
    String m_deviceIdentifierHashSalt;
    bool m_hasPersistentAccess { false };
};

class UserMediaPermissionRequestManagerProxy {
public:
    explicit UserMediaPermissionRequestManagerProxy(WebPageProxy&);
    ~UserMediaPermissionRequestManagerProxy();

    WebPageProxy& page() const { return m_page; }

    void invalidatePendingRequests();

    void requestUserMediaPermissionForFrame(uint64_t userMediaID, uint64_t frameID, String userMediaDocumentOriginIdentifier, String topLevelDocumentOriginIdentifier, const WebCore::MediaConstraintsData& audioConstraintsData, const WebCore::MediaConstraintsData& videoConstraintsData);

    void userMediaAccessWasGranted(uint64_t, const String& audioDeviceUID, const String& videoDeviceUID);
    void userMediaAccessWasDenied(uint64_t, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason);

    template <typename RequestType>
    FrameAuthorizationState& stateForRequest(RequestType&);

    void enumerateMediaDevicesForFrame(uint64_t userMediaID, uint64_t frameID, String userMediaDocumentOriginIdentifier, String topLevelDocumentOriginIdentifier);

    void stopCapture();
    void scheduleNextRejection();
    void rejectionTimerFired();
    void clearCachedState();

    void startedCaptureSession();
    void endedCaptureSession();

private:
    Ref<UserMediaPermissionRequestProxy> createRequest(uint64_t userMediaID, uint64_t frameID, const String&userMediaDocumentOriginIdentifier, const String& topLevelDocumentOriginIdentifier, const Vector<String>& audioDeviceUIDs, const Vector<String>& videoDeviceUIDs);
    void denyRequest(uint64_t userMediaID, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason, const String& invalidConstraint);

    void getUserMediaPermissionInfo(uint64_t userMediaID, uint64_t frameID, UserMediaPermissionCheckProxy::CompletionHandler&&, const String&&userMediaDocumentOriginIdentifier, const String&& topLevelDocumentOriginIdentifier);

    void syncWithWebCorePrefs() const;

    HashMap<uint64_t, RefPtr<UserMediaPermissionRequestProxy>> m_pendingUserMediaRequests;
    HashMap<uint64_t, RefPtr<UserMediaPermissionCheckProxy>> m_pendingDeviceRequests;
    HashMap<uint64_t, std::unique_ptr<FrameAuthorizationState>> m_frameStates;

    WebPageProxy& m_page;

    WebCore::Timer m_rejectionTimer;
    Vector<uint64_t> m_pendingRejections;
};

} // namespace WebKit

