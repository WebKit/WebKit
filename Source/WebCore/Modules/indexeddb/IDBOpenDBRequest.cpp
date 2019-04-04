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
#include "IDBOpenDBRequest.h"

#if ENABLE(INDEXED_DATABASE)

#include "DOMException.h"
#include "EventNames.h"
#include "IDBConnectionProxy.h"
#include "IDBConnectionToServer.h"
#include "IDBDatabase.h"
#include "IDBError.h"
#include "IDBRequestCompletionEvent.h"
#include "IDBResultData.h"
#include "IDBTransaction.h"
#include "IDBVersionChangeEvent.h"
#include "Logging.h"
#include "ScriptExecutionContext.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(IDBOpenDBRequest);

Ref<IDBOpenDBRequest> IDBOpenDBRequest::createDeleteRequest(ScriptExecutionContext& context, IDBClient::IDBConnectionProxy& connectionProxy, const IDBDatabaseIdentifier& databaseIdentifier)
{
    return adoptRef(*new IDBOpenDBRequest(context, connectionProxy, databaseIdentifier, 0, IndexedDB::RequestType::Delete));
}

Ref<IDBOpenDBRequest> IDBOpenDBRequest::createOpenRequest(ScriptExecutionContext& context, IDBClient::IDBConnectionProxy& connectionProxy, const IDBDatabaseIdentifier& databaseIdentifier, uint64_t version)
{
    return adoptRef(*new IDBOpenDBRequest(context, connectionProxy, databaseIdentifier, version, IndexedDB::RequestType::Open));
}
    
IDBOpenDBRequest::IDBOpenDBRequest(ScriptExecutionContext& context, IDBClient::IDBConnectionProxy& connectionProxy, const IDBDatabaseIdentifier& databaseIdentifier, uint64_t version, IndexedDB::RequestType requestType)
    : IDBRequest(context, connectionProxy)
    , m_databaseIdentifier(databaseIdentifier)
    , m_version(version)
{
    m_requestType = requestType;
}

IDBOpenDBRequest::~IDBOpenDBRequest()
{
    ASSERT(&originThread() == &Thread::current());
}

void IDBOpenDBRequest::onError(const IDBResultData& data)
{
    ASSERT(&originThread() == &Thread::current());

    m_domError = data.error().toDOMException();
    enqueueEvent(IDBRequestCompletionEvent::create(eventNames().errorEvent, Event::CanBubble::Yes, Event::IsCancelable::Yes, *this));
}

void IDBOpenDBRequest::versionChangeTransactionDidFinish()
{
    ASSERT(&originThread() == &Thread::current());

    // 3.3.7 "versionchange" transaction steps
    // When the transaction is finished, after firing complete/abort on the transaction, immediately set request's transaction property to null.
    m_shouldExposeTransactionToDOM = false;
}

void IDBOpenDBRequest::fireSuccessAfterVersionChangeCommit()
{
    LOG(IndexedDB, "IDBOpenDBRequest::fireSuccessAfterVersionChangeCommit() - %s", resourceIdentifier().loggingString().utf8().data());

    ASSERT(&originThread() == &Thread::current());
    ASSERT(hasPendingActivity());
    m_transaction->addRequest(*this);

    auto event = IDBRequestCompletionEvent::create(eventNames().successEvent, Event::CanBubble::No, Event::IsCancelable::No, *this);
    m_openDatabaseSuccessEvent = &event.get();

    enqueueEvent(WTFMove(event));
}

void IDBOpenDBRequest::fireErrorAfterVersionChangeCompletion()
{
    LOG(IndexedDB, "IDBOpenDBRequest::fireErrorAfterVersionChangeCompletion() - %s", resourceIdentifier().loggingString().utf8().data());

    ASSERT(&originThread() == &Thread::current());
    ASSERT(hasPendingActivity());

    IDBError idbError(AbortError);
    m_domError = DOMException::create(AbortError);
    setResultToUndefined();

    m_transaction->addRequest(*this);
    enqueueEvent(IDBRequestCompletionEvent::create(eventNames().errorEvent, Event::CanBubble::Yes, Event::IsCancelable::Yes, *this));
}

void IDBOpenDBRequest::cancelForStop()
{
    connectionProxy().openDBRequestCancelled({ connectionProxy(), *this });

    if (m_transaction && m_transaction->isVersionChange())
        m_transaction->removeRequest(*this);
}

