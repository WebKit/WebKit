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

#include "V8Binding.h"
#include "V8WebGLArrayBuffer.h"
#include "V8Proxy.h"

namespace WebCore {

// Template function used by the WebGLArray*Constructor callbacks.
template<class ArrayClass>
v8::Handle<v8::Value> constructWebGLArray(const v8::Arguments& args,
                                          int classIndex)
{
    if (!args.IsConstructCall())
        return throwError("DOM object constructor cannot be called as a function.");

    int argLen = args.Length();
    if (argLen == 0) {
        // This happens when we return a previously constructed
        // WebGLArray, e.g. from the call to WebGL<T>Array.slice().
        // The V8DOMWrapper will set the internal pointer in the
        // created object. Unfortunately it doesn't look like it's
        // possible to distinguish between this case and that where
        // the user calls "new WebGL<T>Array()" from JavaScript.
        return args.Holder();
    }

    // Supported constructors:
    // WebGL<T>Array(n) where n is an integer:
    //   -- create an empty array of n elements
    // WebGL<T>Array(arr) where arr is an array:
    //   -- create a WebGL<T>Array containing the contents of "arr"
    // WebGL<T>Array(buf, offset, length)
    //   -- create a WebGL<T>Array pointing to the WebGLArrayBuffer
    //      "buf", starting at the specified offset, for the given
    //      length

    // See whether the first argument is a WebGLArrayBuffer.
    if (V8WebGLArrayBuffer::HasInstance(args[0])) {
        if (argLen > 3)
            return throwError("Wrong number of arguments to new WebGL<T>Array(WebGLArrayBuffer, int, int)");

        WebGLArrayBuffer* buf = V8WebGLArrayBuffer::toNative(args[0]->ToObject());
        if (buf == NULL)
            return throwError("Could not convert argument 0 to a WebGLArrayBuffer");
        bool ok;
        int offset = 0;
        if (argLen > 1) {
            offset = toInt32(args[1], ok);
            if (!ok)
                return throwError("Could not convert argument 1 to an integer");
        }
        int length = buf->byteLength() - offset;
        if (argLen > 2) {
            length = toInt32(args[2], ok);
            if (!ok)
                return throwError("Could not convert argument 2 to an integer");
        }
        if (length < 0)
            return throwError("Length / offset out of range");

        RefPtr<ArrayClass> array = ArrayClass::create(buf, offset, length);
        if (array == NULL)
            return throwError("Invalid arguments to new WebGL<T>Array(WebGLArrayBuffer, int, int)");
        // Transform the holder into a wrapper object for the array.
        V8DOMWrapper::setDOMWrapper(args.Holder(), classIndex, array.get());
        V8DOMWrapper::setIndexedPropertiesToExternalArray(args.Holder(),
                                                          classIndex,
                                                          array.get()->baseAddress(),
                                                          array.get()->length());
        return toV8(array.release(), args.Holder());
    }

    int len = 0;
    v8::Handle<v8::Object> srcArray;
    if (argLen != 1)
        return throwError("Wrong number of arguments to new WebGL<T>Array(int / array)");

    if (args[0]->IsInt32()) {
        len = toInt32(args[0]);
    } else if (args[0]->IsObject()) {
        srcArray = args[0]->ToObject();
        if (srcArray.IsEmpty())
            return throwError("Could not convert argument 0 to an object");
        len = toInt32(srcArray->Get(v8::String::New("length")));
    } else
        return throwError("Could not convert argument 0 to either an int32 or an object");

    RefPtr<ArrayClass> array = ArrayClass::create(len);
    if (!srcArray.IsEmpty()) {
        // Need to copy the incoming array into the newly created WebGLArray.
        for (int i = 0; i < len; i++) {
            v8::Local<v8::Value> val = srcArray->Get(v8::Integer::New(i));
            array->set(i, val->NumberValue());
        }
    }

    // Transform the holder into a wrapper object for the array.
    V8DOMWrapper::setDOMWrapper(args.Holder(), classIndex, array.get());
    V8DOMWrapper::setIndexedPropertiesToExternalArray(args.Holder(),
                                                      classIndex,
                                                      array.get()->baseAddress(),
                                                      array.get()->length());
    return toV8(array.release(), args.Holder());
}

template <class T, typename ElementType>
v8::Handle<v8::Value> getWebGLArrayElement(const v8::Arguments& args,
                                           V8ClassIndex::V8WrapperType wrapperType)
{
    if (args.Length() != 1) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }
    bool ok;
    uint32_t index = toInt32(args[0], ok);
    if (!ok) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }
    T* array = reinterpret_cast<T*>(args.Holder()->GetPointerFromInternalField(v8DOMWrapperObjectIndex));
    if (index >= array->length())
        return v8::Undefined();
    ElementType result;
    if (!array->get(index, result))
        return v8::Undefined();
    return v8::Number::New(result);
}

template <class T>
v8::Handle<v8::Value> setWebGLArrayFromArray(T* webGLArray, const v8::Arguments& args)
{
    if (args[0]->IsObject()) {
        // void set(in sequence<long> array, [Optional] in unsigned long offset);
        v8::Local<v8::Object> array = args[0]->ToObject();
        uint32_t offset = 0;
        if (args.Length() == 2)
            offset = toInt32(args[1]);
        uint32_t length = toInt32(array->Get(v8::String::New("length")));
        if (offset + length > webGLArray->length())
            V8Proxy::setDOMException(INDEX_SIZE_ERR);
        else
            for (uint32_t i = 0; i < length; i++)
                webGLArray->set(offset + i, array->Get(v8::Integer::New(i))->NumberValue());
    }

    return v8::Undefined();
}

template <class CPlusPlusArrayType, class JavaScriptWrapperArrayType>
v8::Handle<v8::Value> setWebGLArray(const v8::Arguments& args,
                                    V8ClassIndex::V8WrapperType wrapperType)
{
    if (args.Length() < 1 || args.Length() > 2) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }

    CPlusPlusArrayType* array = JavaScriptWrapperArrayType::toNative(args.Holder());

    if (args.Length() == 2 && args[0]->IsInt32()) {
        // void set(in unsigned long index, in {long|float} value);
        uint32_t index = toInt32(args[0]);
        array->set(index, args[1]->NumberValue());
        return v8::Undefined();
    }

    if (JavaScriptWrapperArrayType::HasInstance(args[0])) {
        // void set(in WebGL<T>Array array, [Optional] in unsigned long offset);
        CPlusPlusArrayType* src = JavaScriptWrapperArrayType::toNative(args[0]->ToObject());
        uint32_t offset = 0;
        if (args.Length() == 2)
            offset = toInt32(args[1]);
        ExceptionCode ec = 0;
        array->set(src, offset, ec);
        V8Proxy::setDOMException(ec);
        return v8::Undefined();
    }

    return setWebGLArrayFromArray(array, args);
}

}

#endif // ENABLE(3D_CANVAS)
