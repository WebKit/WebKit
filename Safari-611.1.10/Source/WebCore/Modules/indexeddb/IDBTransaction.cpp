/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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

#include "DOMException.h"
#include "DOMStringList.h"
#include "DOMWindow.h"
#include "Event.h"
#include "EventDispatcher.h"
#include "EventNames.h"
#include "EventQueue.h"
#include "IDBCursorWithValue.h"
#include "IDBDatabase.h"
#include "IDBError.h"
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
#include <wtf/CompletionHandler.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
using namespace JSC;

WTF_MAKE_ISO_ALLOCATED_IMPL(IDBTransaction);

std::atomic<unsigned> IDBTransaction::numberOfIDBTransactions { 0 };

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
    , m_openDBRequest(request)
    , m_currentlyCompletingRequest(request)

{
    LOG(IndexedDB, "IDBTransaction::IDBTransaction - %s", m_info.loggingString().utf8().data());
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    ++numberOfIDBTransactions;

    if (m_info.mode() == IDBTransactionMode::Versionchange) {
        ASSERT(m_openDBRequest);
        m_openDBRequest->setVersionChangeTransaction(*this);
        m_startedOnServer = true;
    } else {
        activate();

        auto* context = scriptExecutionContext();
        ASSERT(context);

        JSC::VM& vm = context->vm();
        vm.whenIdle([protectedThis = makeRef(*this)]() {
            protectedThis->deactivate();
        });

        establishOnServer();
    }

    suspendIfNeeded();
}

IDBTransaction::~IDBTransaction()
{
    --numberOfIDBTransactions;
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));
}

IDBClient::IDBConnectionProxy& IDBTransaction::connectionProxy()
{
    return m_database->connectionProxy();
}

Ref<DOMStringList> IDBTransaction::objectStoreNames() const
{
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    const Vector<String> names = isVersionChange() ? m_database->info().objectStoreNames() : m_info.objectStores();

    Ref<DOMStringList> objectStoreNames = DOMStringList::create();
    for (auto& name : names)
        objectStoreNames->append(name);

    objectStoreNames->sort();
    return objectStoreNames;
}

IDBDatabase* IDBTransaction::db()
{
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));
    return m_database.ptr();
}

DOMException* IDBTransaction::error() const
{
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));
    return m_domError.get();
}

ExceptionOr<Ref<IDBObjectStore>> IDBTransaction::objectStore(const String& objectStoreName)
{
    LOG(IndexedDB, "IDBTransaction::objectStore");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    if (!scriptExecutionContext())
        return Exception { InvalidStateError };

    if (isFinishedOrFinishing())
        return Exception { InvalidStateError, "Failed to execute 'objectStore' on 'IDBTransaction': The transaction finished."_s };

    Locker<Lock> locker(m_referencedObjectStoreLock);

    if (auto* store = m_referencedObjectStores.get(objectStoreName))
        return makeRef(*store);

    bool found = false;
    for (auto& objectStore : m_info.objectStores()) {
        if (objectStore == objectStoreName) {
            found = true;
            break;
        }
    }

    auto* info = m_database->info().infoForExistingObjectStore(objectStoreName);
    if (!info)
        return Exception { NotFoundError, "Failed to execute 'objectStore' on 'IDBTransaction': The specified object store was not found."_s };

    // Version change transactions are scoped to every object store in the database.
    if (!info || (!found && !isVersionChange()))
        return Exception { NotFoundError, "Failed to execute 'objectStore' on 'IDBTransaction': The specified object store was not found."_s };

    auto objectStore = makeUnique<IDBObjectStore>(*scriptExecutionContext(), *info, *this);
    auto* rawObjectStore = objectStore.get();
    m_referencedObjectStores.set(objectStoreName, WTFMove(objectStore));

    return Ref<IDBObjectStore>(*rawObjectStore);
}


void IDBTransaction::abortDueToFailedRequest(DOMException& error)
{
    LOG(IndexedDB, "IDBTransaction::abortDueToFailedRequest");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    if (isFinishedOrFinishing())
        return;

    m_domError = &error;
    internalAbort();
}

void IDBTransaction::transitionedToFinishing(IndexedDB::TransactionState state)
{
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    ASSERT(!isFinishedOrFinishing());
    m_state = state;
    ASSERT(isFinishedOrFinishing());
}

ExceptionOr<void> IDBTransaction::abort()
{
    LOG(IndexedDB, "IDBTransaction::abort");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    if (isFinishedOrFinishing())
        return Exception { InvalidStateError, "Failed to execute 'abort' on 'IDBTransaction': The transaction is inactive or finished."_s };

    internalAbort();

    return { };
}

void IDBTransaction::internalAbort()
{
    LOG(IndexedDB, "IDBTransaction::internalAbort");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));
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
    scheduleOperation(IDBClient::TransactionOperationImpl::create(*this, nullptr, [protectedThis = makeRef(*this)] (auto& operation) {
        protectedThis->abortOnServerAndCancelRequests(operation);
    }));
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

    m_transactionOperationResultMap.clear();

    m_currentlyCompletingRequest = nullptr;
    connectionProxy().forgetActiveOperations(inProgressAbortVector);
}

void IDBTransaction::abortOnServerAndCancelRequests(IDBClient::TransactionOperation& operation)
{
    LOG(IndexedDB, "IDBTransaction::abortOnServerAndCancelRequests");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));
    ASSERT(m_pendingTransactionOperationQueue.isEmpty());

    m_database->connectionProxy().abortTransaction(*this);

    ASSERT(m_transactionOperationMap.contains(operation.identifier()));
    ASSERT(m_transactionOperationsInProgressQueue.last() == &operation);
    m_transactionOperationMap.remove(operation.identifier());
    m_transactionOperationsInProgressQueue.removeLast();

    m_currentlyCompletingRequest = nullptr;
    
    IDBError error(AbortError);

    abortInProgressOperations(error);

    for (auto& operation : m_abortQueue) {
        m_transactionOperationsInProgressQueue.append(operation.get());
        operation->doComplete(IDBResultData::error(operation->identifier(), error));
        m_currentlyCompletingRequest = nullptr;
    }

    m_abortQueue.clear();
    m_openRequests.clear();
    // Since we're aborting, it should be impossible to have queued any further operations.
    ASSERT(m_pendingTransactionOperationQueue.isEmpty());
}

