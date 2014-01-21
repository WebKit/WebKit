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
#include "WebSoupCustomProtocolRequestManager.h"

#if ENABLE(CUSTOM_PROTOCOLS)

#include "APIData.h"
#include "CustomProtocolManagerMessages.h"
#include "WebContext.h"
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>

#if PLATFORM(GTK)
#include <WebCore/ErrorsGtk.h>
#endif

namespace WebKit {

const char* WebSoupCustomProtocolRequestManager::supplementName()
{
    return "WebSoupCustomProtocolRequestManager";
}

PassRefPtr<WebSoupCustomProtocolRequestManager> WebSoupCustomProtocolRequestManager::create(WebContext* context)
{
    return adoptRef(new WebSoupCustomProtocolRequestManager(context));
}

WebSoupCustomProtocolRequestManager::WebSoupCustomProtocolRequestManager(WebContext* context)
    : WebContextSupplement(context)
{
}

WebSoupCustomProtocolRequestManager::~WebSoupCustomProtocolRequestManager()
{
}

void WebSoupCustomProtocolRequestManager::initializeClient(const WKSoupCustomProtocolRequestManagerClientBase* client)
{
    m_client.initialize(client);
}

// WebContextSupplement
void WebSoupCustomProtocolRequestManager::contextDestroyed()
{
}

void WebSoupCustomProtocolRequestManager::processDidClose(WebProcessProxy*)
{
}

void WebSoupCustomProtocolRequestManager::refWebContextSupplement()
{
    API::Object::ref();
}

void WebSoupCustomProtocolRequestManager::derefWebContextSupplement()
{
    API::Object::deref();
}

void WebSoupCustomProtocolRequestManager::registerSchemeForCustomProtocol(const String& scheme)
{
    if (!context())
        return;

    context()->registerSchemeForCustomProtocol(scheme);

    ASSERT(!m_registeredSchemes.contains(scheme));
    m_registeredSchemes.append(scheme);
}

void WebSoupCustomProtocolRequestManager::unregisterSchemeForCustomProtocol(const String& scheme)
{
    if (!context())
        return;

    context()->unregisterSchemeForCustomProtocol(scheme);

    ASSERT(m_registeredSchemes.contains(scheme));
    m_registeredSchemes.remove(m_registeredSchemes.find(scheme));
}

void WebSoupCustomProtocolRequestManager::startLoading(uint64_t customProtocolID, const WebCore::ResourceRequest& request)
{
    if (!m_client.startLoading(this, customProtocolID, request))
        didFailWithError(customProtocolID, WebCore::cannotShowURLError(request));
}

void WebSoupCustomProtocolRequestManager::stopLoading(uint64_t customProtocolID)
{
    m_client.stopLoading(this, customProtocolID);
}

void WebSoupCustomProtocolRequestManager::didReceiveResponse(uint64_t customProtocolID, const WebCore::ResourceResponse& response)
{
    if (!context())
        return;

    context()->networkingProcessConnection()->send(Messages::CustomProtocolManager::DidReceiveResponse(customProtocolID, response, 0), 0);
}

void WebSoupCustomProtocolRequestManager::didLoadData(uint64_t customProtocolID, const API::Data* data)
{
    if (!context())
        return;

    context()->networkingProcessConnection()->send(Messages::CustomProtocolManager::DidLoadData(customProtocolID, data->dataReference()), 0);
}

void WebSoupCustomProtocolRequestManager::didFailWithError(uint64_t customProtocolID, const WebCore::ResourceError& error)
{
    if (!context())
        return;

    context()->networkingProcessConnection()->send(Messages::CustomProtocolManager::DidFailWithError(customProtocolID, error), 0);
}

void WebSoupCustomProtocolRequestManager::didFinishLoading(uint64_t customProtocolID)
{
    if (!context())
        return;

    context()->networkingProcessConnection()->send(Messages::CustomProtocolManager::DidFinishLoading(customProtocolID), 0);
}

} // namespace WebKit

#endif // ENABLE(CUSTOM_PROTOCOLS)
