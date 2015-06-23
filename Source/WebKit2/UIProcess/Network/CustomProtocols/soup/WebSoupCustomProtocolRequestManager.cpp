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

#include "APIData.h"
#include "CustomProtocolManagerMessages.h"
#include "WebProcessPool.h"
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>

#if PLATFORM(GTK)
#include <WebCore/ErrorsGtk.h>
#elif PLATFORM(EFL)
#include <WebCore/ErrorsEfl.h>
#endif

namespace WebKit {

const char* WebSoupCustomProtocolRequestManager::supplementName()
{
    return "WebSoupCustomProtocolRequestManager";
}

Ref<WebSoupCustomProtocolRequestManager> WebSoupCustomProtocolRequestManager::create(WebProcessPool* processPool)
{
    return adoptRef(*new WebSoupCustomProtocolRequestManager(processPool));
}

WebSoupCustomProtocolRequestManager::WebSoupCustomProtocolRequestManager(WebProcessPool* processPool)
    : WebContextSupplement(processPool)
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
void WebSoupCustomProtocolRequestManager::processPoolDestroyed()
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
    ASSERT(!scheme.isNull());
    if (m_registeredSchemes.contains(scheme))
        return;

    if (!processPool())
        return;

    processPool()->registerSchemeForCustomProtocol(scheme);

    m_registeredSchemes.append(scheme);
}

void WebSoupCustomProtocolRequestManager::unregisterSchemeForCustomProtocol(const String& scheme)
{
    if (!processPool())
        return;

    processPool()->unregisterSchemeForCustomProtocol(scheme);

    bool removed = m_registeredSchemes.removeFirst(scheme);
    ASSERT_UNUSED(removed, removed);
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
    if (!processPool())
        return;

    processPool()->networkingProcessConnection()->send(Messages::CustomProtocolManager::DidReceiveResponse(customProtocolID, response, 0), 0);
}

void WebSoupCustomProtocolRequestManager::didLoadData(uint64_t customProtocolID, const API::Data* data)
{
    if (!processPool())
        return;

    processPool()->networkingProcessConnection()->send(Messages::CustomProtocolManager::DidLoadData(customProtocolID, data->dataReference()), 0);
}

void WebSoupCustomProtocolRequestManager::didFailWithError(uint64_t customProtocolID, const WebCore::ResourceError& error)
{
    if (!processPool())
        return;

    processPool()->networkingProcessConnection()->send(Messages::CustomProtocolManager::DidFailWithError(customProtocolID, error), 0);
}

void WebSoupCustomProtocolRequestManager::didFinishLoading(uint64_t customProtocolID)
{
    if (!processPool())
        return;

    processPool()->networkingProcessConnection()->send(Messages::CustomProtocolManager::DidFinishLoading(customProtocolID), 0);
}

} // namespace WebKit
