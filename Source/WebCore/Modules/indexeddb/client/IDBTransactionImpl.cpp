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
#include "IDBTransactionImpl.h"

#if ENABLE(INDEXED_DATABASE)

#include "DOMError.h"
#include "EventQueue.h"
#include "IDBDatabaseImpl.h"
#include "IDBEventDispatcher.h"
#include "IDBObjectStore.h"
#include "Logging.h"
#include "ScriptExecutionContext.h"

namespace WebCore {
namespace IDBClient {

Ref<IDBTransaction> IDBTransaction::create(IDBDatabase& database, const IDBTransactionInfo& info)
{
    return adoptRef(*new IDBTransaction(database, info));
}

IDBTransaction::IDBTransaction(IDBDatabase& database, const IDBTransactionInfo& info)
    : WebCore::IDBTransaction(database.scriptExecutionContext())
    , m_database(database)
    , m_info(info)
    , m_operationTimer(*this, &IDBTransaction::operationTimerFired)

{
    suspendIfNeeded();
    scheduleOperationTimer();
    m_state = IndexedDB::TransactionState::Running;
}

IDBTransaction::~IDBTransaction()
{
}

const String& IDBTransaction::mode() const
{
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
    return &m_database.get();
}

RefPtr<DOMError> IDBTransaction::error() const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

RefPtr<IDBObjectStore> IDBTransaction::objectStore(const String&, ExceptionCode&)
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

void IDBTransaction::abort(ExceptionCode&)
{
    ASSERT_NOT_REACHED();
}

const char* IDBTransaction::activeDOMObjectName() const
{
    return "IDBTransaction";
}

bool IDBTransaction::canSuspendForPageCache() const
{
    return false;
}

bool IDBTransaction::hasPendingActivity() const
{
    return m_state != IndexedDB::TransactionState::Finished;
}

bool IDBTransaction::isActive() const
{
    return m_state == IndexedDB::TransactionState::Running;
}

void IDBTransaction::scheduleOperationTimer()
{
    if (!m_operationTimer.isActive())
        m_operationTimer.startOneShot(0);
}

void IDBTransaction::operationTimerFired()
{
    LOG(IndexedDB, "IDBTransaction::operationTimerFired");

    if (m_state == IndexedDB::TransactionState::Unstarted)
        return;

    // FIXME: Once transactions can do things, like configure the database or insert data into it,
    // those operations will be handled here, and will prevent the transaction from committing
    // as long as outstanding operations exist.

    if (isActive())
        commit();
}

void IDBTransaction::commit()
{
    LOG(IndexedDB, "IDBTransaction::commit");

    if (m_state != IndexedDB::TransactionState::Running) {
        m_state = IndexedDB::TransactionState::Finished;
        return;
    }

    m_state = IndexedDB::TransactionState::Committing;

    m_database->commitTransaction(*this);
}

void IDBTransaction::didCommit(const IDBError& error)
{
    LOG(IndexedDB, "IDBTransaction::didCommit");

    ASSERT(m_state == IndexedDB::TransactionState::Committing);

    if (error.isNull()) {
        m_database->didCommitTransaction(*this);
        fireOnComplete();
    } else {
        m_database->didAbortTransaction(*this);
        m_idbError = error;
        fireOnAbort();
    }

    m_state = IndexedDB::TransactionState::Finished;
}

void IDBTransaction::fireOnComplete()
{
    LOG(IndexedDB, "IDBTransaction::fireOnComplete");
    enqueueEvent(Event::create(eventNames().completeEvent, false, false));
}

void IDBTransaction::fireOnAbort()
{
    LOG(IndexedDB, "IDBTransaction::fireOnAbort");
    enqueueEvent(Event::create(eventNames().abortEvent, true, false));
}

void IDBTransaction::enqueueEvent(Ref<Event> event)
{
    ASSERT(m_state != IndexedDB::TransactionState::Finished);

    if (!scriptExecutionContext())
        return;

    event->setTarget(this);
    scriptExecutionContext()->eventQueue().enqueueEvent(&event.get());
}

bool IDBTransaction::dispatchEvent(PassRefPtr<Event> event)
{
    LOG(IndexedDB, "IDBTransaction::dispatchEvent");

    ASSERT(scriptExecutionContext());
    ASSERT(event->target() == this);
    ASSERT(event->type() == eventNames().completeEvent || event->type() == eventNames().abortEvent);

    Vector<RefPtr<EventTarget>> targets;
    targets.append(this);
    targets.append(db());

    return IDBEventDispatcher::dispatch(event.get(), targets);
}

} // namespace IDBClient
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
