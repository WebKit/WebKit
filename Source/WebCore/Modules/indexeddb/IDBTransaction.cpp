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
#include "IDBTransaction.h"

#if ENABLE(INDEXED_DATABASE)

#include "DOMError.h"
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
#include "TransactionOperation.h"
#include <wtf/NeverDestroyed.h>

using namespace JSC;

namespace WebCore {

const AtomicString& IDBTransaction::modeReadOnly()
{
    static NeverDestroyed<AtomicString> readonly("readonly", AtomicString::ConstructFromLiteral);
    return readonly;
}

const AtomicString& IDBTransaction::modeReadWrite()
{
    static NeverDestroyed<AtomicString> readwrite("readwrite", AtomicString::ConstructFromLiteral);
    return readwrite;
}

const AtomicString& IDBTransaction::modeVersionChange()
{
    static NeverDestroyed<AtomicString> versionchange("versionchange", AtomicString::ConstructFromLiteral);
    return versionchange;
}

const AtomicString& IDBTransaction::modeReadOnlyLegacy()
{
    static NeverDestroyed<AtomicString> readonly("0", AtomicString::ConstructFromLiteral);
    return readonly;
}

const AtomicString& IDBTransaction::modeReadWriteLegacy()
{
    static NeverDestroyed<AtomicString> readwrite("1", AtomicString::ConstructFromLiteral);
    return readwrite;
}

IndexedDB::TransactionMode IDBTransaction::stringToMode(const String& modeString, ExceptionCode& ec)
{
    if (modeString.isNull()
        || modeString == IDBTransaction::modeReadOnly())
        return IndexedDB::TransactionMode::ReadOnly;
    if (modeString == IDBTransaction::modeReadWrite())
        return IndexedDB::TransactionMode::ReadWrite;

    ec = TypeError;
    return IndexedDB::TransactionMode::ReadOnly;
}

const AtomicString& IDBTransaction::modeToString(IndexedDB::TransactionMode mode)
{
    switch (mode) {
    case IndexedDB::TransactionMode::ReadOnly:
        return IDBTransaction::modeReadOnly();

    case IndexedDB::TransactionMode::ReadWrite:
        return IDBTransaction::modeReadWrite();

    case IndexedDB::TransactionMode::VersionChange:
        return IDBTransaction::modeVersionChange();
    }

    ASSERT_NOT_REACHED();
    return IDBTransaction::modeReadOnly();
}

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
    , m_operationTimer(*this, &IDBTransaction::operationTimerFired)
    , m_openDBRequest(request)

{
    LOG(IndexedDB, "IDBTransaction::IDBTransaction - %s", m_info.loggingString().utf8().data());
    ASSERT(currentThread() == m_database->originThreadID());

    if (m_info.mode() == IndexedDB::TransactionMode::VersionChange) {
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

const String& IDBTransaction::mode() const
{
    ASSERT(currentThread() == m_database->originThreadID());

    switch (m_info.mode()) {
    case IndexedDB::TransactionMode::ReadOnly:
        return IDBTransaction::modeReadOnly();
    case IndexedDB::TransactionMode::ReadWrite:
        return IDBTransaction::modeReadWrite();
    case IndexedDB::TransactionMode::VersionChange:
        return IDBTransaction::modeVersionChange();
    }

    RELEASE_ASSERT_NOT_REACHED();
}

WebCore::IDBDatabase* IDBTransaction::db()
{
    ASSERT(currentThread() == m_database->originThreadID());
    return &m_database.get();
}

RefPtr<DOMError> IDBTransaction::error() const
{
    ASSERT(currentThread() == m_database->originThreadID());
    return m_domError;
}

RefPtr<WebCore::IDBObjectStore> IDBTransaction::objectStore(const String& objectStoreName, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBTransaction::objectStore");
    ASSERT(currentThread() == m_database->originThreadID());

    if (!scriptExecutionContext())
        return nullptr;

    if (isFinishedOrFinishing()) {
        ec.code = IDBDatabaseException::InvalidStateError;
        ec.message = ASCIILiteral("Failed to execute 'objectStore' on 'IDBTransaction': The transaction finished.");
        return nullptr;
    }

    auto iterator = m_referencedObjectStores.find(objectStoreName);
    if (iterator != m_referencedObjectStores.end())
        return iterator->value;

    bool found = false;
    for (auto& objectStore : m_info.objectStores()) {
        if (objectStore == objectStoreName) {
            found = true;
            break;
        }
    }

    auto* info = m_database->info().infoForExistingObjectStore(objectStoreName);
    if (!info) {
        ec.code = IDBDatabaseException::NotFoundError;
        ec.message = ASCIILiteral("Failed to execute 'objectStore' on 'IDBTransaction': The specified object store was not found.");
        return nullptr;
    }

    // Version change transactions are scoped to every object store in the database.
    if (!info || (!found && !isVersionChange())) {
        ec.code = IDBDatabaseException::NotFoundError;
        ec.message = ASCIILiteral("Failed to execute 'objectStore' on 'IDBTransaction': The specified object store was not found.");
        return nullptr;
    }

    auto objectStore = IDBObjectStore::create(*scriptExecutionContext(), *info, *this);
    m_referencedObjectStores.set(objectStoreName, &objectStore.get());

    return adoptRef(&objectStore.leakRef());
}


void IDBTransaction::abortDueToFailedRequest(DOMError& error)
{
    LOG(IndexedDB, "IDBTransaction::abortDueToFailedRequest");
    ASSERT(currentThread() == m_database->originThreadID());

    if (isFinishedOrFinishing())
        return;

    m_domError = &error;
    ExceptionCodeWithMessage ec;
    abort(ec);
}

void IDBTransaction::transitionedToFinishing(IndexedDB::TransactionState state)
{
    ASSERT(currentThread() == m_database->originThreadID());

    ASSERT(!isFinishedOrFinishing());
    m_state = state;
    ASSERT(isFinishedOrFinishing());
    m_referencedObjectStores.clear();
}

void IDBTransaction::abort(ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBTransaction::abort");
    ASSERT(currentThread() == m_database->originThreadID());

    if (isFinishedOrFinishing()) {
        ec.code = IDBDatabaseException::InvalidStateError;
        ec.message = ASCIILiteral("Failed to execute 'abort' on 'IDBTransaction': The transaction is inactive or finished.");
        return;
    }

    m_database->willAbortTransaction(*this);

    if (isVersionChange()) {
        for (auto& objectStore : m_referencedObjectStores.values())
            objectStore->rollbackInfoForVersionChangeAbort();
    }

    transitionedToFinishing(IndexedDB::TransactionState::Aborting);
    
    m_abortQueue.swap(m_transactionOperationQueue);

    auto operation = IDBClient::createTransactionOperation(*this, nullptr, &IDBTransaction::abortOnServerAndCancelRequests);
    scheduleOperation(WTFMove(operation));
}

void IDBTransaction::abortOnServerAndCancelRequests(IDBClient::TransactionOperation& operation)
{
    LOG(IndexedDB, "IDBTransaction::abortOnServerAndCancelRequests");
    ASSERT(currentThread() == m_database->originThreadID());
    ASSERT(m_transactionOperationQueue.isEmpty());

    m_database->connectionProxy().abortTransaction(*this);

    ASSERT(m_transactionOperationMap.contains(operation.identifier()));
    m_transactionOperationMap.remove(operation.identifier());

    IDBError error(IDBDatabaseException::AbortError);
    for (auto& operation : m_abortQueue)
        operation->completed(IDBResultData::error(operation->identifier(), error));

    // Since we're aborting, it should be impossible to have queued any further operations.
    ASSERT(m_transactionOperationQueue.isEmpty());
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
    ASSERT(currentThread() == m_database->originThreadID());
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

    ExceptionCodeWithMessage ec;
    abort(ec);
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

    m_transactionOperationQueue.append(operation);
    m_transactionOperationMap.set(operation->identifier(), WTFMove(operation));

    scheduleOperationTimer();
}

void IDBTransaction::scheduleOperationTimer()
{
    ASSERT(currentThread() == m_database->originThreadID());

    if (!m_operationTimer.isActive())
        m_operationTimer.startOneShot(0);
}

void IDBTransaction::operationTimerFired()
{
    LOG(IndexedDB, "IDBTransaction::operationTimerFired (%p)", this);
    ASSERT(currentThread() == m_database->originThreadID());

    if (!m_startedOnServer)
        return;

    if (!m_transactionOperationQueue.isEmpty()) {
        auto operation = m_transactionOperationQueue.takeFirst();
        operation->perform();

        return;
    }

    if (!m_transactionOperationMap.isEmpty() || !m_openRequests.isEmpty())
        return;

    if (!isFinishedOrFinishing())
        commit();
}

void IDBTransaction::commit()
{
    LOG(IndexedDB, "IDBTransaction::commit");
    ASSERT(currentThread() == m_database->originThreadID());
    ASSERT(!isFinishedOrFinishing());

    transitionedToFinishing(IndexedDB::TransactionState::Committing);
    m_database->willCommitTransaction(*this);

    auto operation = IDBClient::createTransactionOperation(*this, nullptr, &IDBTransaction::commitOnServer);
    scheduleOperation(WTFMove(operation));
}

void IDBTransaction::commitOnServer(IDBClient::TransactionOperation& operation)
{
    LOG(IndexedDB, "IDBTransaction::commitOnServer");
    ASSERT(currentThread() == m_database->originThreadID());

    m_database->connectionProxy().commitTransaction(*this);

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

    scheduleOperationTimer();
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

    Ref<IDBObjectStore> objectStore = IDBObjectStore::create(*scriptExecutionContext(), info, *this);
    m_referencedObjectStores.set(info.name(), &objectStore.get());

    auto operation = IDBClient::createTransactionOperation(*this, &IDBTransaction::didCreateObjectStoreOnServer, &IDBTransaction::createObjectStoreOnServer, info);
    scheduleOperation(WTFMove(operation));

    return objectStore;
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

std::unique_ptr<IDBIndex> IDBTransaction::createIndex(IDBObjectStore& objectStore, const IDBIndexInfo& info)
{
    LOG(IndexedDB, "IDBTransaction::createIndex");
    ASSERT(isVersionChange());
    ASSERT(currentThread() == m_database->originThreadID());

    if (!scriptExecutionContext())
        return nullptr;

    auto operation = IDBClient::createTransactionOperation(*this, &IDBTransaction::didCreateIndexOnServer, &IDBTransaction::createIndexOnServer, info);
    scheduleOperation(WTFMove(operation));

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

Ref<IDBRequest> IDBTransaction::requestOpenCursor(ExecState& execState, IDBObjectStore& objectStore, const IDBCursorInfo& info)
{
    LOG(IndexedDB, "IDBTransaction::requestOpenCursor");
    ASSERT(currentThread() == m_database->originThreadID());

    return doRequestOpenCursor(execState, IDBCursorWithValue::create(*this, objectStore, info));
}

Ref<IDBRequest> IDBTransaction::requestOpenCursor(ExecState& execState, IDBIndex& index, const IDBCursorInfo& info)
{
    LOG(IndexedDB, "IDBTransaction::requestOpenCursor");
    ASSERT(currentThread() == m_database->originThreadID());

    if (info.cursorType() == IndexedDB::CursorType::KeyOnly)
        return doRequestOpenCursor(execState, IDBCursor::create(*this, index, info));

    return doRequestOpenCursor(execState, IDBCursorWithValue::create(*this, index, info));
}

Ref<IDBRequest> IDBTransaction::doRequestOpenCursor(ExecState& execState, Ref<IDBCursor>&& cursor)
{
    ASSERT(isActive());
    ASSERT(currentThread() == m_database->originThreadID());

    RELEASE_ASSERT(scriptExecutionContext());
    ASSERT_UNUSED(execState, scriptExecutionContext() == scriptExecutionContextFromExecState(&execState));

    Ref<IDBRequest> request = IDBRequest::create(*scriptExecutionContext(), cursor.get(), *this);
    addRequest(request.get());

    auto operation = IDBClient::createTransactionOperation(*this, request.get(), &IDBTransaction::didOpenCursorOnServer, &IDBTransaction::openCursorOnServer, cursor->info());
    scheduleOperation(WTFMove(operation));

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

    request.didOpenOrIterateCursor(resultData);
}

void IDBTransaction::iterateCursor(IDBCursor& cursor, const IDBKeyData& key, unsigned long count)
{
    LOG(IndexedDB, "IDBTransaction::iterateCursor");
    ASSERT(isActive());
    ASSERT(cursor.request());
    ASSERT(currentThread() == m_database->originThreadID());

    addRequest(*cursor.request());

    auto operation = IDBClient::createTransactionOperation(*this, *cursor.request(), &IDBTransaction::didIterateCursorOnServer, &IDBTransaction::iterateCursorOnServer, key, count);
    scheduleOperation(WTFMove(operation));
}

void IDBTransaction::iterateCursorOnServer(IDBClient::TransactionOperation& operation, const IDBKeyData& key, const unsigned long& count)
{
    LOG(IndexedDB, "IDBTransaction::iterateCursorOnServer");
    ASSERT(currentThread() == m_database->originThreadID());

    m_database->connectionProxy().iterateCursor(operation, key, count);
}

void IDBTransaction::didIterateCursorOnServer(IDBRequest& request, const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBTransaction::didIterateCursorOnServer");
    ASSERT(currentThread() == m_database->originThreadID());

    request.didOpenOrIterateCursor(resultData);
}

Ref<IDBRequest> IDBTransaction::requestGetRecord(ExecState& execState, IDBObjectStore& objectStore, const IDBGetRecordData& getRecordData)
{
    LOG(IndexedDB, "IDBTransaction::requestGetRecord");
    ASSERT(isActive());
    ASSERT(!getRecordData.keyRangeData.isNull);
    ASSERT(currentThread() == m_database->originThreadID());

    RELEASE_ASSERT(scriptExecutionContext());
    ASSERT_UNUSED(execState, scriptExecutionContext() == scriptExecutionContextFromExecState(&execState));

    Ref<IDBRequest> request = IDBRequest::create(*scriptExecutionContext(), objectStore, *this);
    addRequest(request.get());

    auto operation = IDBClient::createTransactionOperation(*this, request.get(), &IDBTransaction::didGetRecordOnServer, &IDBTransaction::getRecordOnServer, getRecordData);
    scheduleOperation(WTFMove(operation));

    return request;
}

Ref<IDBRequest> IDBTransaction::requestGetValue(ExecState& execState, IDBIndex& index, const IDBKeyRangeData& range)
{
    LOG(IndexedDB, "IDBTransaction::requestGetValue");
    ASSERT(currentThread() == m_database->originThreadID());

    return requestIndexRecord(execState, index, IndexedDB::IndexRecordType::Value, range);
}

Ref<IDBRequest> IDBTransaction::requestGetKey(ExecState& execState, IDBIndex& index, const IDBKeyRangeData& range)
{
    LOG(IndexedDB, "IDBTransaction::requestGetValue");
    ASSERT(currentThread() == m_database->originThreadID());

    return requestIndexRecord(execState, index, IndexedDB::IndexRecordType::Key, range);
}

Ref<IDBRequest> IDBTransaction::requestIndexRecord(ExecState& execState, IDBIndex& index, IndexedDB::IndexRecordType type, const IDBKeyRangeData& range)
{
    LOG(IndexedDB, "IDBTransaction::requestGetValue");
    ASSERT(isActive());
    ASSERT(!range.isNull);
    ASSERT(currentThread() == m_database->originThreadID());

    RELEASE_ASSERT(scriptExecutionContext());
    ASSERT_UNUSED(execState, scriptExecutionContext() == scriptExecutionContextFromExecState(&execState));

    Ref<IDBRequest> request = IDBRequest::createGet(*scriptExecutionContext(), index, type, *this);
    addRequest(request.get());

    IDBGetRecordData getRecordData = { range };
    auto operation = IDBClient::createTransactionOperation(*this, request.get(), &IDBTransaction::didGetRecordOnServer, &IDBTransaction::getRecordOnServer, getRecordData);
    scheduleOperation(WTFMove(operation));

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
        request.requestCompleted(resultData);
        return;
    }

    ASSERT(resultData.type() == IDBResultType::GetRecordSuccess);

    const IDBGetResult& result = resultData.getResult();

    if (request.sourceIndexIdentifier() && request.requestedIndexRecordType() == IndexedDB::IndexRecordType::Key) {
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

    request.requestCompleted(resultData);
}

Ref<IDBRequest> IDBTransaction::requestCount(ExecState& execState, IDBObjectStore& objectStore, const IDBKeyRangeData& range)
{
    LOG(IndexedDB, "IDBTransaction::requestCount (IDBObjectStore)");
    ASSERT(isActive());
    ASSERT(!range.isNull);
    ASSERT(currentThread() == m_database->originThreadID());

    RELEASE_ASSERT(scriptExecutionContext());
    ASSERT_UNUSED(execState, scriptExecutionContext() == scriptExecutionContextFromExecState(&execState));

    Ref<IDBRequest> request = IDBRequest::create(*scriptExecutionContext(), objectStore, *this);
    addRequest(request.get());

    scheduleOperation(IDBClient::createTransactionOperation(*this, request.get(), &IDBTransaction::didGetCountOnServer, &IDBTransaction::getCountOnServer, range));

    return request;
}

Ref<IDBRequest> IDBTransaction::requestCount(ExecState& execState, IDBIndex& index, const IDBKeyRangeData& range)
{
    LOG(IndexedDB, "IDBTransaction::requestCount (IDBIndex)");
    ASSERT(isActive());
    ASSERT(!range.isNull);
    ASSERT(currentThread() == m_database->originThreadID());

    RELEASE_ASSERT(scriptExecutionContext());
    ASSERT_UNUSED(execState, scriptExecutionContext() == scriptExecutionContextFromExecState(&execState));

    Ref<IDBRequest> request = IDBRequest::createCount(*scriptExecutionContext(), index, *this);
    addRequest(request.get());

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
    request.requestCompleted(resultData);
}

Ref<IDBRequest> IDBTransaction::requestDeleteRecord(ExecState& execState, IDBObjectStore& objectStore, const IDBKeyRangeData& range)
{
    LOG(IndexedDB, "IDBTransaction::requestDeleteRecord");
    ASSERT(isActive());
    ASSERT(!range.isNull);
    ASSERT(currentThread() == m_database->originThreadID());

    RELEASE_ASSERT(scriptExecutionContext());
    ASSERT_UNUSED(execState, scriptExecutionContext() == scriptExecutionContextFromExecState(&execState));

    Ref<IDBRequest> request = IDBRequest::create(*scriptExecutionContext(), objectStore, *this);
    addRequest(request.get());

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
    request.requestCompleted(resultData);
}

Ref<IDBRequest> IDBTransaction::requestClearObjectStore(ExecState& execState, IDBObjectStore& objectStore)
{
    LOG(IndexedDB, "IDBTransaction::requestClearObjectStore");
    ASSERT(isActive());
    ASSERT(currentThread() == m_database->originThreadID());

    RELEASE_ASSERT(scriptExecutionContext());
    ASSERT_UNUSED(execState, scriptExecutionContext() == scriptExecutionContextFromExecState(&execState));

    Ref<IDBRequest> request = IDBRequest::create(*scriptExecutionContext(), objectStore, *this);
    addRequest(request.get());

    uint64_t objectStoreIdentifier = objectStore.info().identifier();
    auto operation = IDBClient::createTransactionOperation(*this, request.get(), &IDBTransaction::didClearObjectStoreOnServer, &IDBTransaction::clearObjectStoreOnServer, objectStoreIdentifier);
    scheduleOperation(WTFMove(operation));

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
    request.requestCompleted(resultData);
}

Ref<IDBRequest> IDBTransaction::requestPutOrAdd(ExecState& execState, IDBObjectStore& objectStore, IDBKey* key, SerializedScriptValue& value, IndexedDB::ObjectStoreOverwriteMode overwriteMode)
{
    LOG(IndexedDB, "IDBTransaction::requestPutOrAdd");
    ASSERT(isActive());
    ASSERT(!isReadOnly());
    ASSERT(objectStore.info().autoIncrement() || key);
    ASSERT(currentThread() == m_database->originThreadID());

    RELEASE_ASSERT(scriptExecutionContext());
    ASSERT_UNUSED(execState, scriptExecutionContext() == scriptExecutionContextFromExecState(&execState));

    Ref<IDBRequest> request = IDBRequest::create(*scriptExecutionContext(), objectStore, *this);
    addRequest(request.get());

    auto operation = IDBClient::createTransactionOperation(*this, request.get(), &IDBTransaction::didPutOrAddOnServer, &IDBTransaction::putOrAddOnServer, key, &value, overwriteMode);
    scheduleOperation(WTFMove(operation));

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
                protectedOperation->completed(result);
            });
        }
        return;
    }

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
            protectedOperation->completed(result);
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
    request.requestCompleted(resultData);
}

void IDBTransaction::deleteObjectStore(const String& objectStoreName)
{
    LOG(IndexedDB, "IDBTransaction::deleteObjectStore");
    ASSERT(currentThread() == m_database->originThreadID());
    ASSERT(isVersionChange());

    if (auto objectStore = m_referencedObjectStores.take(objectStoreName))
        objectStore->markAsDeleted();

    auto operation = IDBClient::createTransactionOperation(*this, &IDBTransaction::didDeleteObjectStoreOnServer, &IDBTransaction::deleteObjectStoreOnServer, objectStoreName);
    scheduleOperation(WTFMove(operation));
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

    auto operation = IDBClient::createTransactionOperation(*this, &IDBTransaction::didDeleteIndexOnServer, &IDBTransaction::deleteIndexOnServer, objectStoreIdentifier, indexName);
    scheduleOperation(WTFMove(operation));
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

void IDBTransaction::operationDidComplete(IDBClient::TransactionOperation& operation)
{
    ASSERT(m_transactionOperationMap.get(operation.identifier()) == &operation);
    ASSERT(currentThread() == m_database->originThreadID());
    ASSERT(currentThread() == operation.originThreadID());

    m_transactionOperationMap.remove(operation.identifier());

    scheduleOperationTimer();
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

    scheduleOperationTimer();
}

void IDBTransaction::connectionClosedFromServer(const IDBError& error)
{
    LOG(IndexedDB, "IDBTransaction::connectionClosedFromServer - %s", error.message().utf8().data());

    m_state = IndexedDB::TransactionState::Aborting;

    Vector<RefPtr<IDBClient::TransactionOperation>> operations;
    copyValuesToVector(m_transactionOperationMap, operations);

    for (auto& operation : operations)
        operation->completed(IDBResultData::error(operation->identifier(), error));

    connectionProxy().forgetActiveOperations(operations);

    m_transactionOperationQueue.clear();
    m_abortQueue.clear();
    m_transactionOperationMap.clear();

    m_idbError = error;
    m_domError = error.toDOMError();
    fireOnAbort();
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
