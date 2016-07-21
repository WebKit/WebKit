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
#include "IDBRequest.h"

#if ENABLE(INDEXED_DATABASE)

#include "DOMError.h"
#include "Event.h"
#include "EventNames.h"
#include "EventQueue.h"
#include "IDBBindingUtilities.h"
#include "IDBConnectionProxy.h"
#include "IDBCursor.h"
#include "IDBDatabase.h"
#include "IDBDatabaseException.h"
#include "IDBEventDispatcher.h"
#include "IDBIndex.h"
#include "IDBKeyData.h"
#include "IDBObjectStore.h"
#include "IDBResultData.h"
#include "Logging.h"
#include "ScopeGuard.h"
#include "ScriptExecutionContext.h"
#include "ThreadSafeDataBuffer.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

Ref<IDBRequest> IDBRequest::create(ScriptExecutionContext& context, IDBObjectStore& objectStore, IDBTransaction& transaction)
{
    return adoptRef(*new IDBRequest(context, objectStore, transaction));
}

Ref<IDBRequest> IDBRequest::create(ScriptExecutionContext& context, IDBCursor& cursor, IDBTransaction& transaction)
{
    return adoptRef(*new IDBRequest(context, cursor, transaction));
}

Ref<IDBRequest> IDBRequest::createCount(ScriptExecutionContext& context, IDBIndex& index, IDBTransaction& transaction)
{
    return adoptRef(*new IDBRequest(context, index, transaction));
}

Ref<IDBRequest> IDBRequest::createGet(ScriptExecutionContext& context, IDBIndex& index, IndexedDB::IndexRecordType requestedRecordType, IDBTransaction& transaction)
{
    return adoptRef(*new IDBRequest(context, index, requestedRecordType, transaction));
}

IDBRequest::IDBRequest(ScriptExecutionContext& context, IDBClient::IDBConnectionProxy& connectionProxy)
    : IDBActiveDOMObject(&context)
    , m_resourceIdentifier(connectionProxy)
    , m_connectionProxy(connectionProxy)
{
    suspendIfNeeded();
}

IDBRequest::IDBRequest(ScriptExecutionContext& context, IDBObjectStore& objectStore, IDBTransaction& transaction)
    : IDBActiveDOMObject(&context)
    , m_transaction(&transaction)
    , m_resourceIdentifier(transaction.connectionProxy())
    , m_objectStoreSource(&objectStore)
    , m_connectionProxy(transaction.database().connectionProxy())
{
    suspendIfNeeded();
}

IDBRequest::IDBRequest(ScriptExecutionContext& context, IDBCursor& cursor, IDBTransaction& transaction)
    : IDBActiveDOMObject(&context)
    , m_transaction(&transaction)
    , m_resourceIdentifier(transaction.connectionProxy())
    , m_objectStoreSource(cursor.objectStore())
    , m_indexSource(cursor.index())
    , m_pendingCursor(&cursor)
    , m_connectionProxy(transaction.database().connectionProxy())
{
    suspendIfNeeded();

    cursor.setRequest(*this);
}

IDBRequest::IDBRequest(ScriptExecutionContext& context, IDBIndex& index, IDBTransaction& transaction)
    : IDBActiveDOMObject(&context)
    , m_transaction(&transaction)
    , m_resourceIdentifier(transaction.connectionProxy())
    , m_indexSource(&index)
    , m_connectionProxy(transaction.database().connectionProxy())
{
    suspendIfNeeded();
}

IDBRequest::IDBRequest(ScriptExecutionContext& context, IDBIndex& index, IndexedDB::IndexRecordType requestedRecordType, IDBTransaction& transaction)
    : IDBRequest(context, index, transaction)
{
    m_requestedIndexRecordType = requestedRecordType;
}

IDBRequest::~IDBRequest()
{
    ASSERT(currentThread() == originThreadID());

    if (m_cursorResult)
        m_cursorResult->clearRequest();
}

unsigned short IDBRequest::errorCode(ExceptionCode&) const
{
    ASSERT(currentThread() == originThreadID());

    return 0;
}

RefPtr<DOMError> IDBRequest::error(ExceptionCodeWithMessage& ec) const
{
    ASSERT(currentThread() == originThreadID());

    if (m_isDone)
        return m_domError;

    ec.code = IDBDatabaseException::InvalidStateError;
    ec.message = ASCIILiteral("Failed to read the 'error' property from 'IDBRequest': The request has not finished.");
    return nullptr;
}

void IDBRequest::setSource(IDBCursor& cursor)
{
    ASSERT(currentThread() == originThreadID());
    ASSERT(!m_cursorRequestNotifier);

    m_objectStoreSource = nullptr;
    m_indexSource = nullptr;
    m_cursorSource = &cursor;
    m_cursorRequestNotifier = std::make_unique<ScopeGuard>([this]() {
        ASSERT(m_cursorSource);
        m_cursorSource->decrementOutstandingRequestCount();
    });
}

