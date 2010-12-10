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
#include "IDBBindingUtilities.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBDatabaseException.h"
#include "IDBKey.h"
#include "IDBKeyPath.h"
#include "SerializedScriptValue.h"
#include "V8Binding.h"
#include <wtf/Vector.h>

namespace WebCore {

PassRefPtr<IDBKey> createIDBKeyFromValue(v8::Handle<v8::Value> value)
{
    if (value->IsNull())
        return IDBKey::create();
    if (value->IsNumber())
        return IDBKey::create(value->NumberValue());
    if (value->IsString())
        return IDBKey::create(v8ValueToWebCoreString(value));
    if (value->IsDate())
        return 0; // Signals type error. FIXME: Implement dates.

    return 0; // Signals type error.
}

template<typename T>
bool getValueFrom(T indexOrName, v8::Handle<v8::Value>& v8Value)
{
    v8::Local<v8::Object> object = v8Value->ToObject();
    if (!object->Has(indexOrName))
        return false;
    v8Value = object->Get(indexOrName);
    return true;
}

class LocalContext {
public:
    LocalContext()
        : m_context(v8::Context::New())
    {
        m_context->Enter();
    }

    ~LocalContext()
    {
        m_context->Exit();
        m_context.Dispose();
    }

private:
    v8::HandleScope m_scope;
    v8::Persistent<v8::Context> m_context;
};

PassRefPtr<IDBKey> createIDBKeyFromSerializedValueAndKeyPath(PassRefPtr<SerializedScriptValue> value, const Vector<IDBKeyPathElement>& keyPath)
{
    LocalContext localContext;
    v8::Handle<v8::Value> v8Value(value->deserialize());
    for (size_t i = 0; i < keyPath.size(); ++i) {
        switch (keyPath[i].type) {
        case IDBKeyPathElement::IsIndexed:
            if (!v8Value->IsArray() || !getValueFrom(keyPath[i].index, v8Value))
                return 0;
            break;
        case IDBKeyPathElement::IsNamed:
            if (!v8Value->IsObject() || !getValueFrom(v8String(keyPath[i].identifier), v8Value))
                return 0;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    }
    return createIDBKeyFromValue(v8Value);
}

} // namespace WebCore

#endif
