/*
 * Copyright (C) 2015-2020 Apple Inc. All rights reserved.
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

#pragma once

#include "IDBDatabaseIdentifier.h"
#include "IDBResourceIdentifier.h"
#include "IndexedDB.h"

namespace WebCore {

class IDBOpenDBRequest;
class IDBTransaction;

namespace IndexedDB {
enum class IndexRecordType : bool;
}

namespace IDBClient {
class IDBConnectionProxy;
class TransactionOperation;
}

class IDBRequestData {
public:
    IDBRequestData(const IDBClient::IDBConnectionProxy&, const IDBOpenDBRequest&);
    explicit IDBRequestData(IDBClient::TransactionOperation&);
    WEBCORE_EXPORT IDBRequestData(const IDBRequestData&);
    IDBRequestData(IDBRequestData&&) = default;
    IDBRequestData& operator=(IDBRequestData&&) = default;

    enum IsolatedCopyTag { IsolatedCopy };
    IDBRequestData(const IDBRequestData&, IsolatedCopyTag);
    WEBCORE_EXPORT IDBRequestData isolatedCopy() const;

    IDBConnectionIdentifier serverConnectionIdentifier() const;
    IDBResourceIdentifier requestIdentifier() const;
    IDBResourceIdentifier transactionIdentifier() const;
    uint64_t objectStoreIdentifier() const;
    uint64_t indexIdentifier() const;
    IndexedDB::IndexRecordType indexRecordType() const;
    IDBResourceIdentifier cursorIdentifier() const;

    const IDBDatabaseIdentifier& databaseIdentifier() const;
    uint64_t requestedVersion() const;

    bool isOpenRequest() const { return m_requestType == IndexedDB::RequestType::Open; }
    bool isDeleteRequest() const { return m_requestType == IndexedDB::RequestType::Delete; }

    IDBRequestData isolatedCopy();

    WEBCORE_EXPORT IDBRequestData();

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static WARN_UNUSED_RETURN bool decode(Decoder&, IDBRequestData&);

private:
    static void isolatedCopy(const IDBRequestData& source, IDBRequestData& destination);

    IDBConnectionIdentifier m_serverConnectionIdentifier;
    std::unique_ptr<IDBResourceIdentifier> m_requestIdentifier;
    std::unique_ptr<IDBResourceIdentifier> m_transactionIdentifier;
    std::unique_ptr<IDBResourceIdentifier> m_cursorIdentifier;
    uint64_t m_objectStoreIdentifier { 0 };
    uint64_t m_indexIdentifier { 0 };
    IndexedDB::IndexRecordType m_indexRecordType { IndexedDB::IndexRecordType::Key };

    mutable std::optional<IDBDatabaseIdentifier> m_databaseIdentifier;
    uint64_t m_requestedVersion { 0 };

    IndexedDB::RequestType m_requestType { IndexedDB::RequestType::Other };
};

inline const IDBDatabaseIdentifier& IDBRequestData::databaseIdentifier() const
{
    ASSERT(m_databaseIdentifier);
    if (!m_databaseIdentifier)
        m_databaseIdentifier = IDBDatabaseIdentifier { };
    return *m_databaseIdentifier;
}

template<class Encoder>
void IDBRequestData::encode(Encoder& encoder) const
{
    encoder << m_serverConnectionIdentifier << m_objectStoreIdentifier << m_indexIdentifier << m_databaseIdentifier << m_requestedVersion;

    encoder << m_indexRecordType;
    encoder << m_requestType;

    encoder << !!m_requestIdentifier;
    if (m_requestIdentifier)
        encoder << *m_requestIdentifier;

    encoder << !!m_transactionIdentifier;
    if (m_transactionIdentifier)
        encoder << *m_transactionIdentifier;

    encoder << !!m_cursorIdentifier;
    if (m_cursorIdentifier)
        encoder << *m_cursorIdentifier;
}

template<class Decoder>
bool IDBRequestData::decode(Decoder& decoder, IDBRequestData& request)
{
    if (!decoder.decode(request.m_serverConnectionIdentifier))
        return false;

    if (!decoder.decode(request.m_objectStoreIdentifier))
        return false;

    if (!decoder.decode(request.m_indexIdentifier))
        return false;

    std::optional<std::optional<IDBDatabaseIdentifier>> databaseIdentifier;
    decoder >> databaseIdentifier;
    if (!databaseIdentifier)
        return false;
    request.m_databaseIdentifier = WTFMove(*databaseIdentifier);

    if (!decoder.decode(request.m_requestedVersion))
        return false;

    if (!decoder.decode(request.m_indexRecordType))
        return false;

    if (!decoder.decode(request.m_requestType))
        return false;

    bool hasObject;

    if (!decoder.decode(hasObject))
        return false;
    if (hasObject) {
        std::unique_ptr<IDBResourceIdentifier> object = makeUnique<IDBResourceIdentifier>();
        if (!decoder.decode(*object))
            return false;
        request.m_requestIdentifier = WTFMove(object);
    }

    if (!decoder.decode(hasObject))
        return false;
    if (hasObject) {
        std::unique_ptr<IDBResourceIdentifier> object = makeUnique<IDBResourceIdentifier>();
        if (!decoder.decode(*object))
            return false;
        request.m_transactionIdentifier = WTFMove(object);
    }

    if (!decoder.decode(hasObject))
        return false;
    if (hasObject) {
        std::unique_ptr<IDBResourceIdentifier> object = makeUnique<IDBResourceIdentifier>();
        if (!decoder.decode(*object))
            return false;
        request.m_cursorIdentifier = WTFMove(object);
    }

    return true;
}

} // namespace WebCore
