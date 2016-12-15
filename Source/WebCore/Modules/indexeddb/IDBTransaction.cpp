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
#include "IDBTransaction.h"

#if ENABLE(INDEXED_DATABASE)

#include "DOMError.h"
#include "DOMStringList.h"
#include "DOMWindow.h"
#include "Event.h"
#include "EventNames.h"
#include "EventQueue.h"
#include "IDBCursorWithValue.h"
#include "IDBDatabase.h"
#include "IDBDatabaseException.h"
#include "IDBError.h"
#include "IDBEventDispatcher.h"
#include "IDBGetRecordData.h"
#include "IDBIndex.h"
#include "IDBIterateCursorData.h"
#include "IDBKeyData.h"
#include "IDBKeyRangeData.h"
#include "IDBObjectStore.h"
#include "IDBOpenDBRequest.h"
#include "IDBRequest.h"
#include "IDBResultData.h"
#include "IDBValue.h"
#include "JSDOMWindowBase.h"
#include "Logging.h"
#include "ScriptExecutionContext.h"
#include "ScriptState.h"
#include "SerializedScriptValue.h"
#include "TransactionOperation.h"
#include <wtf/NeverDestroyed.h>

using namespace JSC;

namespace WebCore {

Ref<IDBTransaction> IDBTransaction::create(IDBDatabase& database, const IDBTransactionInfo& info)
{
    return adoptRef(*new IDBTransaction(database, info, nullptr));
}

Ref<IDBTransaction> IDBTransaction::create(IDBDatabase& database, const IDBTransactionInfo& info, IDBOpenDBRequest& request)
{
    return adoptRef(*new IDBTransaction(database, info, &request));
}

IDBTransaction::IDBTransaction(IDBDatabase& database, const IDBTransactionInfo& info, IDBOpenDBRequest* request)
    : IDBActiveDOMObject(database.scriptExecutionContext())
    , m_database(database)
    , m_info(info)
    , m_pendingOperationTimer(*this, &IDBTransaction::pendingOperationTimerFired)
    , m_completedOperationTimer(*this, &IDBTransaction::completedOperationTimerFired)
    , m_openDBRequest(request)
    , m_currentlyCompletingRequest(request)

{
    LOG(IndexedDB, "IDBTransaction::IDBTransaction - %s", m_info.loggingString().utf8().data());
    ASSERT(currentThread() == m_database->originThreadID());

    if (m_info.mode() == IDBTransactionMode::Versionchange) {
        ASSERT(m_openDBRequest);
        m_openDBRequest->setVersionChangeTransaction(*this);
        m_startedOnServer = true;
    } else {
        activate();

        auto* context = scriptExecutionContext();
        ASSERT(context);

        RefPtr<IDBTransaction> self;
        JSC::VM& vm = context->vm();
        vm.whenIdle([self, this]() {
            deactivate();
        });

        establishOnServer();
    }

    suspendIfNeeded();
}

IDBTransaction::~IDBTransaction()
{
    ASSERT(currentThread() == m_database->originThreadID());
}

IDBClient::IDBConnectionProxy& IDBTransaction::connectionProxy()
{
    return m_database->connectionProxy();
}

Ref<DOMStringList> IDBTransaction::objectStoreNames() const
{
    ASSERT(currentThread() == m_database->originThreadID());

    const Vector<String> names = isVersionChange() ? m_database->info().objectStoreNames() : m_info.objectStores();

    Ref<DOMStringList> objectStoreNames = DOMStringList::create();
    for (auto& name : names)
        objectStoreNames->append(name);

    objectStoreNames->sort();
    return objectStoreNames;
}

IDBDatabase* IDBTransaction::db()
{
    ASSERT(currentThread() == m_database->originThreadID());
    return m_database.ptr();
}

DOMError* IDBTransaction::error() const
{
    ASSERT(currentThread() == m_database->originThreadID());
    return m_domError.get();
}

ExceptionOr<Ref<IDBObjectStore>> IDBTransaction::objectStore(const String& objectStoreName)
{
    LOG(IndexedDB, "IDBTransaction::objectStore");
    ASSERT(currentThread() == m_database->originThreadID());

    if (!scriptExecutionContext())
        return Exception { IDBDatabaseException::InvalidStateError };

    if (isFinishedOrFinishing())
        return Exception { IDBDatabaseException::InvalidStateError, ASCIILiteral("Failed to execute 'objectStore' on 'IDBTransaction': The transaction finished.") };

    Locker<Lock> locker(m_referencedObjectStoreLock);

    auto iterator = m_referencedObjectStores.find(objectStoreName);
    if (iterator != m_referencedObjectStores.end())
        return Ref<IDBObjectStore> { *iterator->value };

    bool found = false;
    for (auto& objectStore : m_info.objectStores()) {
        if (objectStore == objectStoreName) {
            found = true;
            break;
        }
    }

    auto* info = m_database->info().infoForExistingObjectStore(objectStoreName);
    if (!info)
        return Exception { IDBDatabaseException::NotFoundError, ASCIILiteral("Failed to execute 'objectStore' on 'IDBTransaction': The specified object store was not found.") };

    // Version change transactions are scoped to every object store in the database.
    if (!info || (!found && !isVersionChange()))
        return Exception { IDBDatabaseException::NotFoundError, ASCIILiteral("Failed to execute 'objectStore' on 'IDBTransaction': The specified object store was not found.") };

    auto objectStore = std::make_unique<IDBObjectStore>(*scriptExecutionContext(), *info, *this);
    auto* rawObjectStore = objectStore.get();
    m_referencedObjectStores.set(objectStoreName, WTFMove(objectStore));

    return Ref<IDBObjectStore>(*rawObjectStore);
}


void IDBTransaction::abortDueToFailedRequest(DOMError& error)
{
    LOG(IndexedDB, "IDBTransaction::abortDueToFailedRequest");
    ASSERT(currentThread() == m_database->originThreadID());

    if (isFinishedOrFinishing())
        return;

    m_domError = &error;
    internalAbort();
}

void IDBTransaction::transitionedToFinishing(IndexedDB::TransactionState state)
{
    ASSERT(currentThread() == m_database->originThreadID());

    ASSERT(!isFinishedOrFinishing());
    m_state = state;
    ASSERT(isFinishedOrFinishing());
}

ExceptionOr<void> IDBTransaction::abort()
{
    LOG(IndexedDB, "IDBTransaction::abort");
    ASSERT(currentThread() == m_database->originThreadID());

    if (isFinishedOrFinishing())
        return Exception { IDBDatabaseException::InvalidStateError, ASCIILiteral("Failed to execute 'abort' on 'IDBTransaction': The transaction is inactive or finished.") };

    internalAbort();

    return { };
}

void IDBTransaction::internalAbort()
{
    LOG(IndexedDB, "IDBTransaction::internalAbort");
    ASSERT(currentThread() == m_database->originThreadID());
    ASSERT(!isFinishedOrFinishing());

    m_database->willAbortTransaction(*this);

    if (isVersionChange()) {
        Locker<Lock> locker(m_referencedObjectStoreLock);

        auto& info = m_database->info();
        Vector<uint64_t> identifiersToRemove;
        for (auto& iterator : m_deletedObjectStores) {
            if (info.infoForExistingObjectStore(iterator.key)) {
                auto name = iterator.value->info().name();
                m_referencedObjectStores.set(name, WTFMove(iterator.value));
                identifiersToRemove.append(iterator.key);
            }
        }

        for (auto identifier : identifiersToRemove)
            m_deletedObjectStores.remove(identifier);

        for (auto& objectStore : m_referencedObjectStores.values())
            objectStore->rollbackForVersionChangeAbort();
    }

    transitionedToFinishing(IndexedDB::TransactionState::Aborting);
    
    m_abortQueue.swap(m_pendingTransactionOperationQueue);

    LOG(IndexedDBOperations, "IDB abort-on-server operation: Transaction %s", info().identifier().loggingString().utf8().data());
    scheduleOperation(IDBClient::createTransactionOperation(*this, nullptr, &IDBTransaction::abortOnServerAndCancelRequests));
}

void IDBTransaction::abortInProgressOperations(const IDBError& error)
{
    LOG(IndexedDB, "IDBTransaction::abortInProgressOperations");

    Vector<RefPtr<IDBClient::TransactionOperation>> inProgressAbortVector;
    inProgressAbortVector.reserveInitialCapacity(m_transactionOperationsInProgressQueue.size());
    while (!m_transactionOperationsInProgressQueue.isEmpty())
        inProgressAbortVector.uncheckedAppend(m_transactionOperationsInProgressQueue.takeFirst());

    for (auto& operation : inProgressAbortVector) {
        m_transactionOperationsInProgressQueue.append(operation.get());
        m_currentlyCompletingRequest = nullptr;
        operation->doComplete(IDBResultData::error(operation->identifier(), error));
    }

    Vector<RefPtr<IDBClient::TransactionOperation>> completedOnServerAbortVector;
    completedOnServerAbortVector.reserveInitialCapacity(m_completedOnServerQueue.size());
    while (!m_completedOnServerQueue.isEmpty())
        completedOnServerAbortVector.uncheckedAppend(m_completedOnServerQueue.takeFirst().first);

    for (auto& operation : completedOnServerAbortVector) {
        m_currentlyCompletingRequest = nullptr;
        operation->doComplete(IDBResultData::error(operation->identifier(), error));
    }

    connectionProxy().forgetActiveOperations(inProgressAbortVector);
}

void IDBTransaction::abortOnServerAndCancelRequests(IDBClient::TransactionOperation& operation)
{
    LOG(IndexedDB, "IDBTransaction::abortOnServerAndCancelRequests");
    ASSERT(currentThread() == m_database->originThreadID());
    ASSERT(m_pendingTransactionOperationQueue.isEmpty());

    m_database->connectionProxy().abortTransaction(*this);

    ASSERT(m_transactionOperationMap.contains(operation.identifier()));
    ASSERT(m_transactionOperationsInProgressQueue.last() == &operation);
    m_transactionOperationMap.remove(operation.identifier());
    m_transactionOperationsInProgressQueue.removeLast();

    m_currentlyCompletingRequest = nullptr;
    
    IDBError error(IDBDatabaseException::AbortError);

    abortInProgressOperations(error);

    for (auto& operation : m_abortQueue) {
        m_currentlyCompletingRequest = nullptr;
        m_transactionOperationsInProgressQueue.append(operation.get());
        operation->doComplete(IDBResultData::error(operation->identifier(), error));
    }

    // Since we're aborting, it should be impossible to have queued any further operations.
    ASSERT(m_pendingTransactionOperationQueue.isEmpty());
}

const char* IDBTransaction::activeDOMObjectName() const
{
    ASSERT(currentThread() == m_database->originThreadID());
    return "IDBTransaction";
}

bool IDBTransaction::canSuspendForDocumentSuspension() const
{
    ASSERT(currentThread() == m_database->originThreadID());
    return false;
}

bool IDBTransaction::hasPendingActivity() const
{
    ASSERT(currentThread() == m_database->originThreadID() || mayBeGCThread());
    return !m_contextStopped && m_state != IndexedDB::TransactionState::Finished;
}

void IDBTransaction::stop()
{
    LOG(IndexedDB, "IDBTransaction::stop - %s", m_info.loggingString().utf8().data());
    ASSERT(currentThread() == m_database->originThreadID());

    // IDBDatabase::stop() calls IDBTransaction::stop() for each of its active transactions.
    // Since the order of calling ActiveDOMObject::stop() is random, we might already have been stopped.
    if (m_contextStopped)
        return;

    removeAllEventListeners();

    m_contextStopped = true;

    if (isFinishedOrFinishing())
        return;

    internalAbort();
}

bool IDBTransaction::isActive() const
{
    ASSERT(currentThread() == m_database->originThreadID());
    return m_state == IndexedDB::TransactionState::Active;
}

bool IDBTransaction::isFinishedOrFinishing() const
{
    ASSERT(currentThread() == m_database->originThreadID());

    return m_state == IndexedDB::TransactionState::Committing
        || m_state == IndexedDB::TransactionState::Aborting
        || m_state == IndexedDB::TransactionState::Finished;
}

void IDBTransaction::addRequest(IDBRequest& request)
{
    ASSERT(currentThread() == m_database->originThreadID());
    m_openRequests.add(&request);
}

void IDBTransaction::removeRequest(IDBRequest& request)
{
    ASSERT(currentThread() == m_database->originThreadID());
    ASSERT(m_openRequests.contains(&request));
    m_openRequests.remove(&request);
}

void IDBTransaction::scheduleOperation(RefPtr<IDBClient::TransactionOperation>&& operation)
{
    ASSERT(!m_transactionOperationMap.contains(operation->identifier()));
    ASSERT(currentThread() == m_database->originThreadID());

    m_pendingTransactionOperationQueue.append(operation);
    m_transactionOperationMap.set(operation->identifier(), WTFMove(operation));

    schedulePendingOperationTimer();
}

void IDBTransaction::schedulePendingOperationTimer()
{
    ASSERT(currentThread() == m_database->originThreadID());

    if (!m_pendingOperationTimer.isActive())
        m_pendingOperationTimer.startOneShot(0);
}

void IDBTransaction::pendingOperationTimerFired()
{
    LOG(IndexedDB, "IDBTransaction::pendingOperationTimerFired (%p)", this);
    ASSERT(currentThread() == m_database->originThreadID());

    if (!m_startedOnServer)
        return;

    // If the last in-progress operation we've sent to the server is not an IDBRequest operation,
    // then we have to wait until it completes before sending any more.
    if (!m_transactionOperationsInProgressQueue.isEmpty() && !m_transactionOperationsInProgressQueue.last()->nextRequestCanGoToServer())
        return;

    // We want to batch operations together without spinning the runloop for performance,
    // but don't want to affect responsiveness of the main thread.
    // This number is a good compromise in ad-hoc testing.
    static const size_t operationBatchLimit = 128;

    for (size_t iterations = 0; !m_pendingTransactionOperationQueue.isEmpty() && iterations < operationBatchLimit; ++iterations) {
        auto operation = m_pendingTransactionOperationQueue.takeFirst();
        m_transactionOperationsInProgressQueue.append(operation.get());
        operation->perform();

        if (!operation->nextRequestCanGoToServer())
            break;

    }

    if (!m_transactionOperationMap.isEmpty() || !m_openRequests.isEmpty())
        return;

    if (!isFinishedOrFinishing())
        commit();
}

void IDBTransaction::operationCompletedOnServer(const IDBResultData& data, IDBClient::TransactionOperation& operation)
{
    ASSERT(currentThread() == m_database->originThreadID());
    ASSERT(currentThread() == operation.originThreadID());

    m_completedOnServerQueue.append({ &operation, data });
    scheduleCompletedOperationTimer();
}

void IDBTransaction::scheduleCompletedOperationTimer()
{
    ASSERT(currentThread() == m_database->originThreadID());

    if (!m_completedOperationTimer.isActive())
        m_completedOperationTimer.startOneShot(0);
}

void IDBTransaction::completedOperationTimerFired()
{
    LOG(IndexedDB, "IDBTransaction::completedOperationTimerFired (%p)", this);
    ASSERT(currentThread() == m_database->originThreadID());

    if (m_completedOnServerQueue.isEmpty() || m_currentlyCompletingRequest)
        return;

    auto iterator = m_completedOnServerQueue.takeFirst();
    iterator.first->doComplete(iterator.second);

    if (!m_completedOnServerQueue.isEmpty() && !m_currentlyCompletingRequest)
        scheduleCompletedOperationTimer();
}

void IDBTransaction::completeNoncursorRequest(IDBRequest& request, const IDBResultData& result)
{
    ASSERT(!m_currentlyCompletingRequest);

    request.completeRequestAndDispatchEvent(result);

    m_currentlyCompletingRequest = &request;
}

void IDBTransaction::completeCursorRequest(IDBRequest& request, const IDBResultData& result)
{
    ASSERT(!m_currentlyCompletingRequest);

    request.didOpenOrIterateCursor(result);

    m_currentlyCompletingRequest = &request;
}

void IDBTransaction::finishedDispatchEventForRequest(IDBRequest& request)
{
    if (isFinishedOrFinishing())
        return;

    ASSERT_UNUSED(request, !m_currentlyCompletingRequest || m_currentlyCompletingRequest == &request);

    m_currentlyCompletingRequest = nullptr;
    scheduleCompletedOperationTimer();
}

void IDBTransaction::commit()
{
    LOG(IndexedDB, "IDBTransaction::commit");
    ASSERT(currentThread() == m_database->originThreadID());
    ASSERT(!isFinishedOrFinishing());

    transitionedToFinishing(IndexedDB::TransactionState::Committing);
    m_database->willCommitTransaction(*this);

    LOG(IndexedDBOperations, "IDB commit operation: Transaction %s", info().identifier().loggingString().utf8().data());
    scheduleOperation(IDBClient::createTransactionOperation(*this, nullptr, &IDBTransaction::commitOnServer));
}

void IDBTransaction::commitOnServer(IDBClient::TransactionOperation& operation)
{
    LOG(IndexedDB, "IDBTransaction::commitOnServer");
    ASSERT(currentThread() == m_database->originThreadID());

    m_database->connectionProxy().commitTransaction(*this);

    ASSERT(!m_transactionOperationsInProgressQueue.isEmpty());
    ASSERT(m_transactionOperationsInProgressQueue.last() == &operation);
    m_transactionOperationsInProgressQueue.removeLast();

    ASSERT(m_transactionOperationMap.contains(operation.identifier()));
    m_transactionOperationMap.remove(operation.identifier());
}

void IDBTransaction::finishAbortOrCommit()
{
    ASSERT(m_state != IndexedDB::TransactionState::Finished);
    ASSERT(currentThread() == m_database->originThreadID());

    m_state = IndexedDB::TransactionState::Finished;
}

void IDBTransaction::didStart(const IDBError& error)
{
    LOG(IndexedDB, "IDBTransaction::didStart");
    ASSERT(currentThread() == m_database->originThreadID());

    m_database->didStartTransaction(*this);

    m_startedOnServer = true;

    // It's possible the transaction failed to start on the server.
    // That equates to an abort.
    if (!error.isNull()) {
        didAbort(error);
        return;
    }

    schedulePendingOperationTimer();
}

void IDBTransaction::notifyDidAbort(const IDBError& error)
{
    ASSERT(currentThread() == m_database->originThreadID());

    m_database->didAbortTransaction(*this);
    m_idbError = error;
    fireOnAbort();

    if (isVersionChange()) {
        ASSERT(m_openDBRequest);
        m_openDBRequest->fireErrorAfterVersionChangeCompletion();
    }
}

void IDBTransaction::didAbort(const IDBError& error)
{
    LOG(IndexedDB, "IDBTransaction::didAbort");
    ASSERT(currentThread() == m_database->originThreadID());

    if (m_state == IndexedDB::TransactionState::Finished)
        return;

    notifyDidAbort(error);

    finishAbortOrCommit();
}

void IDBTransaction::didCommit(const IDBError& error)
{
    LOG(IndexedDB, "IDBTransaction::didCommit");
    ASSERT(currentThread() == m_database->originThreadID());
    ASSERT(m_state == IndexedDB::TransactionState::Committing);

    if (error.isNull()) {
        m_database->didCommitTransaction(*this);
        fireOnComplete();
    } else {
        m_database->willAbortTransaction(*this);
        notifyDidAbort(error);
    }

    finishAbortOrCommit();
}

void IDBTransaction::fireOnComplete()
{
    LOG(IndexedDB, "IDBTransaction::fireOnComplete");
    ASSERT(currentThread() == m_database->originThreadID());
    enqueueEvent(Event::create(eventNames().completeEvent, false, false));
}

void IDBTransaction::fireOnAbort()
{
    LOG(IndexedDB, "IDBTransaction::fireOnAbort");
    ASSERT(currentThread() == m_database->originThreadID());
    enqueueEvent(Event::create(eventNames().abortEvent, true, false));
}

void IDBTransaction::enqueueEvent(Ref<Event>&& event)
{
    ASSERT(m_state != IndexedDB::TransactionState::Finished);
    ASSERT(currentThread() == m_database->originThreadID());

    if (!scriptExecutionContext() || m_contextStopped)
        return;

    event->setTarget(this);
    scriptExecutionContext()->eventQueue().enqueueEvent(WTFMove(event));
}

bool IDBTransaction::dispatchEvent(Event& event)
{
    LOG(IndexedDB, "IDBTransaction::dispatchEvent");

    ASSERT(currentThread() == m_database->originThreadID());
    ASSERT(scriptExecutionContext());
    ASSERT(!m_contextStopped);
    ASSERT(event.target() == this);
    ASSERT(event.type() == eventNames().completeEvent || event.type() == eventNames().abortEvent);

    Vector<RefPtr<EventTarget>> targets;
    targets.append(this);
    targets.append(db());

    bool result = IDBEventDispatcher::dispatch(event, targets);

    if (isVersionChange()) {
        ASSERT(m_openDBRequest);
        m_openDBRequest->versionChangeTransactionDidFinish();

        if (event.type() == eventNames().completeEvent) {
            if (m_database->isClosingOrClosed())
                m_openDBRequest->fireErrorAfterVersionChangeCompletion();
            else
                m_openDBRequest->fireSuccessAfterVersionChangeCommit();
        }

        m_openDBRequest = nullptr;
    }

    return result;
}

Ref<IDBObjectStore> IDBTransaction::createObjectStore(const IDBObjectStoreInfo& info)
{
    LOG(IndexedDB, "IDBTransaction::createObjectStore");
    ASSERT(isVersionChange());
    ASSERT(scriptExecutionContext());
    ASSERT(currentThread() == m_database->originThreadID());

    Locker<Lock> locker(m_referencedObjectStoreLock);

    auto objectStore = std::make_unique<IDBObjectStore>(*scriptExecutionContext(), info, *this);
    auto* rawObjectStore = objectStore.get();
    m_referencedObjectStores.set(info.name(), WTFMove(objectStore));

    LOG(IndexedDBOperations, "IDB create object store operation: %s", info.condensedLoggingString().utf8().data());
    scheduleOperation(IDBClient::createTransactionOperation(*this, &IDBTransaction::didCreateObjectStoreOnServer, &IDBTransaction::createObjectStoreOnServer, info));

    return *rawObjectStore;
}

void IDBTransaction::createObjectStoreOnServer(IDBClient::TransactionOperation& operation, const IDBObjectStoreInfo& info)
{
    LOG(IndexedDB, "IDBTransaction::createObjectStoreOnServer");
    ASSERT(currentThread() == m_database->originThreadID());
    ASSERT(isVersionChange());

    m_database->connectionProxy().createObjectStore(operation, info);
}

void IDBTransaction::didCreateObjectStoreOnServer(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBTransaction::didCreateObjectStoreOnServer");
    ASSERT(currentThread() == m_database->originThreadID());
    ASSERT_UNUSED(resultData, resultData.type() == IDBResultType::CreateObjectStoreSuccess || resultData.type() == IDBResultType::Error);
}

void IDBTransaction::renameObjectStore(IDBObjectStore& objectStore, const String& newName)
{
    LOG(IndexedDB, "IDBTransaction::renameObjectStore");

    Locker<Lock> locker(m_referencedObjectStoreLock);

    ASSERT(isVersionChange());
    ASSERT(scriptExecutionContext());
    ASSERT(currentThread() == m_database->originThreadID());

    ASSERT(m_referencedObjectStores.contains(objectStore.info().name()));
    ASSERT(!m_referencedObjectStores.contains(newName));
    ASSERT(m_referencedObjectStores.get(objectStore.info().name()) == &objectStore);

    uint64_t objectStoreIdentifier = objectStore.info().identifier();

    LOG(IndexedDBOperations, "IDB rename object store operation: %s to %s", objectStore.info().condensedLoggingString().utf8().data(), newName.utf8().data());
    scheduleOperation(IDBClient::createTransactionOperation(*this, &IDBTransaction::didRenameObjectStoreOnServer, &IDBTransaction::renameObjectStoreOnServer, objectStoreIdentifier, newName));

    m_referencedObjectStores.set(newName, m_referencedObjectStores.take(objectStore.info().name()));
}

void IDBTransaction::renameObjectStoreOnServer(IDBClient::TransactionOperation& operation, const uint64_t& objectStoreIdentifier, const String& newName)
{
    LOG(IndexedDB, "IDBTransaction::renameObjectStoreOnServer");
    ASSERT(currentThread() == m_database->originThreadID());
    ASSERT(isVersionChange());

    m_database->connectionProxy().renameObjectStore(operation, objectStoreIdentifier, newName);
}

void IDBTransaction::didRenameObjectStoreOnServer(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBTransaction::didRenameObjectStoreOnServer");
    ASSERT(currentThread() == m_database->originThreadID());
    ASSERT_UNUSED(resultData, resultData.type() == IDBResultType::RenameObjectStoreSuccess || resultData.type() == IDBResultType::Error);
}

std::unique_ptr<IDBIndex> IDBTransaction::createIndex(IDBObjectStore& objectStore, const IDBIndexInfo& info)
{
    LOG(IndexedDB, "IDBTransaction::createIndex");
    ASSERT(isVersionChange());
    ASSERT(currentThread() == m_database->originThreadID());

    if (!scriptExecutionContext())
        return nullptr;

    LOG(IndexedDBOperations, "IDB create index operation: %s under object store %s", info.condensedLoggingString().utf8().data(), objectStore.info().condensedLoggingString().utf8().data());
    scheduleOperation(IDBClient::createTransactionOperation(*this, &IDBTransaction::didCreateIndexOnServer, &IDBTransaction::createIndexOnServer, info));

    return std::make_unique<IDBIndex>(*scriptExecutionContext(), info, objectStore);
}

void IDBTransaction::createIndexOnServer(IDBClient::TransactionOperation& operation, const IDBIndexInfo& info)
{
    LOG(IndexedDB, "IDBTransaction::createIndexOnServer");
    ASSERT(currentThread() == m_database->originThreadID());
    ASSERT(isVersionChange());

    m_database->connectionProxy().createIndex(operation, info);
}

void IDBTransaction::didCreateIndexOnServer(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBTransaction::didCreateIndexOnServer");
    ASSERT(currentThread() == m_database->originThreadID());

    if (resultData.type() == IDBResultType::CreateIndexSuccess)
        return;

    ASSERT(resultData.type() == IDBResultType::Error);

    // This operation might have failed because the transaction is already aborting.
    if (m_state == IndexedDB::TransactionState::Aborting)
        return;

    // Otherwise, failure to create an index forced abortion of the transaction.
    abortDueToFailedRequest(DOMError::create(IDBDatabaseException::getErrorName(resultData.error().code()), resultData.error().message()));
}

void IDBTransaction::renameIndex(IDBIndex& index, const String& newName)
{
    LOG(IndexedDB, "IDBTransaction::renameIndex");
    Locker<Lock> locker(m_referencedObjectStoreLock);

    ASSERT(isVersionChange());
    ASSERT(scriptExecutionContext());
    ASSERT(currentThread() == m_database->originThreadID());

    ASSERT(m_referencedObjectStores.contains(index.objectStore().info().name()));
    ASSERT(m_referencedObjectStores.get(index.objectStore().info().name()) == &index.objectStore());

    index.objectStore().renameReferencedIndex(index, newName);

    uint64_t objectStoreIdentifier = index.objectStore().info().identifier();
    uint64_t indexIdentifier = index.info().identifier();

    LOG(IndexedDBOperations, "IDB rename index operation: %s to %s under object store %" PRIu64, index.info().condensedLoggingString().utf8().data(), newName.utf8().data(), index.info().objectStoreIdentifier());
    scheduleOperation(IDBClient::createTransactionOperation(*this, &IDBTransaction::didRenameIndexOnServer, &IDBTransaction::renameIndexOnServer, objectStoreIdentifier, indexIdentifier, newName));
}

void IDBTransaction::renameIndexOnServer(IDBClient::TransactionOperation& operation, const uint64_t& objectStoreIdentifier, const uint64_t& indexIdentifier, const String& newName)
{
    LOG(IndexedDB, "IDBTransaction::renameIndexOnServer");
    ASSERT(currentThread() == m_database->originThreadID());
    ASSERT(isVersionChange());

    m_database->connectionProxy().renameIndex(operation, objectStoreIdentifier, indexIdentifier, newName);
}

void IDBTransaction::didRenameIndexOnServer(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBTransaction::didRenameIndexOnServer");
    ASSERT(currentThread() == m_database->originThreadID());
    ASSERT_UNUSED(resultData, resultData.type() == IDBResultType::RenameIndexSuccess || resultData.type() == IDBResultType::Error);
}

Ref<IDBRequest> IDBTransaction::requestOpenCursor(ExecState& state, IDBObjectStore& objectStore, const IDBCursorInfo& info)
{
    LOG(IndexedDB, "IDBTransaction::requestOpenCursor");
    ASSERT(currentThread() == m_database->originThreadID());

    if (info.cursorType() == IndexedDB::CursorType::KeyOnly)
        return doRequestOpenCursor(state, IDBCursor::create(*this, objectStore, info));

    return doRequestOpenCursor(state, IDBCursorWithValue::create(*this, objectStore, info));
}

Ref<IDBRequest> IDBTransaction::requestOpenCursor(ExecState& state, IDBIndex& index, const IDBCursorInfo& info)
{
    LOG(IndexedDB, "IDBTransaction::requestOpenCursor");
    ASSERT(currentThread() == m_database->originThreadID());

    if (info.cursorType() == IndexedDB::CursorType::KeyOnly)
        return doRequestOpenCursor(state, IDBCursor::create(*this, index, info));

    return doRequestOpenCursor(state, IDBCursorWithValue::create(*this, index, info));
}

Ref<IDBRequest> IDBTransaction::doRequestOpenCursor(ExecState& state, Ref<IDBCursor>&& cursor)
{
    ASSERT(isActive());
    ASSERT(currentThread() == m_database->originThreadID());

    ASSERT_UNUSED(state, scriptExecutionContext() == scriptExecutionContextFromExecState(&state));

    auto request = IDBRequest::create(*scriptExecutionContext(), cursor.get(), *this);
    addRequest(request.get());

    LOG(IndexedDBOperations, "IDB open cursor operation: %s", cursor->info().loggingString().utf8().data());
    scheduleOperation(IDBClient::createTransactionOperation(*this, request.get(), &IDBTransaction::didOpenCursorOnServer, &IDBTransaction::openCursorOnServer, cursor->info()));

    return request;
}

void IDBTransaction::openCursorOnServer(IDBClient::TransactionOperation& operation, const IDBCursorInfo& info)
{
    LOG(IndexedDB, "IDBTransaction::openCursorOnServer");
    ASSERT(currentThread() == m_database->originThreadID());

    m_database->connectionProxy().openCursor(operation, info);
}

void IDBTransaction::didOpenCursorOnServer(IDBRequest& request, const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBTransaction::didOpenCursorOnServer");
    ASSERT(currentThread() == m_database->originThreadID());

    completeCursorRequest(request, resultData);
}

void IDBTransaction::iterateCursor(IDBCursor& cursor, const IDBIterateCursorData& data)
{
    LOG(IndexedDB, "IDBTransaction::iterateCursor");
    ASSERT(isActive());
    ASSERT(cursor.request());
    ASSERT(currentThread() == m_database->originThreadID());

    addRequest(*cursor.request());

    LOG(IndexedDBOperations, "IDB iterate cursor operation: %s %s", cursor.info().loggingString().utf8().data(), data.loggingString().utf8().data());
    scheduleOperation(IDBClient::createTransactionOperation(*this, *cursor.request(), &IDBTransaction::didIterateCursorOnServer, &IDBTransaction::iterateCursorOnServer, data));
}

// FIXME: changes here
void IDBTransaction::iterateCursorOnServer(IDBClient::TransactionOperation& operation, const IDBIterateCursorData& data)
{
    LOG(IndexedDB, "IDBTransaction::iterateCursorOnServer");
    ASSERT(currentThread() == m_database->originThreadID());

    m_database->connectionProxy().iterateCursor(operation, data);
}

void IDBTransaction::didIterateCursorOnServer(IDBRequest& request, const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBTransaction::didIterateCursorOnServer");
    ASSERT(currentThread() == m_database->originThreadID());

    completeCursorRequest(request, resultData);
}

Ref<IDBRequest> IDBTransaction::requestGetAllObjectStoreRecords(JSC::ExecState& state, IDBObjectStore& objectStore, const IDBKeyRangeData& keyRangeData, IndexedDB::GetAllType getAllType, std::optional<uint32_t> count)
{
    LOG(IndexedDB, "IDBTransaction::requestGetAllObjectStoreRecords");
    ASSERT(isActive());
    ASSERT(currentThread() == m_database->originThreadID());

    ASSERT_UNUSED(state, scriptExecutionContext() == scriptExecutionContextFromExecState(&state));

    auto request = IDBRequest::create(*scriptExecutionContext(), objectStore, *this);
    addRequest(request.get());

    IDBGetAllRecordsData getAllRecordsData { keyRangeData, getAllType, count, objectStore.info().identifier(), 0 };

    LOG(IndexedDBOperations, "IDB get all object store records operation: %s", getAllRecordsData.loggingString().utf8().data());
    scheduleOperation(IDBClient::createTransactionOperation(*this, request.get(), &IDBTransaction::didGetAllRecordsOnServer, &IDBTransaction::getAllRecordsOnServer, getAllRecordsData));

    return request;
}

Ref<IDBRequest> IDBTransaction::requestGetAllIndexRecords(JSC::ExecState& state, IDBIndex& index, const IDBKeyRangeData& keyRangeData, IndexedDB::GetAllType getAllType, std::optional<uint32_t> count)
{
    LOG(IndexedDB, "IDBTransaction::requestGetAllIndexRecords");
    ASSERT(isActive());
    ASSERT(currentThread() == m_database->originThreadID());

    ASSERT_UNUSED(state, scriptExecutionContext() == scriptExecutionContextFromExecState(&state));

    auto request = IDBRequest::create(*scriptExecutionContext(), index, *this);
    addRequest(request.get());

    IDBGetAllRecordsData getAllRecordsData { keyRangeData, getAllType, count, index.objectStore().info().identifier(), index.info().identifier() };

    LOG(IndexedDBOperations, "IDB get all index records operation: %s", getAllRecordsData.loggingString().utf8().data());
    scheduleOperation(IDBClient::createTransactionOperation(*this, request.get(), &IDBTransaction::didGetAllRecordsOnServer, &IDBTransaction::getAllRecordsOnServer, getAllRecordsData));

    return request;
}

void IDBTransaction::getAllRecordsOnServer(IDBClient::TransactionOperation& operation, const IDBGetAllRecordsData& getAllRecordsData)
{
    LOG(IndexedDB, "IDBTransaction::getAllRecordsOnServer");
    ASSERT(currentThread() == m_database->originThreadID());

    m_database->connectionProxy().getAllRecords(operation, getAllRecordsData);
}

void IDBTransaction::didGetAllRecordsOnServer(IDBRequest& request, const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBTransaction::didGetAllRecordsOnServer");
    ASSERT(currentThread() == m_database->originThreadID());

    if (resultData.type() == IDBResultType::Error) {
        completeNoncursorRequest(request, resultData);
        return;
    }

    ASSERT(resultData.type() == IDBResultType::GetAllRecordsSuccess);

    auto& getAllResult = resultData.getAllResult();
    switch (getAllResult.type()) {
    case IndexedDB::GetAllType::Keys:
        request.setResult(getAllResult.keys());
        break;
    case IndexedDB::GetAllType::Values:
        request.setResult(getAllResult.values());
        break;
    }

    completeNoncursorRequest(request, resultData);
}

Ref<IDBRequest> IDBTransaction::requestGetRecord(ExecState& state, IDBObjectStore& objectStore, const IDBGetRecordData& getRecordData)
{
    LOG(IndexedDB, "IDBTransaction::requestGetRecord");
    ASSERT(isActive());
    ASSERT(!getRecordData.keyRangeData.isNull);
    ASSERT(currentThread() == m_database->originThreadID());

    ASSERT_UNUSED(state, scriptExecutionContext() == scriptExecutionContextFromExecState(&state));

    IndexedDB::ObjectStoreRecordType type = getRecordData.type == IDBGetRecordDataType::KeyAndValue ? IndexedDB::ObjectStoreRecordType::ValueOnly : IndexedDB::ObjectStoreRecordType::KeyOnly;

    auto request = IDBRequest::createObjectStoreGet(*scriptExecutionContext(), objectStore, type, *this);
    addRequest(request.get());

    LOG(IndexedDBOperations, "IDB get record operation: %s %s", objectStore.info().condensedLoggingString().utf8().data(), getRecordData.loggingString().utf8().data());
    scheduleOperation(IDBClient::createTransactionOperation(*this, request.get(), &IDBTransaction::didGetRecordOnServer, &IDBTransaction::getRecordOnServer, getRecordData));

    return request;
}

Ref<IDBRequest> IDBTransaction::requestGetValue(ExecState& state, IDBIndex& index, const IDBKeyRangeData& range)
{
    LOG(IndexedDB, "IDBTransaction::requestGetValue");
    ASSERT(currentThread() == m_database->originThreadID());

    return requestIndexRecord(state, index, IndexedDB::IndexRecordType::Value, range);
}

Ref<IDBRequest> IDBTransaction::requestGetKey(ExecState& state, IDBIndex& index, const IDBKeyRangeData& range)
{
    LOG(IndexedDB, "IDBTransaction::requestGetValue");
    ASSERT(currentThread() == m_database->originThreadID());

    return requestIndexRecord(state, index, IndexedDB::IndexRecordType::Key, range);
}

Ref<IDBRequest> IDBTransaction::requestIndexRecord(ExecState& state, IDBIndex& index, IndexedDB::IndexRecordType type, const IDBKeyRangeData& range)
{
    LOG(IndexedDB, "IDBTransaction::requestGetValue");
    ASSERT(isActive());
    ASSERT(!range.isNull);
    ASSERT(currentThread() == m_database->originThreadID());

    ASSERT_UNUSED(state, scriptExecutionContext() == scriptExecutionContextFromExecState(&state));

    auto request = IDBRequest::createIndexGet(*scriptExecutionContext(), index, type, *this);
    addRequest(request.get());

    IDBGetRecordData getRecordData = { range, IDBGetRecordDataType::KeyAndValue };

    LOG(IndexedDBOperations, "IDB get index record operation: %s %s", index.info().condensedLoggingString().utf8().data(), getRecordData.loggingString().utf8().data());
    scheduleOperation(IDBClient::createTransactionOperation(*this, request.get(), &IDBTransaction::didGetRecordOnServer, &IDBTransaction::getRecordOnServer, getRecordData));

    return request;
}

void IDBTransaction::getRecordOnServer(IDBClient::TransactionOperation& operation, const IDBGetRecordData& getRecordData)
{
    LOG(IndexedDB, "IDBTransaction::getRecordOnServer");
    ASSERT(currentThread() == m_database->originThreadID());

    m_database->connectionProxy().getRecord(operation, getRecordData);
}

void IDBTransaction::didGetRecordOnServer(IDBRequest& request, const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBTransaction::didGetRecordOnServer");
    ASSERT(currentThread() == m_database->originThreadID());

    if (resultData.type() == IDBResultType::Error) {
        completeNoncursorRequest(request, resultData);
        return;
    }

    ASSERT(resultData.type() == IDBResultType::GetRecordSuccess);

    bool useResultKey = request.sourceIndexIdentifier() && request.requestedIndexRecordType() == IndexedDB::IndexRecordType::Key;
    if (!useResultKey)
        useResultKey = request.requestedObjectStoreRecordType() == IndexedDB::ObjectStoreRecordType::KeyOnly;

    const IDBGetResult& result = resultData.getResult();

    if (useResultKey) {
        if (!result.keyData().isNull())
            request.setResult(result.keyData());
        else
            request.setResultToUndefined();
    } else {
        if (resultData.getResult().value().data().data())
            request.setResultToStructuredClone(resultData.getResult().value());
        else
            request.setResultToUndefined();
    }

    completeNoncursorRequest(request, resultData);
}

Ref<IDBRequest> IDBTransaction::requestCount(ExecState& state, IDBObjectStore& objectStore, const IDBKeyRangeData& range)
{
    LOG(IndexedDB, "IDBTransaction::requestCount (IDBObjectStore)");
    ASSERT(isActive());
    ASSERT(!range.isNull);
    ASSERT(currentThread() == m_database->originThreadID());

    ASSERT_UNUSED(state, scriptExecutionContext() == scriptExecutionContextFromExecState(&state));

    auto request = IDBRequest::create(*scriptExecutionContext(), objectStore, *this);
    addRequest(request.get());

    LOG(IndexedDBOperations, "IDB object store count operation: %s, range %s", objectStore.info().condensedLoggingString().utf8().data(), range.loggingString().utf8().data());
    scheduleOperation(IDBClient::createTransactionOperation(*this, request.get(), &IDBTransaction::didGetCountOnServer, &IDBTransaction::getCountOnServer, range));

    return request;
}

Ref<IDBRequest> IDBTransaction::requestCount(ExecState& state, IDBIndex& index, const IDBKeyRangeData& range)
{
    LOG(IndexedDB, "IDBTransaction::requestCount (IDBIndex)");
    ASSERT(isActive());
    ASSERT(!range.isNull);
    ASSERT(currentThread() == m_database->originThreadID());

    ASSERT_UNUSED(state, scriptExecutionContext() == scriptExecutionContextFromExecState(&state));

    auto request = IDBRequest::create(*scriptExecutionContext(), index, *this);
    addRequest(request.get());

    LOG(IndexedDBOperations, "IDB index count operation: %s, range %s", index.info().condensedLoggingString().utf8().data(), range.loggingString().utf8().data());
    scheduleOperation(IDBClient::createTransactionOperation(*this, request.get(), &IDBTransaction::didGetCountOnServer, &IDBTransaction::getCountOnServer, range));

    return request;
}

void IDBTransaction::getCountOnServer(IDBClient::TransactionOperation& operation, const IDBKeyRangeData& keyRange)
{
    LOG(IndexedDB, "IDBTransaction::getCountOnServer");
    ASSERT(currentThread() == m_database->originThreadID());

    m_database->connectionProxy().getCount(operation, keyRange);
}

void IDBTransaction::didGetCountOnServer(IDBRequest& request, const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBTransaction::didGetCountOnServer");
    ASSERT(currentThread() == m_database->originThreadID());

    request.setResult(resultData.resultInteger());
    completeNoncursorRequest(request, resultData);
}

Ref<IDBRequest> IDBTransaction::requestDeleteRecord(ExecState& state, IDBObjectStore& objectStore, const IDBKeyRangeData& range)
{
    LOG(IndexedDB, "IDBTransaction::requestDeleteRecord");
    ASSERT(isActive());
    ASSERT(!range.isNull);
    ASSERT(currentThread() == m_database->originThreadID());

    ASSERT_UNUSED(state, scriptExecutionContext() == scriptExecutionContextFromExecState(&state));

    auto request = IDBRequest::create(*scriptExecutionContext(), objectStore, *this);
    addRequest(request.get());

    LOG(IndexedDBOperations, "IDB delete record operation: %s, range %s", objectStore.info().condensedLoggingString().utf8().data(), range.loggingString().utf8().data());
    scheduleOperation(IDBClient::createTransactionOperation(*this, request.get(), &IDBTransaction::didDeleteRecordOnServer, &IDBTransaction::deleteRecordOnServer, range));
    return request;
}

void IDBTransaction::deleteRecordOnServer(IDBClient::TransactionOperation& operation, const IDBKeyRangeData& keyRange)
{
    LOG(IndexedDB, "IDBTransaction::deleteRecordOnServer");
    ASSERT(currentThread() == m_database->originThreadID());

    m_database->connectionProxy().deleteRecord(operation, keyRange);
}

void IDBTransaction::didDeleteRecordOnServer(IDBRequest& request, const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBTransaction::didDeleteRecordOnServer");
    ASSERT(currentThread() == m_database->originThreadID());

    request.setResultToUndefined();
    completeNoncursorRequest(request, resultData);
}

Ref<IDBRequest> IDBTransaction::requestClearObjectStore(ExecState& state, IDBObjectStore& objectStore)
{
    LOG(IndexedDB, "IDBTransaction::requestClearObjectStore");
    ASSERT(isActive());
    ASSERT(currentThread() == m_database->originThreadID());

    ASSERT_UNUSED(state, scriptExecutionContext() == scriptExecutionContextFromExecState(&state));

    auto request = IDBRequest::create(*scriptExecutionContext(), objectStore, *this);
    addRequest(request.get());

    uint64_t objectStoreIdentifier = objectStore.info().identifier();

    LOG(IndexedDBOperations, "IDB clear object store operation: %s", objectStore.info().condensedLoggingString().utf8().data());
    scheduleOperation(IDBClient::createTransactionOperation(*this, request.get(), &IDBTransaction::didClearObjectStoreOnServer, &IDBTransaction::clearObjectStoreOnServer, objectStoreIdentifier));

    return request;
}

void IDBTransaction::clearObjectStoreOnServer(IDBClient::TransactionOperation& operation, const uint64_t& objectStoreIdentifier)
{
    LOG(IndexedDB, "IDBTransaction::clearObjectStoreOnServer");
    ASSERT(currentThread() == m_database->originThreadID());

    m_database->connectionProxy().clearObjectStore(operation, objectStoreIdentifier);
}

void IDBTransaction::didClearObjectStoreOnServer(IDBRequest& request, const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBTransaction::didClearObjectStoreOnServer");
    ASSERT(currentThread() == m_database->originThreadID());

    request.setResultToUndefined();
    completeNoncursorRequest(request, resultData);
}

Ref<IDBRequest> IDBTransaction::requestPutOrAdd(ExecState& state, IDBObjectStore& objectStore, IDBKey* key, SerializedScriptValue& value, IndexedDB::ObjectStoreOverwriteMode overwriteMode)
{
    LOG(IndexedDB, "IDBTransaction::requestPutOrAdd");
    ASSERT(isActive());
    ASSERT(!isReadOnly());
    ASSERT(objectStore.info().autoIncrement() || key);
    ASSERT(currentThread() == m_database->originThreadID());

    ASSERT_UNUSED(state, scriptExecutionContext() == scriptExecutionContextFromExecState(&state));

    auto request = IDBRequest::create(*scriptExecutionContext(), objectStore, *this);
    addRequest(request.get());

    LOG(IndexedDBOperations, "IDB putOrAdd operation: %s key: %s", objectStore.info().condensedLoggingString().utf8().data(), key ? key->loggingString().utf8().data() : "<null key>");
    scheduleOperation(IDBClient::createTransactionOperation(*this, request.get(), &IDBTransaction::didPutOrAddOnServer, &IDBTransaction::putOrAddOnServer, key, &value, overwriteMode));

    return request;
}

void IDBTransaction::putOrAddOnServer(IDBClient::TransactionOperation& operation, RefPtr<IDBKey> key, RefPtr<SerializedScriptValue> value, const IndexedDB::ObjectStoreOverwriteMode& overwriteMode)
{
    LOG(IndexedDB, "IDBTransaction::putOrAddOnServer");
    ASSERT(currentThread() == originThreadID());
    ASSERT(!isReadOnly());
    ASSERT(value);

    if (!value->hasBlobURLs()) {
        m_database->connectionProxy().putOrAdd(operation, key.get(), *value, overwriteMode);
        return;
    }

    // Due to current limitations on our ability to post tasks back to a worker thread,
    // workers currently write blobs to disk synchronously.
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=157958 - Make this asynchronous after refactoring allows it.
    if (!isMainThread()) {
        auto idbValue = value->writeBlobsToDiskForIndexedDBSynchronously();
        if (idbValue.data().data())
            m_database->connectionProxy().putOrAdd(operation, key.get(), idbValue, overwriteMode);
        else {
            // If the IDBValue doesn't have any data, then something went wrong writing the blobs to disk.
            // In that case, we cannot successfully store this record, so we callback with an error.
            RefPtr<IDBClient::TransactionOperation> protectedOperation(&operation);
            auto result = IDBResultData::error(operation.identifier(), { IDBDatabaseException::UnknownError, ASCIILiteral("Error preparing Blob/File data to be stored in object store") });
            scriptExecutionContext()->postTask([protectedOperation = WTFMove(protectedOperation), result = WTFMove(result)](ScriptExecutionContext&) {
                protectedOperation->doComplete(result);
            });
        }
        return;
    }

    // Since this request won't actually go to the server until the blob writes are complete,
    // stop future requests from going to the server ahead of it.
    operation.setNextRequestCanGoToServer(false);

    value->writeBlobsToDiskForIndexedDB([protectedThis = makeRef(*this), this, protectedOperation = Ref<IDBClient::TransactionOperation>(operation), keyData = IDBKeyData(key.get()).isolatedCopy(), overwriteMode](const IDBValue& idbValue) mutable {
        ASSERT(currentThread() == originThreadID());
        ASSERT(isMainThread());
        if (idbValue.data().data()) {
            m_database->connectionProxy().putOrAdd(protectedOperation.get(), WTFMove(keyData), idbValue, overwriteMode);
            return;
        }

        // If the IDBValue doesn't have any data, then something went wrong writing the blobs to disk.
        // In that case, we cannot successfully store this record, so we callback with an error.
        auto result = IDBResultData::error(protectedOperation->identifier(), { IDBDatabaseException::UnknownError, ASCIILiteral("Error preparing Blob/File data to be stored in object store") });
        callOnMainThread([protectedThis = WTFMove(protectedThis), protectedOperation = WTFMove(protectedOperation), result = WTFMove(result)]() mutable {
            protectedOperation->doComplete(result);
        });
    });
}

void IDBTransaction::didPutOrAddOnServer(IDBRequest& request, const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBTransaction::didPutOrAddOnServer");
    ASSERT(currentThread() == m_database->originThreadID());

    if (auto* result = resultData.resultKey())
        request.setResult(*result);
    else
        request.setResultToUndefined();
    completeNoncursorRequest(request, resultData);
}

void IDBTransaction::deleteObjectStore(const String& objectStoreName)
{
    LOG(IndexedDB, "IDBTransaction::deleteObjectStore");
    ASSERT(currentThread() == m_database->originThreadID());
    ASSERT(isVersionChange());

    Locker<Lock> locker(m_referencedObjectStoreLock);

    if (auto objectStore = m_referencedObjectStores.take(objectStoreName)) {
        objectStore->markAsDeleted();
        auto identifier = objectStore->info().identifier();
        m_deletedObjectStores.set(identifier, WTFMove(objectStore));
    }

    LOG(IndexedDBOperations, "IDB delete object store operation: %s", objectStoreName.utf8().data());
    scheduleOperation(IDBClient::createTransactionOperation(*this, &IDBTransaction::didDeleteObjectStoreOnServer, &IDBTransaction::deleteObjectStoreOnServer, objectStoreName));
}

void IDBTransaction::deleteObjectStoreOnServer(IDBClient::TransactionOperation& operation, const String& objectStoreName)
{
    LOG(IndexedDB, "IDBTransaction::deleteObjectStoreOnServer");
    ASSERT(isVersionChange());
    ASSERT(currentThread() == m_database->originThreadID());

    m_database->connectionProxy().deleteObjectStore(operation, objectStoreName);
}

void IDBTransaction::didDeleteObjectStoreOnServer(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBTransaction::didDeleteObjectStoreOnServer");
    ASSERT(currentThread() == m_database->originThreadID());
    ASSERT_UNUSED(resultData, resultData.type() == IDBResultType::DeleteObjectStoreSuccess || resultData.type() == IDBResultType::Error);
}

void IDBTransaction::deleteIndex(uint64_t objectStoreIdentifier, const String& indexName)
{
    LOG(IndexedDB, "IDBTransaction::deleteIndex");
    ASSERT(currentThread() == m_database->originThreadID());
    ASSERT(isVersionChange());

    LOG(IndexedDBOperations, "IDB delete index operation: %s (%" PRIu64 ")", indexName.utf8().data(), objectStoreIdentifier);
    scheduleOperation(IDBClient::createTransactionOperation(*this, &IDBTransaction::didDeleteIndexOnServer, &IDBTransaction::deleteIndexOnServer, objectStoreIdentifier, indexName));
}

void IDBTransaction::deleteIndexOnServer(IDBClient::TransactionOperation& operation, const uint64_t& objectStoreIdentifier, const String& indexName)
{
    LOG(IndexedDB, "IDBTransaction::deleteIndexOnServer");
    ASSERT(isVersionChange());
    ASSERT(currentThread() == m_database->originThreadID());

    m_database->connectionProxy().deleteIndex(operation, objectStoreIdentifier, indexName);
}

void IDBTransaction::didDeleteIndexOnServer(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBTransaction::didDeleteIndexOnServer");
    ASSERT(currentThread() == m_database->originThreadID());
    ASSERT_UNUSED(resultData, resultData.type() == IDBResultType::DeleteIndexSuccess || resultData.type() == IDBResultType::Error);
}

void IDBTransaction::operationCompletedOnClient(IDBClient::TransactionOperation& operation)
{
    LOG(IndexedDB, "IDBTransaction::operationCompletedOnClient");

    ASSERT(currentThread() == m_database->originThreadID());
    ASSERT(currentThread() == operation.originThreadID());
    ASSERT(m_transactionOperationMap.get(operation.identifier()) == &operation);
    ASSERT(m_transactionOperationsInProgressQueue.first() == &operation);

    m_transactionOperationMap.remove(operation.identifier());
    m_transactionOperationsInProgressQueue.removeFirst();

    schedulePendingOperationTimer();
}

void IDBTransaction::establishOnServer()
{
    LOG(IndexedDB, "IDBTransaction::establishOnServer");
    ASSERT(currentThread() == m_database->originThreadID());

    m_database->connectionProxy().establishTransaction(*this);
}

void IDBTransaction::activate()
{
    ASSERT(currentThread() == m_database->originThreadID());

    if (isFinishedOrFinishing())
        return;

    m_state = IndexedDB::TransactionState::Active;
}

void IDBTransaction::deactivate()
{
    ASSERT(currentThread() == m_database->originThreadID());

    if (m_state == IndexedDB::TransactionState::Active)
        m_state = IndexedDB::TransactionState::Inactive;

    schedulePendingOperationTimer();
}

void IDBTransaction::connectionClosedFromServer(const IDBError& error)
{
    LOG(IndexedDB, "IDBTransaction::connectionClosedFromServer - %s", error.message().utf8().data());

    m_state = IndexedDB::TransactionState::Aborting;

    abortInProgressOperations(error);

    Vector<RefPtr<IDBClient::TransactionOperation>> operations;
    copyValuesToVector(m_transactionOperationMap, operations);

    for (auto& operation : operations) {
        m_currentlyCompletingRequest = nullptr;
        m_transactionOperationsInProgressQueue.append(operation.get());
        ASSERT(m_transactionOperationsInProgressQueue.first() == operation.get());
        operation->doComplete(IDBResultData::error(operation->identifier(), error));
    }

    connectionProxy().forgetActiveOperations(operations);

    m_pendingTransactionOperationQueue.clear();
    m_abortQueue.clear();
    m_transactionOperationMap.clear();

    m_idbError = error;
    m_domError = error.toDOMError();
    fireOnAbort();
}

void IDBTransaction::visitReferencedObjectStores(JSC::SlotVisitor& visitor) const
{
    Locker<Lock> locker(m_referencedObjectStoreLock);
    for (auto& objectStore : m_referencedObjectStores.values())
        visitor.addOpaqueRoot(objectStore.get());
    for (auto& objectStore : m_deletedObjectStores.values())
        visitor.addOpaqueRoot(objectStore.get());
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
