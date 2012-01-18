/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "V8Internals.h"

#include "V8Binding.h"
#include "V8Proxy.h"

namespace WebCore {

v8::Handle<v8::Value> V8Internals::userPreferredLanguagesAccessorGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    Internals* internals = V8Internals::toNative(info.Holder());
    const Vector<String> languages = internals->userPreferredLanguages();
    if (languages.isEmpty())
        return v8::Null();

    v8::Local<v8::Array> array = v8::Array::New(languages.size());
    Vector<String>::const_iterator end = languages.end();
    int index = 0;
    for (Vector<String>::const_iterator iter = languages.begin(); iter != end; ++iter)
        array->Set(v8::Integer::New(index++), v8String(*iter));
    return array;
}

void V8Internals::userPreferredLanguagesAccessorSetter(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
    if (!value->IsArray()) {
        throwError("setUserPreferredLanguages: Expected Array");
        return;
    }
    
    Vector<String> languages;

    v8::Local<v8::Array> array = v8::Local<v8::Array>::Cast(value);
    for (size_t i = 0; i < array->Length(); ++i)
        languages.append(toWebCoreString(array->Get(i)));
    
    Internals* internals = V8Internals::toNative(info.Holder());
    internals->setUserPreferredLanguages(languages);
}
    
} // namespace WebCore
