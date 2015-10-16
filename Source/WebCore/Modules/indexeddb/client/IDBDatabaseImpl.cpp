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
#include "IDBDatabaseImpl.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBConnectionToServer.h"
#include "IDBOpenDBRequestImpl.h"
#include "IDBResultData.h"
#include "IDBTransactionImpl.h"
#include "Logging.h"

namespace WebCore {
namespace IDBClient {

Ref<IDBDatabase> IDBDatabase::create(ScriptExecutionContext& context, IDBConnectionToServer& connection, const IDBResultData& resultData)
{
    return adoptRef(*new IDBDatabase(context, connection, resultData));
}

IDBDatabase::IDBDatabase(ScriptExecutionContext& context, IDBConnectionToServer& connection, const IDBResultData& resultData)
    : WebCore::IDBDatabase(&context)
    , m_serverConnection(connection)
    , m_info(resultData.databaseInfo())
    , m_databaseConnectionIdentifier(resultData.databaseConnectionIdentifier())
{
    suspendIfNeeded();
    relaxAdoptionRequirement();
    m_serverConnection->registerDatabaseConnection(*this);
}

IDBDatabase::~IDBDatabase()
{
    m_serverConnection->unregisterDatabaseConnection(*this);
}

const String IDBDatabase::name() const
{
    return m_info.name();
}

uint64_t IDBDatabase::version() const
{
    return m_info.version();
}

RefPtr<DOMStringList> IDBDatabase::objectStoreNames() const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

RefPtr<IDBObjectStore> IDBDatabase::createObjectStore(const String&, const Dictionary&, ExceptionCode&)
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

RefPtr<IDBObjectStore> IDBDatabase::createObjectStore(const String&, const IDBKeyPath&, bool, ExceptionCode&)
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

RefPtr<WebCore::IDBTransaction> IDBDatabase::transaction(ScriptExecutionContext*, const Vector<String>&, const String&, ExceptionCode&)
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

RefPtr<WebCore::IDBTransaction> IDBDatabase::transaction(ScriptExecutionContext*, const String&, const String&, ExceptionCode&)
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

void IDBDatabase::deleteObjectStore(const String&, ExceptionCode&)
{
    ASSERT_NOT_REACHED();
}

void IDBDatabase::close()
{
    m_closePending = true;
    maybeCloseInServer();
}

void IDBDatabase::maybeCloseInServer()
{
    if (m_closedInServer)
        return;

    // 3.3.9 Database closing steps
    // Wait for all transactions created using this connection to complete.
    // Once they are complete, this connection is closed.
    if (!m_activeTransactions.isEmpty() || !m_committingTransactions.isEmpty())
        return;

    m_closedInServer = true;
    m_serverConnection->databaseConnectionClosed(*this);
}

const char* IDBDatabase::activeDOMObjectName() const
{
    return "IDBDatabase";
}

bool IDBDatabase::canSuspendForPageCache() const
{
    // FIXME: This value will sometimes be false when database operations are actually in progress.
    // Such database operations do not yet exist.
    return true;
}

Ref<IDBTransaction> IDBDatabase::startVersionChangeTransaction(const IDBTransactionInfo& info)
{
    LOG(IndexedDB, "IDBDatabase::startVersionChangeTransaction");

    ASSERT(!m_versionChangeTransaction);
    ASSERT(info.mode() == IndexedDB::TransactionMode::VersionChange);

    Ref<IDBTransaction> transaction = IDBTransaction::create(*this, info);
    m_versionChangeTransaction = &transaction.get();

    m_activeTransactions.set(transaction->info().identifier(), &transaction.get());

    return WTF::move(transaction);
}

void IDBDatabase::commitTransaction(IDBTransaction& transaction)
{
    LOG(IndexedDB, "IDBDatabase::commitTransaction");

    auto refTransaction = m_activeTransactions.take(transaction.info().identifier());
    ASSERT(refTransaction);
    m_committingTransactions.set(transaction.info().identifier(), WTF::move(refTransaction));

    m_serverConnection->commitTransaction(transaction);
}

void IDBDatabase::didCommitTransaction(IDBTransaction& transaction)
{
    LOG(IndexedDB, "IDBDatabase::didCommitTransaction");

    if (m_versionChangeTransaction == &transaction)
        m_info.setVersion(transaction.info().newVersion());

    didCommitOrAbortTransaction(transaction);
}

void IDBDatabase::didAbortTransaction(IDBTransaction& transaction)
{
    LOG(IndexedDB, "IDBDatabase::didAbortTransaction");
    didCommitOrAbortTransaction(transaction);
}

void IDBDatabase::didCommitOrAbortTransaction(IDBTransaction& transaction)
{
    LOG(IndexedDB, "IDBDatabase::didCommitOrAbortTransaction");

    if (m_versionChangeTransaction == &transaction)
        m_versionChangeTransaction = nullptr;
    
    ASSERT(m_activeTransactions.contains(transaction.info().identifier()) || m_committingTransactions.contains(transaction.info().identifier()));

    m_activeTransactions.remove(transaction.info().identifier());
    m_committingTransactions.remove(transaction.info().identifier());
}

} // namespace IDBClient
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
