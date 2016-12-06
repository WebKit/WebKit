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

#ifndef UserMediaPermissionRequestManager_h
#define UserMediaPermissionRequestManager_h

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

class UserMediaPermissionRequestManager
    : private WebCore::MediaCanStartListener {
public:
    explicit UserMediaPermissionRequestManager(WebPage&);
    ~UserMediaPermissionRequestManager();

    void startUserMediaRequest(WebCore::UserMediaRequest&);
    void cancelUserMediaRequest(WebCore::UserMediaRequest&);
    void userMediaAccessWasGranted(uint64_t, const String& audioDeviceUID, const String& videoDeviceUID);
    void userMediaAccessWasDenied(uint64_t, WebCore::UserMediaRequest::MediaAccessDenialReason, const String&);

    void enumerateMediaDevices(WebCore::MediaDevicesEnumerationRequest&);
    void cancelMediaDevicesEnumeration(WebCore::MediaDevicesEnumerationRequest&);
    void didCompleteMediaDeviceEnumeration(uint64_t, const Vector<WebCore::CaptureDevice>& deviceList, const String& deviceIdentifierHashSalt, bool originHasPersistentAccess);

    void grantUserMediaDeviceSandboxExtensions(const MediaDeviceSandboxExtensions&);
    void revokeUserMediaDeviceSandboxExtensions(const Vector<String>&);

private:
    void sendUserMediaRequest(WebCore::UserMediaRequest&);

    // WebCore::MediaCanStartListener
    void mediaCanStart(WebCore::Document&) override;

    void removeMediaRequestFromMaps(WebCore::UserMediaRequest&);

    WebPage& m_page;

    HashMap<uint64_t, RefPtr<WebCore::UserMediaRequest>> m_idToUserMediaRequestMap;
    HashMap<RefPtr<WebCore::UserMediaRequest>, uint64_t> m_userMediaRequestToIDMap;

    HashMap<uint64_t, RefPtr<WebCore::MediaDevicesEnumerationRequest>> m_idToMediaDevicesEnumerationRequestMap;
    HashMap<RefPtr<WebCore::MediaDevicesEnumerationRequest>, uint64_t> m_mediaDevicesEnumerationRequestToIDMap;

    HashMap<String, RefPtr<SandboxExtension>> m_userMediaDeviceSandboxExtensions;

    HashMap<RefPtr<WebCore::Document>, Vector<RefPtr<WebCore::UserMediaRequest>>> m_blockedRequests;
};

} // namespace WebKit

#endif // ENABLE(MEDIA_STREAM)

#endif // UserMediaPermissionRequestManager_h
