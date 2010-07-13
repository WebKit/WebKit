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
#include "IDBObjectStoreRequest.h"

#include "DOMStringList.h"
#include "IDBAny.h"
#include "IDBIndexRequest.h"
#include "IDBKey.h"
#include "SerializedScriptValue.h"
#include <wtf/UnusedParam.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

IDBObjectStoreRequest::IDBObjectStoreRequest(PassRefPtr<IDBObjectStore> idbObjectStore)
    : m_objectStore(idbObjectStore)
{
    // We pass a reference to this object before it can be adopted.
    relaxAdoptionRequirement();
}

String IDBObjectStoreRequest::name() const
{
    return m_objectStore->name();
}

String IDBObjectStoreRequest::keyPath() const
{
    return m_objectStore->keyPath();
}

PassRefPtr<DOMStringList> IDBObjectStoreRequest::indexNames() const
{
    return m_objectStore->indexNames();
}

PassRefPtr<IDBRequest> IDBObjectStoreRequest::get(ScriptExecutionContext* context, PassRefPtr<IDBKey> key)
{
    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this));
    m_objectStore->get(key, request);
    return request;
}

PassRefPtr<IDBRequest> IDBObjectStoreRequest::add(ScriptExecutionContext* context, PassRefPtr<SerializedScriptValue> value, PassRefPtr<IDBKey> key)
{
    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this));
    m_objectStore->put(value, key, true, request);
    return request;
}

PassRefPtr<IDBRequest> IDBObjectStoreRequest::put(ScriptExecutionContext* context, PassRefPtr<SerializedScriptValue> value, PassRefPtr<IDBKey> key)
{
    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this));
    m_objectStore->put(value, key, false, request);
    return request;
}

PassRefPtr<IDBRequest> IDBObjectStoreRequest::remove(ScriptExecutionContext* context, PassRefPtr<IDBKey> key)
{
    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this));
    m_objectStore->remove(key, request);
    return request;
}

PassRefPtr<IDBRequest> IDBObjectStoreRequest::createIndex(ScriptExecutionContext* context, const String& name, const String& keyPath, bool unique)
{
    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this));
    m_objectStore->createIndex(name, keyPath, unique, request);
    return request;
}

PassRefPtr<IDBIndexRequest> IDBObjectStoreRequest::index(const String& name)
{
    RefPtr<IDBIndex> index = m_objectStore->index(name);
    ASSERT(index); // FIXME: If this is null, we should raise a NOT_FOUND_ERR.
    return IDBIndexRequest::create(index.release());
}

PassRefPtr<IDBRequest> IDBObjectStoreRequest::removeIndex(ScriptExecutionContext* context, const String& name)
{
    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this));
    m_objectStore->removeIndex(name, request);
    return request;
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
