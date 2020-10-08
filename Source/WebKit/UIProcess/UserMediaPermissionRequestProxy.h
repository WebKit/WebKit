/*
 * Copyright (C) 2014 Igalia S.L.
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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

#include "APIObject.h"
#include <WebCore/CaptureDevice.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/MediaStreamRequest.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class SecurityOrigin;
}

namespace WebKit {

class UserMediaPermissionRequestManagerProxy;

class UserMediaPermissionRequestProxy : public API::ObjectImpl<API::Object::Type::UserMediaPermissionRequest> {
public:
    static Ref<UserMediaPermissionRequestProxy> create(UserMediaPermissionRequestManagerProxy& manager, uint64_t userMediaID, WebCore::FrameIdentifier mainFrameID, WebCore::FrameIdentifier frameID, Ref<WebCore::SecurityOrigin>&& userMediaDocumentOrigin, Ref<WebCore::SecurityOrigin>&& topLevelDocumentOrigin, Vector<WebCore::CaptureDevice>&& audioDevices, Vector<WebCore::CaptureDevice>&& videoDevices, WebCore::MediaStreamRequest&& request)
    {
        return adoptRef(*new UserMediaPermissionRequestProxy(manager, userMediaID, mainFrameID, frameID, WTFMove(userMediaDocumentOrigin), WTFMove(topLevelDocumentOrigin), WTFMove(audioDevices), WTFMove(videoDevices), WTFMove(request)));
    }

    void allow(const String& audioDeviceUID, const String& videoDeviceUID);
    void allow();

    enum class UserMediaAccessDenialReason { NoConstraints, UserMediaDisabled, NoCaptureDevices, InvalidConstraint, HardwareError, PermissionDenied, OtherFailure };
    void deny(UserMediaAccessDenialReason = UserMediaAccessDenialReason::UserMediaDisabled);

    void doDefaultAction();

    void invalidate();
    bool isPending() const { return m_manager; }

    bool requiresAudioCapture() const { return m_eligibleAudioDevices.size(); }
    bool requiresVideoCapture() const { return !requiresDisplayCapture() && m_eligibleVideoDevices.size(); }
    bool requiresDisplayCapture() const { return m_request.type == WebCore::MediaStreamRequest::Type::DisplayMedia && m_eligibleVideoDevices.size(); }

    void setEligibleVideoDeviceUIDs(Vector<WebCore::CaptureDevice>&& devices) { m_eligibleVideoDevices = WTFMove(devices); }
    void setEligibleAudioDeviceUIDs(Vector<WebCore::CaptureDevice>&& devices) { m_eligibleAudioDevices = WTFMove(devices); }

    Vector<String> videoDeviceUIDs() const;
    Vector<String> audioDeviceUIDs() const;
    bool hasAudioDevice() const { return !m_eligibleAudioDevices.isEmpty(); }
    bool hasVideoDevice() const { return !m_eligibleVideoDevices.isEmpty(); }

    bool hasPersistentAccess() const { return m_hasPersistentAccess; }
    void setHasPersistentAccess() { m_hasPersistentAccess = true; }

    uint64_t userMediaID() const { return m_userMediaID; }
    WebCore::FrameIdentifier mainFrameID() const { return m_mainFrameID; }
    WebCore::FrameIdentifier frameID() const { return m_frameID; }

    WebCore::SecurityOrigin& topLevelDocumentSecurityOrigin() { return m_topLevelDocumentSecurityOrigin.get(); }
    WebCore::SecurityOrigin& userMediaDocumentSecurityOrigin() { return m_userMediaDocumentSecurityOrigin.get(); }
    const WebCore::SecurityOrigin& topLevelDocumentSecurityOrigin() const { return m_topLevelDocumentSecurityOrigin.get(); }
    const WebCore::SecurityOrigin& userMediaDocumentSecurityOrigin() const { return m_userMediaDocumentSecurityOrigin.get(); }

    const WebCore::MediaStreamRequest& userRequest() const { return m_request; }

    WebCore::MediaStreamRequest::Type requestType() const { return m_request.type; }

    void setDeviceIdentifierHashSalt(String&& salt) { m_deviceIdentifierHashSalt = WTFMove(salt); }
    const String& deviceIdentifierHashSalt() const { return m_deviceIdentifierHashSalt; }

    WebCore::CaptureDevice audioDevice() const { return m_eligibleAudioDevices.isEmpty() ? WebCore::CaptureDevice { } : m_eligibleAudioDevices[0]; }
    WebCore::CaptureDevice videoDevice() const { return m_eligibleVideoDevices.isEmpty() ? WebCore::CaptureDevice { } : m_eligibleVideoDevices[0]; }

#if ENABLE(MEDIA_STREAM)
    bool isUserGesturePriviledged() const { return m_request.isUserGesturePriviledged; }
#endif

private:
    UserMediaPermissionRequestProxy(UserMediaPermissionRequestManagerProxy&, uint64_t userMediaID, WebCore::FrameIdentifier mainFrameID, WebCore::FrameIdentifier, Ref<WebCore::SecurityOrigin>&& userMediaDocumentOrigin, Ref<WebCore::SecurityOrigin>&& topLevelDocumentOrigin, Vector<WebCore::CaptureDevice>&& audioDevices, Vector<WebCore::CaptureDevice>&& videoDevices, WebCore::MediaStreamRequest&&);

    UserMediaPermissionRequestManagerProxy* m_manager;
    uint64_t m_userMediaID;
    WebCore::FrameIdentifier m_mainFrameID;
    WebCore::FrameIdentifier m_frameID;
    Ref<WebCore::SecurityOrigin> m_userMediaDocumentSecurityOrigin;
    Ref<WebCore::SecurityOrigin> m_topLevelDocumentSecurityOrigin;
    Vector<WebCore::CaptureDevice> m_eligibleVideoDevices;
    Vector<WebCore::CaptureDevice> m_eligibleAudioDevices;
    WebCore::MediaStreamRequest m_request;
    bool m_hasPersistentAccess { false };
    String m_deviceIdentifierHashSalt;
};

String convertEnumerationToString(UserMediaPermissionRequestProxy::UserMediaAccessDenialReason);

} // namespace WebKit

namespace WTF {

template<typename Type>
struct LogArgument;

template <>
struct LogArgument<WebKit::UserMediaPermissionRequestProxy::UserMediaAccessDenialReason> {
    static String toString(const WebKit::UserMediaPermissionRequestProxy::UserMediaAccessDenialReason type)
    {
        return convertEnumerationToString(type);
    }
};

}; // namespace WTF

