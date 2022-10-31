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

#include "IdentifierTypes.h"
#include "SandboxExtension.h"
#include <WebCore/MediaCanStartListener.h>
#include <WebCore/MediaConstraints.h>
#include <WebCore/RealtimeMediaSourceCenter.h>
#include <WebCore/UserMediaClient.h>
#include <WebCore/UserMediaRequest.h>
#include <wtf/HashMap.h>
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>

namespace WebKit {

class WebPage;

class UserMediaPermissionRequestManager : public WebCore::MediaCanStartListener
#if USE(GSTREAMER)
                                        , public WebCore::RealtimeMediaSourceCenter::Observer
#endif
{
    WTF_MAKE_FAST_ALLOCATED;
public:
    using WebCore::MediaCanStartListener::weakPtrFactory;
    using WebCore::MediaCanStartListener::WeakValueType;
    using WebCore::MediaCanStartListener::WeakPtrImplType;

    explicit UserMediaPermissionRequestManager(WebPage&);
    ~UserMediaPermissionRequestManager() = default;

    void startUserMediaRequest(WebCore::UserMediaRequest&);
    void cancelUserMediaRequest(WebCore::UserMediaRequest&);
    void userMediaAccessWasGranted(WebCore::UserMediaRequestIdentifier, WebCore::CaptureDevice&& audioDevice, WebCore::CaptureDevice&& videoDevice, WebCore::MediaDeviceHashSalts&&, CompletionHandler<void()>&&);
    void userMediaAccessWasDenied(WebCore::UserMediaRequestIdentifier, WebCore::UserMediaRequest::MediaAccessDenialReason, String&&);

    void enumerateMediaDevices(WebCore::Document&, CompletionHandler<void(const Vector<WebCore::CaptureDevice>&, WebCore::MediaDeviceHashSalts&&)>&&);

    WebCore::UserMediaClient::DeviceChangeObserverToken addDeviceChangeObserver(WTF::Function<void()>&&);
    void removeDeviceChangeObserver(WebCore::UserMediaClient::DeviceChangeObserverToken);

    void captureDevicesChanged();

private:
#if USE(GSTREAMER)
    // WebCore::RealtimeMediaSourceCenter::Observer
    void devicesChanged() final;
#endif

    void sendUserMediaRequest(WebCore::UserMediaRequest&);

    // WebCore::MediaCanStartListener
    void mediaCanStart(WebCore::Document&) final;

    WebPage& m_page;

    HashMap<WebCore::UserMediaRequestIdentifier, Ref<WebCore::UserMediaRequest>> m_ongoingUserMediaRequests;
    HashMap<RefPtr<WebCore::Document>, Vector<Ref<WebCore::UserMediaRequest>>> m_pendingUserMediaRequests;

    HashMap<WebCore::UserMediaClient::DeviceChangeObserverToken, Function<void()>> m_deviceChangeObserverMap;
    bool m_monitoringDeviceChange { false };

#if USE(GSTREAMER)
    enum class ShouldNotify : bool { No, Yes };
    void updateCaptureDevices(ShouldNotify);

    Vector<WebCore::CaptureDevice> m_captureDevices;
#endif
};

} // namespace WebKit

namespace WTF {

} // namespace WTF

#endif // ENABLE(MEDIA_STREAM)
