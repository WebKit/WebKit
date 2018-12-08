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

#if ENABLE(MEDIA_STREAM)

#include "MediaDeviceSandboxExtensions.h"
#include "SandboxExtension.h"
#include <WebCore/MediaCanStartListener.h>
#include <WebCore/MediaConstraints.h>
#include <WebCore/MediaDevicesEnumerationRequest.h>
#include <WebCore/UserMediaClient.h>
#include <WebCore/UserMediaRequest.h>
#include <wtf/HashMap.h>
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>

namespace WebKit {

class WebPage;

class UserMediaPermissionRequestManager : public CanMakeWeakPtr<UserMediaPermissionRequestManager>, private WebCore::MediaCanStartListener {
public:
    explicit UserMediaPermissionRequestManager(WebPage&);
    ~UserMediaPermissionRequestManager();

    void startUserMediaRequest(WebCore::UserMediaRequest&);
    void cancelUserMediaRequest(WebCore::UserMediaRequest&);
    void userMediaAccessWasGranted(uint64_t, WebCore::CaptureDevice&& audioDevice, WebCore::CaptureDevice&& videoDevice, String&& deviceIdentifierHashSalt);
    void userMediaAccessWasDenied(uint64_t, WebCore::UserMediaRequest::MediaAccessDenialReason, String&&);

    void enumerateMediaDevices(WebCore::MediaDevicesEnumerationRequest&);
    void cancelMediaDevicesEnumeration(WebCore::MediaDevicesEnumerationRequest&);
    void didCompleteMediaDeviceEnumeration(uint64_t, const Vector<WebCore::CaptureDevice>& deviceList, String&& deviceIdentifierHashSalt, bool originHasPersistentAccess);

    void grantUserMediaDeviceSandboxExtensions(MediaDeviceSandboxExtensions&&);
    void revokeUserMediaDeviceSandboxExtensions(const Vector<String>&);

    WebCore::UserMediaClient::DeviceChangeObserverToken addDeviceChangeObserver(WTF::Function<void()>&&);
    void removeDeviceChangeObserver(WebCore::UserMediaClient::DeviceChangeObserverToken);

    void captureDevicesChanged();

private:
    void sendUserMediaRequest(WebCore::UserMediaRequest&);

    // WebCore::MediaCanStartListener
    void mediaCanStart(WebCore::Document&) final;

    void removeMediaRequestFromMaps(WebCore::UserMediaRequest&);

    WebPage& m_page;

    HashMap<uint64_t, RefPtr<WebCore::UserMediaRequest>> m_idToUserMediaRequestMap;
    HashMap<RefPtr<WebCore::UserMediaRequest>, uint64_t> m_userMediaRequestToIDMap;
    HashMap<RefPtr<WebCore::Document>, Vector<RefPtr<WebCore::UserMediaRequest>>> m_blockedUserMediaRequests;

    HashMap<uint64_t, RefPtr<WebCore::MediaDevicesEnumerationRequest>> m_idToMediaDevicesEnumerationRequestMap;
    HashMap<RefPtr<WebCore::MediaDevicesEnumerationRequest>, uint64_t> m_mediaDevicesEnumerationRequestToIDMap;

    HashMap<String, RefPtr<SandboxExtension>> m_userMediaDeviceSandboxExtensions;

    HashMap<WebCore::UserMediaClient::DeviceChangeObserverToken, WTF::Function<void()>> m_deviceChangeObserverMap;
    bool m_monitoringDeviceChange { false };
};

} // namespace WebKit

namespace WTF {

} // namespace WTF

#endif // ENABLE(MEDIA_STREAM)
