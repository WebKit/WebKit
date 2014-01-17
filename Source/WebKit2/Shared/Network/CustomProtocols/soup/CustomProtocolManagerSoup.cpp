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

#if ENABLE(CUSTOM_PROTOCOLS)

#include "ChildProcess.h"
#include "CustomProtocolManagerMessages.h"
#include "CustomProtocolManagerProxyMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcessCreationParameters.h"
#include <WebCore/NotImplemented.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/URL.h>

#if ENABLE(NETWORK_PROCESS)
#include "NetworkProcessCreationParameters.h"
#endif

namespace WebKit {

const char* CustomProtocolManager::supplementName()
{
    return "CustomProtocolManager";
}

CustomProtocolManager::CustomProtocolManager(ChildProcess* childProcess)
    : m_childProcess(childProcess)
    , m_messageQueue(WorkQueue::create("com.apple.WebKit.CustomProtocolManager"))
{
}

void CustomProtocolManager::initializeConnection(IPC::Connection* connection)
{
    connection->addWorkQueueMessageReceiver(Messages::CustomProtocolManager::messageReceiverName(), m_messageQueue.get(), this);
}

void CustomProtocolManager::initialize(const WebProcessCreationParameters&)
{
}

#if ENABLE(NETWORK_PROCESS)
void CustomProtocolManager::initialize(const NetworkProcessCreationParameters&)
{
}
#endif

void CustomProtocolManager::registerScheme(const String&)
{
    notImplemented();
}

void CustomProtocolManager::unregisterScheme(const String&)
{
    notImplemented();
}

bool CustomProtocolManager::supportsScheme(const String&)
{
    notImplemented();
    return false;
}

void CustomProtocolManager::didFailWithError(uint64_t, const WebCore::ResourceError&)
{
    notImplemented();
}

void CustomProtocolManager::didLoadData(uint64_t, const IPC::DataReference&)
{
    notImplemented();
}

void CustomProtocolManager::didReceiveResponse(uint64_t, const WebCore::ResourceResponse&, uint32_t)
{
    notImplemented();
}

void CustomProtocolManager::didFinishLoading(uint64_t)
{
    notImplemented();
}

} // namespace WebKit

#endif // ENABLE(CUSTOM_PROTOCOLS)
