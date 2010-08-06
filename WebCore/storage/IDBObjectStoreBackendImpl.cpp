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
#include "IDBObjectStoreBackendImpl.h"

#include "DOMStringList.h"
#include "IDBBindingUtilities.h"
#include "IDBCallbacks.h"
#include "IDBCursorBackendImpl.h"
#include "IDBDatabaseException.h"
#include "IDBIndexBackendImpl.h"
#include "IDBKeyRange.h"
#include "IDBKeyTree.h"

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

IDBObjectStoreBackendImpl::~IDBObjectStoreBackendImpl()
{
}

IDBObjectStoreBackendImpl::IDBObjectStoreBackendImpl(const String& name, const String& keyPath, bool autoIncrement)
    : m_name(name)
    , m_keyPath(keyPath)
    , m_autoIncrement(autoIncrement)
    , m_tree(Tree::create())
{
}

PassRefPtr<DOMStringList> IDBObjectStoreBackendImpl::indexNames() const
{
    RefPtr<DOMStringList> indexNames = DOMStringList::create();
    for (IndexMap::const_iterator it = m_indexes.begin(); it != m_indexes.end(); ++it)
        indexNames->append(it->first);
    return indexNames.release();
}

void IDBObjectStoreBackendImpl::get(PassRefPtr<IDBKey> key, PassRefPtr<IDBCallbacks> callbacks)
{
    RefPtr<SerializedScriptValue> value = m_tree->get(key.get());
    if (!value) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::NOT_FOUND_ERR, "Key does not exist in the object store."));
        return;
    }
    callbacks->onSuccess(value.get());
}

void IDBObjectStoreBackendImpl::put(PassRefPtr<SerializedScriptValue> value, PassRefPtr<IDBKey> prpKey, bool addOnly, PassRefPtr<IDBCallbacks> callbacks)
{
    RefPtr<IDBKey> key = prpKey;

    if (!m_keyPath.isNull()) {
        if (key) {
            callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UNKNOWN_ERR, "A key was supplied for an objectStore that has a keyPath.")); 
            return;
        }
        ASSERT_NOT_REACHED();
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UNKNOWN_ERR, "FIXME: keyPath not yet supported."));
        return;
    }

    if (!key) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::DATA_ERR, "No key supplied."));
        return;
    }

    if (addOnly && m_tree->get(key.get())) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::CONSTRAINT_ERR, "Key already exists in the object store."));
        return;
    }

    m_tree->put(key.get(), value.get());
    callbacks->onSuccess(key.get());
}

void IDBObjectStoreBackendImpl::remove(PassRefPtr<IDBKey> key, PassRefPtr<IDBCallbacks> callbacks)
{
    m_tree->remove(key.get());
    callbacks->onSuccess();
}

void IDBObjectStoreBackendImpl::createIndex(const String& name, const String& keyPath, bool unique, PassRefPtr<IDBCallbacks> callbacks)
{
    if (m_indexes.contains(name)) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::CONSTRAINT_ERR, "Index name already exists."));
        return;
    }

    RefPtr<IDBIndexBackendInterface> index = IDBIndexBackendImpl::create(name, keyPath, unique);
    ASSERT(index->name() == name);
    m_indexes.set(name, index);
    callbacks->onSuccess(index.release());
}

PassRefPtr<IDBIndexBackendInterface> IDBObjectStoreBackendImpl::index(const String& name)
{
    return m_indexes.get(name);
}

void IDBObjectStoreBackendImpl::removeIndex(const String& name, PassRefPtr<IDBCallbacks> callbacks)
{
    if (!m_indexes.contains(name)) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::NOT_FOUND_ERR, "Index name does not exist."));
        return;
    }

    m_indexes.remove(name);
    callbacks->onSuccess();
}

void IDBObjectStoreBackendImpl::openCursor(PassRefPtr<IDBKeyRange> range, unsigned short direction, PassRefPtr<IDBCallbacks> callbacks)
{
    RefPtr<IDBKey> key = range->left();
    RefPtr<SerializedScriptValue> value = m_tree->get(key.get());
    if (value) {
        RefPtr<IDBCursorBackendInterface> cursor = IDBCursorBackendImpl::create(this, range, static_cast<IDBCursor::Direction>(direction), key, value);
        callbacks->onSuccess(cursor.release());
    } else
      callbacks->onSuccess();
}

} // namespace WebCore

#endif
