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

#ifndef IDBRequestData_h
#define IDBRequestData_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBDatabaseIdentifier.h"
#include "IDBResourceIdentifier.h"
#include "IndexedDB.h"

namespace WebCore {

namespace IndexedDB {
enum class IndexRecordType;
}

namespace IDBClient {
class IDBConnectionToServer;
class IDBOpenDBRequest;
class IDBTransaction;
class TransactionOperation;
}

class IDBRequestData {
public:
    IDBRequestData(const IDBClient::IDBConnectionToServer&, const IDBClient::IDBOpenDBRequest&);
    explicit IDBRequestData(IDBClient::TransactionOperation&);
    IDBRequestData(const IDBRequestData&);

    uint64_t serverConnectionIdentifier() const;
    IDBResourceIdentifier requestIdentifier() const;
    IDBResourceIdentifier transactionIdentifier() const;
    uint64_t objectStoreIdentifier() const;
    uint64_t indexIdentifier() const;
    IndexedDB::IndexRecordType indexRecordType() const;
    IDBResourceIdentifier cursorIdentifier() const;

    const IDBDatabaseIdentifier& databaseIdentifier() const { return m_databaseIdentifier; }
    uint64_t requestedVersion() const;

    bool isOpenRequest() const { return m_requestType == IndexedDB::RequestType::Open; }
    bool isDeleteRequest() const { return m_requestType == IndexedDB::RequestType::Delete; }

    IDBRequestData isolatedCopy();

    WEBCORE_EXPORT IDBRequestData();

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static bool decode(Decoder&, IDBRequestData&);

private:
    uint64_t m_serverConnectionIdentifier { 0 };
    std::unique_ptr<IDBResourceIdentifier> m_requestIdentifier;
    std::unique_ptr<IDBResourceIdentifier> m_transactionIdentifier;
    std::unique_ptr<IDBResourceIdentifier> m_cursorIdentifier;
    uint64_t m_objectStoreIdentifier { 0 };
    uint64_t m_indexIdentifier { 0 };
    IndexedDB::IndexRecordType m_indexRecordType;

    IDBDatabaseIdentifier m_databaseIdentifier;
    uint64_t m_requestedVersion { 0 };

    IndexedDB::RequestType m_requestType { IndexedDB::RequestType::Other };
};

template<class Decoder> bool IDBRequestData::decode(Decoder&, IDBRequestData&)
{
    return false;
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBRequestData_h
