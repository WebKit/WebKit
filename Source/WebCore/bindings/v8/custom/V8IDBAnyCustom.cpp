/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(INDEXED_DATABASE)

#include "V8IDBAny.h"

#include "ScriptValue.h"
#include "V8Binding.h"
#include "V8DOMStringList.h"
#include "V8IDBCursor.h"
#include "V8IDBCursorWithValue.h"
#include "V8IDBDatabase.h"
#include "V8IDBFactory.h"
#include "V8IDBIndex.h"
#include "V8IDBObjectStore.h"
#include "V8IDBTransaction.h"

namespace WebCore {

static v8::Handle<v8::Value> toV8(const IDBKeyPath& value, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    switch (value.type()) {
    case IDBKeyPath::NullType:
        return v8NullWithCheck(isolate);
    case IDBKeyPath::StringType:
        return v8String(value.string(), isolate);
    case IDBKeyPath::ArrayType:
        RefPtr<DOMStringList> keyPaths = DOMStringList::create();
        for (Vector<String>::const_iterator it = value.array().begin(); it != value.array().end(); ++it)
            keyPaths->append(*it);
        return toV8(keyPaths.release(), creationContext, isolate);
    }
    ASSERT_NOT_REACHED();
    return v8::Undefined();
}

v8::Handle<v8::Value> toV8(IDBAny* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    if (!impl)
        return v8NullWithCheck(isolate);

    switch (impl->type()) {
    case IDBAny::UndefinedType:
        return v8::Undefined();
    case IDBAny::NullType:
        return v8NullWithCheck(isolate);
    case IDBAny::DOMStringListType:
        return toV8(impl->domStringList(), creationContext, isolate);
    case IDBAny::IDBCursorType:
        return toV8(impl->idbCursor(), creationContext, isolate);
    case IDBAny::IDBCursorWithValueType:
        return toV8(impl->idbCursorWithValue(), creationContext, isolate);
    case IDBAny::IDBDatabaseType:
        return toV8(impl->idbDatabase(), creationContext, isolate);
    case IDBAny::IDBFactoryType:
        return toV8(impl->idbFactory(), creationContext, isolate);
    case IDBAny::IDBIndexType:
        return toV8(impl->idbIndex(), creationContext, isolate);
    case IDBAny::IDBObjectStoreType:
        return toV8(impl->idbObjectStore(), creationContext, isolate);
    case IDBAny::IDBTransactionType:
        return toV8(impl->idbTransaction(), creationContext, isolate);
    case IDBAny::ScriptValueType:
        return impl->scriptValue().v8Value();
    case IDBAny::StringType:
        return v8String(impl->string(), isolate);
    case IDBAny::IntegerType:
        return v8::Number::New(impl->integer());
    case IDBAny::KeyPathType:
        return toV8(impl->keyPath(), creationContext, isolate);
    }

    ASSERT_NOT_REACHED();
    return v8::Undefined();
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