const char* IDBTransaction::activeDOMObjectName() const
{
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));
    return "IDBTransaction";
}

bool IDBTransaction::virtualHasPendingActivity() const
{
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()) || Thread::mayBeGCThread());
    return m_state != IndexedDB::TransactionState::Finished;
}

void IDBTransaction::stop()
{
    LOG(IndexedDB, "IDBTransaction::stop - %s", m_info.loggingString().utf8().data());
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    // IDBDatabase::stop() calls IDBTransaction::stop() for each of its active transactions.
    // Since the order of calling ActiveDOMObject::stop() is random, we might already have been stopped.
    if (m_isStopped)
        return;

    removeAllEventListeners();

    m_isStopped = true;

    if (isVersionChange())
        m_openDBRequest = nullptr;

    if (isFinishedOrFinishing())
        return;

    internalAbort();
}

bool IDBTransaction::isActive() const
{
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));
    return m_state == IndexedDB::TransactionState::Active;
}

bool IDBTransaction::isFinishedOrFinishing() const
{
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    return m_state == IndexedDB::TransactionState::Committing
        || m_state == IndexedDB::TransactionState::Aborting
        || m_state == IndexedDB::TransactionState::Finished;
}

void IDBTransaction::addRequest(IDBRequest& request)
{
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));
    m_openRequests.add(&request);
}

void IDBTransaction::removeRequest(IDBRequest& request)
{
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));
    if (m_currentlyCompletingRequest == &request)
        return;

    m_openRequests.remove(&request);

    autoCommit();
}

void IDBTransaction::scheduleOperation(Ref<IDBClient::TransactionOperation>&& operation, IsWriteOperation isWriteOperation)
{
    ASSERT(!m_transactionOperationMap.contains(operation->identifier()));
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    if (isWriteOperation == IsWriteOperation::Yes)
        m_lastWriteOperationID = operation->operationID();

    auto identifier = operation->identifier();
    m_pendingTransactionOperationQueue.append(operation.copyRef());
    m_transactionOperationMap.set(identifier, WTFMove(operation));

    handlePendingOperations();
}

void IDBTransaction::operationCompletedOnServer(const IDBResultData& data, IDBClient::TransactionOperation& operation)
{
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));
    ASSERT(canCurrentThreadAccessThreadLocalData(operation.originThread()));

    if (!m_transactionOperationMap.contains(operation.identifier()))
        return;

    m_transactionOperationResultMap.set(&operation, IDBResultData(data));

    if (!m_currentlyCompletingRequest)
        handleOperationsCompletedOnServer();
}

void IDBTransaction::handleOperationsCompletedOnServer()
{
    LOG(IndexedDB, "IDBTransaction::handleOperationsCompletedOnServer");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    while (!m_transactionOperationsInProgressQueue.isEmpty() && !m_currentlyCompletingRequest) {
        RefPtr<IDBClient::TransactionOperation> currentOperation = m_transactionOperationsInProgressQueue.first();
        if (!m_transactionOperationResultMap.contains(currentOperation))
            return;

        currentOperation->doComplete(m_transactionOperationResultMap.take(currentOperation));
    }
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
    if (isFinished())
        return;

    ASSERT_UNUSED(request, !m_currentlyCompletingRequest || m_currentlyCompletingRequest == &request);

    m_currentlyCompletingRequest = nullptr;
    handleOperationsCompletedOnServer();
}

void IDBTransaction::commit()
{
    LOG(IndexedDB, "IDBTransaction::commit");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));
    ASSERT(!isFinishedOrFinishing());

    transitionedToFinishing(IndexedDB::TransactionState::Committing);
    m_database->willCommitTransaction(*this);

    LOG(IndexedDBOperations, "IDB commit operation: Transaction %s", info().identifier().loggingString().utf8().data());
    scheduleOperation(IDBClient::TransactionOperationImpl::create(*this, nullptr, [protectedThis = makeRef(*this)] (auto& operation) {
        protectedThis->commitOnServer(operation);
    }));
}

void IDBTransaction::commitOnServer(IDBClient::TransactionOperation& operation)
{
    LOG(IndexedDB, "IDBTransaction::commitOnServer");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

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
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    m_state = IndexedDB::TransactionState::Finished;
}

void IDBTransaction::didStart(const IDBError& error)
{
    LOG(IndexedDB, "IDBTransaction::didStart");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    m_database->didStartTransaction(*this);

    m_startedOnServer = true;

    // It's possible the transaction failed to start on the server.
    // That equates to an abort.
    if (!error.isNull()) {
        didAbort(error);
        return;
    }

    handlePendingOperations();

    // It's possible transaction does not create requests (or creates but finishes them early
    // because of error) during intialization. In this case, since the transaction will
    // not be active any more, we can end it.
    autoCommit();
}

void IDBTransaction::notifyDidAbort(const IDBError& error)
{
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    m_database->didAbortTransaction(*this);
    m_idbError = error;
    fireOnAbort();

    if (isVersionChange() && !isContextStopped()) {
        ASSERT(m_openDBRequest);
        m_openDBRequest->fireErrorAfterVersionChangeCompletion();
    }
}

void IDBTransaction::didAbort(const IDBError& error)
{
    LOG(IndexedDB, "IDBTransaction::didAbort");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    if (m_state == IndexedDB::TransactionState::Finished)
        return;

    notifyDidAbort(error);

    finishAbortOrCommit();
}

void IDBTransaction::didCommit(const IDBError& error)
{
    LOG(IndexedDB, "IDBTransaction::didCommit");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));
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
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));
    enqueueEvent(Event::create(eventNames().completeEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

void IDBTransaction::fireOnAbort()
{
    LOG(IndexedDB, "IDBTransaction::fireOnAbort");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));
    enqueueEvent(Event::create(eventNames().abortEvent, Event::CanBubble::Yes, Event::IsCancelable::No));
}

void IDBTransaction::enqueueEvent(Ref<Event>&& event)
{
    ASSERT(m_state != IndexedDB::TransactionState::Finished);
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    if (!scriptExecutionContext() || isContextStopped())
        return;

    queueTaskToDispatchEvent(*this, TaskSource::DatabaseAccess, WTFMove(event));
}

