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
#include "IDBRequestData.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBConnectionToServer.h"
#include "IDBOpenDBRequestImpl.h"

namespace WebCore {

IDBRequestData::IDBRequestData()
{
}

IDBRequestData::IDBRequestData(const IDBClient::IDBConnectionToServer& connection, const IDBClient::IDBOpenDBRequest& request)
    : m_serverConnectionIdentifier(connection.identifier())
    , m_requestIdentifier(std::make_unique<IDBResourceIdentifier>(connection, request))
    , m_databaseIdentifier(request.databaseIdentifier())
    , m_requestedVersion(request.version())
    , m_requestType(request.requestType())
{
}

IDBRequestData::IDBRequestData(IDBClient::TransactionOperation& operation)
    : m_serverConnectionIdentifier(operation.transaction().database().serverConnection().identifier())
    , m_requestIdentifier(std::make_unique<IDBResourceIdentifier>(operation.identifier()))
    , m_transactionIdentifier(std::make_unique<IDBResourceIdentifier>(operation.transactionIdentifier()))
    , m_objectStoreIdentifier(operation.objectStoreIdentifier())
    , m_indexIdentifier(operation.indexIdentifier())
{
    if (m_indexIdentifier)
        m_indexRecordType = operation.indexRecordType();

    if (operation.cursorIdentifier())
        m_cursorIdentifier = std::make_unique<IDBResourceIdentifier>(*operation.cursorIdentifier());
}

IDBRequestData::IDBRequestData(const IDBRequestData& other)
    : m_serverConnectionIdentifier(other.m_serverConnectionIdentifier)
    , m_objectStoreIdentifier(other.m_objectStoreIdentifier)
    , m_indexIdentifier(other.m_indexIdentifier)
    , m_indexRecordType(other.m_indexRecordType)
    , m_databaseIdentifier(other.m_databaseIdentifier)
    , m_requestedVersion(other.m_requestedVersion)
    , m_requestType(other.m_requestType)
{
    if (other.m_requestIdentifier)
        m_requestIdentifier = std::make_unique<IDBResourceIdentifier>(*other.m_requestIdentifier);
    if (other.m_transactionIdentifier)
        m_transactionIdentifier = std::make_unique<IDBResourceIdentifier>(*other.m_transactionIdentifier);
    if (other.m_cursorIdentifier)
        m_cursorIdentifier = std::make_unique<IDBResourceIdentifier>(*other.m_cursorIdentifier);
}

uint64_t IDBRequestData::serverConnectionIdentifier() const
{
    ASSERT(m_serverConnectionIdentifier);
    return m_serverConnectionIdentifier;
}

IDBResourceIdentifier IDBRequestData::requestIdentifier() const
{
    ASSERT(m_requestIdentifier);
    return *m_requestIdentifier;
}

IDBResourceIdentifier IDBRequestData::transactionIdentifier() const
{
    ASSERT(m_transactionIdentifier);
    return *m_transactionIdentifier;
}

IDBResourceIdentifier IDBRequestData::cursorIdentifier() const
{
    ASSERT(m_cursorIdentifier);
    return *m_cursorIdentifier;
}

uint64_t IDBRequestData::objectStoreIdentifier() const
{
    ASSERT(m_objectStoreIdentifier);
    return m_objectStoreIdentifier;
}

uint64_t IDBRequestData::indexIdentifier() const
{
    ASSERT(m_objectStoreIdentifier || m_indexIdentifier);
    return m_indexIdentifier;
}

IndexedDB::IndexRecordType IDBRequestData::indexRecordType() const
{
    ASSERT(m_indexIdentifier);
    return m_indexRecordType;
}

uint64_t IDBRequestData::requestedVersion() const
{
    return m_requestedVersion;
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
