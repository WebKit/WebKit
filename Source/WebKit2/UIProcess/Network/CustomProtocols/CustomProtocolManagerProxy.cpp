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
#include "CustomProtocolManagerProxy.h"

#include "APICustomProtocolManagerClient.h"
#include "ChildProcessProxy.h"
#include "CustomProtocolManagerMessages.h"
#include "CustomProtocolManagerProxyMessages.h"
#include "WebProcessPool.h"
#include <WebCore/ResourceRequest.h>

namespace WebKit {

CustomProtocolManagerProxy::CustomProtocolManagerProxy(ChildProcessProxy* childProcessProxy, WebProcessPool& processPool)
    : m_childProcessProxy(childProcessProxy)
    , m_processPool(processPool)
{
    ASSERT(m_childProcessProxy);
    m_childProcessProxy->addMessageReceiver(Messages::CustomProtocolManagerProxy::messageReceiverName(), *this);
}

CustomProtocolManagerProxy::~CustomProtocolManagerProxy()
{
    m_childProcessProxy->removeMessageReceiver(Messages::CustomProtocolManagerProxy::messageReceiverName());
}

void CustomProtocolManagerProxy::startLoading(uint64_t customProtocolID, const WebCore::ResourceRequest& request)
{
    m_processPool.customProtocolManagerClient().startLoading(*this, customProtocolID, request);
}

void CustomProtocolManagerProxy::stopLoading(uint64_t customProtocolID)
{
    m_processPool.customProtocolManagerClient().stopLoading(*this, customProtocolID);
}

void CustomProtocolManagerProxy::processDidClose()
{
    m_processPool.customProtocolManagerClient().invalidate(*this);
}

void CustomProtocolManagerProxy::wasRedirectedToRequest(uint64_t customProtocolID, const WebCore::ResourceRequest& request, const WebCore::ResourceResponse& redirectResponse)
{
    m_childProcessProxy->send(Messages::CustomProtocolManager::WasRedirectedToRequest(customProtocolID, request, redirectResponse), 0);
}

void CustomProtocolManagerProxy::didReceiveResponse(uint64_t customProtocolID, const WebCore::ResourceResponse& response, uint32_t cacheStoragePolicy)
{
    m_childProcessProxy->send(Messages::CustomProtocolManager::DidReceiveResponse(customProtocolID, response, cacheStoragePolicy), 0);
}

void CustomProtocolManagerProxy::didLoadData(uint64_t customProtocolID, const IPC::DataReference& data)
{
    m_childProcessProxy->send(Messages::CustomProtocolManager::DidLoadData(customProtocolID, data), 0);
}

void CustomProtocolManagerProxy::didFailWithError(uint64_t customProtocolID, const WebCore::ResourceError& error)
{
    m_childProcessProxy->send(Messages::CustomProtocolManager::DidFailWithError(customProtocolID, error), 0);
}

void CustomProtocolManagerProxy::didFinishLoading(uint64_t customProtocolID)
{
    m_childProcessProxy->send(Messages::CustomProtocolManager::DidFinishLoading(customProtocolID), 0);
}

} // namespace WebKit