void IDBTransaction::dispatchEvent(Event& event)
{
    LOG(IndexedDB, "IDBTransaction::dispatchEvent");

    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));
    ASSERT(scriptExecutionContext());
    ASSERT(!isContextStopped());
    ASSERT(event.type() == eventNames().completeEvent || event.type() == eventNames().abortEvent);

    auto protectedThis = makeRef(*this);

    EventDispatcher::dispatchEvent({ this, m_database.ptr() }, event);
    m_didDispatchAbortOrCommit = true;

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
}

Ref<IDBObjectStore> IDBTransaction::createObjectStore(const IDBObjectStoreInfo& info)
{
    LOG(IndexedDB, "IDBTransaction::createObjectStore");
    ASSERT(isVersionChange());
    ASSERT(scriptExecutionContext());
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    Locker<Lock> locker(m_referencedObjectStoreLock);

    auto objectStore = makeUnique<IDBObjectStore>(*scriptExecutionContext(), info, *this);
    auto* rawObjectStore = objectStore.get();
    m_referencedObjectStores.set(info.name(), WTFMove(objectStore));

    LOG(IndexedDBOperations, "IDB create object store operation: %s", info.condensedLoggingString().utf8().data());
    scheduleOperation(IDBClient::TransactionOperationImpl::create(*this, [protectedThis = makeRef(*this)] (const auto& result) {
        protectedThis->didCreateObjectStoreOnServer(result);
    }, [protectedThis = makeRef(*this), info = info.isolatedCopy()] (auto& operation) {
        protectedThis->createObjectStoreOnServer(operation, info);
    }), IsWriteOperation::Yes);

    return *rawObjectStore;
}

void IDBTransaction::createObjectStoreOnServer(IDBClient::TransactionOperation& operation, const IDBObjectStoreInfo& info)
{
    LOG(IndexedDB, "IDBTransaction::createObjectStoreOnServer");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));
    ASSERT(isVersionChange());

    m_database->connectionProxy().createObjectStore(operation, info);
}

void IDBTransaction::didCreateObjectStoreOnServer(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBTransaction::didCreateObjectStoreOnServer");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));
    ASSERT_UNUSED(resultData, resultData.type() == IDBResultType::CreateObjectStoreSuccess || resultData.type() == IDBResultType::Error);
}

void IDBTransaction::renameObjectStore(IDBObjectStore& objectStore, const String& newName)
{
    LOG(IndexedDB, "IDBTransaction::renameObjectStore");

    Locker<Lock> locker(m_referencedObjectStoreLock);

    ASSERT(isVersionChange());
    ASSERT(scriptExecutionContext());
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    ASSERT(m_referencedObjectStores.contains(objectStore.info().name()));
    ASSERT(!m_referencedObjectStores.contains(newName));
    ASSERT(m_referencedObjectStores.get(objectStore.info().name()) == &objectStore);

    uint64_t objectStoreIdentifier = objectStore.info().identifier();

    LOG(IndexedDBOperations, "IDB rename object store operation: %s to %s", objectStore.info().condensedLoggingString().utf8().data(), newName.utf8().data());
    scheduleOperation(IDBClient::TransactionOperationImpl::create(*this, [protectedThis = makeRef(*this)] (const auto& result) {
        protectedThis->didRenameObjectStoreOnServer(result);
    }, [protectedThis = makeRef(*this), objectStoreIdentifier, newName = newName.isolatedCopy()] (auto& operation) {
        protectedThis->renameObjectStoreOnServer(operation, objectStoreIdentifier, newName);
    }), IsWriteOperation::Yes);

    m_referencedObjectStores.set(newName, m_referencedObjectStores.take(objectStore.info().name()));
}

void IDBTransaction::renameObjectStoreOnServer(IDBClient::TransactionOperation& operation, const uint64_t& objectStoreIdentifier, const String& newName)
{
    LOG(IndexedDB, "IDBTransaction::renameObjectStoreOnServer");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));
    ASSERT(isVersionChange());

    m_database->connectionProxy().renameObjectStore(operation, objectStoreIdentifier, newName);
}

void IDBTransaction::didRenameObjectStoreOnServer(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBTransaction::didRenameObjectStoreOnServer");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));
    ASSERT_UNUSED(resultData, resultData.type() == IDBResultType::RenameObjectStoreSuccess || resultData.type() == IDBResultType::Error);
}

std::unique_ptr<IDBIndex> IDBTransaction::createIndex(IDBObjectStore& objectStore, const IDBIndexInfo& info)
{
    LOG(IndexedDB, "IDBTransaction::createIndex");
    ASSERT(isVersionChange());
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    if (!scriptExecutionContext())
        return nullptr;

    LOG(IndexedDBOperations, "IDB create index operation: %s under object store %s", info.condensedLoggingString().utf8().data(), objectStore.info().condensedLoggingString().utf8().data());
    scheduleOperation(IDBClient::TransactionOperationImpl::create(*this, [protectedThis = makeRef(*this)] (const auto& result) {
        protectedThis->didCreateIndexOnServer(result);
    }, [protectedThis = makeRef(*this), info = info.isolatedCopy()] (auto& operation) {
        protectedThis->createIndexOnServer(operation, info);
    }), IsWriteOperation::Yes);

    return makeUnique<IDBIndex>(*scriptExecutionContext(), info, objectStore);
}

void IDBTransaction::createIndexOnServer(IDBClient::TransactionOperation& operation, const IDBIndexInfo& info)
{
    LOG(IndexedDB, "IDBTransaction::createIndexOnServer");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));
    ASSERT(isVersionChange());

    m_database->connectionProxy().createIndex(operation, info);
}

void IDBTransaction::didCreateIndexOnServer(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBTransaction::didCreateIndexOnServer");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    if (resultData.type() == IDBResultType::CreateIndexSuccess)
        return;

    ASSERT(resultData.type() == IDBResultType::Error);

    // This operation might have failed because the transaction is already aborting.
    if (m_state == IndexedDB::TransactionState::Aborting)
        return;

    // Otherwise, failure to create an index forced abortion of the transaction.
    abortDueToFailedRequest(DOMException::create(resultData.error().message(), resultData.error().name()));
}

