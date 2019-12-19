/*
 * Copyright (C) 2015, 2016 Apple Inc. All rights reserved.
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
#include "IDBIndex.h"
#include "IDBObjectStore.h"
#include "IDBOpenDBRequest.h"
#include "IDBResultData.h"
#include "IDBTransaction.h"
#include "IDBVersionChangeEvent.h"
#include "Logging.h"
#include "ScriptExecutionContext.h"
#include <JavaScriptCore/HeapInlines.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(IDBDatabase);

Ref<IDBDatabase> IDBDatabase::create(ScriptExecutionContext& context, IDBClient::IDBConnectionProxy& connectionProxy, const IDBResultData& resultData)
{
    return adoptRef(*new IDBDatabase(context, connectionProxy, resultData));
}

IDBDatabase::IDBDatabase(ScriptExecutionContext& context, IDBClient::IDBConnectionProxy& connectionProxy, const IDBResultData& resultData)
    : IDBActiveDOMObject(&context)
    , m_connectionProxy(connectionProxy)
    , m_info(resultData.databaseInfo())
    , m_databaseConnectionIdentifier(resultData.databaseConnectionIdentifier())
    , m_eventNames(eventNames())
{
    LOG(IndexedDB, "IDBDatabase::IDBDatabase - Creating database %s with version %" PRIu64 " connection %" PRIu64 " (%p)", m_info.name().utf8().data(), m_info.version(), m_databaseConnectionIdentifier, this);
    suspendIfNeeded();
    m_connectionProxy->registerDatabaseConnection(*this);
}

IDBDatabase::~IDBDatabase()
{
    ASSERT(canCurrentThreadAccessThreadLocalData(originThread()));

    if (!m_closedInServer)
        m_connectionProxy->databaseConnectionClosed(*this);

    m_connectionProxy->unregisterDatabaseConnection(*this);
}

bool IDBDatabase::hasPendingActivity() const
{
    ASSERT(canCurrentThreadAccessThreadLocalData(originThread()) || Thread::mayBeGCThread());

    if (m_closedInServer || isContextStopped())
        return false;

    if (!m_activeTransactions.isEmpty() || !m_committingTransactions.isEmpty() || !m_abortingTransactions.isEmpty())
        return true;

    return hasEventListeners(m_eventNames.abortEvent) || hasEventListeners(m_eventNames.errorEvent) || hasEventListeners(m_eventNames.versionchangeEvent);
}

const String IDBDatabase::name() const
{
    ASSERT(canCurrentThreadAccessThreadLocalData(originThread()));
    return m_info.name();
}

uint64_t IDBDatabase::version() const
{
    ASSERT(canCurrentThreadAccessThreadLocalData(originThread()));
    return m_info.version();
}

Ref<DOMStringList> IDBDatabase::objectStoreNames() const
{
    ASSERT(canCurrentThreadAccessThreadLocalData(originThread()));

    auto objectStoreNames = DOMStringList::create();
    for (auto& name : m_info.objectStoreNames())
        objectStoreNames->append(name);
    objectStoreNames->sort();
    return objectStoreNames;
}

void IDBDatabase::renameObjectStore(IDBObjectStore& objectStore, const String& newName)
{
    ASSERT(canCurrentThreadAccessThreadLocalData(originThread()));
    ASSERT(m_versionChangeTransaction);
    ASSERT(m_info.hasObjectStore(objectStore.info().name()));

    m_info.renameObjectStore(objectStore.info().identifier(), newName);

    m_versionChangeTransaction->renameObjectStore(objectStore, newName);
}

void IDBDatabase::renameIndex(IDBIndex& index, const String& newName)
{
    ASSERT(canCurrentThreadAccessThreadLocalData(originThread()));
    ASSERT(m_versionChangeTransaction);
    ASSERT(m_info.hasObjectStore(index.objectStore().info().name()));
    ASSERT(m_info.infoForExistingObjectStore(index.objectStore().info().name())->hasIndex(index.info().name()));

    m_info.infoForExistingObjectStore(index.objectStore().info().name())->infoForExistingIndex(index.info().identifier())->rename(newName);

    m_versionChangeTransaction->renameIndex(index, newName);
}

ExceptionOr<Ref<IDBObjectStore>> IDBDatabase::createObjectStore(const String& name, ObjectStoreParameters&& parameters)
{
    LOG(IndexedDB, "IDBDatabase::createObjectStore - (%s %s)", m_info.name().utf8().data(), name.utf8().data());

    ASSERT(canCurrentThreadAccessThreadLocalData(originThread()));
    ASSERT(!m_versionChangeTransaction || m_versionChangeTransaction->isVersionChange());

    if (!m_versionChangeTransaction)
        return Exception { InvalidStateError, "Failed to execute 'createObjectStore' on 'IDBDatabase': The database is not running a version change transaction."_s };

    if (!m_versionChangeTransaction->isActive())
        return Exception { TransactionInactiveError };

    auto& keyPath = parameters.keyPath;
    if (keyPath && !isIDBKeyPathValid(keyPath.value()))
        return Exception { SyntaxError, "Failed to execute 'createObjectStore' on 'IDBDatabase': The keyPath option is not a valid key path."_s };

    if (m_info.hasObjectStore(name))
        return Exception { ConstraintError, "Failed to execute 'createObjectStore' on 'IDBDatabase': An object store with the specified name already exists."_s };

    if (keyPath && parameters.autoIncrement && ((WTF::holds_alternative<String>(keyPath.value()) && WTF::get<String>(keyPath.value()).isEmpty()) || WTF::holds_alternative<Vector<String>>(keyPath.value())))
        return Exception { InvalidAccessError, "Failed to execute 'createObjectStore' on 'IDBDatabase': The autoIncrement option was set but the keyPath option was empty or an array."_s };

    // Install the new ObjectStore into the connection's metadata.
    auto info = m_info.createNewObjectStore(name, WTFMove(keyPath), parameters.autoIncrement);

    // Create the actual IDBObjectStore from the transaction, which also schedules the operation server side.
    return m_versionChangeTransaction->createObjectStore(info);
}

ExceptionOr<Ref<IDBTransaction>> IDBDatabase::transaction(StringOrVectorOfStrings&& storeNames, IDBTransactionMode mode)
{
    LOG(IndexedDB, "IDBDatabase::transaction");

    ASSERT(canCurrentThreadAccessThreadLocalData(originThread()));

    if (m_versionChangeTransaction && !m_versionChangeTransaction->isFinishedOrFinishing())
        return Exception { InvalidStateError, "Failed to execute 'transaction' on 'IDBDatabase': A version change transaction is running."_s };

    if (m_closePending)
        return Exception { InvalidStateError, "Failed to execute 'transaction' on 'IDBDatabase': The database connection is closing."_s };

    Vector<String> objectStores;
    if (WTF::holds_alternative<Vector<String>>(storeNames))
        objectStores = WTFMove(WTF::get<Vector<String>>(storeNames));
    else
        objectStores.append(WTFMove(WTF::get<String>(storeNames)));

    // It is valid for javascript to pass in a list of object store names with the same name listed twice,
    // so we need to put them all in a set to get a unique list.
    HashSet<String> objectStoreSet;
    for (auto& objectStore : objectStores)
        objectStoreSet.add(objectStore);

    objectStores = copyToVector(objectStoreSet);

    for (auto& objectStoreName : objectStores) {
        if (m_info.hasObjectStore(objectStoreName))
            continue;
        return Exception { NotFoundError, "Failed to execute 'transaction' on 'IDBDatabase': One of the specified object stores was not found."_s };
    }

    if (objectStores.isEmpty())
        return Exception { InvalidAccessError, "Failed to execute 'transaction' on 'IDBDatabase': The storeNames parameter was empty."_s };

    if (mode != IDBTransactionMode::Readonly && mode != IDBTransactionMode::Readwrite)
        return Exception { TypeError };

    auto info = IDBTransactionInfo::clientTransaction(m_connectionProxy.get(), objectStores, mode);

    LOG(IndexedDBOperations, "IDB creating transaction: %s", info.loggingString().utf8().data());
    auto transaction = IDBTransaction::create(*this, info);

    LOG(IndexedDB, "IDBDatabase::transaction - Added active transaction %s", info.identifier().loggingString().utf8().data());

    m_activeTransactions.set(info.identifier(), transaction.ptr());

    return transaction;
}

ExceptionOr<void> IDBDatabase::deleteObjectStore(const String& objectStoreName)
{
    LOG(IndexedDB, "IDBDatabase::deleteObjectStore");

    ASSERT(canCurrentThreadAccessThreadLocalData(originThread()));

    if (!m_versionChangeTransaction)
        return Exception { InvalidStateError, "Failed to execute 'deleteObjectStore' on 'IDBDatabase': The database is not running a version change transaction."_s };

    if (!m_versionChangeTransaction->isActive())
        return Exception { TransactionInactiveError };

    if (!m_info.hasObjectStore(objectStoreName))
        return Exception { NotFoundError, "Failed to execute 'deleteObjectStore' on 'IDBDatabase': The specified object store was not found."_s };

    m_info.deleteObjectStore(objectStoreName);
    m_versionChangeTransaction->deleteObjectStore(objectStoreName);

    return { };
}

void IDBDatabase::close()
{
    LOG(IndexedDB, "IDBDatabase::close - %" PRIu64, m_databaseConnectionIdentifier);

    ASSERT(canCurrentThreadAccessThreadLocalData(originThread()));

    if (!m_closePending) {
        m_closePending = true;
        m_connectionProxy->databaseConnectionPendingClose(*this);
    }

    maybeCloseInServer();
}

void IDBDatabase::didCloseFromServer(const IDBError& error)
{
    LOG(IndexedDB, "IDBDatabase::didCloseFromServer - %" PRIu64, m_databaseConnectionIdentifier);

    connectionToServerLost(error);
}

void IDBDatabase::connectionToServerLost(const IDBError& error)
{
    LOG(IndexedDB, "IDBDatabase::connectionToServerLost - %" PRIu64, m_databaseConnectionIdentifier);

    ASSERT(canCurrentThreadAccessThreadLocalData(originThread()));

    m_closePending = true;
    m_closedInServer = true;

    auto activeTransactions = copyToVector(m_activeTransactions.values());
    for (auto& transaction : activeTransactions)
        transaction->connectionClosedFromServer(error);
    
    auto committingTransactions = copyToVector(m_committingTransactions.values());
    for (auto& transaction : committingTransactions)
        transaction->connectionClosedFromServer(error);

    auto errorEvent = Event::create(m_eventNames.errorEvent, Event::CanBubble::Yes, Event::IsCancelable::No);
    errorEvent->setTarget(this);

    if (scriptExecutionContext())
        queueTaskToDispatchEvent(*this, TaskSource::DatabaseAccess, WTFMove(errorEvent));

    auto closeEvent = Event::create(m_eventNames.closeEvent, Event::CanBubble::Yes, Event::IsCancelable::No);
    closeEvent->setTarget(this);

    if (scriptExecutionContext())
        queueTaskToDispatchEvent(*this, TaskSource::DatabaseAccess, WTFMove(closeEvent));
}

void IDBDatabase::maybeCloseInServer()
{
    LOG(IndexedDB, "IDBDatabase::maybeCloseInServer - %" PRIu64, m_databaseConnectionIdentifier);

    ASSERT(canCurrentThreadAccessThreadLocalData(originThread()));

    if (m_closedInServer)
        return;

    // 3.3.9 Database closing steps
    // Wait for all transactions created using this connection to complete.
    // Once they are complete, this connection is closed.
    if (!m_activeTransactions.isEmpty() || !m_committingTransactions.isEmpty())
        return;

    m_closedInServer = true;
    m_connectionProxy->databaseConnectionClosed(*this);
}

const char* IDBDatabase::activeDOMObjectName() const
{
    ASSERT(canCurrentThreadAccessThreadLocalData(originThread()));
    return "IDBDatabase";
}

void IDBDatabase::stop()
{
    LOG(IndexedDB, "IDBDatabase::stop - %" PRIu64, m_databaseConnectionIdentifier);

    ASSERT(canCurrentThreadAccessThreadLocalData(originThread()));

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

    ASSERT(canCurrentThreadAccessThreadLocalData(originThread()));
    ASSERT(!m_versionChangeTransaction);
    ASSERT(info.mode() == IDBTransactionMode::Versionchange);
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
    ASSERT(canCurrentThreadAccessThreadLocalData(originThread()));

    // It is possible for the client to have aborted a transaction before the server replies back that it has started.
    if (m_abortingTransactions.contains(transaction.info().identifier()))
        return;

    m_activeTransactions.set(transaction.info().identifier(), &transaction);
}

void IDBDatabase::willCommitTransaction(IDBTransaction& transaction)
{
    LOG(IndexedDB, "IDBDatabase::willCommitTransaction %s", transaction.info().identifier().loggingString().utf8().data());

    ASSERT(canCurrentThreadAccessThreadLocalData(originThread()));

    auto refTransaction = m_activeTransactions.take(transaction.info().identifier());
    ASSERT(refTransaction);
    m_committingTransactions.set(transaction.info().identifier(), WTFMove(refTransaction));
}

void IDBDatabase::didCommitTransaction(IDBTransaction& transaction)
{
    LOG(IndexedDB, "IDBDatabase::didCommitTransaction %s", transaction.info().identifier().loggingString().utf8().data());

    ASSERT(canCurrentThreadAccessThreadLocalData(originThread()));

    if (m_versionChangeTransaction == &transaction)
        m_info.setVersion(transaction.info().newVersion());

    didCommitOrAbortTransaction(transaction);
}

void IDBDatabase::willAbortTransaction(IDBTransaction& transaction)
{
    LOG(IndexedDB, "IDBDatabase::willAbortTransaction %s", transaction.info().identifier().loggingString().utf8().data());

    ASSERT(canCurrentThreadAccessThreadLocalData(originThread()));

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

    ASSERT(canCurrentThreadAccessThreadLocalData(originThread()));

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

    ASSERT(canCurrentThreadAccessThreadLocalData(originThread()));

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

    ASSERT(canCurrentThreadAccessThreadLocalData(originThread()));

    if (!scriptExecutionContext() || m_closePending) {
        connectionProxy().didFireVersionChangeEvent(m_databaseConnectionIdentifier, requestIdentifier);
        return;
    }
    
    Ref<Event> event = IDBVersionChangeEvent::create(requestIdentifier, currentVersion, requestedVersion, m_eventNames.versionchangeEvent);
    queueTaskToDispatchEvent(*this, TaskSource::DatabaseAccess, WTFMove(event));
}

void IDBDatabase::dispatchEvent(Event& event)
{
    LOG(IndexedDB, "IDBDatabase::dispatchEvent (%" PRIu64 ") (%p)", m_databaseConnectionIdentifier, this);
    ASSERT(canCurrentThreadAccessThreadLocalData(originThread()));

    auto protectedThis = makeRef(*this);

    EventTargetWithInlineData::dispatchEvent(event);

    if (event.isVersionChangeEvent() && event.type() == m_eventNames.versionchangeEvent)
        m_connectionProxy->didFireVersionChangeEvent(m_databaseConnectionIdentifier, downcast<IDBVersionChangeEvent>(event).requestIdentifier());
}

void IDBDatabase::didCreateIndexInfo(const IDBIndexInfo& info)
{
    ASSERT(canCurrentThreadAccessThreadLocalData(originThread()));

    auto* objectStore = m_info.infoForExistingObjectStore(info.objectStoreIdentifier());
    ASSERT(objectStore);
    objectStore->addExistingIndex(info);
}

void IDBDatabase::didDeleteIndexInfo(const IDBIndexInfo& info)
{
    ASSERT(canCurrentThreadAccessThreadLocalData(originThread()));

    auto* objectStore = m_info.infoForExistingObjectStore(info.objectStoreIdentifier());
    ASSERT(objectStore);
    objectStore->deleteIndex(info.name());
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
