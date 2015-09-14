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

#include <memory>
#include <wtf/HashMap.h>
#include <wtf/ListHashSet.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class IDBTransactionBackend;

// Transactions are executed in the order the were created.
class IDBTransactionCoordinator {
public:
    IDBTransactionCoordinator();
    virtual ~IDBTransactionCoordinator();

    // Called by transactions as they start and finish.
    void didCreateTransaction(IDBTransactionBackend*);
    void didOpenBackingStoreTransaction(IDBTransactionBackend*);
    void didStartTransaction(IDBTransactionBackend*);
    void didFinishTransaction(IDBTransactionBackend*);

#ifndef NDEBUG
    bool isActive(IDBTransactionBackend*);
#endif

private:
    void processStartedTransactions();
    bool canRunTransaction(IDBTransactionBackend*);

    // This is just an efficient way to keep references to all transactions.
    HashMap<IDBTransactionBackend*, RefPtr<IDBTransactionBackend> > m_transactions;
    // Transactions in different states are grouped below.
    ListHashSet<IDBTransactionBackend*> m_queuedTransactions;
    HashSet<IDBTransactionBackend*> m_startedTransactions;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

#endif // IDBTransactionCoordinator_h
