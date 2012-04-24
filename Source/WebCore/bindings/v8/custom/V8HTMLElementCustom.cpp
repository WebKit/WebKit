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
#include "V8HTMLElement.h"

#include "V8HTMLElementWrapperFactory.h"

#if ENABLE(MICRODATA)
#include "MicroDataItemValue.h"
#include "V8Binding.h"
#endif

namespace WebCore {

#if ENABLE(MICRODATA)
static v8::Handle<v8::Value> toV8Object(MicroDataItemValue* itemValue, v8::Isolate* isolate)
{
    if (!itemValue)
        return v8::Null();

    if (itemValue->isNode())
        return toV8(itemValue->getNode(), isolate);

    return v8String(itemValue->getString());
}
#endif

v8::Handle<v8::Value> toV8(HTMLElement* impl, v8::Isolate* isolate, bool forceNewObject)
{
    if (!impl)
        return v8::Null();
    return createV8HTMLWrapper(impl, forceNewObject);
}

#if ENABLE(MICRODATA)
v8::Handle<v8::Value> V8HTMLElement::itemValueAccessorGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    HTMLElement* impl = V8HTMLElement::toNative(info.Holder());
    return toV8Object(impl->itemValue().get(), info.GetIsolate());
}

void V8HTMLElement::itemValueAccessorSetter(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
    HTMLElement* impl = V8HTMLElement::toNative(info.Holder());
    ExceptionCode ec = 0;
    impl->setItemValue(toWebCoreString(value), ec);
    if (ec)
        V8Proxy::setDOMException(ec, info.GetIsolate());
}
#endif

} // namespace WebCore
