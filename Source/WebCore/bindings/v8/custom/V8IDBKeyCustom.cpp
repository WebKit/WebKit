/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "IDBBindingUtilities.h"
#include "IDBKey.h"
#include "V8Binding.h"

#include <wtf/Vector.h>

namespace WebCore {

v8::Handle<v8::Value> toV8(IDBKey* key, v8::Isolate* isolate)
{
    if (!key)
        return v8::Null();

    switch (key->type()) {
    case IDBKey::InvalidType:
    case IDBKey::MinType:
        ASSERT_NOT_REACHED();
        return v8::Undefined();
    case IDBKey::NumberType:
        return v8::Number::New(key->number());
    case IDBKey::StringType:
        return v8String(key->string());
    case IDBKey::DateType:
        return v8::Date::New(key->date());
    case IDBKey::ArrayType:
        {
            v8::Local<v8::Array> array = v8::Array::New(key->array().size());
            for (size_t i = 0; i < key->array().size(); ++i)
                array->Set(i, toV8(key->array()[i].get(), isolate));
            return array;
        }
    }

    ASSERT_NOT_REACHED();
    return v8::Undefined();
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
