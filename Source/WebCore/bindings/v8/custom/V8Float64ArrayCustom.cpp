/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "V8Float64Array.h"

#include "V8ArrayBuffer.h"
#include "V8ArrayBufferViewCustom.h"
#include "V8Binding.h"
#include "V8Proxy.h"

#include <wtf/ArrayBuffer.h>
#include <wtf/Float64Array.h>

namespace WebCore {

v8::Handle<v8::Value> V8Float64Array::constructorCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.Float64Array.Contructor");

    return constructWebGLArray<Float64Array, double>(args, &info, v8::kExternalDoubleArray);
}

v8::Handle<v8::Value> V8Float64Array::setCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.Float64Array.set()");
    return setWebGLArrayHelper<Float64Array, V8Float64Array>(args);
}

v8::Handle<v8::Value> toV8(Float64Array* impl, v8::Isolate* isolate)
{
    if (!impl)
        return v8::Null();
    v8::Handle<v8::Object> wrapper = V8Float64Array::wrap(impl, isolate);
    if (!wrapper.IsEmpty())
        wrapper->SetIndexedPropertiesToExternalArrayData(impl->baseAddress(), v8::kExternalDoubleArray, impl->length());
    return wrapper;
}

} // namespace WebCore
