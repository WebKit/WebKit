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
#include "IDBPendingTransactionMonitor.h"
#include "IDBTransactionBackendInterface.h"
#include <wtf/ThreadSpecific.h>

using WTF::ThreadSpecific;

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

static ThreadSpecific<Vector<IDBTransactionBackendInterface*> >& transactions()
{
    // FIXME: Move the Vector to ScriptExecutionContext to avoid dealing with
    // thread-local storage.
    AtomicallyInitializedStatic(ThreadSpecific<Vector<IDBTransactionBackendInterface*> >*, transactions = new ThreadSpecific<Vector<IDBTransactionBackendInterface*> >);
    return *transactions;
}

void IDBPendingTransactionMonitor::addPendingTransaction(IDBTransactionBackendInterface* transaction)
{
    transactions()->append(transaction);
}

void IDBPendingTransactionMonitor::removePendingTransaction(IDBTransactionBackendInterface* transaction)
{
    ThreadSpecific<Vector<IDBTransactionBackendInterface*> >& transactionList = transactions();
    size_t pos = transactionList->find(transaction);
    if (pos == notFound)
        return;

    transactionList->remove(pos);
}

void IDBPendingTransactionMonitor::abortPendingTransactions()
{
    ThreadSpecific<Vector<IDBTransactionBackendInterface*> >& transactionList = transactions();
    for (size_t i = 0; i < transactions()->size(); ++i)
        transactionList->at(i)->abort();
    // FIXME: Exercise this call to clear() in a layout test.
    transactionList->clear();
}

};
#endif // ENABLE(INDEXED_DATABASE)
