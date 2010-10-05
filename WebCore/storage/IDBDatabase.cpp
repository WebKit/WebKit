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
#include "IDBDatabase.h"

#include "IDBAny.h"
#include "IDBDatabaseError.h"
#include "IDBDatabaseException.h"
#include "IDBFactoryBackendInterface.h"
#include "IDBObjectStore.h"
#include "IDBRequest.h"
#include "IDBTransaction.h"
#include "ScriptExecutionContext.h"

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

IDBDatabase::IDBDatabase(PassRefPtr<IDBDatabaseBackendInterface> backend)
    : m_backend(backend)
{
    // We pass a reference to this object before it can be adopted.
    relaxAdoptionRequirement();
}

IDBDatabase::~IDBDatabase()
{
}

void IDBDatabase::setSetVersionTransaction(IDBTransactionBackendInterface* transaction)
{
    m_setVersionTransaction = transaction;
}

PassRefPtr<IDBObjectStore> IDBDatabase::createObjectStore(const String& name, const String& keyPath, bool autoIncrement)
{
    // FIXME: Raise a NOT_ALLOWED_ERR if m_setVersionTransaction is 0.
    RefPtr<IDBObjectStoreBackendInterface> objectStore = m_backend->createObjectStore(name, keyPath, autoIncrement, m_setVersionTransaction.get());
    if (!objectStore)
        return 0;
    return IDBObjectStore::create(objectStore.release(), m_setVersionTransaction.get());
}

void IDBDatabase::removeObjectStore(const String& name)
{
    m_backend->removeObjectStore(name, m_setVersionTransaction.get());
}

PassRefPtr<IDBRequest> IDBDatabase::setVersion(ScriptExecutionContext* context, const String& version)
{
    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this), 0);
    m_backend->setVersion(version, request);
    return request;
}

PassRefPtr<IDBTransaction> IDBDatabase::transaction(ScriptExecutionContext* context, DOMStringList* storeNames, unsigned short mode, unsigned long timeout)
{
    // We need to create a new transaction synchronously. Locks are acquired asynchronously. Operations
    // can be queued against the transaction at any point. They will start executing as soon as the
    // appropriate locks have been acquired.
    // Also note that each backend object corresponds to exactly one IDBTransaction object.
    RefPtr<IDBTransactionBackendInterface> transactionBackend = m_backend->transaction(storeNames, mode, timeout);
    RefPtr<IDBTransaction> transaction = IDBTransaction::create(context, transactionBackend, this);
    transactionBackend->setCallbacks(transaction.get());
    return transaction.release();
}

void IDBDatabase::close()
{
    m_backend->close();
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