void IDBTransaction::renameIndex(IDBIndex& index, const String& newName)
{
    LOG(IndexedDB, "IDBTransaction::renameIndex");
    Locker<Lock> locker(m_referencedObjectStoreLock);

    ASSERT(isVersionChange());
    ASSERT(scriptExecutionContext());
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    ASSERT(m_referencedObjectStores.contains(index.objectStore().info().name()));
    ASSERT(m_referencedObjectStores.get(index.objectStore().info().name()) == &index.objectStore());

    index.objectStore().renameReferencedIndex(index, newName);

    uint64_t objectStoreIdentifier = index.objectStore().info().identifier();
    uint64_t indexIdentifier = index.info().identifier();

    LOG(IndexedDBOperations, "IDB rename index operation: %s to %s under object store %" PRIu64, index.info().condensedLoggingString().utf8().data(), newName.utf8().data(), index.info().objectStoreIdentifier());
    scheduleOperation(IDBClient::TransactionOperationImpl::create(*this, [protectedThis = makeRef(*this)] (const auto& result) {
        protectedThis->didRenameIndexOnServer(result);
    }, [protectedThis = makeRef(*this), objectStoreIdentifier, indexIdentifier, newName = newName.isolatedCopy()] (auto& operation) {
        protectedThis->renameIndexOnServer(operation, objectStoreIdentifier, indexIdentifier, newName);
    }), IsWriteOperation::Yes);
}

void IDBTransaction::renameIndexOnServer(IDBClient::TransactionOperation& operation, const uint64_t& objectStoreIdentifier, const uint64_t& indexIdentifier, const String& newName)
{
    LOG(IndexedDB, "IDBTransaction::renameIndexOnServer");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));
    ASSERT(isVersionChange());

    m_database->connectionProxy().renameIndex(operation, objectStoreIdentifier, indexIdentifier, newName);
}

void IDBTransaction::didRenameIndexOnServer(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBTransaction::didRenameIndexOnServer");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));
    ASSERT_UNUSED(resultData, resultData.type() == IDBResultType::RenameIndexSuccess || resultData.type() == IDBResultType::Error);
}

Ref<IDBRequest> IDBTransaction::requestOpenCursor(JSGlobalObject& state, IDBObjectStore& objectStore, const IDBCursorInfo& info)
{
    LOG(IndexedDB, "IDBTransaction::requestOpenCursor");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    if (info.cursorType() == IndexedDB::CursorType::KeyOnly)
        return doRequestOpenCursor(state, IDBCursor::create(objectStore, info));

    return doRequestOpenCursor(state, IDBCursorWithValue::create(objectStore, info));
}

Ref<IDBRequest> IDBTransaction::requestOpenCursor(JSGlobalObject& state, IDBIndex& index, const IDBCursorInfo& info)
{
    LOG(IndexedDB, "IDBTransaction::requestOpenCursor");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    if (info.cursorType() == IndexedDB::CursorType::KeyOnly)
        return doRequestOpenCursor(state, IDBCursor::create(index, info));

    return doRequestOpenCursor(state, IDBCursorWithValue::create(index, info));
}

Ref<IDBRequest> IDBTransaction::doRequestOpenCursor(JSGlobalObject& state, Ref<IDBCursor>&& cursor)
{
    ASSERT(isActive());
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    ASSERT_UNUSED(state, scriptExecutionContext() == scriptExecutionContextFromExecState(&state));

    auto request = IDBRequest::create(*scriptExecutionContext(), cursor.get(), *this);
    addRequest(request.get());

    LOG(IndexedDBOperations, "IDB open cursor operation: %s", cursor->info().loggingString().utf8().data());
    scheduleOperation(IDBClient::TransactionOperationImpl::create(*this, request.get(), [protectedThis = makeRef(*this), request] (const auto& result) {
        protectedThis->didOpenCursorOnServer(request.get(), result);
    }, [protectedThis = makeRef(*this), info = cursor->info().isolatedCopy()] (auto& operation) {
        protectedThis->openCursorOnServer(operation, info);
    }));

    return request;
}

void IDBTransaction::openCursorOnServer(IDBClient::TransactionOperation& operation, const IDBCursorInfo& info)
{
    LOG(IndexedDB, "IDBTransaction::openCursorOnServer");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    m_database->connectionProxy().openCursor(operation, info);
}

void IDBTransaction::didOpenCursorOnServer(IDBRequest& request, const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBTransaction::didOpenCursorOnServer");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    completeCursorRequest(request, resultData);
}

void IDBTransaction::iterateCursor(IDBCursor& cursor, const IDBIterateCursorData& data)
{
    LOG(IndexedDB, "IDBTransaction::iterateCursor");
    ASSERT(isActive());
    ASSERT(cursor.request());
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    addRequest(*cursor.request());

    LOG(IndexedDBOperations, "IDB iterate cursor operation: %s %s", cursor.info().loggingString().utf8().data(), data.loggingString().utf8().data());
    scheduleOperation(IDBClient::TransactionOperationImpl::create(*this, *cursor.request(), [protectedThis = makeRef(*this), request = makeRef(*cursor.request())] (const auto& result) {
        protectedThis->didIterateCursorOnServer(request.get(), result);
    }, [protectedThis = makeRef(*this), data = data.isolatedCopy()] (auto& operation) {
        protectedThis->iterateCursorOnServer(operation, data);
    }));
}

// FIXME: changes here
void IDBTransaction::iterateCursorOnServer(IDBClient::TransactionOperation& operation, const IDBIterateCursorData& data)
{
    LOG(IndexedDB, "IDBTransaction::iterateCursorOnServer");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));
    ASSERT(operation.idbRequest());

    auto* cursor = operation.idbRequest()->pendingCursor();
    ASSERT(cursor);

    if (data.keyData.isNull() && data.primaryKeyData.isNull()) {
        if (auto getResult = cursor->iterateWithPrefetchedRecords(data.count, m_lastWriteOperationID)) {
            auto result = IDBResultData::iterateCursorSuccess(operation.identifier(), getResult.value());
            m_database->connectionProxy().iterateCursor(operation, { data.keyData, data.primaryKeyData, data.count, IndexedDB::CursorIterateOption::DoNotReply });
            operationCompletedOnServer(result, operation);
            return;
        }
    }

    cursor->clearPrefetchedRecords();

    ASSERT(data.option == IndexedDB::CursorIterateOption::Reply);
    m_database->connectionProxy().iterateCursor(operation, data);
}