void IDBRequest::setVersionChangeTransaction(IDBTransaction& transaction)
{
    ASSERT(currentThread() == originThreadID());
    ASSERT(!m_transaction);
    ASSERT(transaction.isVersionChange());
    ASSERT(!transaction.isFinishedOrFinishing());

    m_transaction = &transaction;
}

RefPtr<WebCore::IDBTransaction> IDBRequest::transaction() const
{
    ASSERT(currentThread() == originThreadID());
    return m_shouldExposeTransactionToDOM ? m_transaction : nullptr;
}

const String& IDBRequest::readyState() const
{
    ASSERT(currentThread() == originThreadID());

    static NeverDestroyed<String> pendingString(ASCIILiteral("pending"));
    static NeverDestroyed<String> doneString(ASCIILiteral("done"));
    return m_isDone ? doneString : pendingString;
}

uint64_t IDBRequest::sourceObjectStoreIdentifier() const
{
    ASSERT(currentThread() == originThreadID());

    if (m_objectStoreSource)
        return m_objectStoreSource->info().identifier();
    if (m_indexSource)
        return m_indexSource->info().objectStoreIdentifier();
    return 0;
}

uint64_t IDBRequest::sourceIndexIdentifier() const
{
    ASSERT(currentThread() == originThreadID());

    if (!m_indexSource)
        return 0;
    return m_indexSource->info().identifier();
}

IndexedDB::IndexRecordType IDBRequest::requestedIndexRecordType() const
{
    ASSERT(currentThread() == originThreadID());
    ASSERT(m_indexSource);

    return m_requestedIndexRecordType;
}

EventTargetInterface IDBRequest::eventTargetInterface() const
{
    ASSERT(currentThread() == originThreadID());

    return IDBRequestEventTargetInterfaceType;
}

const char* IDBRequest::activeDOMObjectName() const
{
    ASSERT(currentThread() == originThreadID());

    return "IDBRequest";
}

bool IDBRequest::canSuspendForDocumentSuspension() const
{
    ASSERT(currentThread() == originThreadID());
    return false;
}

bool IDBRequest::hasPendingActivity() const
{
    ASSERT(currentThread() == originThreadID());
    return m_hasPendingActivity;
}

void IDBRequest::stop()
{
    ASSERT(currentThread() == originThreadID());
    ASSERT(!m_contextStopped);

    cancelForStop();

    removeAllEventListeners();

    m_contextStopped = true;
}

void IDBRequest::cancelForStop()
{
    // The base IDBRequest class has nothing additional to do here.
}

void IDBRequest::enqueueEvent(Ref<Event>&& event)
{
    ASSERT(currentThread() == originThreadID());
    if (!scriptExecutionContext() || m_contextStopped)
        return;

    event->setTarget(this);
    scriptExecutionContext()->eventQueue().enqueueEvent(WTFMove(event));
}

bool IDBRequest::dispatchEvent(Event& event)
{
    LOG(IndexedDB, "IDBRequest::dispatchEvent - %s (%p)", event.type().string().utf8().data(), this);

    ASSERT(currentThread() == originThreadID());
    ASSERT(m_hasPendingActivity);
    ASSERT(!m_contextStopped);

    if (event.type() != eventNames().blockedEvent)
        m_isDone = true;

    Vector<RefPtr<EventTarget>> targets;
    targets.append(this);

    if (&event == m_openDatabaseSuccessEvent)
        m_openDatabaseSuccessEvent = nullptr;
    else if (m_transaction && !m_transaction->isFinished()) {
        targets.append(m_transaction);
        targets.append(m_transaction->db());
    }

    m_hasPendingActivity = false;

    m_cursorRequestNotifier = nullptr;

    bool dontPreventDefault;
    {
        TransactionActivator activator(m_transaction.get());
        dontPreventDefault = IDBEventDispatcher::dispatch(event, targets);
    }

    // IDBEventDispatcher::dispatch() might have set the pending activity flag back to true, suggesting the request will be reused.
    // We might also re-use the request if this event was the upgradeneeded event for an IDBOpenDBRequest.
    if (!m_hasPendingActivity)
        m_hasPendingActivity = isOpenDBRequest() && (event.type() == eventNames().upgradeneededEvent || event.type() == eventNames().blockedEvent);

    // The request should only remain in the transaction's request list if it represents a pending cursor operation, or this is an open request that was blocked.
    if (m_transaction && !m_pendingCursor && event.type() != eventNames().blockedEvent)
        m_transaction->removeRequest(*this);

    if (dontPreventDefault && event.type() == eventNames().errorEvent && m_transaction && !m_transaction->isFinishedOrFinishing()) {
        ASSERT(m_domError);
        m_transaction->abortDueToFailedRequest(*m_domError);
    }

    return dontPreventDefault;
}

