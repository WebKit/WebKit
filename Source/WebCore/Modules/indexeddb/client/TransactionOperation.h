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

#ifndef TransactionOperation_h
#define TransactionOperation_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBRequest.h"
#include "IDBResourceIdentifier.h"
#include "IDBTransaction.h"

namespace WebCore {

class IDBResultData;

namespace IndexedDB {
enum class IndexRecordType;
}

namespace IDBClient {

class TransactionOperation : public ThreadSafeRefCounted<TransactionOperation> {
public:
    void perform()
    {
        ASSERT(m_performFunction);
        m_performFunction();
        m_performFunction = { };
    }

    void completed(const IDBResultData& data)
    {
        ASSERT(m_completeFunction);
        m_completeFunction(data);
        m_transaction->operationDidComplete(*this);
        m_completeFunction = { };
    }

    const IDBResourceIdentifier& identifier() const { return m_identifier; }
    IDBResourceIdentifier transactionIdentifier() const { return m_transaction->info().identifier(); }
    uint64_t objectStoreIdentifier() const { return m_objectStoreIdentifier; }
    uint64_t indexIdentifier() const { return m_indexIdentifier; }
    IDBResourceIdentifier* cursorIdentifier() const { return m_cursorIdentifier.get(); }
    IDBTransaction& transaction() { return m_transaction.get(); }
    IndexedDB::IndexRecordType indexRecordType() const { return m_indexRecordType; }

protected:
    TransactionOperation(IDBTransaction& transaction)
        : m_transaction(transaction)
        , m_identifier(transaction.serverConnection())
    {
    }

    TransactionOperation(IDBTransaction&, IDBRequest&);

    Ref<IDBTransaction> m_transaction;
    IDBResourceIdentifier m_identifier;
    uint64_t m_objectStoreIdentifier { 0 };
    uint64_t m_indexIdentifier { 0 };
    std::unique_ptr<IDBResourceIdentifier> m_cursorIdentifier;
    IndexedDB::IndexRecordType m_indexRecordType;
    std::function<void ()> m_performFunction;
    std::function<void (const IDBResultData&)> m_completeFunction;
};

template <typename... Arguments>
class TransactionOperationImpl final : public TransactionOperation {
public:
    TransactionOperationImpl(IDBTransaction& transaction, void (IDBTransaction::*completeMethod)(const IDBResultData&), void (IDBTransaction::*performMethod)(TransactionOperation&, Arguments...), Arguments&&... arguments)
        : TransactionOperation(transaction)
    {
        RefPtr<TransactionOperation> self(this);

        ASSERT(performMethod);
        m_performFunction = [self, this, performMethod, arguments...] {
            (&m_transaction.get()->*performMethod)(*this, arguments...);
        };

        if (completeMethod) {
            m_completeFunction = [self, this, completeMethod](const IDBResultData& resultData) {
                if (completeMethod)
                    (&m_transaction.get()->*completeMethod)(resultData);
            };
        }
    }

    TransactionOperationImpl(IDBTransaction& transaction, IDBRequest& request, void (IDBTransaction::*completeMethod)(IDBRequest&, const IDBResultData&), void (IDBTransaction::*performMethod)(TransactionOperation&, Arguments...), Arguments&&... arguments)
        : TransactionOperation(transaction, request)
    {
        RefPtr<TransactionOperation> self(this);

        ASSERT(performMethod);
        m_performFunction = [self, this, performMethod, arguments...] {
            (&m_transaction.get()->*performMethod)(*this, arguments...);
        };

        if (completeMethod) {
            RefPtr<IDBRequest> refRequest(&request);
            m_completeFunction = [self, this, refRequest, completeMethod](const IDBResultData& resultData) {
                if (completeMethod)
                    (&m_transaction.get()->*completeMethod)(*refRequest, resultData);
            };
        }
    }
};

inline RefPtr<TransactionOperation> createTransactionOperation(
    IDBTransaction& transaction,
    void (IDBTransaction::*complete)(const IDBResultData&),
    void (IDBTransaction::*perform)(TransactionOperation&))
{
    auto operation = new TransactionOperationImpl<>(transaction, complete, perform);
    return adoptRef(operation);
}

template<typename MP1, typename P1>
RefPtr<TransactionOperation> createTransactionOperation(
    IDBTransaction& transaction,
    void (IDBTransaction::*complete)(const IDBResultData&),
    void (IDBTransaction::*perform)(TransactionOperation&, MP1),
    const P1& parameter1)
{
    auto operation = new TransactionOperationImpl<MP1>(transaction, complete, perform, parameter1);
    return adoptRef(operation);
}

template<typename MP1, typename P1, typename MP2, typename P2>
RefPtr<TransactionOperation> createTransactionOperation(
    IDBTransaction& transaction,
    void (IDBTransaction::*complete)(const IDBResultData&),
    void (IDBTransaction::*perform)(TransactionOperation&, MP1, MP2),
    const P1& parameter1,
    const P2& parameter2)
{
    auto operation = new TransactionOperationImpl<MP1, MP2>(transaction, complete, perform, parameter1, parameter2);
    return adoptRef(operation);
}

template<typename MP1, typename P1>
RefPtr<TransactionOperation> createTransactionOperation(
    IDBTransaction& transaction,
    IDBRequest& request,
    void (IDBTransaction::*complete)(IDBRequest&, const IDBResultData&),
    void (IDBTransaction::*perform)(TransactionOperation&, MP1),
    const P1& parameter1)
{
    auto operation = new TransactionOperationImpl<MP1>(transaction, request, complete, perform, parameter1);
    return adoptRef(operation);
}

template<typename MP1, typename P1, typename MP2, typename P2>
RefPtr<TransactionOperation> createTransactionOperation(
    IDBTransaction& transaction,
    IDBRequest& request,
    void (IDBTransaction::*complete)(IDBRequest&, const IDBResultData&),
    void (IDBTransaction::*perform)(TransactionOperation&, MP1, MP2),
    const P1& parameter1,
    const P2& parameter2)
{
    auto operation = new TransactionOperationImpl<MP1, MP2>(transaction, request, complete, perform, parameter1, parameter2);
    return adoptRef(operation);
}

template<typename MP1, typename MP2, typename MP3, typename P1, typename P2, typename P3>
RefPtr<TransactionOperation> createTransactionOperation(
    IDBTransaction& transaction,
    IDBRequest& request,
    void (IDBTransaction::*complete)(IDBRequest&, const IDBResultData&),
    void (IDBTransaction::*perform)(TransactionOperation&, MP1, MP2, MP3),
    const P1& parameter1,
    const P2& parameter2,
    const P3& parameter3)
{
    auto operation = new TransactionOperationImpl<MP1, MP2, MP3>(transaction, request, complete, perform, parameter1, parameter2, parameter3);
    return adoptRef(operation);
}

} // namespace IDBClient
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // TransactionOperation_h
