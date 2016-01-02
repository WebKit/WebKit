/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LegacyTransaction.h"

#if ENABLE(INDEXED_DATABASE)

#include "EventQueue.h"
#include "ExceptionCodePlaceholder.h"
#include "IDBDatabase.h"
#include "IDBDatabaseException.h"
#include "IDBEventDispatcher.h"
#include "IDBIndex.h"
#include "IDBObjectStore.h"
#include "IDBPendingTransactionMonitor.h"
#include "LegacyCursor.h"
#include "LegacyObjectStore.h"
#include "LegacyOpenDBRequest.h"
#include "Logging.h"
#include "ScriptExecutionContext.h"

namespace WebCore {

Ref<LegacyTransaction> LegacyTransaction::create(ScriptExecutionContext* context, int64_t id, const Vector<String>& objectStoreNames, IndexedDB::TransactionMode mode, LegacyDatabase* db)
{
    LegacyOpenDBRequest* openDBRequest = nullptr;
    Ref<LegacyTransaction> transaction(adoptRef(*new LegacyTransaction(context, id, objectStoreNames, mode, db, openDBRequest, IDBDatabaseMetadata())));
    transaction->suspendIfNeeded();
    return transaction;
}

Ref<LegacyTransaction> LegacyTransaction::create(ScriptExecutionContext* context, int64_t id, LegacyDatabase* db, LegacyOpenDBRequest* openDBRequest, const IDBDatabaseMetadata& previousMetadata)
{
    Ref<LegacyTransaction> transaction(adoptRef(*new LegacyTransaction(context, id, Vector<String>(), IndexedDB::TransactionMode::VersionChange, db, openDBRequest, previousMetadata)));
    transaction->suspendIfNeeded();
    return transaction;
}

LegacyTransaction::LegacyTransaction(ScriptExecutionContext* context, int64_t id, const Vector<String>& objectStoreNames, IndexedDB::TransactionMode mode, LegacyDatabase* db, LegacyOpenDBRequest* openDBRequest, const IDBDatabaseMetadata& previousMetadata)
    : IDBTransaction(context)
    , m_id(id)
    , m_database(db)
    , m_objectStoreNames(objectStoreNames)
    , m_openDBRequest(openDBRequest)
    , m_mode(mode)
    , m_state(Active)
    , m_hasPendingActivity(true)
    , m_contextStopped(false)
    , m_previousMetadata(previousMetadata)
{
    if (mode == IndexedDB::TransactionMode::VersionChange) {
        // Not active until the callback.
        m_state = Inactive;
    }

    // We pass a reference of this object before it can be adopted.
    relaxAdoptionRequirement();
    if (m_state == Active)
        IDBPendingTransactionMonitor::addNewTransaction(this);
    m_database->transactionCreated(this);
}

LegacyTransaction::~LegacyTransaction()
{
    ASSERT(m_state == Finished || m_contextStopped);
    ASSERT(m_requestList.isEmpty() || m_contextStopped);
}

const String& LegacyTransaction::mode() const
{
    return modeToString(m_mode);
}

IDBDatabase* LegacyTransaction::db()
{
    return m_database.get();
}

void LegacyTransaction::setError(PassRefPtr<DOMError> error, const String& errorMessage)
{
    ASSERT(m_state != Finished);
    ASSERT(error);

    // The first error to be set is the true cause of the
    // transaction abort.
    if (!m_error) {
        m_error = error;
        m_errorMessage = errorMessage;
    }
}

RefPtr<IDBObjectStore> LegacyTransaction::objectStore(const String& name, ExceptionCodeWithMessage& ec)
{
    if (m_state == Finished) {
        ec.code = IDBDatabaseException::InvalidStateError;
        return 0;
    }

    IDBObjectStoreMap::iterator it = m_objectStoreMap.find(name);
    if (it != m_objectStoreMap.end())
        return it->value;

    if (!isVersionChange() && !m_objectStoreNames.contains(name)) {
        ec.code = IDBDatabaseException::NotFoundError;
        return 0;
    }

    int64_t objectStoreId = m_database->findObjectStoreId(name);
    if (objectStoreId == IDBObjectStoreMetadata::InvalidId) {
        ASSERT(isVersionChange());
        ec.code = IDBDatabaseException::NotFoundError;
        return 0;
    }

    const IDBDatabaseMetadata& metadata = m_database->metadata();

    RefPtr<LegacyObjectStore> objectStore = LegacyObjectStore::create(metadata.objectStores.get(objectStoreId), this);
    objectStoreCreated(name, objectStore);
    return objectStore.release();
}

void LegacyTransaction::objectStoreCreated(const String& name, PassRefPtr<LegacyObjectStore> prpObjectStore)
{
    ASSERT(m_state != Finished);
    RefPtr<LegacyObjectStore> objectStore = prpObjectStore;
    m_objectStoreMap.set(name, objectStore);
    if (isVersionChange())
        m_objectStoreCleanupMap.set(objectStore, objectStore->metadata());
}

void LegacyTransaction::objectStoreDeleted(const String& name)
{
    ASSERT(m_state != Finished);
    ASSERT(isVersionChange());
    IDBObjectStoreMap::iterator it = m_objectStoreMap.find(name);
    if (it != m_objectStoreMap.end()) {
        RefPtr<LegacyObjectStore> objectStore = it->value;
        m_objectStoreMap.remove(name);
        objectStore->markDeleted();
        m_objectStoreCleanupMap.set(objectStore, objectStore->metadata());
        m_deletedObjectStores.add(objectStore);
    }
}

void LegacyTransaction::setActive(bool active)
{
    LOG(StorageAPI, "LegacyTransaction::setActive(%s) for transaction id %lli", active ? "true" : "false", static_cast<long long>(m_id));
    ASSERT_WITH_MESSAGE(m_state != Finished, "A finished transaction tried to setActive(%s)", active ? "true" : "false");
    if (m_state == Finishing)
        return;
    ASSERT(active != (m_state == Active));
    m_state = active ? Active : Inactive;

    if (!active && m_requestList.isEmpty())
        backendDB()->commit(m_id);
}

void LegacyTransaction::abort(ExceptionCodeWithMessage& ec)
{
    if (m_state == Finishing || m_state == Finished) {
        ec.code = IDBDatabaseException::InvalidStateError;
        return;
    }

    m_state = Finishing;

    while (!m_requestList.isEmpty()) {
        RefPtr<LegacyRequest> request = *m_requestList.begin();
        m_requestList.remove(request);
        request->abort();
    }

    RefPtr<LegacyTransaction> selfRef = this;
    backendDB()->abort(m_id);
}

LegacyTransaction::OpenCursorNotifier::OpenCursorNotifier(PassRefPtr<LegacyTransaction> transaction, LegacyCursor* cursor)
    : m_transaction(transaction)
    , m_cursor(cursor)
{
    m_transaction->registerOpenCursor(m_cursor);
}

LegacyTransaction::OpenCursorNotifier::~OpenCursorNotifier()
{
    if (m_cursor)
        m_transaction->unregisterOpenCursor(m_cursor);
}

void LegacyTransaction::OpenCursorNotifier::cursorFinished()
{
    if (m_cursor) {
        m_transaction->unregisterOpenCursor(m_cursor);
        m_cursor = nullptr;
        m_transaction = nullptr;
    }
}

void LegacyTransaction::registerOpenCursor(LegacyCursor* cursor)
{
    m_openCursors.add(cursor);
}

void LegacyTransaction::unregisterOpenCursor(LegacyCursor* cursor)
{
    m_openCursors.remove(cursor);
}

void LegacyTransaction::closeOpenCursors()
{
    HashSet<LegacyCursor*> cursors;
    cursors.swap(m_openCursors);
    for (auto& cursor : cursors)
        cursor->close();
}

void LegacyTransaction::registerRequest(LegacyRequest* request)
{
    ASSERT(request);
    ASSERT(m_state == Active);
    m_requestList.add(request);
}

void LegacyTransaction::unregisterRequest(LegacyRequest* request)
{
    ASSERT(request);
    // If we aborted the request, it will already have been removed.
    m_requestList.remove(request);
}

void LegacyTransaction::onAbort(PassRefPtr<IDBDatabaseError> prpError)
{
    LOG(StorageAPI, "LegacyTransaction::onAbort");
    RefPtr<IDBDatabaseError> error = prpError;
    ASSERT(m_state != Finished);

    if (m_state != Finishing) {
        ASSERT(error.get());
        setError(DOMError::create(error->name()), error->message());

        // Abort was not triggered by front-end, so outstanding requests must
        // be aborted now.
        while (!m_requestList.isEmpty()) {
            RefPtr<LegacyRequest> request = *m_requestList.begin();
            m_requestList.remove(request);
            request->abort();
        }
        m_state = Finishing;
    }

    if (isVersionChange()) {
        for (auto& objectStore : m_objectStoreCleanupMap)
            objectStore.key->setMetadata(objectStore.value);
        m_database->setMetadata(m_previousMetadata);
        m_database->close();
    }
    m_objectStoreCleanupMap.clear();
    closeOpenCursors();

    // Enqueue events before notifying database, as database may close which enqueues more events and order matters.
    enqueueEvent(Event::create(eventNames().abortEvent, true, false));
    m_database->transactionFinished(this);
}

void LegacyTransaction::onComplete()
{
    LOG(StorageAPI, "LegacyTransaction::onComplete");
    ASSERT(m_state != Finished);
    m_state = Finishing;
    m_objectStoreCleanupMap.clear();
    closeOpenCursors();

    // Enqueue events before notifying database, as database may close which enqueues more events and order matters.
    enqueueEvent(Event::create(eventNames().completeEvent, false, false));
    m_database->transactionFinished(this);
}

bool LegacyTransaction::hasPendingActivity() const
{
    // FIXME: In an ideal world, we should return true as long as anyone has a or can
    //        get a handle to us or any child request object and any of those have
    //        event listeners. This is  in order to handle user generated events properly.
    return m_hasPendingActivity && !m_contextStopped;
}

bool LegacyTransaction::dispatchEvent(Event& event)
{
    LOG(StorageAPI, "LegacyTransaction::dispatchEvent");
    ASSERT(m_state != Finished);
    ASSERT(m_hasPendingActivity);
    ASSERT(scriptExecutionContext());
    ASSERT(event.target() == this);
    m_state = Finished;

    // Break reference cycles.
    for (auto& objectStore : m_objectStoreMap)
        objectStore.value->transactionFinished();
    m_objectStoreMap.clear();
    for (auto& objectStore : m_deletedObjectStores)
        objectStore->transactionFinished();
    m_deletedObjectStores.clear();

    Vector<RefPtr<EventTarget>> targets;
    targets.append(this);
    targets.append(db());

    // FIXME: When we allow custom event dispatching, this will probably need to change.
    ASSERT(event.type() == eventNames().completeEvent || event.type() == eventNames().abortEvent);
    bool returnValue = IDBEventDispatcher::dispatch(event, targets);
    // FIXME: Try to construct a test where |this| outlives openDBRequest and we
    // get a crash.
    if (m_openDBRequest) {
        ASSERT(isVersionChange());
        m_openDBRequest->transactionDidFinishAndDispatch();
    }
    m_hasPendingActivity = false;
    return returnValue;
}

bool LegacyTransaction::canSuspendForDocumentSuspension() const
{
    // FIXME: Technically we can suspend before the first request is schedule
    //        and after the complete/abort event is enqueued.
    return m_state == Finished;
}

void LegacyTransaction::stop()
{
    m_contextStopped = true;
    ExceptionCodeWithMessage ec;
    abort(ec);
}

const char* LegacyTransaction::activeDOMObjectName() const
{
    return "LegacyTransaction";
}

void LegacyTransaction::enqueueEvent(Ref<Event>&& event)
{
    ASSERT_WITH_MESSAGE(m_state != Finished, "A finished transaction tried to enqueue an event of type %s.", event->type().string().utf8().data());
    if (m_contextStopped || !scriptExecutionContext())
        return;

    event->setTarget(this);
    scriptExecutionContext()->eventQueue().enqueueEvent(WTFMove(event));
}

IDBDatabaseBackend* LegacyTransaction::backendDB() const
{
    return m_database->backend();
}

}

#endif // ENABLE(INDEXED_DATABASE)
