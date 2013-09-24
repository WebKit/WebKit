/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "IDBBackingStore.h"

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

PassRefPtr<IDBBackingStore> IDBBackingStore::open(SecurityOrigin*, const String&, const String&)
{
    LOG_ERROR("The default implementation of IDBBackingStore::open is currently empty");
    return PassRefPtr<IDBBackingStore>();
}

PassRefPtr<IDBBackingStore> IDBBackingStore::openInMemory(SecurityOrigin*, const String&)
{
    LOG_ERROR("The default implementation of IDBBackingStore::openInMemory is currently empty");
    return PassRefPtr<IDBBackingStore>();
}

bool IDBBackingStore::getObjectStores(int64_t, IDBDatabaseMetadata::ObjectStoreMap*)
{
    LOG_ERROR("The default implementation of IDBBackingStore::getObjectStores is currently empty");
    return false;
}

bool IDBBackingStore::Cursor::advance(unsigned long)
{
    LOG_ERROR("The default implementation of IDBBackingStore::Cursor::advance is currently empty");
    return false;
}

bool IDBBackingStore::Cursor::continueFunction(const IDBKey*, IteratorState)
{
    LOG_ERROR("The default implementation of IDBBackingStore::Cursor::continueFunction is currently empty");
    return false;
}

IDBBackingStore::Transaction::Transaction(IDBBackingStore* backingStore)
    : m_backingStore(backingStore)
{
}

void IDBBackingStore::Transaction::begin()
{
    LOG_ERROR("The default implementation of IDBBackingStore::Transaction::begin is currently empty");
}

bool IDBBackingStore::Transaction::commit()
{
    LOG_ERROR("The default implementation of IDBBackingStore::Transaction::commit is currently empty");

    return false;
}

void IDBBackingStore::Transaction::rollback()
{
    LOG_ERROR("The default implementation of IDBBackingStore::Transaction::rollback is currently empty");
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
