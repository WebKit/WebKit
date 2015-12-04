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
#include "LegacyDatabase.h"

#if ENABLE(INDEXED_DATABASE)

#include "DOMStringList.h"
#include "EventQueue.h"
#include "ExceptionCode.h"
#include "IDBAny.h"
#include "IDBDatabaseBackend.h"
#include "IDBDatabaseCallbacks.h"
#include "IDBDatabaseError.h"
#include "IDBDatabaseException.h"
#include "IDBEventDispatcher.h"
#include "IDBIndex.h"
#include "IDBKeyPath.h"
#include "IDBObjectStore.h"
#include "IDBTransaction.h"
#include "IDBVersionChangeEvent.h"
#include "LegacyObjectStore.h"
#include "LegacyVersionChangeEvent.h"
#include "Logging.h"
#include "ScriptExecutionContext.h"
#include <atomic>
#include <inspector/ScriptCallStack.h>
#include <limits>
#include <wtf/Atomics.h>

namespace WebCore {

Ref<LegacyDatabase> LegacyDatabase::create(ScriptExecutionContext* context, PassRefPtr<IDBDatabaseBackend> database, PassRefPtr<IDBDatabaseCallbacks> callbacks)
{
    Ref<LegacyDatabase> legacyDatabase(adoptRef(*new LegacyDatabase(context, database, callbacks)));
    legacyDatabase->suspendIfNeeded();
    return legacyDatabase;
}

LegacyDatabase::LegacyDatabase(ScriptExecutionContext* context, PassRefPtr<IDBDatabaseBackend> backend, PassRefPtr<IDBDatabaseCallbacks> callbacks)
    : IDBDatabase(context)
    , m_backend(backend)
    , m_closePending(false)
    , m_isClosed(false)
    , m_contextStopped(false)
    , m_databaseCallbacks(callbacks)
{
    // We pass a reference of this object before it can be adopted.
    relaxAdoptionRequirement();
}

LegacyDatabase::~LegacyDatabase()
{
    // This does what LegacyDatabase::close does, but without any ref/deref of the
    // database since it is already in the process of being deleted. The logic here
    // is also simpler since we know there are no transactions (since they ref the
    // database when they are alive).

    ASSERT(m_transactions.isEmpty());

    if (!m_closePending) {
        m_closePending = true;
        m_backend->close(m_databaseCallbacks);
    }

    if (auto* context = scriptExecutionContext()) {
        // Remove any pending versionchange events scheduled to fire on this
        // connection. They would have been scheduled by the backend when another
        // connection called setVersion, but the frontend connection is being
        // closed before they could fire.
        for (auto& event : m_enqueuedEvents)
            context->eventQueue().cancelEvent(event);
    }
}

int64_t LegacyDatabase::nextTransactionId()
{
    // Only keep a 32-bit counter to allow ports to use the other 32
    // bits of the id.
    static std::atomic<uint32_t> currentTransactionId;

    return ++currentTransactionId;
}

void LegacyDatabase::transactionCreated(LegacyTransaction* transaction)
{
    ASSERT(transaction);
    ASSERT(!m_transactions.contains(transaction->id()));
    m_transactions.add(transaction->id(), transaction);

    if (transaction->isVersionChange()) {
        ASSERT(!m_versionChangeTransaction);
        m_versionChangeTransaction = transaction;
    }
}

void LegacyDatabase::transactionFinished(LegacyTransaction* transaction)
{
    ASSERT(transaction);
    ASSERT(m_transactions.contains(transaction->id()));
    ASSERT(m_transactions.get(transaction->id()) == transaction);
    m_transactions.remove(transaction->id());

    if (transaction->isVersionChange()) {
        ASSERT(m_versionChangeTransaction == transaction);
        m_versionChangeTransaction = nullptr;
    }

    if (m_closePending && m_transactions.isEmpty())
        closeConnection();
}

void LegacyDatabase::onAbort(int64_t transactionId, PassRefPtr<IDBDatabaseError> error)
{
    ASSERT(m_transactions.contains(transactionId));
    m_transactions.get(transactionId)->onAbort(error);
}

void LegacyDatabase::onComplete(int64_t transactionId)
{
    ASSERT(m_transactions.contains(transactionId));
    m_transactions.get(transactionId)->onComplete();
}

RefPtr<DOMStringList> LegacyDatabase::objectStoreNames() const
{
    RefPtr<DOMStringList> objectStoreNames = DOMStringList::create();
    for (auto& objectStore : m_metadata.objectStores.values())
        objectStoreNames->append(objectStore.name);
    objectStoreNames->sort();
    return objectStoreNames.release();
}

uint64_t LegacyDatabase::version() const
{
    // NoIntVersion is a special value for internal use only and shouldn't be exposed to script.
    // DefaultIntVersion should be exposed instead.
    return m_metadata.version != IDBDatabaseMetadata::NoIntVersion ? m_metadata.version : static_cast<uint64_t>(IDBDatabaseMetadata::DefaultIntVersion);
}

RefPtr<IDBObjectStore> LegacyDatabase::createObjectStore(const String& name, const Dictionary& options, ExceptionCodeWithMessage& ec)
{
    IDBKeyPath keyPath;
    bool autoIncrement = false;
    if (!options.isUndefinedOrNull()) {
        String keyPathString;
        Vector<String> keyPathArray;
        if (options.get("keyPath", keyPathArray))
            keyPath = IDBKeyPath(keyPathArray);
        else if (options.getWithUndefinedOrNullCheck("keyPath", keyPathString))
            keyPath = IDBKeyPath(keyPathString);

        options.get("autoIncrement", autoIncrement);
    }

    return createObjectStore(name, keyPath, autoIncrement, ec);
}

RefPtr<IDBObjectStore> LegacyDatabase::createObjectStore(const String& name, const IDBKeyPath& keyPath, bool autoIncrement, ExceptionCodeWithMessage& ec)
{
    LOG(StorageAPI, "LegacyDatabase::createObjectStore");
    if (!m_versionChangeTransaction) {
        ec.code = IDBDatabaseException::InvalidStateError;
        return 0;
    }
    if (!m_versionChangeTransaction->isActive()) {
        ec.code = IDBDatabaseException::TransactionInactiveError;
        return 0;
    }

    if (containsObjectStore(name)) {
        ec.code = IDBDatabaseException::ConstraintError;
        return 0;
    }

    if (!keyPath.isNull() && !keyPath.isValid()) {
        ec.code = IDBDatabaseException::SyntaxError;
        return 0;
    }

    if (autoIncrement && ((keyPath.type() == IndexedDB::KeyPathType::String && keyPath.string().isEmpty()) || keyPath.type() == IndexedDB::KeyPathType::Array)) {
        ec.code = IDBDatabaseException::InvalidAccessError;
        return 0;
    }

    int64_t objectStoreId = m_metadata.maxObjectStoreId + 1;
    m_backend->createObjectStore(m_versionChangeTransaction->id(), objectStoreId, name, keyPath, autoIncrement);

    IDBObjectStoreMetadata metadata(name, objectStoreId, keyPath, autoIncrement, IDBDatabaseBackend::MinimumIndexId);
    RefPtr<LegacyObjectStore> objectStore = LegacyObjectStore::create(metadata, m_versionChangeTransaction.get());
    m_metadata.objectStores.set(metadata.id, metadata);
    ++m_metadata.maxObjectStoreId;

    m_versionChangeTransaction->objectStoreCreated(name, objectStore);
    return objectStore.release();
}

void LegacyDatabase::deleteObjectStore(const String& name, ExceptionCodeWithMessage& ec)
{
    LOG(StorageAPI, "LegacyDatabase::deleteObjectStore");
    if (!m_versionChangeTransaction) {
        ec.code = IDBDatabaseException::InvalidStateError;
        return;
    }
    if (!m_versionChangeTransaction->isActive()) {
        ec.code = IDBDatabaseException::TransactionInactiveError;
        return;
    }

    int64_t objectStoreId = findObjectStoreId(name);
    if (objectStoreId == IDBObjectStoreMetadata::InvalidId) {
        ec.code = IDBDatabaseException::NotFoundError;
        return;
    }

    m_backend->deleteObjectStore(m_versionChangeTransaction->id(), objectStoreId);
    m_versionChangeTransaction->objectStoreDeleted(name);
    m_metadata.objectStores.remove(objectStoreId);
}

RefPtr<IDBTransaction> LegacyDatabase::transaction(ScriptExecutionContext* context, const Vector<String>& scope, const String& modeString, ExceptionCodeWithMessage& ec)
{
    LOG(StorageAPI, "LegacyDatabase::transaction");
    if (!scope.size()) {
        ec.code = IDBDatabaseException::InvalidAccessError;
        return 0;
    }

    IndexedDB::TransactionMode mode = IDBTransaction::stringToMode(modeString, ec.code);
    if (ec.code)
        return 0;

    if (m_versionChangeTransaction || m_closePending) {
        ec.code = IDBDatabaseException::InvalidStateError;
        return 0;
    }

    Vector<int64_t> objectStoreIds;
    for (auto& name : scope) {
        int64_t objectStoreId = findObjectStoreId(name);
        if (objectStoreId == IDBObjectStoreMetadata::InvalidId) {
            ec.code = IDBDatabaseException::NotFoundError;
            return 0;
        }
        objectStoreIds.append(objectStoreId);
    }

    int64_t transactionId = nextTransactionId();
    m_backend->createTransaction(transactionId, m_databaseCallbacks, objectStoreIds, mode);

    RefPtr<LegacyTransaction> transaction = LegacyTransaction::create(context, transactionId, scope, mode, this);
    return transaction.release();
}

RefPtr<IDBTransaction> LegacyDatabase::transaction(ScriptExecutionContext* context, const String& storeName, const String& mode, ExceptionCodeWithMessage& ec)
{
    RefPtr<DOMStringList> storeNames = DOMStringList::create();
    storeNames->append(storeName);
    return transaction(context, storeNames, mode, ec);
}

void LegacyDatabase::forceClose()
{
    ExceptionCodeWithMessage ec;
    for (auto& transaction : m_transactions.values())
        transaction->abort(ec);
    this->close();
}

void LegacyDatabase::close()
{
    LOG(StorageAPI, "LegacyDatabase::close");
    if (m_closePending)
        return;

    m_closePending = true;

    if (m_transactions.isEmpty())
        closeConnection();
}

void LegacyDatabase::closeConnection()
{
    ASSERT(m_closePending);
    ASSERT(m_transactions.isEmpty());

    // Closing may result in deallocating the last transaction, which could result in deleting
    // this LegacyDatabase. We need the deallocation to happen after we are through.
    Ref<LegacyDatabase> protect(*this);

    m_backend->close(m_databaseCallbacks);

    if (m_contextStopped || !scriptExecutionContext())
        return;

    EventQueue& eventQueue = scriptExecutionContext()->eventQueue();
    // Remove any pending versionchange events scheduled to fire on this
    // connection. They would have been scheduled by the backend when another
    // connection called setVersion, but the frontend connection is being
    // closed before they could fire.
    for (auto& event : m_enqueuedEvents) {
        bool removed = eventQueue.cancelEvent(event);
        ASSERT_UNUSED(removed, removed);
    }

    m_isClosed = true;
}

void LegacyDatabase::onVersionChange(uint64_t oldVersion, uint64_t newVersion)
{
    LOG(StorageAPI, "LegacyDatabase::onVersionChange");
    if (m_contextStopped || !scriptExecutionContext())
        return;

    if (m_closePending)
        return;

    ASSERT(newVersion != IDBDatabaseMetadata::NoIntVersion);
    enqueueEvent(LegacyVersionChangeEvent::create(oldVersion, newVersion, eventNames().versionchangeEvent));
}

void LegacyDatabase::enqueueEvent(Ref<Event>&& event)
{
    ASSERT(!m_contextStopped);
    ASSERT(!m_isClosed);
    ASSERT(scriptExecutionContext());
    event->setTarget(this);
    scriptExecutionContext()->eventQueue().enqueueEvent(event.copyRef());
    m_enqueuedEvents.append(WTF::move(event));
}

bool LegacyDatabase::dispatchEvent(Event& event)
{
    LOG(StorageAPI, "LegacyDatabase::dispatchEvent");
    ASSERT(event.type() == eventNames().versionchangeEvent);
    for (size_t i = 0; i < m_enqueuedEvents.size(); ++i) {
        if (m_enqueuedEvents[i].ptr() == &event)
            m_enqueuedEvents.remove(i);
    }
    return EventTarget::dispatchEvent(event);
}

int64_t LegacyDatabase::findObjectStoreId(const String& name) const
{
    for (auto& objectStore : m_metadata.objectStores) {
        if (objectStore.value.name == name) {
            ASSERT(objectStore.key != IDBObjectStoreMetadata::InvalidId);
            return objectStore.key;
        }
    }
    return IDBObjectStoreMetadata::InvalidId;
}

bool LegacyDatabase::hasPendingActivity() const
{
    // The script wrapper must not be collected before the object is closed or
    // we can't fire a "versionchange" event to let script manually close the connection.
    return !m_closePending && hasEventListeners() && !m_contextStopped;
}

void LegacyDatabase::stop()
{
    // Stop fires at a deterministic time, so we need to call close in it.
    close();

    m_contextStopped = true;
}

const char* LegacyDatabase::activeDOMObjectName() const
{
    return "LegacyDatabase";
}

bool LegacyDatabase::canSuspendForDocumentSuspension() const
{
    return m_isClosed;
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
