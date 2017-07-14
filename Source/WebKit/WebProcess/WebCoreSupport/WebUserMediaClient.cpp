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

#include "config.h"
#include "WebUserMediaClient.h"

#if ENABLE(MEDIA_STREAM)

#include "UserMediaPermissionRequestManager.h"
#include "WebPage.h"
#include <WebCore/UserMediaController.h>
#include <WebCore/UserMediaRequest.h>

using namespace WebCore;

namespace WebKit {

WebUserMediaClient::WebUserMediaClient(WebPage& page)
    : m_page(page)
{
}

void WebUserMediaClient::pageDestroyed()
{
    delete this;
}

void WebUserMediaClient::requestUserMediaAccess(UserMediaRequest& request)
{
    m_page.userMediaPermissionRequestManager().startUserMediaRequest(request);
}

void WebUserMediaClient::cancelUserMediaAccessRequest(UserMediaRequest& request)
{
    m_page.userMediaPermissionRequestManager().cancelUserMediaRequest(request);
}

void WebUserMediaClient::enumerateMediaDevices(MediaDevicesEnumerationRequest& request)
{
    m_page.userMediaPermissionRequestManager().enumerateMediaDevices(request);
}

void WebUserMediaClient::cancelMediaDevicesEnumerationRequest(MediaDevicesEnumerationRequest& request)
{
    m_page.userMediaPermissionRequestManager().cancelMediaDevicesEnumeration(request);
}

} // namespace WebKit;

#endif // MEDIA_STREAM
