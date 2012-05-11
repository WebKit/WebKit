/*
 * Copyright (C) 2012 Igalia S.L.
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
#include "WebSoupRequestManagerProxy.h"

#include "WebContext.h"
#include "WebData.h"
#include "WebSoupRequestManagerMessages.h"

namespace WebKit {

PassRefPtr<WebSoupRequestManagerProxy> WebSoupRequestManagerProxy::create(WebContext* context)
{
    return adoptRef(new WebSoupRequestManagerProxy(context));
}

WebSoupRequestManagerProxy::WebSoupRequestManagerProxy(WebContext* context)
    : m_webContext(context)
{
}

WebSoupRequestManagerProxy::~WebSoupRequestManagerProxy()
{
}

void WebSoupRequestManagerProxy::invalidate()
{
}

void WebSoupRequestManagerProxy::initializeClient(const WKSoupRequestManagerClient* client)
{
    m_client.initialize(client);
}

void WebSoupRequestManagerProxy::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    didReceiveWebSoupRequestManagerProxyMessage(connection, messageID, arguments);
}

void WebSoupRequestManagerProxy::registerURIScheme(const String& scheme)
{
    ASSERT(m_webContext);
    m_webContext->sendToAllProcessesRelaunchingThemIfNecessary(Messages::WebSoupRequestManager::RegisterURIScheme(scheme));
}

void WebSoupRequestManagerProxy::didHandleURIRequest(const WebData* requestData, uint64_t contentLength, const String& mimeType, uint64_t requestID)
{
    ASSERT(m_webContext);
    m_webContext->sendToAllProcesses(Messages::WebSoupRequestManager::DidHandleURIRequest(requestData->dataReference(), contentLength, mimeType, requestID));
}

void WebSoupRequestManagerProxy::didReceiveURIRequestData(const WebData* requestData, uint64_t requestID)
{
    ASSERT(m_webContext);
    m_webContext->sendToAllProcesses(Messages::WebSoupRequestManager::DidReceiveURIRequestData(requestData->dataReference(), requestID));
}

void WebSoupRequestManagerProxy::didReceiveURIRequest(const String& uriString, uint64_t requestID)
{
    if (!m_client.didReceiveURIRequest(this, WebURL::create(uriString).get(), requestID))
        didHandleURIRequest(WebData::create(0, 0).get(), 0, String(), requestID);
}

} // namespace WebKit
