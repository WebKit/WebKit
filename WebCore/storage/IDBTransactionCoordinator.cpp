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
#include "IDBTransactionCoordinator.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBDatabaseBackendImpl.h"
#include "IDBObjectStoreBackendInterface.h"
#include "IDBTransactionBackendImpl.h"
#include "SQLiteDatabase.h"
#include "ScriptExecutionContext.h"

namespace WebCore {

IDBTransactionCoordinator::IDBTransactionCoordinator() 
    : m_nextID(0)
{
}

IDBTransactionCoordinator::~IDBTransactionCoordinator()
{
}

PassRefPtr<IDBTransactionBackendInterface> IDBTransactionCoordinator::createTransaction(DOMStringList* objectStores, unsigned short mode, unsigned long timeout, IDBDatabaseBackendImpl* database)
{
    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::create(objectStores, mode, timeout, ++m_nextID, database);
    m_transactions.add(m_nextID, transaction);
    return transaction.release();
}

void IDBTransactionCoordinator::didStartTransaction(IDBTransactionBackendImpl* transaction)
{
    ASSERT(m_transactions.contains(transaction->id()));

    m_startedTransactions.add(transaction);
    processStartedTransactions();
}

void IDBTransactionCoordinator::didFinishTransaction(IDBTransactionBackendImpl* transaction)
{
    ASSERT(m_transactions.contains(transaction->id()));

    if (m_startedTransactions.contains(transaction)) {
        ASSERT(!m_runningTransactions.contains(transaction));
        m_startedTransactions.remove(transaction);
    } else {
        ASSERT(m_runningTransactions.contains(transaction));
        m_runningTransactions.remove(transaction);
    }
    m_transactions.remove(transaction->id());

    processStartedTransactions();
}

void IDBTransactionCoordinator::processStartedTransactions()
{
    // FIXME: This should allocate a thread to the next transaction that's
    // ready to run. For now we only have a single running transaction.
    if (m_startedTransactions.isEmpty() || !m_runningTransactions.isEmpty())
        return;

    ASSERT(m_transactions.contains(transaction->id()));

    IDBTransactionBackendImpl* transaction = *m_startedTransactions.begin();
    m_startedTransactions.remove(transaction);
    m_runningTransactions.add(transaction);
    transaction->run();
}

};

#endif // ENABLE(INDEXED_DATABASE)
