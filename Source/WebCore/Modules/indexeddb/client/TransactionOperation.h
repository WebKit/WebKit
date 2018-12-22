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

#pragma once

#if ENABLE(INDEXED_DATABASE)

#include "IDBRequest.h"
#include "IDBRequestData.h"
#include "IDBResourceIdentifier.h"
#include "IDBResultData.h"
#include "IDBTransaction.h"
#include <wtf/Function.h>
#include <wtf/MainThread.h>
#include <wtf/Threading.h>

namespace WebCore {

class IDBResultData;

namespace IndexedDB {
enum class IndexRecordType;
}

namespace IDBClient {

class TransactionOperation : public ThreadSafeRefCounted<TransactionOperation> {
    friend IDBRequestData::IDBRequestData(TransactionOperation&);
public:
    virtual ~TransactionOperation()
    {
        ASSERT(m_originThread.ptr() == &Thread::current());
    }

    void perform()
    {
        ASSERT(m_originThread.ptr() == &Thread::current());
        ASSERT(m_performFunction);
        m_performFunction();
        m_performFunction = { };
    }

    void transitionToCompleteOnThisThread(const IDBResultData& data)
    {
        ASSERT(m_originThread.ptr() == &Thread::current());
        m_transaction->operationCompletedOnServer(data, *this);
    }

    void transitionToComplete(const IDBResultData& data, RefPtr<TransactionOperation>&& lastRef)
    {
        ASSERT(isMainThread());

        if (m_originThread.ptr() == &Thread::current())
            transitionToCompleteOnThisThread(data);
        else {
            m_transaction->performCallbackOnOriginThread(*this, &TransactionOperation::transitionToCompleteOnThisThread, data);
            m_transaction->callFunctionOnOriginThread([lastRef = WTFMove(lastRef)]() {
            });
        }
    }

    void doComplete(const IDBResultData& data)
    {
        ASSERT(m_originThread.ptr() == &Thread::current());

        // Due to race conditions between the server sending an "operation complete" message and the client
        // forcefully aborting an operation, it's unavoidable that this method might be called twice.
        // It's okay to handle that gracefully with an early return.
        if (!m_completeFunction)
            return;

        m_completeFunction(data);
        m_transaction->operationCompletedOnClient(*this);

        // m_completeFunction might be holding the last ref to this TransactionOperation,
        // so we need to do this trick to null it out without first destroying it.
        WTF::Function<void (const IDBResultData&)> oldCompleteFunction;
        std::swap(m_completeFunction, oldCompleteFunction);
    }

    const IDBResourceIdentifier& identifier() const { return m_identifier; }

    Thread& originThread() const { return m_originThread.get(); }

    IDBRequest* idbRequest() { return m_idbRequest.get(); }

    bool nextRequestCanGoToServer() const { return m_nextRequestCanGoToServer && m_idbRequest; }
    void setNextRequestCanGoToServer(bool nextRequestCanGoToServer) { m_nextRequestCanGoToServer = nextRequestCanGoToServer; }

protected:
    TransactionOperation(IDBTransaction& transaction)
        : m_transaction(transaction)
        , m_identifier(transaction.connectionProxy())
    {
    }

    TransactionOperation(IDBTransaction&, IDBRequest&);

    Ref<IDBTransaction> m_transaction;
    IDBResourceIdentifier m_identifier;
    uint64_t m_objectStoreIdentifier { 0 };
    uint64_t m_indexIdentifier { 0 };
    std::unique_ptr<IDBResourceIdentifier> m_cursorIdentifier;
    IndexedDB::IndexRecordType m_indexRecordType;
    WTF::Function<void ()> m_performFunction;
    WTF::Function<void (const IDBResultData&)> m_completeFunction;

private:
    IDBResourceIdentifier transactionIdentifier() const { return m_transaction->info().identifier(); }
    uint64_t objectStoreIdentifier() const { return m_objectStoreIdentifier; }
    uint64_t indexIdentifier() const { return m_indexIdentifier; }
    IDBResourceIdentifier* cursorIdentifier() const { return m_cursorIdentifier.get(); }
    IDBTransaction& transaction() { return m_transaction.get(); }
    IndexedDB::IndexRecordType indexRecordType() const { return m_indexRecordType; }

    Ref<Thread> m_originThread { Thread::current() };
    RefPtr<IDBRequest> m_idbRequest;
    bool m_nextRequestCanGoToServer { true };
};

template <typename... Arguments>
class TransactionOperationImpl final : public TransactionOperation {
public:
    TransactionOperationImpl(IDBTransaction& transaction, void (IDBTransaction::*completeMethod)(const IDBResultData&), void (IDBTransaction::*performMethod)(TransactionOperation&, Arguments...), Arguments&&... arguments)
        : TransactionOperation(transaction)
    {
        RefPtr<TransactionOperation> protectedThis(this);

        ASSERT(performMethod);
        m_performFunction = [protectedThis, this, performMethod, arguments...] {
            (&m_transaction.get()->*performMethod)(*this, arguments...);
        };

        if (completeMethod) {
            m_completeFunction = [protectedThis, this, completeMethod](const IDBResultData& resultData) {
                if (completeMethod)
                    (&m_transaction.get()->*completeMethod)(resultData);
            };
        }
    }