void IDBOpenDBRequest::dispatchEvent(Event& event)
{
    ASSERT(&originThread() == &Thread::current());

    auto protectedThis = makeRef(*this);

    IDBRequest::dispatchEvent(event);

    if (m_transaction && m_transaction->isVersionChange() && (event.type() == eventNames().errorEvent || event.type() == eventNames().successEvent))
        m_transaction->database().connectionProxy().didFinishHandlingVersionChangeTransaction(m_transaction->database().databaseConnectionIdentifier(), *m_transaction);
}

void IDBOpenDBRequest::onSuccess(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBOpenDBRequest::onSuccess()");

    ASSERT(&originThread() == &Thread::current());

    setResult(IDBDatabase::create(*scriptExecutionContext(), connectionProxy(), resultData));
    m_readyState = ReadyState::Done;

    enqueueEvent(IDBRequestCompletionEvent::create(eventNames().successEvent, Event::CanBubble::No, Event::IsCancelable::No, *this));
}

void IDBOpenDBRequest::onUpgradeNeeded(const IDBResultData& resultData)
{
    ASSERT(&originThread() == &Thread::current());

    Ref<IDBDatabase> database = IDBDatabase::create(*scriptExecutionContext(), connectionProxy(), resultData);
    Ref<IDBTransaction> transaction = database->startVersionChangeTransaction(resultData.transactionInfo(), *this);

    ASSERT(transaction->info().mode() == IDBTransactionMode::Versionchange);
    ASSERT(transaction->originalDatabaseInfo());

    uint64_t oldVersion = transaction->originalDatabaseInfo()->version();
    uint64_t newVersion = transaction->info().newVersion();

    LOG(IndexedDB, "IDBOpenDBRequest::onUpgradeNeeded() - current version is %" PRIu64 ", new is %" PRIu64, oldVersion, newVersion);

    setResult(WTFMove(database));
    m_readyState = ReadyState::Done;
    m_transaction = WTFMove(transaction);
    m_transaction->addRequest(*this);

    enqueueEvent(IDBVersionChangeEvent::create(oldVersion, newVersion, eventNames().upgradeneededEvent));
}

void IDBOpenDBRequest::onDeleteDatabaseSuccess(const IDBResultData& resultData)
{
    ASSERT(&originThread() == &Thread::current());

    uint64_t oldVersion = resultData.databaseInfo().version();

    LOG(IndexedDB, "IDBOpenDBRequest::onDeleteDatabaseSuccess() - current version is %" PRIu64, oldVersion);

    m_readyState = ReadyState::Done;
    setResultToUndefined();

    enqueueEvent(IDBVersionChangeEvent::create(oldVersion, 0, eventNames().successEvent));
}

void IDBOpenDBRequest::requestCompleted(const IDBResultData& data)
{
    LOG(IndexedDB, "IDBOpenDBRequest::requestCompleted");

    ASSERT(&originThread() == &Thread::current());

    // If an Open request was completed after the page has navigated, leaving this request
    // with a stopped script execution context, we need to message back to the server so it
    // doesn't hang waiting on a database connection or transaction that will never exist.
    if (m_contextStopped) {
        switch (data.type()) {
        case IDBResultType::OpenDatabaseSuccess:
            connectionProxy().abortOpenAndUpgradeNeeded(data.databaseConnectionIdentifier(), IDBResourceIdentifier::emptyValue());
            break;
        case IDBResultType::OpenDatabaseUpgradeNeeded:
            connectionProxy().abortOpenAndUpgradeNeeded(data.databaseConnectionIdentifier(), data.transactionInfo().identifier());
            break;
        default:
            break;
        }

        return;
    }

    switch (data.type()) {
    case IDBResultType::Error:
        onError(data);
        break;
    case IDBResultType::OpenDatabaseSuccess:
        onSuccess(data);
        break;
    case IDBResultType::OpenDatabaseUpgradeNeeded:
        onUpgradeNeeded(data);
        break;
    case IDBResultType::DeleteDatabaseSuccess:
        onDeleteDatabaseSuccess(data);
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

void IDBOpenDBRequest::requestBlocked(uint64_t oldVersion, uint64_t newVersion)
{
    ASSERT(&originThread() == &Thread::current());

    LOG(IndexedDB, "IDBOpenDBRequest::requestBlocked");
    enqueueEvent(IDBVersionChangeEvent::create(oldVersion, newVersion, eventNames().blockedEvent));
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
