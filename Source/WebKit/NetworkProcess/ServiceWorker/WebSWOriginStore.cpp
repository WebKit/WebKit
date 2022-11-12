/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(SERVICE_WORKER)
#include "WebSWOriginStore.h"

#include "WebSWClientConnectionMessages.h"
#include "WebSWServerConnection.h"
#include <WebCore/SecurityOrigin.h>

namespace WebKit {

using namespace WebCore;

WebSWOriginStore::WebSWOriginStore()
    : m_store(*this)
{
}

void WebSWOriginStore::addToStore(const SecurityOriginData& origin)
{
    m_store.scheduleAddition(computeSharedStringHash(origin.toString()));
    m_store.flushPendingChanges();
}

void WebSWOriginStore::removeFromStore(const SecurityOriginData& origin)
{
    m_store.scheduleRemoval(computeSharedStringHash(origin.toString()));
    m_store.flushPendingChanges();
}

void WebSWOriginStore::clearStore()
{
    m_store.clear();
}

void WebSWOriginStore::importComplete()
{
    m_isImported = true;
    for (auto& connection : m_webSWServerConnections)
        connection.send(Messages::WebSWClientConnection::SetSWOriginTableIsImported());
}

void WebSWOriginStore::registerSWServerConnection(WebSWServerConnection& connection)
{
    m_webSWServerConnections.add(connection);

    if (!m_store.isEmpty())
        sendStoreHandle(connection);

    if (m_isImported)
        connection.send(Messages::WebSWClientConnection::SetSWOriginTableIsImported());
}

void WebSWOriginStore::unregisterSWServerConnection(WebSWServerConnection& connection)
{
    m_webSWServerConnections.remove(connection);
}

void WebSWOriginStore::sendStoreHandle(WebSWServerConnection& connection)
{
    auto handle = m_store.createSharedMemoryHandle();
    if (!handle)
        return;
    connection.send(Messages::WebSWClientConnection::SetSWOriginTableSharedMemory(*handle));
}

void WebSWOriginStore::didInvalidateSharedMemory()
{
    for (auto& connection : m_webSWServerConnections)
        sendStoreHandle(connection);
}

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)
