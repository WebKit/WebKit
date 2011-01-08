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
#include "IDBIndex.h"
#include "IDBObjectStore.h"
#include "IDBRequest.h"
#include "IDBTransaction.h"
#include "ScriptExecutionContext.h"
#include <limits>

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

// FIXME: We need to spec this differently.
const unsigned long defaultTimeout = 0; // Infinite.

IDBDatabase::IDBDatabase(PassRefPtr<IDBDatabaseBackendInterface> backend)
    : m_backend(backend)
{
    // We pass a reference of this object before it can be adopted.
    relaxAdoptionRequirement();
}

IDBDatabase::~IDBDatabase()
{
}

void IDBDatabase::setSetVersionTransaction(IDBTransactionBackendInterface* transaction)
{
    m_setVersionTransaction = transaction;
}

PassRefPtr<IDBObjectStore> IDBDatabase::createObjectStore(const String& name, const OptionsObject& options, ExceptionCode& ec)
{
    if (!m_setVersionTransaction) {
        ec = IDBDatabaseException::NOT_ALLOWED_ERR;
        return 0;
    }

    String keyPath;
    options.getKeyString("keyPath", keyPath);
    bool autoIncrement = false;
    options.getKeyBool("autoIncrement", autoIncrement);
    // FIXME: Look up evictable and pass that on as well.

    if (autoIncrement) {
        // FIXME: Implement support for auto increment.
        ec = IDBDatabaseException::UNKNOWN_ERR;
        return 0;
    }

    RefPtr<IDBObjectStoreBackendInterface> objectStore = m_backend->createObjectStore(name, keyPath, autoIncrement, m_setVersionTransaction.get(), ec);
    if (!objectStore) {
        ASSERT(ec);
        return 0;
    }
    return IDBObjectStore::create(objectStore.release(), m_setVersionTransaction.get());
}

void IDBDatabase::deleteObjectStore(const String& name, ExceptionCode& ec)
{
    if (!m_setVersionTransaction) {
        ec = IDBDatabaseException::NOT_ALLOWED_ERR;
        return;
    }

    m_backend->deleteObjectStore(name, m_setVersionTransaction.get(), ec);
}

PassRefPtr<IDBRequest> IDBDatabase::setVersion(ScriptExecutionContext* context, const String& version, ExceptionCode& ec)
{
    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this), 0);
    m_backend->setVersion(version, request, ec);
    return request;
}

PassRefPtr<IDBTransaction> IDBDatabase::transaction(ScriptExecutionContext* context, const OptionsObject& options, ExceptionCode& ec)
{
    RefPtr<DOMStringList> storeNames = options.getKeyDOMStringList("objectStoreNames");
    if (!storeNames) {
        storeNames = DOMStringList::create();
        String storeName;
        if (options.getKeyString("objectStoreNames", storeName))
            storeNames->append(storeName);
    }

    // Gets cast to an unsigned short.
    int32_t mode = IDBTransaction::READ_ONLY;
    options.getKeyInt32("mode", mode);
    if (mode != IDBTransaction::READ_WRITE && mode != IDBTransaction::READ_ONLY) {
        // FIXME: May need to change when specced: http://www.w3.org/Bugs/Public/show_bug.cgi?id=11406
        ec = IDBDatabaseException::CONSTRAINT_ERR;
        return 0;
    }

    // Gets cast to an unsigned long.
    // FIXME: The spec needs to be updated on this. It should probably take a double.
    int32_t timeout = defaultTimeout; 
    options.getKeyInt32("timeout", timeout);
    int64_t unsignedLongMax = std::numeric_limits<unsigned long>::max();
    if (timeout < 0 || timeout > unsignedLongMax)
        timeout = defaultTimeout; // Ignore illegal values.

    // We need to create a new transaction synchronously. Locks are acquired asynchronously. Operations
    // can be queued against the transaction at any point. They will start executing as soon as the
    // appropriate locks have been acquired.
    // Also note that each backend object corresponds to exactly one IDBTransaction object.
    RefPtr<IDBTransactionBackendInterface> transactionBackend = m_backend->transaction(storeNames.get(), mode, timeout, ec);
    if (!transactionBackend) {
        ASSERT(ec);
        return 0;
    }
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
