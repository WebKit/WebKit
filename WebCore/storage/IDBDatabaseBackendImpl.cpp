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
#include "IDBDatabaseBackendImpl.h"

#include "DOMStringList.h"
#include "IDBDatabaseException.h"
#include "IDBObjectStoreImpl.h"

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

IDBDatabaseBackendImpl::IDBDatabaseBackendImpl(const String& name, const String& description, const String& version)
    : m_name(name)
    , m_description(description)
    , m_version(version)
{
}

IDBDatabaseBackendImpl::~IDBDatabaseBackendImpl()
{
}

PassRefPtr<DOMStringList> IDBDatabaseBackendImpl::objectStores() const
{
    RefPtr<DOMStringList> objectStoreNames = DOMStringList::create();
    for (ObjectStoreMap::const_iterator it = m_objectStores.begin(); it != m_objectStores.end(); ++it)
        objectStoreNames->append(it->first);
    return objectStoreNames.release();
}

void IDBDatabaseBackendImpl::createObjectStore(const String& name, const String& keyPath, bool autoIncrement, PassRefPtr<IDBCallbacks> callbacks)
{
    if (m_objectStores.contains(name)) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::CONSTRAINT_ERR, "An objectStore with that name already exists."));
        return;
    }

    RefPtr<IDBObjectStore> objectStore = IDBObjectStoreImpl::create(name, keyPath, autoIncrement);
    m_objectStores.set(name, objectStore);
    callbacks->onSuccess(objectStore.release());
}

PassRefPtr<IDBObjectStore> IDBDatabaseBackendImpl::objectStore(const String& name, unsigned short mode)
{
    // FIXME: If no transaction is running, this should implicitly start one.
    ASSERT_UNUSED(mode, !mode); // FIXME: Handle non-standard modes.
    return m_objectStores.get(name);
}

void IDBDatabaseBackendImpl::removeObjectStore(const String& name, PassRefPtr<IDBCallbacks> callbacks)
{
    if (!m_objectStores.contains(name)) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::NOT_FOUND_ERR, "No objectStore with that name exists."));
        return;
    }

    m_objectStores.remove(name);
    callbacks->onSuccess();
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
