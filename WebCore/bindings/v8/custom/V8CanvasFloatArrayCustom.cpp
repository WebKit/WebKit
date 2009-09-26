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
#include "CanvasFloatArray.h"

#include "V8Binding.h"
#include "V8CanvasArrayBuffer.h"
#include "V8CanvasArrayCustom.h"
#include "V8CustomBinding.h"
#include "V8Proxy.h"

namespace WebCore {

CALLBACK_FUNC_DECL(CanvasFloatArrayConstructor)
{
    INC_STATS("DOM.CanvasFloatArray.Contructor");

    return constructCanvasArray<CanvasFloatArray>(args, V8ClassIndex::ToInt(V8ClassIndex::CANVASFLOATARRAY));
}

// Get the specified value from the array and return it wrapped as a JavaScript Number object to V8. Accesses outside the valid array range return "undefined".
INDEXED_PROPERTY_GETTER(CanvasFloatArray)
{
    INC_STATS("DOM.CanvasFloatArray.IndexedPropertyGetter");
    CanvasFloatArray* array = V8DOMWrapper::convertToNativeObject<CanvasFloatArray>(V8ClassIndex::CANVASFLOATARRAY, info.Holder());

    if ((index < 0) || (index >= array->length()))
        return v8::Undefined();
    float result;
    if (!array->get(index, result))
        return v8::Undefined();
    return v8::Number::New(result);
}

// Set the specified value in the array. Accesses outside the valid array range are silently ignored.
INDEXED_PROPERTY_SETTER(CanvasFloatArray)
{
    INC_STATS("DOM.CanvasFloatArray.IndexedPropertySetter");
    CanvasFloatArray* array = V8DOMWrapper::convertToNativeObject<CanvasFloatArray>(V8ClassIndex::CANVASFLOATARRAY, info.Holder());

    if ((index >= 0) && (index < array->length()))
        array->set(index, value->NumberValue());
    return value;
}

} // namespace WebCore

#endif // ENABLE(3D_CANVAS)
