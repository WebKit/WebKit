/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
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

#ifndef JSArrayBufferViewHelper_h
#define JSArrayBufferViewHelper_h

#include "ArrayBufferView.h"
#include "ExceptionCode.h"
#include "JSArrayBuffer.h"
#include "JSDOMBinding.h"
#include <interpreter/CallFrame.h>
#include <runtime/ArgList.h>
#include <runtime/Error.h>
#include <runtime/JSObject.h>
#include <runtime/JSValue.h>

namespace WebCore {

template <class T>
JSC::JSValue setWebGLArrayHelper(JSC::ExecState* exec, T* impl, T* (*conversionFunc)(JSC::JSValue))
{
    if (exec->argumentCount() < 1)
        return JSC::throwSyntaxError(exec);

    T* array = (*conversionFunc)(exec->argument(0));
    if (array) {
        // void set(in WebGL<T>Array array, [Optional] in unsigned long offset);
        unsigned offset = 0;
        if (exec->argumentCount() == 2)
            offset = exec->argument(1).toInt32(exec);
        ExceptionCode ec = 0;
        impl->set(array, offset, ec);
        setDOMException(exec, ec);
        return JSC::jsUndefined();
    }

    if (exec->argument(0).isObject()) {
        // void set(in sequence<long> array, [Optional] in unsigned long offset);
        JSC::JSObject* array = JSC::asObject(exec->argument(0));
        uint32_t offset = 0;
        if (exec->argumentCount() == 2)
            offset = exec->argument(1).toInt32(exec);
        uint32_t length = array->get(exec, JSC::Identifier(exec, "length")).toInt32(exec);
        if (offset > impl->length()
            || offset + length > impl->length()
            || offset + length < offset)
            setDOMException(exec, INDEX_SIZE_ERR);
        else {
            for (uint32_t i = 0; i < length; i++) {
                JSC::JSValue v = array->get(exec, i);
                if (exec->hadException())
                    return JSC::jsUndefined();
                impl->set(i + offset, v.toNumber(exec));
            }
        }

        return JSC::jsUndefined();
    }

    return JSC::throwSyntaxError(exec);
}

// Template function used by XXXArrayConstructors.
// If this returns 0, it will already have thrown a JavaScript exception.
template<class C, typename T>
PassRefPtr<C> constructArrayBufferViewWithArrayBufferArgument(JSC::ExecState* exec)
{
    RefPtr<ArrayBuffer> buffer = toArrayBuffer(exec->argument(0));
    if (!buffer)
        return 0;

    unsigned offset = (exec->argumentCount() > 1) ? exec->argument(1).toUInt32(exec) : 0;
    if ((buffer->byteLength() - offset) % sizeof(T))
        throwError(exec, createRangeError(exec, "ArrayBuffer length minus the byteOffset is not a multiple of the element size."));
    unsigned int length = (buffer->byteLength() - offset) / sizeof(T);
    if (exec->argumentCount() > 2)
        length = exec->argument(2).toUInt32(exec);
    RefPtr<C> array = C::create(buffer, offset, length);
    if (!array)
        setDOMException(exec, INDEX_SIZE_ERR);
    return array;
}

template<class C, typename T>
PassRefPtr<C> constructArrayBufferView(JSC::ExecState* exec)
{
    // There are 3 constructors:
    //
    //  1) (in int size)
    //  2) (in ArrayBuffer buffer, [Optional] in int offset, [Optional] in unsigned int length)
    //  3) (in sequence<T>) - This ends up being a JS "array-like" object
    //    
    // For the 0 args case, just create a zero-length view. We could
    // consider raising a SyntaxError for this case, but not all
    // JavaScript DOM bindings can distinguish between "new
    // <Type>Array()" and what occurs when a previously-constructed
    // ArrayBufferView is returned to JavaScript; e.g., from
    // "array.slice()".
    if (exec->argumentCount() < 1)
        return C::create(0);
    
    if (exec->argument(0).isNull()) {
        // Invalid first argument
        throwTypeError(exec);
        return 0;
    }

    if (exec->argument(0).isObject()) {
        RefPtr<C> view = constructArrayBufferViewWithArrayBufferArgument<C, T>(exec);
        if (view)
            return view;
    
        JSC::JSObject* array = asObject(exec->argument(0));
        unsigned length = array->get(exec, JSC::Identifier(exec, "length")).toUInt32(exec);
        void* tempValues;
        if (!tryFastCalloc(length, sizeof(T)).getValue(tempValues)) {
            JSC::throwError(exec, createError(exec, "Error"));
            return 0;
        }
        
        OwnFastMallocPtr<T> values(static_cast<T*>(tempValues));
        for (unsigned i = 0; i < length; ++i) {
            JSC::JSValue v = array->get(exec, i);
            if (exec->hadException())
                return 0;
            values.get()[i] = static_cast<T>(v.toNumber(exec));
        }
        
        RefPtr<C> result = C::create(values.get(), length);
        if (!result)
            setDOMException(exec, INDEX_SIZE_ERR);
        return result;
    }

    int length = exec->argument(0).toInt32(exec);
    RefPtr<C> result;
    if (length >= 0)
        result = C::create(static_cast<unsigned>(length));
    if (!result)
        throwError(exec, createRangeError(exec, "ArrayBufferView size is not a small enough positive integer."));
    return result;
}

} // namespace WebCore

#endif // JSArrayBufferViewHelper_h
