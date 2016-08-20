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
#include "IDBDatabase.h"

#if ENABLE(INDEXED_DATABASE)

#include "DOMStringList.h"
#include "EventNames.h"
#include "EventQueue.h"
#include "IDBConnectionProxy.h"
#include "IDBConnectionToServer.h"
#include "IDBDatabaseException.h"
#include "IDBObjectStore.h"
#include "IDBOpenDBRequest.h"
#include "IDBResultData.h"
#include "IDBTransaction.h"
#include "IDBVersionChangeEvent.h"
#include "Logging.h"
#include "ScriptExecutionContext.h"

namespace WebCore {

Ref<IDBDatabase> IDBDatabase::create(ScriptExecutionContext& context, IDBClient::IDBConnectionProxy& connectionProxy, const IDBResultData& resultData)
{
    return adoptRef(*new IDBDatabase(context, connectionProxy, resultData));
}

IDBDatabase::IDBDatabase(ScriptExecutionContext& context, IDBClient::IDBConnectionProxy& connectionProxy, const IDBResultData& resultData)
    : IDBActiveDOMObject(&context)
    , m_connectionProxy(connectionProxy)
    , m_info(resultData.databaseInfo())
    , m_databaseConnectionIdentifier(resultData.databaseConnectionIdentifier())
{
    LOG(IndexedDB, "IDBDatabase::IDBDatabase - Creating database %s with version %" PRIu64 " connection %" PRIu64 " (%p)", m_info.name().utf8().data(), m_info.version(), m_databaseConnectionIdentifier, this);
    suspendIfNeeded();
    m_connectionProxy->registerDatabaseConnection(*this);
}

IDBDatabase::~IDBDatabase()
{
    ASSERT(currentThread() == originThreadID());

    if (!m_closedInServer)
        m_connectionProxy->databaseConnectionClosed(*this);

    m_connectionProxy->unregisterDatabaseConnection(*this);
}

bool IDBDatabase::hasPendingActivity() const
{
    ASSERT(currentThread() == originThreadID());

    if (m_closedInServer)
        return false;

    if (!m_activeTransactions.isEmpty() || !m_committingTransactions.isEmpty() || !m_abortingTransactions.isEmpty())
        return true;

    return hasEventListeners(eventNames().abortEvent) || hasEventListeners(eventNames().errorEvent) || hasEventListeners(eventNames().versionchangeEvent);
}

const String IDBDatabase::name() const
{
    ASSERT(currentThread() == originThreadID());
    return m_info.name();
}

uint64_t IDBDatabase::version() const
{
    ASSERT(currentThread() == originThreadID());
    return m_info.version();
}

RefPtr<DOMStringList> IDBDatabase::objectStoreNames() const
{
    ASSERT(currentThread() == originThreadID());

    RefPtr<DOMStringList> objectStoreNames = DOMStringList::create();
    for (auto& name : m_info.objectStoreNames())
        objectStoreNames->append(name);
    objectStoreNames->sort();
    return objectStoreNames;
}

RefPtr<WebCore::IDBObjectStore> IDBDatabase::createObjectStore(const String&, const Dictionary&, ExceptionCodeWithMessage&)
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

RefPtr<WebCore::IDBObjectStore> IDBDatabase::createObjectStore(const String& name, const IDBKeyPath& keyPath, bool autoIncrement, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBDatabase::createObjectStore - (%s %s)", m_info.name().utf8().data(), name.utf8().data());

    ASSERT(currentThread() == originThreadID());
    ASSERT(!m_versionChangeTransaction || m_versionChangeTransaction->isVersionChange());

    if (!m_versionChangeTransaction) {
        ec.code = IDBDatabaseException::InvalidStateError;
        ec.message = ASCIILiteral("Failed to execute 'createObjectStore' on 'IDBDatabase': The database is not running a version change transaction.");
        return nullptr;
    }

    if (!m_versionChangeTransaction->isActive()) {
        ec.code = IDBDatabaseException::TransactionInactiveError;
        return nullptr;
    }

    if (m_info.hasObjectStore(name)) {
        ec.code = IDBDatabaseException::ConstraintError;
        ec.message = ASCIILiteral("Failed to execute 'createObjectStore' on 'IDBDatabase': An object store with the specified name already exists.");
        return nullptr;
    }

    if (!keyPath.isNull() && !keyPath.isValid()) {
        ec.code = IDBDatabaseException::SyntaxError;
        ec.message = ASCIILiteral("Failed to execute 'createObjectStore' on 'IDBDatabase': The keyPath option is not a valid key path.");
        return nullptr;
    }

    if (autoIncrement && !keyPath.isNull()) {
        if ((keyPath.type() == IDBKeyPath::Type::String && keyPath.string().isEmpty()) || keyPath.type() == IDBKeyPath::Type::Array) {
            ec.code = IDBDatabaseException::InvalidAccessError;
            ec.message = ASCIILiteral("Failed to execute 'createObjectStore' on 'IDBDatabase': The autoIncrement option was set but the keyPath option was empty or an array.");
            return nullptr;
        }
    }

    // Install the new ObjectStore into the connection's metadata.
    IDBObjectStoreInfo info = m_info.createNewObjectStore(name, keyPath, autoIncrement);

    // Create the actual IDBObjectStore from the transaction, which also schedules the operation server side.
    Ref<IDBObjectStore> objectStore = m_versionChangeTransaction->createObjectStore(info);
    return adoptRef(&objectStore.leakRef());
}

RefPtr<WebCore::IDBTransaction> IDBDatabase::transaction(const Vector<String>& objectStores, const String& modeString, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBDatabase::transaction");

    ASSERT(currentThread() == originThreadID());

    if (m_closePending) {
        ec.code = IDBDatabaseException::InvalidStateError;
        ec.message = ASCIILiteral("Failed to execute 'transaction' on 'IDBDatabase': The database connection is closing.");
        return nullptr;
    }

    if (objectStores.isEmpty()) {
        ec.code = IDBDatabaseException::InvalidAccessError;
        ec.message = ASCIILiteral("Failed to execute 'transaction' on 'IDBDatabase': The storeNames parameter was empty.");
        return nullptr;
    }

    IndexedDB::TransactionMode mode = IDBTransaction::stringToMode(modeString, ec.code);
    if (ec.code) {
        ec.message = makeString(ASCIILiteral("Failed to execute 'transaction' on 'IDBDatabase': The mode provided ('"), modeString, ASCIILiteral("') is not one of 'readonly' or 'readwrite'."));
        return nullptr;
    }

    if (mode != IndexedDB::TransactionMode::ReadOnly && mode != IndexedDB::TransactionMode::ReadWrite) {
        ec.code = TypeError;
        return nullptr;
    }

    if (m_versionChangeTransaction && !m_versionChangeTransaction->isFinishedOrFinishing()) {
        ec.code = IDBDatabaseException::InvalidStateError;
        ec.message = ASCIILiteral("Failed to execute 'transaction' on 'IDBDatabase': A version change transaction is running.");
        return nullptr;
    }

    for (auto& objectStoreName : objectStores) {
        if (m_info.hasObjectStore(objectStoreName))
            continue;
        ec.code = IDBDatabaseException::NotFoundError;
        ec.message = ASCIILiteral("Failed to execute 'transaction' on 'IDBDatabase': One of the specified object stores was not found.");
        return nullptr;
    }

    auto info = IDBTransactionInfo::clientTransaction(m_connectionProxy.get(), objectStores, mode);
    auto transaction = IDBTransaction::create(*this, info);

    LOG(IndexedDB, "IDBDatabase::transaction - Added active transaction %s", info.identifier().loggingString().utf8().data());

    m_activeTransactions.set(info.identifier(), &transaction.get());

    return adoptRef(&transaction.leakRef());
}

RefPtr<WebCore::IDBTransaction> IDBDatabase::transaction(const String& objectStore, const String& mode, ExceptionCodeWithMessage& ec)
{
    ASSERT(currentThread() == originThreadID());

    Vector<String> objectStores(1);
    objectStores[0] = objectStore;
    return transaction(objectStores, mode, ec);
}

void IDBDatabase::deleteObjectStore(const String& objectStoreName, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBDatabase::deleteObjectStore");

    ASSERT(currentThread() == originThreadID());

    if (!m_versionChangeTransaction) {
        ec.code = IDBDatabaseException::InvalidStateError;
        ec.message = ASCIILiteral("Failed to execute 'deleteObjectStore' on 'IDBDatabase': The database is not running a version change transaction.");
        return;
    }

    if (!m_versionChangeTransaction->isActive()) {
        ec.code = IDBDatabaseException::TransactionInactiveError;
        return;
    }

    if (!m_info.hasObjectStore(objectStoreName)) {
        ec.code = IDBDatabaseException::NotFoundError;
        ec.message = ASCIILiteral("Failed to execute 'deleteObjectStore' on 'IDBDatabase': The specified object store was not found.");
        return;
    }

    m_info.deleteObjectStore(objectStoreName);
    m_versionChangeTransaction->deleteObjectStore(objectStoreName);
}

void IDBDatabase::close()
{
    LOG(IndexedDB, "IDBDatabase::close - %" PRIu64, m_databaseConnectionIdentifier);

    ASSERT(currentThread() == originThreadID());

    m_closePending = true;
    maybeCloseInServer();
}

void IDBDatabase::didCloseFromServer(const IDBError& error)
{
    LOG(IndexedDB, "IDBDatabase::didCloseFromServer - %" PRIu64, m_databaseConnectionIdentifier);

    connectionToServerLost(error);

    m_connectionProxy->confirmDidCloseFromServer(*this);
}

void IDBDatabase::connectionToServerLost(const IDBError& error)
{
    LOG(IndexedDB, "IDBDatabase::connectionToServerLost - %" PRIu64, m_databaseConnectionIdentifier);

    ASSERT(currentThread() == originThreadID());

    m_closePending = true;
    m_closedInServer = true;

    for (auto& transaction : m_activeTransactions.values())
        transaction->connectionClosedFromServer(error);

    Ref<Event> event = Event::create(eventNames().errorEvent, true, false);
    event->setTarget(this);

    if (auto* context = scriptExecutionContext())
        context->eventQueue().enqueueEvent(WTFMove(event));
}

void IDBDatabase::maybeCloseInServer()
{
    LOG(IndexedDB, "IDBDatabase::maybeCloseInServer - %" PRIu64, m_databaseConnectionIdentifier);

    ASSERT(currentThread() == originThreadID());

    if (m_closedInServer)
        return;

    // 3.3.9 Database closing steps
    // Wait for all transactions created using this connection to complete.
    // Once they are complete, this connection is closed.
    if (!m_activeTransactions.isEmpty())
        return;

    m_closedInServer = true;
    m_connectionProxy->databaseConnectionClosed(*this);
}

const char* IDBDatabase::activeDOMObjectName() const
{
    ASSERT(currentThread() == originThreadID());
    return "IDBDatabase";
}

bool IDBDatabase::canSuspendForDocumentSuspension() const
{
    ASSERT(currentThread() == originThreadID());

    // FIXME: This value will sometimes be false when database operations are actually in progress.
    // Such database operations do not yet exist.
    return true;
}

void IDBDatabase::stop()
{
    LOG(IndexedDB, "IDBDatabase::stop - %" PRIu64, m_databaseConnectionIdentifier);

    ASSERT(currentThread() == originThreadID());

    removeAllEventListeners();

    Vector<IDBResourceIdentifier> transactionIdentifiers;
    transactionIdentifiers.reserveInitialCapacity(m_activeTransactions.size());

    for (auto& id : m_activeTransactions.keys())
        transactionIdentifiers.uncheckedAppend(id);

    for (auto& id : transactionIdentifiers) {
        IDBTransaction* transaction = m_activeTransactions.get(id);
        if (transaction)
            transaction->stop();
    }

    close();
}

Ref<IDBTransaction> IDBDatabase::startVersionChangeTransaction(const IDBTransactionInfo& info, IDBOpenDBRequest& request)
{
    LOG(IndexedDB, "IDBDatabase::startVersionChangeTransaction %s", info.identifier().loggingString().utf8().data());

    ASSERT(currentThread() == originThreadID());
    ASSERT(!m_versionChangeTransaction);
    ASSERT(info.mode() == IndexedDB::TransactionMode::VersionChange);
    ASSERT(!m_closePending);
    ASSERT(scriptExecutionContext());

    Ref<IDBTransaction> transaction = IDBTransaction::create(*this, info, request);
    m_versionChangeTransaction = &transaction.get();

    m_activeTransactions.set(transaction->info().identifier(), &transaction.get());

    return transaction;
}

void IDBDatabase::didStartTransaction(IDBTransaction& transaction)
{
    LOG(IndexedDB, "IDBDatabase::didStartTransaction %s", transaction.info().identifier().loggingString().utf8().data());
    ASSERT(!m_versionChangeTransaction);
    ASSERT(currentThread() == originThreadID());

    // It is possible for the client to have aborted a transaction before the server replies back that it has started.
    if (m_abortingTransactions.contains(transaction.info().identifier()))
        return;

    m_activeTransactions.set(transaction.info().identifier(), &transaction);
}

void IDBDatabase::willCommitTransaction(IDBTransaction& transaction)
{
    LOG(IndexedDB, "IDBDatabase::willCommitTransaction %s", transaction.info().identifier().loggingString().utf8().data());

    ASSERT(currentThread() == originThreadID());

    auto refTransaction = m_activeTransactions.take(transaction.info().identifier());
    ASSERT(refTransaction);
    m_committingTransactions.set(transaction.info().identifier(), WTFMove(refTransaction));
}

void IDBDatabase::didCommitTransaction(IDBTransaction& transaction)
{
    LOG(IndexedDB, "IDBDatabase::didCommitTransaction %s", transaction.info().identifier().loggingString().utf8().data());

    ASSERT(currentThread() == originThreadID());

    if (m_versionChangeTransaction == &transaction)
        m_info.setVersion(transaction.info().newVersion());

    didCommitOrAbortTransaction(transaction);
}

void IDBDatabase::willAbortTransaction(IDBTransaction& transaction)
{
    LOG(IndexedDB, "IDBDatabase::willAbortTransaction %s", transaction.info().identifier().loggingString().utf8().data());

    ASSERT(currentThread() == originThreadID());

    auto refTransaction = m_activeTransactions.take(transaction.info().identifier());
    if (!refTransaction)
        refTransaction = m_committingTransactions.take(transaction.info().identifier());

    ASSERT(refTransaction);
    m_abortingTransactions.set(transaction.info().identifier(), WTFMove(refTransaction));

    if (transaction.isVersionChange()) {
        ASSERT(transaction.originalDatabaseInfo());
        m_info = *transaction.originalDatabaseInfo();
        m_closePending = true;
    }
}

void IDBDatabase::didAbortTransaction(IDBTransaction& transaction)
{
    LOG(IndexedDB, "IDBDatabase::didAbortTransaction %s", transaction.info().identifier().loggingString().utf8().data());

    ASSERT(currentThread() == originThreadID());

    if (transaction.isVersionChange()) {
        ASSERT(transaction.originalDatabaseInfo());
        ASSERT(m_info.version() == transaction.originalDatabaseInfo()->version());
        m_closePending = true;
        maybeCloseInServer();
    }

    didCommitOrAbortTransaction(transaction);
}

void IDBDatabase::didCommitOrAbortTransaction(IDBTransaction& transaction)
{
    LOG(IndexedDB, "IDBDatabase::didCommitOrAbortTransaction %s", transaction.info().identifier().loggingString().utf8().data());

    ASSERT(currentThread() == originThreadID());

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

    if (m_closePending)
        maybeCloseInServer();
}

void IDBDatabase::fireVersionChangeEvent(const IDBResourceIdentifier& requestIdentifier, uint64_t requestedVersion)
{
    uint64_t currentVersion = m_info.version();
    LOG(IndexedDB, "IDBDatabase::fireVersionChangeEvent - current version %" PRIu64 ", requested version %" PRIu64 ", connection %" PRIu64 " (%p)", currentVersion, requestedVersion, m_databaseConnectionIdentifier, this);

    ASSERT(currentThread() == originThreadID());

    if (!scriptExecutionContext() || m_closePending) {
        connectionProxy().didFireVersionChangeEvent(m_databaseConnectionIdentifier, requestIdentifier);
        return;
    }

    Ref<Event> event = IDBVersionChangeEvent::create(requestIdentifier, currentVersion, requestedVersion, eventNames().versionchangeEvent);
    event->setTarget(this);
    scriptExecutionContext()->eventQueue().enqueueEvent(WTFMove(event));
}

bool IDBDatabase::dispatchEvent(Event& event)
{
    LOG(IndexedDB, "IDBDatabase::dispatchEvent (%" PRIu64 ") (%p)", m_databaseConnectionIdentifier, this);
    ASSERT(currentThread() == originThreadID());

    bool result = EventTargetWithInlineData::dispatchEvent(event);

    if (event.isVersionChangeEvent() && event.type() == eventNames().versionchangeEvent)
        connectionProxy().didFireVersionChangeEvent(m_databaseConnectionIdentifier, downcast<IDBVersionChangeEvent>(event).requestIdentifier());

    return result;
}

void IDBDatabase::didCreateIndexInfo(const IDBIndexInfo& info)
{
    ASSERT(currentThread() == originThreadID());

    auto* objectStore = m_info.infoForExistingObjectStore(info.objectStoreIdentifier());
    ASSERT(objectStore);
    objectStore->addExistingIndex(info);
}

void IDBDatabase::didDeleteIndexInfo(const IDBIndexInfo& info)
{
    ASSERT(currentThread() == originThreadID());

    auto* objectStore = m_info.infoForExistingObjectStore(info.objectStoreIdentifier());
    ASSERT(objectStore);
    objectStore->deleteIndex(info.name());
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
