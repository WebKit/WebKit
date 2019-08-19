/*
 * Copyright (C) 2016 Igalia S.L.
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
#include "WebKitCustomProtocolManagerClient.h"

#include "APICustomProtocolManagerClient.h"
#include "LegacyCustomProtocolManagerProxy.h"
#include "WebKitWebContextPrivate.h"
#include "WebProcessPool.h"

using namespace WebCore;
using namespace WebKit;

class CustomProtocolManagerClient final : public API::CustomProtocolManagerClient {
public:
    explicit CustomProtocolManagerClient(WebKitWebContext* webContext)
        : m_webContext(webContext)
    {
    }

private:
    void startLoading(LegacyCustomProtocolManagerProxy& manager, uint64_t customProtocolID, const ResourceRequest& request) override
    {
        webkitWebContextStartLoadingCustomProtocol(m_webContext, customProtocolID, request, manager);
    }

    void stopLoading(LegacyCustomProtocolManagerProxy&, uint64_t customProtocolID) override
    {
        webkitWebContextStopLoadingCustomProtocol(m_webContext, customProtocolID);
    }

    void invalidate(LegacyCustomProtocolManagerProxy& manager) override
    {
        webkitWebContextInvalidateCustomProtocolRequests(m_webContext, manager);
    }

    WebKitWebContext* m_webContext;
};

void attachCustomProtocolManagerClientToContext(WebKitWebContext* webContext)
{
    webkitWebContextGetProcessPool(webContext).setLegacyCustomProtocolManagerClient(makeUnique<CustomProtocolManagerClient>(webContext));
}
