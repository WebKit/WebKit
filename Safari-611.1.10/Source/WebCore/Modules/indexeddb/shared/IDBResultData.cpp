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
#include "IDBResultData.h"

#if ENABLE(INDEXED_DATABASE)

#include "UniqueIDBDatabase.h"
#include "UniqueIDBDatabaseConnection.h"
#include "UniqueIDBDatabaseTransaction.h"

namespace WebCore {

IDBResultData::IDBResultData()
{
}

IDBResultData::IDBResultData(const IDBResourceIdentifier& requestIdentifier)
    : m_requestIdentifier(requestIdentifier)
{
}

IDBResultData::IDBResultData(IDBResultType type, const IDBResourceIdentifier& requestIdentifier)
    : m_type(type)
    , m_requestIdentifier(requestIdentifier)
{
}

IDBResultData::IDBResultData(const IDBResultData& other)
    : m_type(other.m_type)
    , m_requestIdentifier(other.m_requestIdentifier)
    , m_error(other.m_error)
    , m_databaseConnectionIdentifier(other.m_databaseConnectionIdentifier)
    , m_resultInteger(other.m_resultInteger)
{
    if (other.m_databaseInfo)
        m_databaseInfo = makeUnique<IDBDatabaseInfo>(*other.m_databaseInfo);
    if (other.m_transactionInfo)
        m_transactionInfo = makeUnique<IDBTransactionInfo>(*other.m_transactionInfo);
    if (other.m_resultKey)
        m_resultKey = makeUnique<IDBKeyData>(*other.m_resultKey);
    if (other.m_getResult)
        m_getResult = makeUnique<IDBGetResult>(*other.m_getResult);
    if (other.m_getAllResult)
        m_getAllResult = makeUnique<IDBGetAllResult>(*other.m_getAllResult);
}

IDBResultData::IDBResultData(const IDBResultData& that, IsolatedCopyTag)
{
    isolatedCopy(that, *this);
}

IDBResultData IDBResultData::isolatedCopy() const
{
    return { *this, IsolatedCopy };
}

void IDBResultData::isolatedCopy(const IDBResultData& source, IDBResultData& destination)
{
    destination.m_type = source.m_type;
    destination.m_requestIdentifier = source.m_requestIdentifier.isolatedCopy();
    destination.m_error = source.m_error.isolatedCopy();
    destination.m_databaseConnectionIdentifier = source.m_databaseConnectionIdentifier;
    destination.m_resultInteger = source.m_resultInteger;

    if (source.m_databaseInfo)
        destination.m_databaseInfo = makeUnique<IDBDatabaseInfo>(*source.m_databaseInfo, IDBDatabaseInfo::IsolatedCopy);
    if (source.m_transactionInfo)
        destination.m_transactionInfo = makeUnique<IDBTransactionInfo>(*source.m_transactionInfo, IDBTransactionInfo::IsolatedCopy);
    if (source.m_resultKey)
        destination.m_resultKey = makeUnique<IDBKeyData>(*source.m_resultKey, IDBKeyData::IsolatedCopy);
    if (source.m_getResult)
        destination.m_getResult = makeUnique<IDBGetResult>(*source.m_getResult, IDBGetResult::IsolatedCopy);
    if (source.m_getAllResult)
        destination.m_getAllResult = makeUnique<IDBGetAllResult>(*source.m_getAllResult, IDBGetAllResult::IsolatedCopy);
}

IDBResultData IDBResultData::error(const IDBResourceIdentifier& requestIdentifier, const IDBError& error)
{
    IDBResultData result { requestIdentifier };
    result.m_type = IDBResultType::Error;
    result.m_error = error;
    return result;
}

IDBResultData IDBResultData::openDatabaseSuccess(const IDBResourceIdentifier& requestIdentifier, IDBServer::UniqueIDBDatabaseConnection& connection)
{
    IDBResultData result { requestIdentifier };
    result.m_type = IDBResultType::OpenDatabaseSuccess;
    result.m_databaseConnectionIdentifier = connection.identifier();
    result.m_databaseInfo = makeUnique<IDBDatabaseInfo>(connection.database()->info());
    return result;
}


IDBResultData IDBResultData::openDatabaseUpgradeNeeded(const IDBResourceIdentifier& requestIdentifier, IDBServer::UniqueIDBDatabaseTransaction& transaction)
{
    IDBResultData result { requestIdentifier };
    result.m_type = IDBResultType::OpenDatabaseUpgradeNeeded;
    result.m_databaseConnectionIdentifier = transaction.databaseConnection().identifier();
    result.m_databaseInfo = makeUnique<IDBDatabaseInfo>(transaction.databaseConnection().database()->info());
    result.m_transactionInfo = makeUnique<IDBTransactionInfo>(transaction.info());
    return result;
}

IDBResultData IDBResultData::deleteDatabaseSuccess(const IDBResourceIdentifier& requestIdentifier, const IDBDatabaseInfo& info)
{
    IDBResultData result {IDBResultType::DeleteDatabaseSuccess, requestIdentifier };
    result.m_databaseInfo = makeUnique<IDBDatabaseInfo>(info);
    return result;
}

IDBResultData IDBResultData::createObjectStoreSuccess(const IDBResourceIdentifier& requestIdentifier)
{
    return { IDBResultType::CreateObjectStoreSuccess, requestIdentifier };
}

IDBResultData IDBResultData::deleteObjectStoreSuccess(const IDBResourceIdentifier& requestIdentifier)
{
    return { IDBResultType::DeleteObjectStoreSuccess, requestIdentifier };
}

IDBResultData IDBResultData::renameObjectStoreSuccess(const IDBResourceIdentifier& requestIdentifier)
{
    return { IDBResultType::RenameObjectStoreSuccess, requestIdentifier };
}

IDBResultData IDBResultData::clearObjectStoreSuccess(const IDBResourceIdentifier& requestIdentifier)
{
    return { IDBResultType::ClearObjectStoreSuccess, requestIdentifier };
}

IDBResultData IDBResultData::createIndexSuccess(const IDBResourceIdentifier& requestIdentifier)
{
    return { IDBResultType::CreateIndexSuccess, requestIdentifier };
}

IDBResultData IDBResultData::deleteIndexSuccess(const IDBResourceIdentifier& requestIdentifier)
{
    return { IDBResultType::DeleteIndexSuccess, requestIdentifier };
}

IDBResultData IDBResultData::renameIndexSuccess(const IDBResourceIdentifier& requestIdentifier)
{
    return { IDBResultType::RenameIndexSuccess, requestIdentifier };
}

IDBResultData IDBResultData::putOrAddSuccess(const IDBResourceIdentifier& requestIdentifier, const IDBKeyData& resultKey)
{
    IDBResultData result { IDBResultType::PutOrAddSuccess, requestIdentifier };
    result.m_resultKey = makeUnique<IDBKeyData>(resultKey);
    return result;
}

IDBResultData IDBResultData::getRecordSuccess(const IDBResourceIdentifier& requestIdentifier, const IDBGetResult& getResult)
{
    IDBResultData result { IDBResultType::GetRecordSuccess, requestIdentifier };
    result.m_getResult = makeUnique<IDBGetResult>(getResult);
    return result;
}

IDBResultData IDBResultData::getAllRecordsSuccess(const IDBResourceIdentifier& requestIdentifier, const IDBGetAllResult& getAllResult)
{
    IDBResultData result { IDBResultType::GetAllRecordsSuccess, requestIdentifier };
    result.m_getAllResult = makeUnique<IDBGetAllResult>(getAllResult);
    return result;
}

IDBResultData IDBResultData::getCountSuccess(const IDBResourceIdentifier& requestIdentifier, uint64_t count)
{
    IDBResultData result { IDBResultType::GetRecordSuccess, requestIdentifier };
    result.m_resultInteger = count;
    return result;
}

IDBResultData IDBResultData::deleteRecordSuccess(const IDBResourceIdentifier& requestIdentifier)
{
    return { IDBResultType::DeleteRecordSuccess, requestIdentifier };
}

IDBResultData IDBResultData::openCursorSuccess(const IDBResourceIdentifier& requestIdentifier, const IDBGetResult& getResult)
{
    IDBResultData result { IDBResultType::OpenCursorSuccess, requestIdentifier };
    result.m_getResult = makeUnique<IDBGetResult>(getResult);
    return result;
}

IDBResultData IDBResultData::iterateCursorSuccess(const IDBResourceIdentifier& requestIdentifier, const IDBGetResult& getResult)
{
    IDBResultData result { IDBResultType::IterateCursorSuccess, requestIdentifier };
    result.m_getResult = makeUnique<IDBGetResult>(getResult);
    return result;
}

const IDBDatabaseInfo& IDBResultData::databaseInfo() const
{
    RELEASE_ASSERT(m_databaseInfo);
    return *m_databaseInfo;
}

const IDBTransactionInfo& IDBResultData::transactionInfo() const
{
    RELEASE_ASSERT(m_transactionInfo);
    return *m_transactionInfo;
}

const IDBGetResult& IDBResultData::getResult() const
{
    RELEASE_ASSERT(m_getResult);
    return *m_getResult;
}

IDBGetResult& IDBResultData::getResultRef()
{
    RELEASE_ASSERT(m_getResult);
    return *m_getResult;
}

const IDBGetAllResult& IDBResultData::getAllResult() const
{
    RELEASE_ASSERT(m_getAllResult);
    return *m_getAllResult;
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
