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

#pragma once

#include "IDBDatabaseInfo.h"
#include "IDBError.h"
#include "IDBGetAllResult.h"
#include "IDBGetResult.h"
#include "IDBKeyData.h"
#include "IDBResourceIdentifier.h"
#include "IDBTransactionInfo.h"
#include "ThreadSafeDataBuffer.h"
#include <wtf/EnumTraits.h>

namespace WebCore {

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
    GetAllRecordsSuccess,
    GetCountSuccess,
    DeleteRecordSuccess,
    CreateIndexSuccess,
    DeleteIndexSuccess,
    OpenCursorSuccess,
    IterateCursorSuccess,
    RenameObjectStoreSuccess,
    RenameIndexSuccess,
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
    static IDBResultData renameObjectStoreSuccess(const IDBResourceIdentifier&);
    static IDBResultData clearObjectStoreSuccess(const IDBResourceIdentifier&);
    static IDBResultData createIndexSuccess(const IDBResourceIdentifier&);
    static IDBResultData deleteIndexSuccess(const IDBResourceIdentifier&);
    static IDBResultData renameIndexSuccess(const IDBResourceIdentifier&);
    static IDBResultData putOrAddSuccess(const IDBResourceIdentifier&, const IDBKeyData&);
    static IDBResultData getRecordSuccess(const IDBResourceIdentifier&, const IDBGetResult&);
    static IDBResultData getAllRecordsSuccess(const IDBResourceIdentifier&, const IDBGetAllResult&);
    static IDBResultData getCountSuccess(const IDBResourceIdentifier&, uint64_t count);
    static IDBResultData deleteRecordSuccess(const IDBResourceIdentifier&);
    static IDBResultData openCursorSuccess(const IDBResourceIdentifier&, const IDBGetResult&);
    static IDBResultData iterateCursorSuccess(const IDBResourceIdentifier&, const IDBGetResult&);

    WEBCORE_EXPORT IDBResultData(const IDBResultData&);
    IDBResultData(IDBResultData&&) = default;
    IDBResultData& operator=(IDBResultData&&) = default;

    enum IsolatedCopyTag { IsolatedCopy };
    IDBResultData(const IDBResultData&, IsolatedCopyTag);
    WEBCORE_EXPORT IDBResultData isolatedCopy() const;

    IDBResultType type() const { return m_type; }
    IDBResourceIdentifier requestIdentifier() const { return m_requestIdentifier; }

    const IDBError& error() const { return m_error; }
    uint64_t databaseConnectionIdentifier() const { return m_databaseConnectionIdentifier; }

    const IDBDatabaseInfo& databaseInfo() const;
    const IDBTransactionInfo& transactionInfo() const;

    const IDBKeyData* resultKey() const { return m_resultKey.get(); }
    uint64_t resultInteger() const { return m_resultInteger; }

    WEBCORE_EXPORT const IDBGetResult& getResult() const;
    WEBCORE_EXPORT IDBGetResult& getResultRef();
    WEBCORE_EXPORT const IDBGetAllResult& getAllResult() const;

    WEBCORE_EXPORT IDBResultData();
    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<IDBResultData> decode(Decoder&);

private:
    IDBResultData(const IDBResourceIdentifier&);
    IDBResultData(IDBResultType, const IDBResourceIdentifier&);

    static void isolatedCopy(const IDBResultData& source, IDBResultData& destination);

    IDBResultType m_type { IDBResultType::Error };
    IDBResourceIdentifier m_requestIdentifier;

    IDBError m_error;
    uint64_t m_databaseConnectionIdentifier { 0 };
    std::unique_ptr<IDBDatabaseInfo> m_databaseInfo;
    std::unique_ptr<IDBTransactionInfo> m_transactionInfo;
    std::unique_ptr<IDBKeyData> m_resultKey;
    std::unique_ptr<IDBGetResult> m_getResult;
    std::unique_ptr<IDBGetAllResult> m_getAllResult;
    uint64_t m_resultInteger { 0 };
};

template<class Encoder>
void IDBResultData::encode(Encoder& encoder) const
{
    encoder << m_requestIdentifier << m_error << m_databaseConnectionIdentifier << m_resultInteger;

    encoder << m_type;

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

    encoder << !!m_getAllResult;
    if (m_getAllResult)
        encoder << *m_getAllResult;
}

template<class Decoder> std::optional<IDBResultData> IDBResultData::decode(Decoder& decoder)
{
    IDBResultData result;
    if (!decoder.decode(result.m_requestIdentifier))
        return std::nullopt;

    if (!decoder.decode(result.m_error))
        return std::nullopt;

    if (!decoder.decode(result.m_databaseConnectionIdentifier))
        return std::nullopt;

    if (!decoder.decode(result.m_resultInteger))
        return std::nullopt;

    if (!decoder.decode(result.m_type))
        return std::nullopt;

    bool hasObject;

    if (!decoder.decode(hasObject))
        return std::nullopt;
    if (hasObject) {
        auto object = makeUnique<IDBDatabaseInfo>();
        if (!decoder.decode(*object))
            return std::nullopt;
        result.m_databaseInfo = WTFMove(object);
    }

    if (!decoder.decode(hasObject))
        return std::nullopt;
    if (hasObject) {
        auto object = makeUnique<IDBTransactionInfo>();
        if (!decoder.decode(*object))
            return std::nullopt;
        result.m_transactionInfo = WTFMove(object);
    }

    if (!decoder.decode(hasObject))
        return std::nullopt;
    if (hasObject) {
        auto object = makeUnique<IDBKeyData>();
        std::optional<IDBKeyData> optional;
        decoder >> optional;
        if (!optional)
            return std::nullopt;
        *object = WTFMove(*optional);
        result.m_resultKey = WTFMove(object);
    }

    if (!decoder.decode(hasObject))
        return std::nullopt;
    if (hasObject) {
        auto object = makeUnique<IDBGetResult>();
        if (!decoder.decode(*object))
            return std::nullopt;
        result.m_getResult = WTFMove(object);
    }

    if (!decoder.decode(hasObject))
        return std::nullopt;
    if (hasObject) {
        auto object = makeUnique<IDBGetAllResult>();
        if (!decoder.decode(*object))
            return std::nullopt;
        result.m_getAllResult = WTFMove(object);
    }

    return result;
}

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::IDBResultType> {
    using values = EnumValues<
        WebCore::IDBResultType,
        WebCore::IDBResultType::Error,
        WebCore::IDBResultType::OpenDatabaseSuccess,
        WebCore::IDBResultType::OpenDatabaseUpgradeNeeded,
        WebCore::IDBResultType::DeleteDatabaseSuccess,
        WebCore::IDBResultType::CreateObjectStoreSuccess,
        WebCore::IDBResultType::DeleteObjectStoreSuccess,
        WebCore::IDBResultType::ClearObjectStoreSuccess,
        WebCore::IDBResultType::PutOrAddSuccess,
        WebCore::IDBResultType::GetRecordSuccess,
        WebCore::IDBResultType::GetAllRecordsSuccess,
        WebCore::IDBResultType::GetCountSuccess,
        WebCore::IDBResultType::DeleteRecordSuccess,
        WebCore::IDBResultType::CreateIndexSuccess,
        WebCore::IDBResultType::DeleteIndexSuccess,
        WebCore::IDBResultType::OpenCursorSuccess,
        WebCore::IDBResultType::IterateCursorSuccess,
        WebCore::IDBResultType::RenameObjectStoreSuccess,
        WebCore::IDBResultType::RenameIndexSuccess
    >;
};

} // namespace WTF
