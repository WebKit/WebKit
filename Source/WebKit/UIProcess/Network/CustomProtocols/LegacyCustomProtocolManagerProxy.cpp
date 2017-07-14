/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "ChildProcessProxy.h"
#include "LegacyCustomProtocolManagerMessages.h"
#include "LegacyCustomProtocolManagerProxyMessages.h"
#include "WebProcessPool.h"
#include <WebCore/ResourceRequest.h>

namespace WebKit {

LegacyCustomProtocolManagerProxy::LegacyCustomProtocolManagerProxy(ChildProcessProxy* childProcessProxy, WebProcessPool& processPool)
    : m_childProcessProxy(childProcessProxy)
    , m_processPool(processPool)
{
    ASSERT(m_childProcessProxy);
    m_childProcessProxy->addMessageReceiver(Messages::LegacyCustomProtocolManagerProxy::messageReceiverName(), *this);
}

LegacyCustomProtocolManagerProxy::~LegacyCustomProtocolManagerProxy()
{
    m_childProcessProxy->removeMessageReceiver(Messages::LegacyCustomProtocolManagerProxy::messageReceiverName());
}

void LegacyCustomProtocolManagerProxy::startLoading(uint64_t customProtocolID, const WebCore::ResourceRequest& request)
{
    m_processPool.customProtocolManagerClient().startLoading(*this, customProtocolID, request);
}

void LegacyCustomProtocolManagerProxy::stopLoading(uint64_t customProtocolID)
{
    m_processPool.customProtocolManagerClient().stopLoading(*this, customProtocolID);
}

void LegacyCustomProtocolManagerProxy::processDidClose()
{
    m_processPool.customProtocolManagerClient().invalidate(*this);
}

void LegacyCustomProtocolManagerProxy::wasRedirectedToRequest(uint64_t customProtocolID, const WebCore::ResourceRequest& request, const WebCore::ResourceResponse& redirectResponse)
{
    m_childProcessProxy->send(Messages::LegacyCustomProtocolManager::WasRedirectedToRequest(customProtocolID, request, redirectResponse), 0);
}

void LegacyCustomProtocolManagerProxy::didReceiveResponse(uint64_t customProtocolID, const WebCore::ResourceResponse& response, uint32_t cacheStoragePolicy)
{
    m_childProcessProxy->send(Messages::LegacyCustomProtocolManager::DidReceiveResponse(customProtocolID, response, cacheStoragePolicy), 0);
}

void LegacyCustomProtocolManagerProxy::didLoadData(uint64_t customProtocolID, const IPC::DataReference& data)
{
    m_childProcessProxy->send(Messages::LegacyCustomProtocolManager::DidLoadData(customProtocolID, data), 0);
}

void LegacyCustomProtocolManagerProxy::didFailWithError(uint64_t customProtocolID, const WebCore::ResourceError& error)
{
    m_childProcessProxy->send(Messages::LegacyCustomProtocolManager::DidFailWithError(customProtocolID, error), 0);
}

void LegacyCustomProtocolManagerProxy::didFinishLoading(uint64_t customProtocolID)
{
    m_childProcessProxy->send(Messages::LegacyCustomProtocolManager::DidFinishLoading(customProtocolID), 0);
}

} // namespace WebKit
