/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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
#include "V8DOMStringMap.h"

#include "DOMStringMap.h"
#include "V8Binding.h"
#include "V8DOMWrapper.h"
#include "V8Element.h"

namespace WebCore {

v8::Handle<v8::Integer> V8DOMStringMap::namedPropertyQuery(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.DOMStringMap.NamedPropertyQuery");
    if (V8DOMStringMap::toNative(info.Holder())->contains(toWebCoreString(name)))
        return v8::Integer::New(v8::None);
    return v8::Handle<v8::Integer>();
}

v8::Handle<v8::Value> V8DOMStringMap::namedPropertyGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.DOMStringMap.NamedPropertyGetter");
    String value = V8DOMStringMap::toNative(info.Holder())->item(toWebCoreString(name));
    if (value.isNull())
        return notHandledByInterceptor();
    return v8StringOrUndefined(value);
}

v8::Handle<v8::Array> V8DOMStringMap::namedPropertyEnumerator(const v8::AccessorInfo& info)
{
    INC_STATS("DOM.DOMStringMap.NamedPropertyEnumerator");
    Vector<String> names;
    V8DOMStringMap::toNative(info.Holder())->getNames(names);
    v8::Handle<v8::Array> properties = v8::Array::New(names.size());
    for (size_t i = 0; i < names.size(); ++i)
        properties->Set(v8::Integer::New(i), v8String(names[i]));
    return properties;
}

v8::Handle<v8::Boolean> V8DOMStringMap::namedPropertyDeleter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.DOMStringMap.NamedPropertyDeleter");
    ExceptionCode ec = 0;
    V8DOMStringMap::toNative(info.Holder())->deleteItem(toWebCoreString(name), ec);
    return ec ? v8::False() : v8::True();
}

v8::Handle<v8::Value> V8DOMStringMap::namedPropertySetter(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.DOMStringMap.NamedPropertySetter");
    ExceptionCode ec = 0;
    V8DOMStringMap::toNative(info.Holder())->setItem(toWebCoreString(name), toWebCoreString(value), ec);
    if (ec)
        return throwError(ec);
    return value;
}

} // namespace WebCore