void IDBTransaction::didIterateCursorOnServer(IDBRequest& request, const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBTransaction::didIterateCursorOnServer");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    completeCursorRequest(request, resultData);
}

Ref<IDBRequest> IDBTransaction::requestGetAllObjectStoreRecords(JSC::JSGlobalObject& state, IDBObjectStore& objectStore, const IDBKeyRangeData& keyRangeData, IndexedDB::GetAllType getAllType, Optional<uint32_t> count)
{
    LOG(IndexedDB, "IDBTransaction::requestGetAllObjectStoreRecords");
    ASSERT(isActive());
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    ASSERT_UNUSED(state, scriptExecutionContext() == scriptExecutionContextFromExecState(&state));

    auto request = IDBRequest::create(*scriptExecutionContext(), objectStore, *this);
    addRequest(request.get());

    IDBGetAllRecordsData getAllRecordsData { keyRangeData, getAllType, count, objectStore.info().identifier(), 0 };

    LOG(IndexedDBOperations, "IDB get all object store records operation: %s", getAllRecordsData.loggingString().utf8().data());
    scheduleOperation(IDBClient::TransactionOperationImpl::create(*this, request.get(), [protectedThis = makeRef(*this), request] (const auto& result) {
        protectedThis->didGetAllRecordsOnServer(request.get(), result);
    }, [protectedThis = makeRef(*this), getAllRecordsData = getAllRecordsData.isolatedCopy()] (auto& operation) {
        protectedThis->getAllRecordsOnServer(operation, getAllRecordsData);
    }));

    return request;
}

Ref<IDBRequest> IDBTransaction::requestGetAllIndexRecords(JSC::JSGlobalObject& state, IDBIndex& index, const IDBKeyRangeData& keyRangeData, IndexedDB::GetAllType getAllType, Optional<uint32_t> count)
{
    LOG(IndexedDB, "IDBTransaction::requestGetAllIndexRecords");
    ASSERT(isActive());
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    ASSERT_UNUSED(state, scriptExecutionContext() == scriptExecutionContextFromExecState(&state));

    auto request = IDBRequest::create(*scriptExecutionContext(), index, *this);
    addRequest(request.get());

    IDBGetAllRecordsData getAllRecordsData { keyRangeData, getAllType, count, index.objectStore().info().identifier(), index.info().identifier() };

    LOG(IndexedDBOperations, "IDB get all index records operation: %s", getAllRecordsData.loggingString().utf8().data());
    scheduleOperation(IDBClient::TransactionOperationImpl::create(*this, request.get(), [protectedThis = makeRef(*this), request] (const auto& result) {
        protectedThis->didGetAllRecordsOnServer(request.get(), result);
    }, [protectedThis = makeRef(*this), getAllRecordsData = getAllRecordsData.isolatedCopy()] (auto& operation) {
        protectedThis->getAllRecordsOnServer(operation, getAllRecordsData);
    }));

    return request;
}

void IDBTransaction::getAllRecordsOnServer(IDBClient::TransactionOperation& operation, const IDBGetAllRecordsData& getAllRecordsData)
{
    LOG(IndexedDB, "IDBTransaction::getAllRecordsOnServer");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    m_database->connectionProxy().getAllRecords(operation, getAllRecordsData);
}

void IDBTransaction::didGetAllRecordsOnServer(IDBRequest& request, const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBTransaction::didGetAllRecordsOnServer");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

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
        request.setResult(getAllResult);
        break;
    }

    completeNoncursorRequest(request, resultData);
}

Ref<IDBRequest> IDBTransaction::requestGetRecord(JSGlobalObject& state, IDBObjectStore& objectStore, const IDBGetRecordData& getRecordData)
{
    LOG(IndexedDB, "IDBTransaction::requestGetRecord");
    ASSERT(isActive());
    ASSERT(!getRecordData.keyRangeData.isNull);
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    ASSERT_UNUSED(state, scriptExecutionContext() == scriptExecutionContextFromExecState(&state));

    IndexedDB::ObjectStoreRecordType type = getRecordData.type == IDBGetRecordDataType::KeyAndValue ? IndexedDB::ObjectStoreRecordType::ValueOnly : IndexedDB::ObjectStoreRecordType::KeyOnly;

    auto request = IDBRequest::createObjectStoreGet(*scriptExecutionContext(), objectStore, type, *this);
    addRequest(request.get());

    LOG(IndexedDBOperations, "IDB get record operation: %s %s", objectStore.info().condensedLoggingString().utf8().data(), getRecordData.loggingString().utf8().data());
    scheduleOperation(IDBClient::TransactionOperationImpl::create(*this, request.get(), [protectedThis = makeRef(*this), request] (const auto& result) {
        protectedThis->didGetRecordOnServer(request.get(), result);
    }, [protectedThis = makeRef(*this), getRecordData = getRecordData.isolatedCopy()] (auto& operation) {
        protectedThis->getRecordOnServer(operation, getRecordData);
    }));

    return request;
}

Ref<IDBRequest> IDBTransaction::requestGetValue(JSGlobalObject& state, IDBIndex& index, const IDBKeyRangeData& range)
{
    LOG(IndexedDB, "IDBTransaction::requestGetValue");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    return requestIndexRecord(state, index, IndexedDB::IndexRecordType::Value, range);
}

Ref<IDBRequest> IDBTransaction::requestGetKey(JSGlobalObject& state, IDBIndex& index, const IDBKeyRangeData& range)
{
    LOG(IndexedDB, "IDBTransaction::requestGetValue");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    return requestIndexRecord(state, index, IndexedDB::IndexRecordType::Key, range);
}

