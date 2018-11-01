/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "WebDatabaseProvider.h"

#include <pal/SessionID.h>

WebDatabaseProvider& WebDatabaseProvider::singleton()
{
    static WebDatabaseProvider& databaseProvider = adoptRef(*new WebDatabaseProvider).leakRef();

    return databaseProvider;
}

WebDatabaseProvider::WebDatabaseProvider()
{
}

WebDatabaseProvider::~WebDatabaseProvider()
{
}

#if ENABLE(INDEXED_DATABASE)
WebCore::IDBClient::IDBConnectionToServer& WebDatabaseProvider::idbConnectionToServerForSession(const PAL::SessionID& sessionID)
{
    auto result = m_idbServerMap.add(sessionID.sessionID(), nullptr);
    if (result.isNewEntry) {
        if (sessionID.isEphemeral())
            result.iterator->value = WebCore::InProcessIDBServer::create();
        else
            result.iterator->value = WebCore::InProcessIDBServer::create(indexedDatabaseDirectoryPath());
    }

    result.iterator->value->idbServer().setPerOriginQuota(m_idbPerOriginQuota);

    return result.iterator->value->connectionToServer();
}

void WebDatabaseProvider::deleteAllDatabases()
{
    for (auto& server : m_idbServerMap.values())
        server->idbServer().closeAndDeleteDatabasesModifiedSince(-WallTime::infinity(), [] { });
}

void WebDatabaseProvider::setIDBPerOriginQuota(uint64_t quota)
{
    m_idbPerOriginQuota = quota;

    for (auto& server : m_idbServerMap.values())
        server->idbServer().setPerOriginQuota(quota);
}

#endif
