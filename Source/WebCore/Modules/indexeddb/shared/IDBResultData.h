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
    CreateObjectStoreSuccess,
    PutOrAddSuccess,
    GetRecordSuccess,
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
    static IDBResultData createObjectStoreSuccess(const IDBResourceIdentifier&);
    static IDBResultData putOrAddSuccess(const IDBResourceIdentifier&, const IDBKeyData&);
    static IDBResultData getRecordSuccess(const IDBResourceIdentifier&, const ThreadSafeDataBuffer& valueData);

    IDBResultData(const IDBResultData&);

    IDBResultType type() const { return m_type; }
    IDBResourceIdentifier requestIdentifier() const { return m_requestIdentifier; }

    const IDBError& error() const { return m_error; }
    uint64_t databaseConnectionIdentifier() const { return m_databaseConnectionIdentifier; }

    const IDBDatabaseInfo& databaseInfo() const;
    const IDBTransactionInfo& transactionInfo() const;

    const IDBKeyData* resultKey() const { return m_resultKey.get(); }
    const ThreadSafeDataBuffer& resultData() const { return m_resultData; }

private:
    IDBResultData(const IDBResourceIdentifier&);
    IDBResultData(IDBResultType, const IDBResourceIdentifier&);

    IDBResultType m_type;
    IDBResourceIdentifier m_requestIdentifier;

    IDBError m_error;
    uint64_t m_databaseConnectionIdentifier { 0 };
    std::unique_ptr<IDBDatabaseInfo> m_databaseInfo;
    std::unique_ptr<IDBTransactionInfo> m_transactionInfo;
    std::unique_ptr<IDBKeyData> m_resultKey;
    ThreadSafeDataBuffer m_resultData;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBResultData_h