    TransactionOperationImpl(IDBTransaction& transaction, IDBRequest& request, void (IDBTransaction::*completeMethod)(IDBRequest&, const IDBResultData&), void (IDBTransaction::*performMethod)(TransactionOperation&, Arguments...), Arguments&&... arguments)
        : TransactionOperation(transaction, request)
    {
        RefPtr<TransactionOperation> protectedThis(this);

        ASSERT(performMethod);
        m_performFunction = [protectedThis, this, performMethod, arguments...] {
            (&m_transaction.get()->*performMethod)(*this, arguments...);
        };

        if (completeMethod) {
            RefPtr<IDBRequest> refRequest(&request);
            m_completeFunction = [protectedThis, this, refRequest, completeMethod](const IDBResultData& resultData) {
                if (completeMethod)
                    (&m_transaction.get()->*completeMethod)(*refRequest, resultData);
            };
        }
    }
};

inline Ref<TransactionOperation> createTransactionOperation(
    IDBTransaction& transaction,
    void (IDBTransaction::*complete)(const IDBResultData&),
    void (IDBTransaction::*perform)(TransactionOperation&))
{
    return adoptRef(*new TransactionOperationImpl<>(transaction, complete, perform));
}

template<typename MP1, typename P1>
Ref<TransactionOperation> createTransactionOperation(
    IDBTransaction& transaction,
    void (IDBTransaction::*complete)(const IDBResultData&),
    void (IDBTransaction::*perform)(TransactionOperation&, MP1),
    const P1& parameter1)
{
    return adoptRef(*new TransactionOperationImpl<MP1>(transaction, complete, perform, parameter1));
}

template<typename MP1, typename P1, typename MP2, typename P2>
Ref<TransactionOperation> createTransactionOperation(
    IDBTransaction& transaction,
    void (IDBTransaction::*complete)(const IDBResultData&),
    void (IDBTransaction::*perform)(TransactionOperation&, MP1, MP2),
    const P1& parameter1,
    const P2& parameter2)
{
    return adoptRef(*new TransactionOperationImpl<MP1, MP2>(transaction, complete, perform, parameter1, parameter2));
}

template<typename MP1, typename P1, typename MP2, typename P2, typename MP3, typename P3>
Ref<TransactionOperation> createTransactionOperation(
    IDBTransaction& transaction,
    void (IDBTransaction::*complete)(const IDBResultData&),
    void (IDBTransaction::*perform)(TransactionOperation&, MP1, MP2, MP3),
    const P1& parameter1,
    const P2& parameter2,
    const P3& parameter3)
{
    return adoptRef(*new TransactionOperationImpl<MP1, MP2, MP3>(transaction, complete, perform, parameter1, parameter2, parameter3));
}

template<typename MP1, typename P1>
Ref<TransactionOperation> createTransactionOperation(
    IDBTransaction& transaction,
    IDBRequest& request,
    void (IDBTransaction::*complete)(IDBRequest&, const IDBResultData&),
    void (IDBTransaction::*perform)(TransactionOperation&, MP1),
    const P1& parameter1)
{
    return adoptRef(*new TransactionOperationImpl<MP1>(transaction, request, complete, perform, parameter1));
}

template<typename MP1, typename P1, typename MP2, typename P2>
Ref<TransactionOperation> createTransactionOperation(
    IDBTransaction& transaction,
    IDBRequest& request,
    void (IDBTransaction::*complete)(IDBRequest&, const IDBResultData&),
    void (IDBTransaction::*perform)(TransactionOperation&, MP1, MP2),
    const P1& parameter1,
    const P2& parameter2)
{
    return adoptRef(*new TransactionOperationImpl<MP1, MP2>(transaction, request, complete, perform, parameter1, parameter2));
}

template<typename MP1, typename MP2, typename MP3, typename P1, typename P2, typename P3>
Ref<TransactionOperation> createTransactionOperation(
    IDBTransaction& transaction,
    IDBRequest& request,
    void (IDBTransaction::*complete)(IDBRequest&, const IDBResultData&),
    void (IDBTransaction::*perform)(TransactionOperation&, MP1, MP2, MP3),
    const P1& parameter1,
    const P2& parameter2,
    const P3& parameter3)
{
    return adoptRef(*new TransactionOperationImpl<MP1, MP2, MP3>(transaction, request, complete, perform, parameter1, parameter2, parameter3));
}

} // namespace IDBClient
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
