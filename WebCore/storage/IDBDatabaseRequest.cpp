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
#include "IDBDatabaseRequest.h"

#include "IDBAny.h"
#include "IDBObjectStoreRequest.h"
#include "IDBRequest.h"
#include "IndexedDatabase.h"
#include "ScriptExecutionContext.h"

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

IDBDatabaseRequest::IDBDatabaseRequest(PassRefPtr<IDBDatabase> database)
    : m_database(database)
{
    m_this = IDBAny::create();
    // We pass a reference to this object before it can be adopted.
    relaxAdoptionRequirement();
    m_this->set(this);
}

IDBDatabaseRequest::~IDBDatabaseRequest()
{
}

PassRefPtr<IDBRequest> IDBDatabaseRequest::createObjectStore(ScriptExecutionContext* context, const String& name, const String& keyPath, bool autoIncrement)
{
    RefPtr<IDBRequest> request = IDBRequest::create(context, m_this);
    m_database->createObjectStore(name, keyPath, autoIncrement, request);
    return request;
}

PassRefPtr<IDBObjectStoreRequest> IDBDatabaseRequest::objectStore(const String& name, unsigned short mode)
{
    RefPtr<IDBObjectStore> objectStore = m_database->objectStore(name, mode);
    ASSERT(objectStore); // FIXME: If this is null, we should raise a NOT_FOUND_ERR.
    return IDBObjectStoreRequest::create(objectStore.release());
}

PassRefPtr<IDBRequest> IDBDatabaseRequest::removeObjectStore(ScriptExecutionContext* context, const String& name)
{
    RefPtr<IDBRequest> request = IDBRequest::create(context, m_this);
    m_database->removeObjectStore(name, request);
    return request;
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
