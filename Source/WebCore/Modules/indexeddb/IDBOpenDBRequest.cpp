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

#include "DOMError.h"
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

namespace WebCore {

RefPtr<IDBOpenDBRequest> IDBOpenDBRequest::maybeCreateDeleteRequest(ScriptExecutionContext& context, const IDBDatabaseIdentifier& databaseIdentifier)
{
    ASSERT(databaseIdentifier.isValid());
    auto* proxy = context.idbConnectionProxy();
    if (!proxy)
        return nullptr;

    return adoptRef(new IDBOpenDBRequest(context, proxy->serverConnectionIdentifier(), databaseIdentifier, 0, IndexedDB::RequestType::Delete));
}

RefPtr<IDBOpenDBRequest> IDBOpenDBRequest::maybeCreateOpenRequest(ScriptExecutionContext& context, const IDBDatabaseIdentifier& databaseIdentifier, uint64_t version)
{
    ASSERT(databaseIdentifier.isValid());
    auto* proxy = context.idbConnectionProxy();
    if (!proxy)
        return nullptr;

    return adoptRef(new IDBOpenDBRequest(context, proxy->serverConnectionIdentifier(), databaseIdentifier, version, IndexedDB::RequestType::Open));
}
    
IDBOpenDBRequest::IDBOpenDBRequest(ScriptExecutionContext& context, uint64_t serverConnectionIdentifier, const IDBDatabaseIdentifier& databaseIdentifier, uint64_t version, IndexedDB::RequestType requestType)
    : IDBRequest(context, serverConnectionIdentifier)
    , m_databaseIdentifier(databaseIdentifier)
    , m_version(version)
{
    m_requestType = requestType;
}

IDBOpenDBRequest::~IDBOpenDBRequest()
{
}

void IDBOpenDBRequest::onError(const IDBResultData& data)
{
    m_domError = DOMError::create(data.error().name(), data.error().message());
    enqueueEvent(IDBRequestCompletionEvent::create(eventNames().errorEvent, true, true, *this));
}

void IDBOpenDBRequest::versionChangeTransactionDidFinish()
{
    // 3.3.7 "versionchange" transaction steps
    // When the transaction is finished, after firing complete/abort on the transaction, immediately set request's transaction property to null.
    m_shouldExposeTransactionToDOM = false;
}

void IDBOpenDBRequest::fireSuccessAfterVersionChangeCommit()
{
    LOG(IndexedDB, "IDBOpenDBRequest::fireSuccessAfterVersionChangeCommit()");

    ASSERT(hasPendingActivity());
    m_transaction->addRequest(*this);

    auto event = IDBRequestCompletionEvent::create(eventNames().successEvent, false, false, *this);
    m_openDatabaseSuccessEvent = &event.get();

    enqueueEvent(WTFMove(event));
}

void IDBOpenDBRequest::fireErrorAfterVersionChangeCompletion()
{
    LOG(IndexedDB, "IDBOpenDBRequest::fireErrorAfterVersionChangeCompletion()");

    ASSERT(hasPendingActivity());

    IDBError idbError(IDBDatabaseException::AbortError);
    m_domError = DOMError::create(idbError.name(), idbError.message());
    setResultToUndefined();

    m_transaction->addRequest(*this);
    enqueueEvent(IDBRequestCompletionEvent::create(eventNames().errorEvent, true, true, *this));
}

bool IDBOpenDBRequest::dispatchEvent(Event& event)
{
    bool result = IDBRequest::dispatchEvent(event);

    if (m_transaction && m_transaction->isVersionChange() && (event.type() == eventNames().errorEvent || event.type() == eventNames().successEvent))
        m_transaction->database().serverConnection().didFinishHandlingVersionChangeTransaction(*m_transaction);

    return result;
}

void IDBOpenDBRequest::onSuccess(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBOpenDBRequest::onSuccess()");

    auto* connection = connectionToServer();
    if (!connection)
        return;

    setResult(IDBDatabase::create(*scriptExecutionContext(), *connection, resultData));
    m_isDone = true;

    enqueueEvent(IDBRequestCompletionEvent::create(eventNames().successEvent, false, false, *this));
}

void IDBOpenDBRequest::onUpgradeNeeded(const IDBResultData& resultData)
{
    auto* connection = connectionToServer();
    if (!connection)
        return;

    Ref<IDBDatabase> database = IDBDatabase::create(*scriptExecutionContext(), *connection, resultData);
    Ref<IDBTransaction> transaction = database->startVersionChangeTransaction(resultData.transactionInfo(), *this);

    ASSERT(transaction->info().mode() == IndexedDB::TransactionMode::VersionChange);
    ASSERT(transaction->originalDatabaseInfo());

    uint64_t oldVersion = transaction->originalDatabaseInfo()->version();
    uint64_t newVersion = transaction->info().newVersion();

    LOG(IndexedDB, "IDBOpenDBRequest::onUpgradeNeeded() - current version is %" PRIu64 ", new is %" PRIu64, oldVersion, newVersion);

    setResult(WTFMove(database));
    m_isDone = true;
    m_transaction = WTFMove(transaction);
    m_transaction->addRequest(*this);

    enqueueEvent(IDBVersionChangeEvent::create(oldVersion, newVersion, eventNames().upgradeneededEvent));
}

void IDBOpenDBRequest::onDeleteDatabaseSuccess(const IDBResultData& resultData)
{
    uint64_t oldVersion = resultData.databaseInfo().version();

    LOG(IndexedDB, "IDBOpenDBRequest::onDeleteDatabaseSuccess() - current version is %" PRIu64, oldVersion);

    m_isDone = true;
    setResultToUndefined();

    enqueueEvent(IDBVersionChangeEvent::create(oldVersion, 0, eventNames().successEvent));
}

void IDBOpenDBRequest::requestCompleted(const IDBResultData& data)
{
    LOG(IndexedDB, "IDBOpenDBRequest::requestCompleted");

    auto* connection = connectionToServer();
    if (!connection)
        return;

    // If an Open request was completed after the page has navigated, leaving this request
    // with a stopped script execution context, we need to message back to the server so it
    // doesn't hang waiting on a database connection or transaction that will never exist.
    if (m_contextStopped) {
        switch (data.type()) {
        case IDBResultType::OpenDatabaseSuccess:
            connection->abortOpenAndUpgradeNeeded(data.databaseConnectionIdentifier(), IDBResourceIdentifier::emptyValue());
            break;
        case IDBResultType::OpenDatabaseUpgradeNeeded:
            connection->abortOpenAndUpgradeNeeded(data.databaseConnectionIdentifier(), data.transactionInfo().identifier());
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
    LOG(IndexedDB, "IDBOpenDBRequest::requestBlocked");
    enqueueEvent(IDBVersionChangeEvent::create(oldVersion, newVersion, eventNames().blockedEvent));
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
