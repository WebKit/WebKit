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

#ifndef IDBResultData_h
#define IDBResultData_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBDatabaseInfo.h"
#include "IDBError.h"
#include "IDBGetResult.h"
#include "IDBKeyData.h"
#include "IDBResourceIdentifier.h"
#include "IDBTransactionInfo.h"
#include "ThreadSafeDataBuffer.h"

namespace WebCore {

class IDBResourceIdentifier;
class ThreadSafeDataBuffer;

enum class IDBResultType {
    Error,
    OpenDatabaseSuccess,
    OpenDatabaseUpgradeNeeded,
    DeleteDatabaseSuccess,
    CreateObjectStoreSuccess,
    DeleteObjectStoreSuccess,
    ClearObjectStoreSuccess,
    PutOrAddSuccess,
    GetRecordSuccess,
    GetCountSuccess,
    DeleteRecordSuccess,
    CreateIndexSuccess,
    DeleteIndexSuccess,
    OpenCursorSuccess,
    IterateCursorSuccess,
};

namespace IDBServer {
class UniqueIDBDatabaseConnection;
class UniqueIDBDatabaseTransaction;
}

class IDBResultData {
public:
    static IDBResultData error(const IDBResourceIdentifier&, const IDBError&);
    static IDBResultData openDatabaseSuccess(const IDBResourceIdentifier&, IDBServer::UniqueIDBDatabaseConnection&);
    static IDBResultData openDatabaseUpgradeNeeded(const IDBResourceIdentifier&, IDBServer::UniqueIDBDatabaseTransaction&);
    static IDBResultData deleteDatabaseSuccess(const IDBResourceIdentifier&, const IDBDatabaseInfo&);
    static IDBResultData createObjectStoreSuccess(const IDBResourceIdentifier&);
    static IDBResultData deleteObjectStoreSuccess(const IDBResourceIdentifier&);
    static IDBResultData clearObjectStoreSuccess(const IDBResourceIdentifier&);
    static IDBResultData createIndexSuccess(const IDBResourceIdentifier&);
    static IDBResultData deleteIndexSuccess(const IDBResourceIdentifier&);
    static IDBResultData putOrAddSuccess(const IDBResourceIdentifier&, const IDBKeyData&);
    static IDBResultData getRecordSuccess(const IDBResourceIdentifier&, const IDBGetResult&);
    static IDBResultData getCountSuccess(const IDBResourceIdentifier&, uint64_t count);
    static IDBResultData deleteRecordSuccess(const IDBResourceIdentifier&);
    static IDBResultData openCursorSuccess(const IDBResourceIdentifier&, const IDBGetResult&);
    static IDBResultData iterateCursorSuccess(const IDBResourceIdentifier&, const IDBGetResult&);

    WEBCORE_EXPORT IDBResultData(const IDBResultData&);

    IDBResultType type() const { return m_type; }
    IDBResourceIdentifier requestIdentifier() const { return m_requestIdentifier; }

    const IDBError& error() const { return m_error; }
    uint64_t databaseConnectionIdentifier() const { return m_databaseConnectionIdentifier; }

    const IDBDatabaseInfo& databaseInfo() const;
    const IDBTransactionInfo& transactionInfo() const;

    const IDBKeyData* resultKey() const { return m_resultKey.get(); }
    uint64_t resultInteger() const { return m_resultInteger; }

    WEBCORE_EXPORT const IDBGetResult& getResult() const;

    WEBCORE_EXPORT IDBResultData();
    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static bool decode(Decoder&, IDBResultData&);

private:
    IDBResultData(const IDBResourceIdentifier&);
    IDBResultData(IDBResultType, const IDBResourceIdentifier&);

    IDBResultType m_type { IDBResultType::Error };
    IDBResourceIdentifier m_requestIdentifier;

    IDBError m_error;
    uint64_t m_databaseConnectionIdentifier { 0 };
    std::unique_ptr<IDBDatabaseInfo> m_databaseInfo;
    std::unique_ptr<IDBTransactionInfo> m_transactionInfo;
    std::unique_ptr<IDBKeyData> m_resultKey;
    std::unique_ptr<IDBGetResult> m_getResult;
    uint64_t m_resultInteger { 0 };
};

template<class Encoder>
void IDBResultData::encode(Encoder& encoder) const
{
    encoder << m_requestIdentifier << m_error << m_databaseConnectionIdentifier << m_resultInteger;

    encoder.encodeEnum(m_type);

    encoder << !!m_databaseInfo;
    if (m_databaseInfo)
        encoder << *m_databaseInfo;

    encoder << !!m_transactionInfo;
    if (m_transactionInfo)
        encoder << *m_transactionInfo;

    encoder << !!m_resultKey;
    if (m_resultKey)
        encoder << *m_resultKey;

    encoder << !!m_getResult;
    if (m_getResult)
        encoder << *m_getResult;
}

template<class Decoder> bool IDBResultData::decode(Decoder& decoder, IDBResultData& result)
{
    if (!decoder.decode(result.m_requestIdentifier))
        return false;

    if (!decoder.decode(result.m_error))
        return false;

    if (!decoder.decode(result.m_databaseConnectionIdentifier))
        return false;

    if (!decoder.decode(result.m_resultInteger))
        return false;

    if (!decoder.decodeEnum(result.m_type))
        return false;

    bool hasObject;

    if (!decoder.decode(hasObject))
        return false;
    if (hasObject) {
        std::unique_ptr<IDBDatabaseInfo> object = std::make_unique<IDBDatabaseInfo>();
        if (!decoder.decode(*object))
            return false;
        result.m_databaseInfo = WTFMove(object);
    }

    if (!decoder.decode(hasObject))
        return false;
    if (hasObject) {
        std::unique_ptr<IDBTransactionInfo> object = std::make_unique<IDBTransactionInfo>();
        if (!decoder.decode(*object))
            return false;
        result.m_transactionInfo = WTFMove(object);
    }

    if (!decoder.decode(hasObject))
        return false;
    if (hasObject) {
        std::unique_ptr<IDBKeyData> object = std::make_unique<IDBKeyData>();
        if (!decoder.decode(*object))
            return false;
        result.m_resultKey = WTFMove(object);
    }

    if (!decoder.decode(hasObject))
        return false;
    if (hasObject) {
        std::unique_ptr<IDBGetResult> object = std::make_unique<IDBGetResult>();
        if (!decoder.decode(*object))
            return false;
        result.m_getResult = WTFMove(object);
    }

    return true;
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBResultData_h
