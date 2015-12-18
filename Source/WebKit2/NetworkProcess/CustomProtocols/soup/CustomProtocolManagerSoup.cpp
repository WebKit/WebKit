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
#include "CustomProtocolManager.h"

#include "ChildProcess.h"
#include "CustomProtocolManagerImpl.h"
#include "CustomProtocolManagerMessages.h"
#include "NetworkProcessCreationParameters.h"
#include "WebProcessCreationParameters.h"
#include <WebCore/NotImplemented.h>

namespace WebKit {

const char* CustomProtocolManager::supplementName()
{
    return "CustomProtocolManager";
}

CustomProtocolManager::CustomProtocolManager(ChildProcess* childProcess)
    : m_childProcess(childProcess)
    , m_messageQueue(WorkQueue::create("com.apple.WebKit.CustomProtocolManager"))
    , m_impl(std::make_unique<CustomProtocolManagerImpl>(childProcess))
{
}

void CustomProtocolManager::initializeConnection(IPC::Connection* connection)
{
    connection->addWorkQueueMessageReceiver(Messages::CustomProtocolManager::messageReceiverName(), m_messageQueue.get(), this);
}

void CustomProtocolManager::initialize(const NetworkProcessCreationParameters& parameters)
{
    for (size_t i = 0; i < parameters.urlSchemesRegisteredForCustomProtocols.size(); ++i)
        registerScheme(parameters.urlSchemesRegisteredForCustomProtocols[i]);
}

void CustomProtocolManager::registerScheme(const String& scheme)
{
    m_impl->registerScheme(scheme);
}

void CustomProtocolManager::unregisterScheme(const String&)
{
    notImplemented();
}

bool CustomProtocolManager::supportsScheme(const String& scheme)
{
    return m_impl->supportsScheme(scheme);
}

void CustomProtocolManager::didFailWithError(uint64_t customProtocolID, const WebCore::ResourceError& error)
{
    m_impl->didFailWithError(customProtocolID, error);
}

void CustomProtocolManager::didLoadData(uint64_t customProtocolID, const IPC::DataReference& dataReference)
{
    m_impl->didLoadData(customProtocolID, dataReference);
}

void CustomProtocolManager::didReceiveResponse(uint64_t customProtocolID, const WebCore::ResourceResponse& response, uint32_t)
{
    m_impl->didReceiveResponse(customProtocolID, response);
}

void CustomProtocolManager::didFinishLoading(uint64_t customProtocolID)
{
    m_impl->didFinishLoading(customProtocolID);
}

void CustomProtocolManager::wasRedirectedToRequest(uint64_t, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&)
{
    notImplemented();
}

} // namespace WebKit
