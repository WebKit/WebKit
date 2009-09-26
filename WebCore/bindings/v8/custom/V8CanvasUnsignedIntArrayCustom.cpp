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

#include "CanvasArrayBuffer.h"
#include "CanvasUnsignedIntArray.h"

#include "V8Binding.h"
#include "V8CanvasArrayBuffer.h"
#include "V8CanvasArrayCustom.h"
#include "V8CustomBinding.h"
#include "V8Proxy.h"

namespace WebCore {

CALLBACK_FUNC_DECL(CanvasUnsignedIntArrayConstructor)
{
    INC_STATS("DOM.CanvasUnsignedIntArray.Contructor");

    return constructCanvasArray<CanvasUnsignedIntArray>(args, V8ClassIndex::ToInt(V8ClassIndex::CANVASUNSIGNEDINTARRAY));
}

// Get the specified value from the integer array and return it wrapped as a JavaScript Number object to V8. Accesses outside the valid pixel buffer range return "undefined".
INDEXED_PROPERTY_GETTER(CanvasUnsignedIntArray)
{
    INC_STATS("DOM.CanvasUnsignedIntArray.IndexedPropertyGetter");
    CanvasUnsignedIntArray* array = V8DOMWrapper::convertToNativeObject<CanvasUnsignedIntArray>(V8ClassIndex::CANVASUNSIGNEDINTARRAY, info.Holder());

    if ((index < 0) || (index >= array->length()))
        return v8::Undefined();
    unsigned int result;
    if (!array->get(index, result))
        return v8::Undefined();
    return v8::Number::New(result);
}

// Set the specified value in the integer array. Accesses outside the valid integer array range are silently ignored.
INDEXED_PROPERTY_SETTER(CanvasUnsignedIntArray)
{
    INC_STATS("DOM.CanvasUnsignedIntArray.IndexedPropertySetter");
    CanvasUnsignedIntArray* array = V8DOMWrapper::convertToNativeObject<CanvasUnsignedIntArray>(V8ClassIndex::CANVASUNSIGNEDINTARRAY, info.Holder());

    if ((index >= 0) && (index < array->length())) {
        if (!value->IsNumber())
            return throwError("Could not convert value argument to a number");
        array->set(index, value->NumberValue());
    }
    return value;
}

} // namespace WebCore

#endif // ENABLE(3D_CANVAS)
