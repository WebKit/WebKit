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

#include "config.h"
#include "WebUserMediaClient.h"

#if ENABLE(MEDIA_STREAM)

#include "MessageSenderInlines.h"
#include "UserMediaPermissionRequestManager.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include <WebCore/UserMediaController.h>
#include <WebCore/UserMediaRequest.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebUserMediaClient);

WebUserMediaClient::WebUserMediaClient(WebPage& page)
    : m_page(page)
{
}

Ref<WebPage> WebUserMediaClient::protectedPage() const
{
    return m_page.get();
}

void WebUserMediaClient::pageDestroyed()
{
    delete this;
}

void WebUserMediaClient::requestUserMediaAccess(UserMediaRequest& request)
{
    protectedPage()->protectedUserMediaPermissionRequestManager()->startUserMediaRequest(request);
}

void WebUserMediaClient::cancelUserMediaAccessRequest(UserMediaRequest& request)
{
    protectedPage()->protectedUserMediaPermissionRequestManager()->cancelUserMediaRequest(request);
}

void WebUserMediaClient::enumerateMediaDevices(Document& document, UserMediaClient::EnumerateDevicesCallback&& completionHandler)
{
    protectedPage()->protectedUserMediaPermissionRequestManager()->enumerateMediaDevices(document, WTFMove(completionHandler));
}

WebUserMediaClient::DeviceChangeObserverToken WebUserMediaClient::addDeviceChangeObserver(WTF::Function<void()>&& observer)
{
    return protectedPage()->protectedUserMediaPermissionRequestManager()->addDeviceChangeObserver(WTFMove(observer));
}

void WebUserMediaClient::removeDeviceChangeObserver(DeviceChangeObserverToken token)
{
    protectedPage()->protectedUserMediaPermissionRequestManager()->removeDeviceChangeObserver(token);
}

void WebUserMediaClient::updateCaptureState(const WebCore::Document& document, bool isActive, WebCore::MediaProducerMediaCaptureKind kind, CompletionHandler<void(std::optional<WebCore::Exception>&&)>&& completionHandler)
{
    protectedPage()->protectedUserMediaPermissionRequestManager()->updateCaptureState(document, isActive, kind, WTFMove(completionHandler));
}

void WebUserMediaClient::setShouldListenToVoiceActivity(bool shouldListen)
{
    protectedPage()->send(Messages::WebPageProxy::SetShouldListenToVoiceActivity { shouldListen });
}

} // namespace WebKit;

#endif // MEDIA_STREAM
