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

#include "EventQueue.h"
#include "IDBConnectionToServer.h"
#include "IDBDatabaseException.h"
#include "IDBOpenDBRequestImpl.h"
#include "IDBResultData.h"
#include "IDBTransactionImpl.h"
#include "IDBVersionChangeEventImpl.h"
#include "Logging.h"
#include "ScriptExecutionContext.h"

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
    RefPtr<DOMStringList> objectStoreNames = DOMStringList::create();
    for (auto& name : m_info.objectStoreNames())
        objectStoreNames->append(name);
    objectStoreNames->sort();
    return WTF::move(objectStoreNames);
}

RefPtr<WebCore::IDBObjectStore> IDBDatabase::createObjectStore(const String&, const Dictionary&, ExceptionCode&)
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

RefPtr<WebCore::IDBObjectStore> IDBDatabase::createObjectStore(const String& name, const IDBKeyPath& keyPath, bool autoIncrement, ExceptionCode& ec)
{
    LOG(IndexedDB, "IDBDatabase::createObjectStore");

    ASSERT(!m_versionChangeTransaction || m_versionChangeTransaction->isVersionChange());

    if (!m_versionChangeTransaction) {
        ec = IDBDatabaseException::InvalidStateError;
        return nullptr;
    }

    if (!m_versionChangeTransaction->isActive()) {
        ec = IDBDatabaseException::TransactionInactiveError;
        return nullptr;
    }

    if (m_info.hasObjectStore(name)) {
        ec = IDBDatabaseException::ConstraintError;
        return nullptr;
    }

    if (!keyPath.isNull() && !keyPath.isValid()) {
        ec = IDBDatabaseException::SyntaxError;
        return nullptr;
    }

    if (autoIncrement && !keyPath.isNull()) {
        if ((keyPath.type() == IndexedDB::KeyPathType::String && keyPath.string().isEmpty()) || keyPath.type() == IndexedDB::KeyPathType::Array) {
            ec = IDBDatabaseException::InvalidAccessError;
            return nullptr;
        }
    }

    // Install the new ObjectStore into the connection's metadata.
    IDBObjectStoreInfo info = m_info.createNewObjectStore(name, keyPath, autoIncrement);

    // Create the actual IDBObjectStore from the transaction, which also schedules the operation server side.
    Ref<IDBObjectStore> objectStore = m_versionChangeTransaction->createObjectStore(info);
    return adoptRef(&objectStore.leakRef());
}

RefPtr<WebCore::IDBTransaction> IDBDatabase::transaction(ScriptExecutionContext*, const Vector<String>& objectStores, const String& modeString, ExceptionCode& ec)
{
    LOG(IndexedDB, "IDBDatabase::transaction");

    if (m_closePending) {
        ec = INVALID_STATE_ERR;
        return nullptr;
    }

    if (objectStores.isEmpty()) {
        ec = INVALID_ACCESS_ERR;
        return nullptr;
    }

    IndexedDB::TransactionMode mode = IDBTransaction::stringToMode(modeString, ec);
    if (ec)
        return nullptr;

    if (mode != IndexedDB::TransactionMode::ReadOnly && mode != IndexedDB::TransactionMode::ReadWrite) {
        ec = TypeError;
        return nullptr;
    }

    if (m_versionChangeTransaction) {
        ec = INVALID_STATE_ERR;
        return nullptr;
    }

    for (auto& objectStoreName : objectStores) {
        if (m_info.hasObjectStore(objectStoreName))
            continue;
        ec = NOT_FOUND_ERR;
        return nullptr;
    }

    auto info = IDBTransactionInfo::clientTransaction(m_serverConnection.get(), objectStores, mode);
    auto transaction = IDBTransaction::create(*this, info);

    m_activeTransactions.set(info.identifier(), &transaction.get());

    return adoptRef(&transaction.leakRef());
}

RefPtr<WebCore::IDBTransaction> IDBDatabase::transaction(ScriptExecutionContext* context, const String& objectStore, const String& mode, ExceptionCode& ec)
{
    Vector<String> objectStores(1);
    objectStores[0] = objectStore;
    return transaction(context, objectStores, mode, ec);
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

void IDBDatabase::didStartTransaction(IDBTransaction& transaction)
{
    LOG(IndexedDB, "IDBDatabase::didStartTransaction");
    ASSERT(!m_versionChangeTransaction);

    m_activeTransactions.set(transaction.info().identifier(), &transaction);
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

void IDBDatabase::abortTransaction(IDBTransaction& transaction)
{
    LOG(IndexedDB, "IDBDatabase::abortTransaction");

    auto refTransaction = m_activeTransactions.take(transaction.info().identifier());
    ASSERT(refTransaction);
    m_abortingTransactions.set(transaction.info().identifier(), WTF::move(refTransaction));

    m_serverConnection->abortTransaction(transaction);
}

void IDBDatabase::didAbortTransaction(IDBTransaction& transaction)
{
    LOG(IndexedDB, "IDBDatabase::didAbortTransaction");

    if (transaction.isVersionChange()) {
        ASSERT(transaction.originalDatabaseInfo());
        m_info = *transaction.originalDatabaseInfo();
    }

    didCommitOrAbortTransaction(transaction);
}

void IDBDatabase::didCommitOrAbortTransaction(IDBTransaction& transaction)
{
    LOG(IndexedDB, "IDBDatabase::didCommitOrAbortTransaction");

    if (m_versionChangeTransaction == &transaction)
        m_versionChangeTransaction = nullptr;

#ifndef NDBEBUG
    unsigned count = 0;
    if (m_activeTransactions.contains(transaction.info().identifier()))
        ++count;
    if (m_committingTransactions.contains(transaction.info().identifier()))
        ++count;
    if (m_abortingTransactions.contains(transaction.info().identifier()))
        ++count;

    ASSERT(count == 1);
#endif

    m_activeTransactions.remove(transaction.info().identifier());
    m_committingTransactions.remove(transaction.info().identifier());
    m_abortingTransactions.remove(transaction.info().identifier());
}

void IDBDatabase::fireVersionChangeEvent(uint64_t requestedVersion)
{
    uint64_t currentVersion = m_info.version();
    LOG(IndexedDB, "IDBDatabase::fireVersionChangeEvent - current version %" PRIu64 ", requested version %" PRIu64, currentVersion, requestedVersion);

    if (!scriptExecutionContext())
        return;
    
    Ref<Event> event = IDBVersionChangeEvent::create(currentVersion, requestedVersion, eventNames().versionchangeEvent);
    event->setTarget(this);
    scriptExecutionContext()->eventQueue().enqueueEvent(adoptRef(&event.leakRef()));
}

} // namespace IDBClient
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