Ref<IDBRequest> IDBTransaction::requestIndexRecord(JSGlobalObject& state, IDBIndex& index, IndexedDB::IndexRecordType type, const IDBKeyRangeData& range)
{
    LOG(IndexedDB, "IDBTransaction::requestGetValue");
    ASSERT(isActive());
    ASSERT(!range.isNull);
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    ASSERT_UNUSED(state, scriptExecutionContext() == scriptExecutionContextFromExecState(&state));

    auto request = IDBRequest::createIndexGet(*scriptExecutionContext(), index, type, *this);
    addRequest(request.get());

    IDBGetRecordData getRecordData = { range, IDBGetRecordDataType::KeyAndValue };

    LOG(IndexedDBOperations, "IDB get index record operation: %s %s", index.info().condensedLoggingString().utf8().data(), getRecordData.loggingString().utf8().data());
    scheduleOperation(IDBClient::TransactionOperationImpl::create(*this, request.get(), [protectedThis = makeRef(*this), request] (const auto& result) {
        protectedThis->didGetRecordOnServer(request.get(), result);
    }, [protectedThis = makeRef(*this), getRecordData = getRecordData.isolatedCopy()] (auto& operation) {
        protectedThis->getRecordOnServer(operation, getRecordData);
    }));

    return request;
}

void IDBTransaction::getRecordOnServer(IDBClient::TransactionOperation& operation, const IDBGetRecordData& getRecordData)
{
    LOG(IndexedDB, "IDBTransaction::getRecordOnServer");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    m_database->connectionProxy().getRecord(operation, getRecordData);
}

void IDBTransaction::didGetRecordOnServer(IDBRequest& request, const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBTransaction::didGetRecordOnServer");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

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
            request.setResultToStructuredClone(resultData.getResult());
        else
            request.setResultToUndefined();
    }

    completeNoncursorRequest(request, resultData);
}

Ref<IDBRequest> IDBTransaction::requestCount(JSGlobalObject& state, IDBObjectStore& objectStore, const IDBKeyRangeData& range)
{
    LOG(IndexedDB, "IDBTransaction::requestCount (IDBObjectStore)");
    ASSERT(isActive());
    ASSERT(!range.isNull);
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    ASSERT_UNUSED(state, scriptExecutionContext() == scriptExecutionContextFromExecState(&state));

    auto request = IDBRequest::create(*scriptExecutionContext(), objectStore, *this);
    addRequest(request.get());

    LOG(IndexedDBOperations, "IDB object store count operation: %s, range %s", objectStore.info().condensedLoggingString().utf8().data(), range.loggingString().utf8().data());
    scheduleOperation(IDBClient::TransactionOperationImpl::create(*this, request.get(), [protectedThis = makeRef(*this), request] (const auto& result) {
        protectedThis->didGetCountOnServer(request.get(), result);
    }, [protectedThis = makeRef(*this), range = range.isolatedCopy()] (auto& operation) {
        protectedThis->getCountOnServer(operation, range);
    }));

    return request;
}

Ref<IDBRequest> IDBTransaction::requestCount(JSGlobalObject& state, IDBIndex& index, const IDBKeyRangeData& range)
{
    LOG(IndexedDB, "IDBTransaction::requestCount (IDBIndex)");
    ASSERT(isActive());
    ASSERT(!range.isNull);
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    ASSERT_UNUSED(state, scriptExecutionContext() == scriptExecutionContextFromExecState(&state));

    auto request = IDBRequest::create(*scriptExecutionContext(), index, *this);
    addRequest(request.get());

    LOG(IndexedDBOperations, "IDB index count operation: %s, range %s", index.info().condensedLoggingString().utf8().data(), range.loggingString().utf8().data());
    scheduleOperation(IDBClient::TransactionOperationImpl::create(*this, request.get(), [protectedThis = makeRef(*this), request] (const auto& result) {
        protectedThis->didGetCountOnServer(request.get(), result);
    }, [protectedThis = makeRef(*this), range = range.isolatedCopy()] (auto& operation) {
        protectedThis->getCountOnServer(operation, range);
    }));

    return request;
}

void IDBTransaction::getCountOnServer(IDBClient::TransactionOperation& operation, const IDBKeyRangeData& keyRange)
{
    LOG(IndexedDB, "IDBTransaction::getCountOnServer");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    m_database->connectionProxy().getCount(operation, keyRange);
}

void IDBTransaction::didGetCountOnServer(IDBRequest& request, const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBTransaction::didGetCountOnServer");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    request.setResult(resultData.resultInteger());
    completeNoncursorRequest(request, resultData);
}

Ref<IDBRequest> IDBTransaction::requestDeleteRecord(JSGlobalObject& state, IDBObjectStore& objectStore, const IDBKeyRangeData& range)
{
    LOG(IndexedDB, "IDBTransaction::requestDeleteRecord");
    ASSERT(isActive());
    ASSERT(!range.isNull);
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    ASSERT_UNUSED(state, scriptExecutionContext() == scriptExecutionContextFromExecState(&state));

    auto request = IDBRequest::create(*scriptExecutionContext(), objectStore, *this);
    addRequest(request.get());

    LOG(IndexedDBOperations, "IDB delete record operation: %s, range %s", objectStore.info().condensedLoggingString().utf8().data(), range.loggingString().utf8().data());
    scheduleOperation(IDBClient::TransactionOperationImpl::create(*this, request.get(), [protectedThis = makeRef(*this), request] (const auto& result) {
        protectedThis->didDeleteRecordOnServer(request.get(), result);
    }, [protectedThis = makeRef(*this), range = range.isolatedCopy()] (auto& operation) {
        protectedThis->deleteRecordOnServer(operation, range);
    }), IsWriteOperation::Yes);
    return request;
}

void IDBTransaction::deleteRecordOnServer(IDBClient::TransactionOperation& operation, const IDBKeyRangeData& keyRange)
{
    LOG(IndexedDB, "IDBTransaction::deleteRecordOnServer");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    m_database->connectionProxy().deleteRecord(operation, keyRange);
}

void IDBTransaction::didDeleteRecordOnServer(IDBRequest& request, const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBTransaction::didDeleteRecordOnServer");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    request.setResultToUndefined();
    completeNoncursorRequest(request, resultData);
}

