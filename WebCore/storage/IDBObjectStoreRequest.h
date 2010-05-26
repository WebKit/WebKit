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

#ifndef IDBObjectStoreRequest_h
#define IDBObjectStoreRequest_h

#include "IDBObjectStore.h"
#include "IDBRequest.h"
#include "PlatformString.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

class IDBAny;
class ScriptExecutionContext;
class SerializedScriptValue;

class IDBObjectStoreRequest : public RefCounted<IDBObjectStoreRequest> {
public:
    static PassRefPtr<IDBObjectStoreRequest> create(ScriptExecutionContext* context, PassRefPtr<IDBObjectStore> idbObjectStore)
    {
        return adoptRef(new IDBObjectStoreRequest(context, idbObjectStore));
    }
    ~IDBObjectStoreRequest() { }

    String name() const;
    String keyPath() const;

    PassRefPtr<IDBRequest> get(PassRefPtr<SerializedScriptValue> key);
    PassRefPtr<IDBRequest> add(PassRefPtr<SerializedScriptValue> value, PassRefPtr<SerializedScriptValue> key = 0);
    PassRefPtr<IDBRequest> modify(PassRefPtr<SerializedScriptValue> value, PassRefPtr<SerializedScriptValue> key = 0);
    PassRefPtr<IDBRequest> addOrModify(PassRefPtr<SerializedScriptValue> value, PassRefPtr<SerializedScriptValue> key = 0);
    PassRefPtr<IDBRequest> remove(PassRefPtr<SerializedScriptValue> key);

private:
    IDBObjectStoreRequest(ScriptExecutionContext*, PassRefPtr<IDBObjectStore>);

    RefPtr<IDBObjectStore> m_objectStore;
    RefPtr<ScriptExecutionContext> m_scriptExecutionContext;
    RefPtr<IDBAny> m_this;
};

} // namespace WebCore

#endif

#endif // IDBDatabaseRequest_h

