/*
 * Copyright (C) 2014 Igalia S.L.
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
#include "UserMediaPermissionRequestManagerProxy.h"

#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"

namespace WebKit {

UserMediaPermissionRequestManagerProxy::UserMediaPermissionRequestManagerProxy(WebPageProxy& page)
    : m_page(page)
{
}

void UserMediaPermissionRequestManagerProxy::invalidateRequests()
{
    for (auto& request : m_pendingRequests.values())
        request->invalidate();

    m_pendingRequests.clear();
}

PassRefPtr<UserMediaPermissionRequestProxy> UserMediaPermissionRequestManagerProxy::createRequest(uint64_t userMediaID, bool requiresAudio, bool requiresVideo)
{
    RefPtr<UserMediaPermissionRequestProxy> request = UserMediaPermissionRequestProxy::create(*this, userMediaID, requiresAudio, requiresVideo);
    m_pendingRequests.add(userMediaID, request.get());
    return request.release();
}

void UserMediaPermissionRequestManagerProxy::didReceiveUserMediaPermissionDecision(uint64_t userMediaID, bool allowed)
{
    if (!m_page.isValid())
        return;

    if (!m_pendingRequests.take(userMediaID))
        return;

#if ENABLE(MEDIA_STREAM)
    m_page.process().send(Messages::WebPage::DidReceiveUserMediaPermissionDecision(userMediaID, allowed), m_page.pageID());
#else
    UNUSED_PARAM(allowed);
#endif
}

} // namespace WebKit