Ref<IDBRequest> IDBTransaction::requestClearObjectStore(JSGlobalObject& state, IDBObjectStore& objectStore)
{
    LOG(IndexedDB, "IDBTransaction::requestClearObjectStore");
    ASSERT(isActive());
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    ASSERT_UNUSED(state, scriptExecutionContext() == scriptExecutionContextFromExecState(&state));

    auto request = IDBRequest::create(*scriptExecutionContext(), objectStore, *this);
    addRequest(request.get());

    uint64_t objectStoreIdentifier = objectStore.info().identifier();

    LOG(IndexedDBOperations, "IDB clear object store operation: %s", objectStore.info().condensedLoggingString().utf8().data());
    scheduleOperation(IDBClient::TransactionOperationImpl::create(*this, request.get(), [protectedThis = makeRef(*this), request] (const auto& result) {
        protectedThis->didClearObjectStoreOnServer(request.get(), result);
    }, [protectedThis = makeRef(*this), objectStoreIdentifier] (auto& operation) {
        protectedThis->clearObjectStoreOnServer(operation, objectStoreIdentifier);
    }), IsWriteOperation::Yes);

    return request;
}

void IDBTransaction::clearObjectStoreOnServer(IDBClient::TransactionOperation& operation, const uint64_t& objectStoreIdentifier)
{
    LOG(IndexedDB, "IDBTransaction::clearObjectStoreOnServer");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    m_database->connectionProxy().clearObjectStore(operation, objectStoreIdentifier);
}

void IDBTransaction::didClearObjectStoreOnServer(IDBRequest& request, const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBTransaction::didClearObjectStoreOnServer");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    request.setResultToUndefined();
    completeNoncursorRequest(request, resultData);
}

Ref<IDBRequest> IDBTransaction::requestPutOrAdd(JSGlobalObject& state, IDBObjectStore& objectStore, RefPtr<IDBKey>&& key, SerializedScriptValue& value, IndexedDB::ObjectStoreOverwriteMode overwriteMode)
{
    LOG(IndexedDB, "IDBTransaction::requestPutOrAdd");
    ASSERT(isActive());
    ASSERT(!isReadOnly());
    ASSERT(objectStore.info().autoIncrement() || key);
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    ASSERT_UNUSED(state, scriptExecutionContext() == scriptExecutionContextFromExecState(&state));

    auto request = IDBRequest::create(*scriptExecutionContext(), objectStore, *this);
    addRequest(request.get());

    LOG(IndexedDBOperations, "IDB putOrAdd operation: %s key: %s", objectStore.info().condensedLoggingString().utf8().data(), key ? key->loggingString().utf8().data() : "<null key>");
    scheduleOperation(IDBClient::TransactionOperationImpl::create(*this, request.get(), [protectedThis = makeRef(*this), request] (const auto& result) {
        protectedThis->didPutOrAddOnServer(request.get(), result);
    }, [protectedThis = makeRef(*this), key, value = makeRef(value), overwriteMode] (auto& operation) {
        protectedThis->putOrAddOnServer(operation, key.get(), value.ptr(), overwriteMode);
    }), IsWriteOperation::Yes);

    return request;
}

void IDBTransaction::putOrAddOnServer(IDBClient::TransactionOperation& operation, RefPtr<IDBKey> key, RefPtr<SerializedScriptValue> value, const IndexedDB::ObjectStoreOverwriteMode& overwriteMode)
{
    LOG(IndexedDB, "IDBTransaction::putOrAddOnServer");
    ASSERT(canCurrentThreadAccessThreadLocalData(originThread()));
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
            auto result = IDBResultData::error(operation.identifier(), IDBError { UnknownError, "Error preparing Blob/File data to be stored in object store"_s });
            scriptExecutionContext()->postTask([protectedOperation = WTFMove(protectedOperation), result = WTFMove(result)](ScriptExecutionContext&) {
                protectedOperation->doComplete(result);
            });
        }
        return;
    }

    // Since this request won't actually go to the server until the blob writes are complete,
    // stop future requests from going to the server ahead of it.
    operation.setNextRequestCanGoToServer(false);

    value->writeBlobsToDiskForIndexedDB([protectedThis = makeRef(*this), this, protectedOperation = Ref<IDBClient::TransactionOperation>(operation), keyData = IDBKeyData(key.get()).isolatedCopy(), overwriteMode](IDBValue&& idbValue) mutable {
        ASSERT(canCurrentThreadAccessThreadLocalData(originThread()));
        ASSERT(isMainThread());
        if (idbValue.data().data()) {
            m_database->connectionProxy().putOrAdd(protectedOperation.get(), WTFMove(keyData), idbValue, overwriteMode);
            return;
        }

        // If the IDBValue doesn't have any data, then something went wrong writing the blobs to disk.
        // In that case, we cannot successfully store this record, so we callback with an error.
        auto result = IDBResultData::error(protectedOperation->identifier(), IDBError { UnknownError, "Error preparing Blob/File data to be stored in object store"_s });
        callOnMainThread([protectedThis = WTFMove(protectedThis), protectedOperation = WTFMove(protectedOperation), result = WTFMove(result)]() mutable {
            protectedOperation->doComplete(result);
        });
    });
}

void IDBTransaction::didPutOrAddOnServer(IDBRequest& request, const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBTransaction::didPutOrAddOnServer");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    if (auto* result = resultData.resultKey())
        request.setResult(*result);
    else
        request.setResultToUndefined();
    completeNoncursorRequest(request, resultData);
}

void IDBTransaction::deleteObjectStore(const String& objectStoreName)
{
    LOG(IndexedDB, "IDBTransaction::deleteObjectStore");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));
    ASSERT(isVersionChange());

    Locker<Lock> locker(m_referencedObjectStoreLock);

    if (auto objectStore = m_referencedObjectStores.take(objectStoreName)) {
        objectStore->markAsDeleted();
        auto identifier = objectStore->info().identifier();
        m_deletedObjectStores.set(identifier, WTFMove(objectStore));
    }

    LOG(IndexedDBOperations, "IDB delete object store operation: %s", objectStoreName.utf8().data());
    scheduleOperation(IDBClient::TransactionOperationImpl::create(*this, [protectedThis = makeRef(*this)] (const auto& result) {
        protectedThis->didDeleteObjectStoreOnServer(result);
    }, [protectedThis = makeRef(*this), objectStoreName = objectStoreName.isolatedCopy()] (auto& operation) {
        protectedThis->deleteObjectStoreOnServer(operation, objectStoreName);
    }), IsWriteOperation::Yes);
}

