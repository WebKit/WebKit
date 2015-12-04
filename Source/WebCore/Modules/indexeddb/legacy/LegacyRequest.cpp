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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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
#include "LegacyRequest.h"

#if ENABLE(INDEXED_DATABASE)

#include "EventListener.h"
#include "EventNames.h"
#include "EventQueue.h"
#include "ExceptionCodePlaceholder.h"
#include "IDBBindingUtilities.h"
#include "IDBCursorBackend.h"
#include "IDBCursorWithValue.h"
#include "IDBDatabase.h"
#include "IDBEventDispatcher.h"
#include "IDBIndex.h"
#include "LegacyCursor.h"
#include "LegacyCursorWithValue.h"
#include "LegacyTransaction.h"
#include "Logging.h"
#include "ScriptExecutionContext.h"
#include "SharedBuffer.h"

namespace WebCore {

Ref<LegacyRequest> LegacyRequest::create(ScriptExecutionContext* context, PassRefPtr<LegacyAny> source, LegacyTransaction* transaction)
{
    Ref<LegacyRequest> request(adoptRef(*new LegacyRequest(context, source, IDBDatabaseBackend::NormalTask, transaction)));
    request->suspendIfNeeded();
    // Requests associated with IDBFactory (open/deleteDatabase/getDatabaseNames) are not associated with transactions.
    if (transaction)
        transaction->registerRequest(request.ptr());
    return request;
}

Ref<LegacyRequest> LegacyRequest::create(ScriptExecutionContext* context, PassRefPtr<LegacyAny> source, IDBDatabaseBackend::TaskType taskType, LegacyTransaction* transaction)
{
    Ref<LegacyRequest> request(adoptRef(*new LegacyRequest(context, source, taskType, transaction)));
    request->suspendIfNeeded();
    // Requests associated with IDBFactory (open/deleteDatabase/getDatabaseNames) are not associated with transactions.
    if (transaction)
        transaction->registerRequest(request.ptr());
    return request;
}

LegacyRequest::LegacyRequest(ScriptExecutionContext* context, PassRefPtr<LegacyAny> source, IDBDatabaseBackend::TaskType taskType, LegacyTransaction* transaction)
    : IDBOpenDBRequest(context)
    , m_result(nullptr)
    , m_errorCode(0)
    , m_contextStopped(false)
    , m_transaction(transaction)
    , m_readyState(IDBRequestReadyState::Pending)
    , m_requestAborted(false)
    , m_source(source)
    , m_taskType(taskType)
    , m_hasPendingActivity(true)
    , m_cursorType(IndexedDB::CursorType::KeyAndValue)
    , m_cursorDirection(IndexedDB::CursorDirection::Next)
    , m_cursorFinished(false)
    , m_pendingCursor(nullptr)
    , m_didFireUpgradeNeededEvent(false)
    , m_preventPropagation(false)
    , m_requestState(context)
{
}

LegacyRequest::~LegacyRequest()
{
}

RefPtr<IDBAny> LegacyRequest::result(ExceptionCodeWithMessage& ec) const
{
    if (m_readyState != IDBRequestReadyState::Done) {
        ec.code = IDBDatabaseException::InvalidStateError;
        return 0;
    }
    return m_result;
}

RefPtr<DOMError> LegacyRequest::error(ExceptionCodeWithMessage& ec) const
{
    if (m_readyState != IDBRequestReadyState::Done) {
        ec.code = IDBDatabaseException::InvalidStateError;
        return 0;
    }
    return m_error;
}

unsigned short LegacyRequest::errorCode(ExceptionCode& ec) const
{
    if (m_readyState != IDBRequestReadyState::Done) {
        ec = IDBDatabaseException::InvalidStateError;
        return 0;
    }
    return m_errorCode;
}

RefPtr<IDBAny> LegacyRequest::source() const
{
    return m_source;
}

RefPtr<IDBTransaction> LegacyRequest::transaction() const
{
    return m_transaction;
}

const String& LegacyRequest::readyState() const
{
    ASSERT(m_readyState == IDBRequestReadyState::Pending || m_readyState == IDBRequestReadyState::Done);
    DEPRECATED_DEFINE_STATIC_LOCAL(AtomicString, pending, ("pending", AtomicString::ConstructFromLiteral));
    DEPRECATED_DEFINE_STATIC_LOCAL(AtomicString, done, ("done", AtomicString::ConstructFromLiteral));

    if (m_readyState == IDBRequestReadyState::Pending)
        return pending;

    return done;
}

void LegacyRequest::markEarlyDeath()
{
    ASSERT(m_readyState == IDBRequestReadyState::Pending);
    m_readyState = IDBRequestReadyState::DeprecatedEarlyDeath;
    if (m_transaction)
        m_transaction->unregisterRequest(this);
}

void LegacyRequest::abort()
{
    ASSERT(!m_requestAborted);
    if (m_contextStopped || !scriptExecutionContext())
        return;
    ASSERT(m_readyState == IDBRequestReadyState::Pending || m_readyState == IDBRequestReadyState::Done);
    if (m_readyState == IDBRequestReadyState::Done)
        return;

    // Enqueued events may be the only reference to this object.
    RefPtr<LegacyRequest> self(this);

    EventQueue& eventQueue = scriptExecutionContext()->eventQueue();
    for (auto& event : m_enqueuedEvents) {
        bool removed = eventQueue.cancelEvent(event);
        ASSERT_UNUSED(removed, removed);
    }
    m_enqueuedEvents.clear();

    m_errorCode = 0;
    m_error = nullptr;
    m_errorMessage = String();
    m_result = nullptr;
    onError(IDBDatabaseError::create(IDBDatabaseException::AbortError));
    m_requestAborted = true;
}

void LegacyRequest::setCursorDetails(IndexedDB::CursorType cursorType, IndexedDB::CursorDirection direction)
{
    ASSERT(m_readyState == IDBRequestReadyState::Pending);
    ASSERT(!m_pendingCursor);
    m_cursorType = cursorType;
    m_cursorDirection = direction;
}

void LegacyRequest::setPendingCursor(PassRefPtr<LegacyCursor> cursor)
{
    ASSERT(m_readyState == IDBRequestReadyState::Done);
    ASSERT(scriptExecutionContext());
    ASSERT(m_transaction);
    ASSERT(!m_pendingCursor);
    ASSERT(cursor == getResultCursor());

    m_pendingCursor = cursor;
    m_result = nullptr;
    m_readyState = IDBRequestReadyState::Pending;
    m_errorCode = 0;
    m_error = nullptr;
    m_errorMessage = String();
    m_transaction->registerRequest(this);
}

RefPtr<LegacyCursor> LegacyRequest::getResultCursor()
{
    if (!m_result)
        return nullptr;
    if (m_result->type() == IDBAny::Type::IDBCursor)
        return m_result->legacyCursor();
    if (m_result->type() == IDBAny::Type::IDBCursorWithValue)
        return m_result->legacyCursorWithValue();
    return nullptr;
}

void LegacyRequest::setResultCursor(PassRefPtr<LegacyCursor> cursor, PassRefPtr<IDBKey> key, PassRefPtr<IDBKey> primaryKey, const Deprecated::ScriptValue& value)
{
    ASSERT(m_readyState == IDBRequestReadyState::Pending);
    m_cursorKey = key;
    m_cursorPrimaryKey = primaryKey;
    m_cursorValue = value;

    if (m_cursorType == IndexedDB::CursorType::KeyOnly) {
        m_result = LegacyAny::create(cursor);
        return;
    }

    m_result = LegacyAny::create(LegacyCursorWithValue::fromCursor(cursor));
}

void LegacyRequest::finishCursor()
{
    m_cursorFinished = true;
    if (m_readyState != IDBRequestReadyState::Pending)
        m_hasPendingActivity = false;
}

bool LegacyRequest::shouldEnqueueEvent() const
{
    if (m_contextStopped || !scriptExecutionContext())
        return false;
    ASSERT(m_readyState == IDBRequestReadyState::Pending || m_readyState == IDBRequestReadyState::Done);
    if (m_requestAborted)
        return false;
    ASSERT(m_readyState == IDBRequestReadyState::Pending);
    ASSERT(!m_errorCode && m_errorMessage.isNull() && !m_error && !m_result);
    return true;
}

void LegacyRequest::onError(PassRefPtr<IDBDatabaseError> error)
{
    LOG(StorageAPI, "LegacyRequest::onError() (%s) '%s'", error->name().utf8().data(), error->message().utf8().data());
    if (!shouldEnqueueEvent())
        return;

    m_errorCode = error->code();
    m_errorMessage = error->message();
    m_error = DOMError::create(IDBDatabaseException::getErrorName(error->idbCode()));
    m_pendingCursor = nullptr;
    enqueueEvent(Event::create(eventNames().errorEvent, true, true));
}

static Ref<Event> createSuccessEvent()
{
    return Event::create(eventNames().successEvent, false, false);
}

void LegacyRequest::onSuccess(PassRefPtr<DOMStringList> domStringList)
{
    LOG(StorageAPI, "LegacyRequest::onSuccess(DOMStringList)");
    if (!shouldEnqueueEvent())
        return;

    m_result = LegacyAny::create(domStringList);
    enqueueEvent(createSuccessEvent());
}

void LegacyRequest::onSuccess(PassRefPtr<IDBCursorBackend> prpBackend)
{
    LOG(StorageAPI, "LegacyRequest::onSuccess(IDBCursor)");
    if (!shouldEnqueueEvent())
        return;

    DOMRequestState::Scope scope(m_requestState);

    RefPtr<IDBCursorBackend> backend = prpBackend;
    RefPtr<IDBKey> key = backend->key();
    RefPtr<IDBKey> primaryKey = backend->primaryKey();

    Deprecated::ScriptValue value = deserializeIDBValueBuffer(requestState(), backend->valueBuffer(), !!key);

    ASSERT(!m_pendingCursor);
    RefPtr<LegacyCursor> cursor;
    switch (m_cursorType) {
    case IndexedDB::CursorType::KeyOnly:
        cursor = LegacyCursor::create(backend.get(), m_cursorDirection, this, m_source.get(), m_transaction.get());
        break;
    case IndexedDB::CursorType::KeyAndValue:
        cursor = LegacyCursorWithValue::create(backend.release(), m_cursorDirection, this, m_source.get(), m_transaction.get());
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    setResultCursor(cursor, key.release(), primaryKey.release(), value);

    enqueueEvent(createSuccessEvent());
}

void LegacyRequest::onSuccess(PassRefPtr<IDBKey> idbKey)
{
    LOG(StorageAPI, "LegacyRequest::onSuccess(IDBKey)");
    if (!shouldEnqueueEvent())
        return;

    if (idbKey && idbKey->isValid()) {
        DOMRequestState::Scope scope(m_requestState);
        m_result = LegacyAny::create(idbKeyToScriptValue(requestState(), idbKey));
    } else
        m_result = LegacyAny::createInvalid();
    enqueueEvent(createSuccessEvent());
}

void LegacyRequest::onSuccess(PassRefPtr<SharedBuffer> valueBuffer)
{
    LOG(StorageAPI, "LegacyRequest::onSuccess(SharedBuffer)");
    if (!shouldEnqueueEvent())
        return;

    DOMRequestState::Scope scope(m_requestState);

    // FIXME: By not knowing whether or not the key is defined here, we don't know
    // if a null valueBuffer means the value is null or the value is undefined.
    Deprecated::ScriptValue value = deserializeIDBValueBuffer(requestState(), valueBuffer, true);
    onSuccessInternal(value);
}

#ifndef NDEBUG
static PassRefPtr<IDBObjectStore> effectiveObjectStore(LegacyAny* source)
{
    if (source->type() == IDBAny::Type::IDBObjectStore)
        return source->idbObjectStore();
    if (source->type() == IDBAny::Type::IDBIndex)
        return source->idbIndex()->objectStore();

    ASSERT_NOT_REACHED();
    return 0;
}
#endif

void LegacyRequest::onSuccess(PassRefPtr<SharedBuffer> valueBuffer, PassRefPtr<IDBKey> prpPrimaryKey, const IDBKeyPath& keyPath)
{
    LOG(StorageAPI, "LegacyRequest::onSuccess(SharedBuffer, IDBKey, IDBKeyPath)");
    if (!shouldEnqueueEvent())
        return;

#ifndef NDEBUG
    ASSERT(keyPath == effectiveObjectStore(m_source.get())->keyPath());
#endif
    DOMRequestState::Scope scope(m_requestState);

    // FIXME: By not knowing whether or not the key is defined here, we don't know
    // if a null valueBuffer means the value is null or the value is undefined.
    Deprecated::ScriptValue value = deserializeIDBValueBuffer(requestState(), valueBuffer, true);

    RefPtr<IDBKey> primaryKey = prpPrimaryKey;

    if (!keyPath.isNull()) {
#ifndef NDEBUG
        RefPtr<IDBKey> expectedKey = createIDBKeyFromScriptValueAndKeyPath(requestState()->exec(), value, keyPath);
        ASSERT(!expectedKey || expectedKey->isEqual(primaryKey.get()));
#endif
        bool injected = injectIDBKeyIntoScriptValue(requestState(), primaryKey, value, keyPath);
        ASSERT_UNUSED(injected, injected);
    }

    onSuccessInternal(value);
}

void LegacyRequest::onSuccess(int64_t value)
{
    LOG(StorageAPI, "LegacyRequest::onSuccess(int64_t)");
    if (!shouldEnqueueEvent())
        return;
    return onSuccessInternal(SerializedScriptValue::numberValue(value));
}

void LegacyRequest::onSuccess()
{
    LOG(StorageAPI, "LegacyRequest::onSuccess()");
    if (!shouldEnqueueEvent())
        return;
    return onSuccessInternal(SerializedScriptValue::undefinedValue());
}

void LegacyRequest::onSuccessInternal(PassRefPtr<SerializedScriptValue> value)
{
    ASSERT(!m_contextStopped);
    DOMRequestState::Scope scope(m_requestState);
    return onSuccessInternal(deserializeIDBValue(requestState(), value));
}

void LegacyRequest::onSuccessInternal(const Deprecated::ScriptValue& value)
{
    m_result = LegacyAny::create(value);
    if (m_pendingCursor) {
        m_pendingCursor->close();
        m_pendingCursor = nullptr;
    }
    enqueueEvent(createSuccessEvent());
}

void LegacyRequest::onSuccess(PassRefPtr<IDBKey> key, PassRefPtr<IDBKey> primaryKey, PassRefPtr<SharedBuffer> buffer)
{
    LOG(StorageAPI, "LegacyRequest::onSuccess(key, primaryKey, valueBuffer)");
    if (!shouldEnqueueEvent())
        return;

    DOMRequestState::Scope scope(m_requestState);

    Deprecated::ScriptValue value = deserializeIDBValueBuffer(requestState(), buffer, !!key);

    ASSERT(m_pendingCursor);
    setResultCursor(m_pendingCursor.release(), key, primaryKey, value);
    enqueueEvent(createSuccessEvent());
}

void LegacyRequest::onSuccess(PassRefPtr<IDBDatabaseBackend>, const IDBDatabaseMetadata&)
{
    // Only the LegacyOpenDBRequest version of this should ever be called;
    ASSERT_NOT_REACHED();
}

bool LegacyRequest::hasPendingActivity() const
{
    // FIXME: In an ideal world, we should return true as long as anyone has a or can
    //        get a handle to us and we have event listeners. This is order to handle
    //        user generated events properly.
    return m_hasPendingActivity && !m_contextStopped;
}

void LegacyRequest::stop()
{
    if (m_contextStopped)
        return;

    m_contextStopped = true;
    m_requestState.clear();
    if (m_readyState == IDBRequestReadyState::Pending)
        markEarlyDeath();
}

bool LegacyRequest::canSuspendForDocumentSuspension() const
{
    return !m_hasPendingActivity;
}

const char* LegacyRequest::activeDOMObjectName() const
{
    return "LegacyRequest";
}

EventTargetInterface LegacyRequest::eventTargetInterface() const
{
    return IDBRequestEventTargetInterfaceType;
}

bool LegacyRequest::dispatchEvent(Event& event)
{
    LOG(StorageAPI, "LegacyRequest::dispatchEvent");
    ASSERT(m_readyState == IDBRequestReadyState::Pending);
    ASSERT(!m_contextStopped);
    ASSERT(m_hasPendingActivity);
    ASSERT(m_enqueuedEvents.size());
    ASSERT(scriptExecutionContext());
    ASSERT(event.target() == this);
    ASSERT_WITH_MESSAGE(m_readyState < IDBRequestReadyState::Done, "When dispatching event %s, m_readyState < DONE(%d), was %d", event.type().string().utf8().data(), static_cast<int>(IDBRequestReadyState::Done), static_cast<int>(m_readyState));

    DOMRequestState::Scope scope(m_requestState);

    if (event.type() != eventNames().blockedEvent)
        m_readyState = IDBRequestReadyState::Done;

    for (size_t i = 0; i < m_enqueuedEvents.size(); ++i) {
        if (m_enqueuedEvents[i].ptr() == &event)
            m_enqueuedEvents.remove(i);
    }

    Vector<RefPtr<EventTarget>> targets;
    targets.append(this);
    if (m_transaction && !m_preventPropagation) {
        targets.append(m_transaction);
        // If there ever are events that are associated with a database but
        // that do not have a transaction, then this will not work and we need
        // this object to actually hold a reference to the database (to ensure
        // it stays alive).
        targets.append(m_transaction->db());
    }

    // Cursor properties should not updated until the success event is being dispatched.
    RefPtr<LegacyCursor> cursorToNotify;
    if (event.type() == eventNames().successEvent) {
        cursorToNotify = getResultCursor();
        if (cursorToNotify) {
            cursorToNotify->setValueReady(requestState(), m_cursorKey.release(), m_cursorPrimaryKey.release(), m_cursorValue);
            m_cursorValue.clear();
        }
    }

    if (event.type() == eventNames().upgradeneededEvent) {
        ASSERT(!m_didFireUpgradeNeededEvent);
        m_didFireUpgradeNeededEvent = true;
    }

    // FIXME: When we allow custom event dispatching, this will probably need to change.
    ASSERT_WITH_MESSAGE(event.type() == eventNames().successEvent || event.type() == eventNames().errorEvent || event.type() == eventNames().blockedEvent || event.type() == eventNames().upgradeneededEvent, "event type was %s", event.type().string().utf8().data());
    const bool setTransactionActive = m_transaction && (event.type() == eventNames().successEvent || event.type() == eventNames().upgradeneededEvent || (event.type() == eventNames().errorEvent && m_errorCode != IDBDatabaseException::AbortError));

    if (setTransactionActive)
        m_transaction->setActive(true);

    bool dontPreventDefault = IDBEventDispatcher::dispatch(event, targets);

    if (m_transaction) {
        if (m_readyState == IDBRequestReadyState::Done)
            m_transaction->unregisterRequest(this);

        // Possibly abort the transaction. This must occur after unregistering (so this request
        // doesn't receive a second error) and before deactivating (which might trigger commit).
        if (event.type() == eventNames().errorEvent && dontPreventDefault && !m_requestAborted) {
            m_transaction->setError(m_error, m_errorMessage);
            ExceptionCodeWithMessage ec;
            m_transaction->abort(ec);
        }

        // If this was the last request in the transaction's list, it may commit here.
        if (setTransactionActive)
            m_transaction->setActive(false);
    }

    if (cursorToNotify)
        cursorToNotify->postSuccessHandlerCallback();

    if (m_readyState == IDBRequestReadyState::Done && (!cursorToNotify || m_cursorFinished) && event.type() != eventNames().upgradeneededEvent)
        m_hasPendingActivity = false;

    return dontPreventDefault;
}

void LegacyRequest::uncaughtExceptionInEventHandler()
{
    if (m_transaction && !m_requestAborted) {
        m_transaction->setError(DOMError::create(IDBDatabaseException::getErrorName(IDBDatabaseException::AbortError)), "Uncaught exception in event handler.");
        ExceptionCodeWithMessage ec;
        m_transaction->abort(ec);
    }
}

void LegacyRequest::transactionDidFinishAndDispatch()
{
    ASSERT(m_transaction);
    ASSERT(m_transaction->isVersionChange());
    ASSERT(m_readyState == IDBRequestReadyState::Done);
    ASSERT(scriptExecutionContext());
    m_transaction = nullptr;
    m_readyState = IDBRequestReadyState::Pending;
}

void LegacyRequest::enqueueEvent(Ref<Event>&& event)
{
    ASSERT(m_readyState == IDBRequestReadyState::Pending || m_readyState == IDBRequestReadyState::Done);

    if (m_contextStopped || !scriptExecutionContext())
        return;

    ASSERT_WITH_MESSAGE(m_readyState == IDBRequestReadyState::Pending || m_didFireUpgradeNeededEvent, "When queueing event %s, m_readyState was %d", event->type().string().utf8().data(), static_cast<int>(m_readyState));

    event->setTarget(this);

    if (scriptExecutionContext()->eventQueue().enqueueEvent(event.copyRef()))
        m_enqueuedEvents.append(WTF::move(event));
}

} // namespace WebCore

#endif
