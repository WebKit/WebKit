/*
 * Copyright (C) 2014 Igalia S.L.
 * Copyright (C) 2016-2025 Apple Inc. All rights reserved.
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

void WebUserMediaClient::requestUserMediaAccess(UserMediaRequest& request)
{
    if (RefPtr page = m_page.get())
        page->userMediaPermissionRequestManager().startUserMediaRequest(request);
}

void WebUserMediaClient::cancelUserMediaAccessRequest(UserMediaRequest& request)
{
    if (RefPtr page = m_page.get())
        page->userMediaPermissionRequestManager().cancelUserMediaRequest(request);
}

void WebUserMediaClient::enumerateMediaDevices(Document& document, UserMediaClient::EnumerateDevicesCallback&& completionHandler)
{
    if (RefPtr page = m_page.get())
        page->userMediaPermissionRequestManager().enumerateMediaDevices(document, WTFMove(completionHandler));
}

WebUserMediaClient::DeviceChangeObserverToken WebUserMediaClient::addDeviceChangeObserver(WTF::Function<void()>&& observer)
{
    if (RefPtr page = m_page.get())
        return page->userMediaPermissionRequestManager().addDeviceChangeObserver(WTFMove(observer));
    return DeviceChangeObserverToken { 0 };
}

void WebUserMediaClient::removeDeviceChangeObserver(DeviceChangeObserverToken token)
{
    if (RefPtr page = m_page.get())
        page->userMediaPermissionRequestManager().removeDeviceChangeObserver(token);
}

void WebUserMediaClient::updateCaptureState(const WebCore::Document& document, bool isActive, WebCore::MediaProducerMediaCaptureKind kind, CompletionHandler<void(std::optional<WebCore::Exception>&&)>&& completionHandler)
{
    if (RefPtr page = m_page.get())
        page->userMediaPermissionRequestManager().updateCaptureState(document, isActive, kind, WTFMove(completionHandler));
}

void WebUserMediaClient::setShouldListenToVoiceActivity(bool shouldListen)
{
    if (RefPtr page = m_page.get())
        page->send(Messages::WebPageProxy::SetShouldListenToVoiceActivity { shouldListen });
}

} // namespace WebKit;

#endif // MEDIA_STREAM
