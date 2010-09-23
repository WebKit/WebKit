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

#ifndef IDBTransactionCoordinator_h
#define IDBTransactionCoordinator_h

#if ENABLE(INDEXED_DATABASE)

#include "DOMStringList.h"
#include "IDBTransactionBackendInterface.h"
#include <wtf/ListHashSet.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class IDBTransactionBackendImpl;
class IDBTransactionCallbacks;
class IDBDatabaseBackendImpl;

// This class manages transactions as follows. Requests for new transactions are
// always satisfied and the new transaction is placed in a queue.
// Transactions are not actually started until the first operation is issued.
// Each transaction executes in a separate thread and is committed automatically
// when there are no more operations issued in its context.
// When starting, a transaction will attempt to lock all the object stores in its
// scope. If this does not happen within a given timeout, an exception is raised.
// The Coordinator maintains a pool of threads. If there are no threads available
// the next transaction in the queue will have to wait until a thread becomes
// available.
// Transactions are executed in the order the were created.
class IDBTransactionCoordinator : public RefCounted<IDBTransactionCoordinator> {
public:
    static PassRefPtr<IDBTransactionCoordinator> create() { return adoptRef(new IDBTransactionCoordinator()); }
    virtual ~IDBTransactionCoordinator();

    PassRefPtr<IDBTransactionBackendInterface> createTransaction(DOMStringList* objectStores, unsigned short mode, unsigned long timeout, IDBDatabaseBackendImpl*);

    // Called by transactions as they start and finish.
    void didStartTransaction(IDBTransactionBackendImpl*);
    void didFinishTransaction(IDBTransactionBackendImpl*);

private:
    IDBTransactionCoordinator();

    void processStartedTransactions();

    // This map owns all transactions known to the coordinator.
    HashMap<int, RefPtr<IDBTransactionBackendImpl> > m_transactions;
    // Transactions in different states are grouped below.
    ListHashSet<IDBTransactionBackendImpl* > m_startedTransactions;
    HashSet<IDBTransactionBackendImpl* > m_runningTransactions;
    int m_nextID;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

#endif // IDBTransactionCoordinator_h
