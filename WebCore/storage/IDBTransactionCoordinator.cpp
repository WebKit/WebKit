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

#include "IDBObjectStore.h"
#include "IDBObjectStoreBackendInterface.h"
#include "IDBTransactionBackendImpl.h"
#include "ScriptExecutionContext.h"

namespace WebCore {

IDBTransactionCoordinator::IDBTransactionCoordinator() 
    : m_nextID(0)
{
}

IDBTransactionCoordinator::~IDBTransactionCoordinator()
{
}

PassRefPtr<IDBTransactionBackendInterface> IDBTransactionCoordinator::createTransaction(DOMStringList* objectStores, unsigned short mode, unsigned long timeout)
{
    RefPtr<IDBTransactionBackendInterface> transaction = IDBTransactionBackendImpl::create(objectStores, mode, timeout, ++m_nextID);
    m_transactionQueue.add(transaction.get());
    m_idMap.add(m_nextID, transaction);
    return transaction.release();
}

void IDBTransactionCoordinator::abort(int id)
{
    ASSERT(m_idMap.contains(id));
    RefPtr<IDBTransactionBackendInterface> transaction = m_idMap.get(id);
    ASSERT(transaction);
    m_transactionQueue.remove(transaction.get());
    m_idMap.remove(id);
    transaction->abort();
    // FIXME: this will change once we have transactions actually running.
}

};

#endif // ENABLE(INDEXED_DATABASE)
