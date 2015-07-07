/*
 * Copyright (C) 2013 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "WebSoupCustomProtocolRequestManagerClient.h"

#if ENABLE(CUSTOM_PROTOCOLS)

namespace WebKit {

bool WebSoupCustomProtocolRequestManagerClient::startLoading(WebSoupCustomProtocolRequestManager* soupRequestManager, uint64_t customProtocolID, const WebCore::ResourceRequest& request)
{
    if (!m_client.startLoading)
        return false;

    RefPtr<API::URLRequest> urlRequest = API::URLRequest::create(request);
    m_client.startLoading(toAPI(soupRequestManager), customProtocolID, toAPI(urlRequest.get()), m_client.base.clientInfo);
    return true;
}

void WebSoupCustomProtocolRequestManagerClient::stopLoading(WebSoupCustomProtocolRequestManager* soupRequestManager, uint64_t customProtocolID)
{
    if (m_client.stopLoading)
        m_client.stopLoading(toAPI(soupRequestManager), customProtocolID, m_client.base.clientInfo);
}

} // namespace WebKit

#endif // ENABLE(CUSTOM_PROTOCOLS)
