/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#if ENABLE(3D_CANVAS)

#include "WebGLArrayBuffer.h"
#include "WebGLUnsignedShortArray.h"

#include "V8Binding.h"
#include "V8WebGLArrayBuffer.h"
#include "V8WebGLArrayCustom.h"
#include "V8CustomBinding.h"
#include "V8Proxy.h"

namespace WebCore {

CALLBACK_FUNC_DECL(WebGLUnsignedShortArrayConstructor)
{
    INC_STATS("DOM.WebGLUnsignedShortArray.Contructor");

    return constructCanvasArray<WebGLUnsignedShortArray>(args, V8ClassIndex::ToInt(V8ClassIndex::CANVASUNSIGNEDSHORTARRAY));
}

// Get the specified value from the array and return it wrapped as a JavaScript Number object to V8. Accesses outside the valid array range return "undefined".
INDEXED_PROPERTY_GETTER(WebGLUnsignedShortArray)
{
    INC_STATS("DOM.WebGLUnsignedShortArray.IndexedPropertyGetter");
    WebGLUnsignedShortArray* array = V8DOMWrapper::convertToNativeObject<WebGLUnsignedShortArray>(V8ClassIndex::CANVASUNSIGNEDSHORTARRAY, info.Holder());

    if ((index < 0) || (index >= array->length()))
        return v8::Undefined();
    unsigned short result;
    if (!array->get(index, result))
        return v8::Undefined();
    return v8::Number::New(result);
}

// Set the specified value in the array. Accesses outside the valid array range are silently ignored.
INDEXED_PROPERTY_SETTER(WebGLUnsignedShortArray)
{
    INC_STATS("DOM.WebGLUnsignedShortArray.IndexedPropertySetter");
    WebGLUnsignedShortArray* array = V8DOMWrapper::convertToNativeObject<WebGLUnsignedShortArray>(V8ClassIndex::CANVASUNSIGNEDSHORTARRAY, info.Holder());

    if ((index >= 0) && (index < array->length())) {
        if (!value->IsNumber())
            return throwError("Could not convert value argument to a number");
        array->set(index, value->NumberValue());
    }
    return value;
}

} // namespace WebCore

#endif // ENABLE(3D_CANVAS)
