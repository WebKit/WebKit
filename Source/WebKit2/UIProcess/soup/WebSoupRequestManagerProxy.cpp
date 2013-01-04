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
#include "WebSoupRequestManagerProxyMessages.h"

namespace WebKit {

const AtomicString& WebSoupRequestManagerProxy::supplementName()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("WebSoupRequestManagerProxy", AtomicString::ConstructFromLiteral));
    return name;
}

PassRefPtr<WebSoupRequestManagerProxy> WebSoupRequestManagerProxy::create(WebContext* context)
{
    return adoptRef(new WebSoupRequestManagerProxy(context));
}

WebSoupRequestManagerProxy::WebSoupRequestManagerProxy(WebContext* context)
    : WebContextSupplement(context)
    , m_loadFailed(false)
{
    WebContextSupplement::context()->addMessageReceiver(Messages::WebSoupRequestManagerProxy::messageReceiverName(), this);
}

WebSoupRequestManagerProxy::~WebSoupRequestManagerProxy()
{
}

void WebSoupRequestManagerProxy::initializeClient(const WKSoupRequestManagerClient* client)
{
    m_client.initialize(client);
}

// WebContextSupplement

void WebSoupRequestManagerProxy::contextDestroyed()
{
}

void WebSoupRequestManagerProxy::processDidClose(WebProcessProxy*)
{
}

void WebSoupRequestManagerProxy::refWebContextSupplement()
{
    APIObject::ref();
}

void WebSoupRequestManagerProxy::derefWebContextSupplement()
{
    APIObject::deref();
}

// CoreIPC::MessageReceiver

void WebSoupRequestManagerProxy::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::MessageDecoder& decoder)
{
    didReceiveWebSoupRequestManagerProxyMessage(connection, messageID, decoder);
}

void WebSoupRequestManagerProxy::registerURIScheme(const String& scheme)
{
    if (!context())
        return;

    context()->sendToAllProcessesRelaunchingThemIfNecessary(Messages::WebSoupRequestManager::RegisterURIScheme(scheme));

    ASSERT(!m_registeredURISchemes.contains(scheme));
    m_registeredURISchemes.append(scheme);
}

void WebSoupRequestManagerProxy::didHandleURIRequest(const WebData* requestData, uint64_t contentLength, const String& mimeType, uint64_t requestID)
{
    if (!context())
        return;

    context()->sendToAllProcesses(Messages::WebSoupRequestManager::DidHandleURIRequest(requestData->dataReference(), contentLength, mimeType, requestID));
}

void WebSoupRequestManagerProxy::didReceiveURIRequestData(const WebData* requestData, uint64_t requestID)
{
    if (!context())
        return;

    if (m_loadFailed)
        return;

    context()->sendToAllProcesses(Messages::WebSoupRequestManager::DidReceiveURIRequestData(requestData->dataReference(), requestID));
}

void WebSoupRequestManagerProxy::didReceiveURIRequest(const String& uriString, WebPageProxy* initiaingPage, uint64_t requestID)
{
    if (!m_client.didReceiveURIRequest(this, WebURL::create(uriString).get(), initiaingPage, requestID))
        didHandleURIRequest(WebData::create(0, 0).get(), 0, String(), requestID);
}

void WebSoupRequestManagerProxy::didFailToLoadURIRequest(uint64_t requestID)
{
    m_loadFailed = true;
    m_client.didFailToLoadURIRequest(this, requestID);
}

} // namespace WebKit
