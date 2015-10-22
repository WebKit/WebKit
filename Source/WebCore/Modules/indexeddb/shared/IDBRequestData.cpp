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

IDBRequestData::IDBRequestData(const IDBClient::IDBConnectionToServer& connection, const IDBClient::IDBOpenDBRequest& request)
    : m_requestIdentifier(std::make_unique<IDBResourceIdentifier>(connection, request))
    , m_databaseIdentifier(request.databaseIdentifier())
    , m_requestedVersion(request.version())
{
}

IDBRequestData::IDBRequestData(IDBClient::TransactionOperation& operation)
    : m_requestIdentifier(std::make_unique<IDBResourceIdentifier>(operation.identifier()))
    , m_transactionIdentifier(std::make_unique<IDBResourceIdentifier>(operation.transactionIdentifier()))
{
}

IDBRequestData::IDBRequestData(const IDBRequestData& other)
    : m_databaseIdentifier(other.m_databaseIdentifier)
    , m_requestedVersion(other.m_requestedVersion)
{
    if (other.m_requestIdentifier)
        m_requestIdentifier = std::make_unique<IDBResourceIdentifier>(*other.m_requestIdentifier);
    if (other.m_transactionIdentifier)
        m_transactionIdentifier = std::make_unique<IDBResourceIdentifier>(*other.m_transactionIdentifier);
}

IDBResourceIdentifier IDBRequestData::requestIdentifier() const
{
    RELEASE_ASSERT(m_requestIdentifier);
    return *m_requestIdentifier;
}

IDBResourceIdentifier IDBRequestData::transactionIdentifier() const
{
    RELEASE_ASSERT(m_transactionIdentifier);
    return *m_transactionIdentifier;
}

uint64_t IDBRequestData::requestedVersion() const
{
    return m_requestedVersion;
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