void IDBRequest::uncaughtExceptionInEventHandler()
{
    LOG(IndexedDB, "IDBRequest::uncaughtExceptionInEventHandler");

    ASSERT(currentThread() == originThreadID());

    if (m_transaction && m_idbError.code() != IDBDatabaseException::AbortError)
        m_transaction->abortDueToFailedRequest(DOMError::create(IDBDatabaseException::getErrorName(IDBDatabaseException::AbortError), ASCIILiteral("IDBTransaction will abort due to uncaught exception in an event handler")));
}

void IDBRequest::setResult(const IDBKeyData& keyData)
{
    ASSERT(currentThread() == originThreadID());

    auto* context = scriptExecutionContext();
    if (!context)
        return;

    auto* exec = context->execState();
    if (!exec)
        return;

    clearResult();
    m_scriptResult = { context->vm(), idbKeyDataToScriptValue(*exec, keyData) };
}

void IDBRequest::setResult(uint64_t number)
{
    ASSERT(currentThread() == originThreadID());

    auto* context = scriptExecutionContext();
    if (!context)
        return;

    clearResult();
    m_scriptResult = { context->vm(), JSC::jsNumber(number) };
}

void IDBRequest::setResultToStructuredClone(const IDBValue& value)
{
    ASSERT(currentThread() == originThreadID());

    LOG(IndexedDB, "IDBRequest::setResultToStructuredClone");

    auto* context = scriptExecutionContext();
    if (!context)
        return;

    auto* exec = context->execState();
    if (!exec)
        return;

    clearResult();
    m_scriptResult = { context->vm(), deserializeIDBValueToJSValue(*exec, value) };
}

void IDBRequest::clearResult()
{
    ASSERT(currentThread() == originThreadID());

    m_scriptResult = { };
    m_cursorResult = nullptr;
    m_databaseResult = nullptr;
}

void IDBRequest::setResultToUndefined()
{
    ASSERT(currentThread() == originThreadID());

    auto* context = scriptExecutionContext();
    if (!context)
        return;

    clearResult();
    m_scriptResult = { context->vm(), JSC::jsUndefined() };
}

IDBCursor* IDBRequest::resultCursor()
{
    ASSERT(currentThread() == originThreadID());

    return m_cursorResult.get();
}

void IDBRequest::willIterateCursor(IDBCursor& cursor)
{
    ASSERT(currentThread() == originThreadID());
    ASSERT(m_isDone);
    ASSERT(scriptExecutionContext());
    ASSERT(m_transaction);
    ASSERT(!m_pendingCursor);
    ASSERT(&cursor == resultCursor());
    ASSERT(!m_cursorRequestNotifier);

    m_pendingCursor = &cursor;
    m_hasPendingActivity = true;
    clearResult();
    m_isDone = false;
    m_domError = nullptr;
    m_idbError = { };

    m_cursorRequestNotifier = std::make_unique<ScopeGuard>([this]() {
        m_pendingCursor->decrementOutstandingRequestCount();
    });
}

void IDBRequest::didOpenOrIterateCursor(const IDBResultData& resultData)
{
    ASSERT(currentThread() == originThreadID());
    ASSERT(m_pendingCursor);

    clearResult();

    if (resultData.type() == IDBResultType::IterateCursorSuccess || resultData.type() == IDBResultType::OpenCursorSuccess) {
        m_pendingCursor->setGetResult(*this, resultData.getResult());
        if (resultData.getResult().isDefined())
            m_cursorResult = m_pendingCursor;
    }

    m_cursorRequestNotifier = nullptr;
    m_pendingCursor = nullptr;

    requestCompleted(resultData);
}

void IDBRequest::requestCompleted(const IDBResultData& resultData)
{
    ASSERT(currentThread() == originThreadID());

    m_isDone = true;

    m_idbError = resultData.error();
    if (!m_idbError.isNull())
        onError();
    else
        onSuccess();
}

void IDBRequest::onError()
{
    LOG(IndexedDB, "IDBRequest::onError");

    ASSERT(currentThread() == originThreadID());
    ASSERT(!m_idbError.isNull());

    m_domError = DOMError::create(m_idbError.name(), m_idbError.message());
    enqueueEvent(Event::create(eventNames().errorEvent, true, true));
}

void IDBRequest::onSuccess()
{
    LOG(IndexedDB, "IDBRequest::onSuccess");
    ASSERT(currentThread() == originThreadID());

    enqueueEvent(Event::create(eventNames().successEvent, false, false));
}

void IDBRequest::setResult(Ref<IDBDatabase>&& database)
{
    ASSERT(currentThread() == originThreadID());

    clearResult();
    m_databaseResult = WTFMove(database);
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
