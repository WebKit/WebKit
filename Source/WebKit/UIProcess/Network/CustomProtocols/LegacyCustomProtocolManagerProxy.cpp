/*
 * Copyright (C) 2012-2023 Apple Inc. All rights reserved.
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
#include "LegacyCustomProtocolManagerProxy.h"

#include "APICustomProtocolManagerClient.h"
#include "LegacyCustomProtocolManagerMessages.h"
#include "LegacyCustomProtocolManagerProxyMessages.h"
#include "NetworkProcessProxy.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcessPool.h"
#include <WebCore/ResourceRequest.h>

namespace WebKit {

LegacyCustomProtocolManagerProxy::LegacyCustomProtocolManagerProxy(NetworkProcessProxy& networkProcessProxy)
    : m_networkProcessProxy(networkProcessProxy)
{
    networkProcessProxy.addMessageReceiver(Messages::LegacyCustomProtocolManagerProxy::messageReceiverName(), *this);
}

void LegacyCustomProtocolManagerProxy::ref() const
{
    m_networkProcessProxy->ref();
}

void LegacyCustomProtocolManagerProxy::deref() const
{
    m_networkProcessProxy->deref();
}

Ref<NetworkProcessProxy> LegacyCustomProtocolManagerProxy::protectedProcess()
{
    return m_networkProcessProxy.get();
}

LegacyCustomProtocolManagerProxy::~LegacyCustomProtocolManagerProxy()
{
    Ref<NetworkProcessProxy> networkProcessProxy = m_networkProcessProxy.get();
    networkProcessProxy->removeMessageReceiver(Messages::LegacyCustomProtocolManagerProxy::messageReceiverName());
    invalidate();
}

void LegacyCustomProtocolManagerProxy::startLoading(LegacyCustomProtocolID customProtocolID, const WebCore::ResourceRequest& request)
{
    protectedProcess()->customProtocolManagerClient().startLoading(*this, customProtocolID, request);
}

void LegacyCustomProtocolManagerProxy::stopLoading(LegacyCustomProtocolID customProtocolID)
{
    protectedProcess()->customProtocolManagerClient().stopLoading(*this, customProtocolID);
}

void LegacyCustomProtocolManagerProxy::invalidate()
{
    Ref<NetworkProcessProxy> networkProcessProxy = m_networkProcessProxy.get();
    networkProcessProxy->customProtocolManagerClient().invalidate(*this);
}

void LegacyCustomProtocolManagerProxy::wasRedirectedToRequest(LegacyCustomProtocolID customProtocolID, const WebCore::ResourceRequest& request, const WebCore::ResourceResponse& redirectResponse)
{
    protectedProcess()->send(Messages::LegacyCustomProtocolManager::WasRedirectedToRequest(customProtocolID, request, redirectResponse), 0);
}

void LegacyCustomProtocolManagerProxy::didReceiveResponse(LegacyCustomProtocolID customProtocolID, const WebCore::ResourceResponse& response, CacheStoragePolicy cacheStoragePolicy)
{
    protectedProcess()->send(Messages::LegacyCustomProtocolManager::DidReceiveResponse(customProtocolID, response, cacheStoragePolicy), 0);
}

void LegacyCustomProtocolManagerProxy::didLoadData(LegacyCustomProtocolID customProtocolID, std::span<const uint8_t> data)
{
    protectedProcess()->send(Messages::LegacyCustomProtocolManager::DidLoadData(customProtocolID, data), 0);
}

void LegacyCustomProtocolManagerProxy::didFailWithError(LegacyCustomProtocolID customProtocolID, const WebCore::ResourceError& error)
{
    protectedProcess()->send(Messages::LegacyCustomProtocolManager::DidFailWithError(customProtocolID, error), 0);
}

void LegacyCustomProtocolManagerProxy::didFinishLoading(LegacyCustomProtocolID customProtocolID)
{
    protectedProcess()->send(Messages::LegacyCustomProtocolManager::DidFinishLoading(customProtocolID), 0);
}

} // namespace WebKit
