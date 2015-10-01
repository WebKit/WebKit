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

#include "config.h"
#include "IDBConnectionToServer.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBOpenDBRequestImpl.h"
#include "IDBRequestData.h"
#include "IDBResultData.h"
#include "Logging.h"

namespace WebCore {
namespace IDBClient {

Ref<IDBConnectionToServer> IDBConnectionToServer::create(IDBConnectionToServerDelegate& delegate)
{
    return adoptRef(*new IDBConnectionToServer(delegate));
}

IDBConnectionToServer::IDBConnectionToServer(IDBConnectionToServerDelegate& delegate)
    : m_delegate(delegate)
{
}

uint64_t IDBConnectionToServer::identifier() const
{
    return m_delegate->identifier();
}

void IDBConnectionToServer::deleteDatabase(IDBOpenDBRequest& request)
{
    LOG(IndexedDB, "IDBConnectionToServer::deleteDatabase - %s", request.databaseIdentifier().debugString().utf8().data());

    ASSERT(!m_openDBRequestMap.contains(request.requestIdentifier()));
    m_openDBRequestMap.set(request.requestIdentifier(), &request);
    
    IDBRequestData requestData(*this, request);
    m_delegate->deleteDatabase(requestData);
}

void IDBConnectionToServer::didDeleteDatabase(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBConnectionToServer::didDeleteDatabase");

    auto request = m_openDBRequestMap.take(resultData.requestIdentifier());
    ASSERT(request);

    request->requestCompleted(resultData);
}

void IDBConnectionToServer::openDatabase(IDBOpenDBRequest& request)
{
    LOG(IndexedDB, "IDBConnectionToServer::openDatabase - %s", request.databaseIdentifier().debugString().utf8().data());

    ASSERT(!m_openDBRequestMap.contains(request.requestIdentifier()));
    m_openDBRequestMap.set(request.requestIdentifier(), &request);
    
    IDBRequestData requestData(*this, request);
    m_delegate->openDatabase(requestData);
}

void IDBConnectionToServer::didOpenDatabase(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBConnectionToServer::didOpenDatabase");

    auto request = m_openDBRequestMap.take(resultData.requestIdentifier());
    ASSERT(request);

    request->requestCompleted(resultData);
}

} // namespace IDBClient
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
