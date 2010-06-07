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
#include "IDBObjectStoreImpl.h"

#include "DOMStringList.h"
#include "IDBCallbacks.h"
#include "IDBDatabaseException.h"
#include "IDBIndexImpl.h"

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

IDBObjectStoreImpl::~IDBObjectStoreImpl()
{
}

IDBObjectStoreImpl::IDBObjectStoreImpl(const String& name, const String& keyPath, bool autoIncrement)
    : m_name(name)
    , m_keyPath(keyPath)
    , m_autoIncrement(autoIncrement)
{
}

PassRefPtr<DOMStringList> IDBObjectStoreImpl::indexNames() const
{
    RefPtr<DOMStringList> indexNames = DOMStringList::create();
    for (IndexMap::const_iterator it = m_indexes.begin(); it != m_indexes.end(); ++it)
        indexNames->append(it->first);
    return indexNames.release();
}

void IDBObjectStoreImpl::createIndex(const String& name, const String& keyPath, bool unique, PassRefPtr<IDBCallbacks> callbacks)
{
    if (m_indexes.contains(name)) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::CONSTRAINT_ERR, "Index name already exists."));
        return;
    }

    RefPtr<IDBIndex> index = IDBIndexImpl::create(name, keyPath, unique);
    ASSERT(index->name() == name);
    m_indexes.set(name, index);
    callbacks->onSuccess(index.release());
}

PassRefPtr<IDBIndex> IDBObjectStoreImpl::index(const String& name)
{
    return m_indexes.get(name);
}

void IDBObjectStoreImpl::removeIndex(const String& name, PassRefPtr<IDBCallbacks> callbacks)
{
    if (!m_indexes.contains(name)) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::NOT_FOUND_ERR, "Index name does not exist."));
        return;
    }

    m_indexes.remove(name);
    callbacks->onSuccess();
}


} // namespace WebCore

#endif