void IDBTransaction::deleteObjectStoreOnServer(IDBClient::TransactionOperation& operation, const String& objectStoreName)
{
    LOG(IndexedDB, "IDBTransaction::deleteObjectStoreOnServer");
    ASSERT(isVersionChange());
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    m_database->connectionProxy().deleteObjectStore(operation, objectStoreName);
}

void IDBTransaction::didDeleteObjectStoreOnServer(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBTransaction::didDeleteObjectStoreOnServer");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));
    ASSERT_UNUSED(resultData, resultData.type() == IDBResultType::DeleteObjectStoreSuccess || resultData.type() == IDBResultType::Error);
}

void IDBTransaction::deleteIndex(uint64_t objectStoreIdentifier, const String& indexName)
{
    LOG(IndexedDB, "IDBTransaction::deleteIndex");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));
    ASSERT(isVersionChange());

    LOG(IndexedDBOperations, "IDB delete index operation: %s (%" PRIu64 ")", indexName.utf8().data(), objectStoreIdentifier);
    scheduleOperation(IDBClient::TransactionOperationImpl::create(*this, [protectedThis = makeRef(*this)] (const auto& result) {
        protectedThis->didDeleteIndexOnServer(result);
    }, [protectedThis = makeRef(*this), objectStoreIdentifier, indexName = indexName.isolatedCopy()] (auto& operation) {
        protectedThis->deleteIndexOnServer(operation, objectStoreIdentifier, indexName);
    }), IsWriteOperation::Yes);
}

void IDBTransaction::deleteIndexOnServer(IDBClient::TransactionOperation& operation, const uint64_t& objectStoreIdentifier, const String& indexName)
{
    LOG(IndexedDB, "IDBTransaction::deleteIndexOnServer");
    ASSERT(isVersionChange());
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    m_database->connectionProxy().deleteIndex(operation, objectStoreIdentifier, indexName);
}

void IDBTransaction::didDeleteIndexOnServer(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBTransaction::didDeleteIndexOnServer");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));
    ASSERT_UNUSED(resultData, resultData.type() == IDBResultType::DeleteIndexSuccess || resultData.type() == IDBResultType::Error);
}

void IDBTransaction::operationCompletedOnClient(IDBClient::TransactionOperation& operation)
{
    LOG(IndexedDB, "IDBTransaction::operationCompletedOnClient");

    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));
    ASSERT(canCurrentThreadAccessThreadLocalData(operation.originThread()));
    ASSERT(m_transactionOperationMap.get(operation.identifier()) == &operation);
    ASSERT(m_transactionOperationsInProgressQueue.first() == &operation);

    m_transactionOperationMap.remove(operation.identifier());
    m_transactionOperationsInProgressQueue.removeFirst();

    if (m_transactionOperationsInProgressQueue.isEmpty())
        handlePendingOperations();

    autoCommit();
}

void IDBTransaction::establishOnServer()
{
    LOG(IndexedDB, "IDBTransaction::establishOnServer");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    m_database->connectionProxy().establishTransaction(*this);
}

void IDBTransaction::activate()
{
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    if (isFinishedOrFinishing())
        return;

    m_state = IndexedDB::TransactionState::Active;
}

void IDBTransaction::deactivate()
{
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    if (m_state == IndexedDB::TransactionState::Active)
        m_state = IndexedDB::TransactionState::Inactive;

    autoCommit();
}

void IDBTransaction::connectionClosedFromServer(const IDBError& error)
{
    LOG(IndexedDB, "IDBTransaction::connectionClosedFromServer - %s", error.message().utf8().data());

    m_database->willAbortTransaction(*this);
    m_state = IndexedDB::TransactionState::Aborting;

    // Move operations out of m_pendingTransactionOperationQueue, otherwise we may start handling
    // them after we forcibly complete in-progress transactions.
    Deque<RefPtr<IDBClient::TransactionOperation>> pendingTransactionOperationQueue;
    pendingTransactionOperationQueue.swap(m_pendingTransactionOperationQueue);

    abortInProgressOperations(error);

    auto operations = copyToVector(m_transactionOperationMap.values());
    for (auto& operation : operations) {
        m_currentlyCompletingRequest = nullptr;
        m_transactionOperationsInProgressQueue.append(operation.get());
        ASSERT(m_transactionOperationsInProgressQueue.first() == operation.get());
        operation->doComplete(IDBResultData::error(operation->identifier(), error));
    }
    m_currentlyCompletingRequest = nullptr;
    m_openRequests.clear();
    pendingTransactionOperationQueue.clear();

    connectionProxy().forgetActiveOperations(operations);
    connectionProxy().forgetTransaction(*this);

    m_abortQueue.clear();
    m_transactionOperationMap.clear();

    m_idbError = error;
    m_domError = error.toDOMException();
    m_database->didAbortTransaction(*this);
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

void IDBTransaction::handlePendingOperations()
{
    ASSERT(canCurrentThreadAccessThreadLocalData(m_database->originThread()));

    if (!m_startedOnServer)
        return;

    if (!m_transactionOperationsInProgressQueue.isEmpty() && !m_transactionOperationsInProgressQueue.last()->nextRequestCanGoToServer())
        return;

    while (!m_pendingTransactionOperationQueue.isEmpty()) {
        auto operation = m_pendingTransactionOperationQueue.takeFirst();
        m_transactionOperationsInProgressQueue.append(operation.get());
        operation->perform();

        if (!operation->nextRequestCanGoToServer())
            break;
    }
}

void IDBTransaction::autoCommit()
{
    // If transaction is not inactive, it's active, finished or finishing.
    // If it's active, it may create new requests, so we cannot commit it.
    if (m_state != IndexedDB::TransactionState::Inactive)
        return;

    if (!m_startedOnServer)
        return;

    if (!m_transactionOperationMap.isEmpty())
        return;

    if (!m_openRequests.isEmpty())
        return;
    ASSERT(!m_currentlyCompletingRequest);

    commit();
}

uint64_t IDBTransaction::generateOperationID()
{
    static std::atomic<uint64_t> currentOperationID(1);
    return currentOperationID += 1;
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
