/*
 * Copyright (C) 2009, 2011 Google Inc. All rights reserved.
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

#ifndef V8ArrayBufferViewCustom_h
#define V8ArrayBufferViewCustom_h

#include "ArrayBuffer.h"
#include "ExceptionCode.h"

#include "V8ArrayBuffer.h"
#include "V8Binding.h"
#include "V8Proxy.h"

namespace WebCore {


// Check if the JavaScript 'set' method was already installed
// on the prototype of the given typed array.
bool fastSetInstalled(v8::Handle<v8::Object> array);

// Install the JavaScript 'set' method on the prototype of
// the given typed array.
void installFastSet(v8::Handle<v8::Object> array);

// Copy the elements from the source array to the typed destination array by
// invoking the 'set' method of the destination array in JS.
void copyElements(v8::Handle<v8::Object> destArray, v8::Handle<v8::Object> srcArray, uint32_t offset);


// Template function used by the ArrayBufferView*Constructor callbacks.
template<class ArrayClass, class ElementType>
v8::Handle<v8::Value> constructWebGLArrayWithArrayBufferArgument(const v8::Arguments& args, WrapperTypeInfo* type, v8::ExternalArrayType arrayType, bool hasIndexer)
{
    ArrayBuffer* buf = V8ArrayBuffer::toNative(args[0]->ToObject());
    if (!buf)
        return throwError("Could not convert argument 0 to a ArrayBuffer");
    bool ok;
    uint32_t offset = 0;
    int argLen = args.Length();
    if (argLen > 1) {
        offset = toUInt32(args[1], ok);
        if (!ok)
            return throwError("Could not convert argument 1 to a number");
    }
    uint32_t length = 0;
    if (argLen > 2) {
        length = toUInt32(args[2], ok);
        if (!ok)
            return throwError("Could not convert argument 2 to a number");
    } else {
        if ((buf->byteLength() - offset) % sizeof(ElementType))
            return throwError("ArrayBuffer length minus the byteOffset is not a multiple of the element size.", V8Proxy::RangeError);
        length = (buf->byteLength() - offset) / sizeof(ElementType);
    }
    RefPtr<ArrayClass> array = ArrayClass::create(buf, offset, length);
    if (!array) {
        V8Proxy::setDOMException(INDEX_SIZE_ERR);
        return notHandledByInterceptor();
    }
    // Transform the holder into a wrapper object for the array.
    V8DOMWrapper::setDOMWrapper(args.Holder(), type, array.get());
    if (hasIndexer)
        args.Holder()->SetIndexedPropertiesToExternalArrayData(array.get()->baseAddress(), arrayType, array.get()->length());
    return toV8(array.release(), args.Holder(), MarkIndependent);
}

// Template function used by the ArrayBufferView*Constructor callbacks.
template<class ArrayClass, class ElementType>
v8::Handle<v8::Value> constructWebGLArray(const v8::Arguments& args, WrapperTypeInfo* type, v8::ExternalArrayType arrayType)
{
    if (!args.IsConstructCall())
        return throwError("DOM object constructor cannot be called as a function.", V8Proxy::TypeError);

    if (ConstructorMode::current() == ConstructorMode::WrapExistingObject)
        return args.Holder();

    int argLen = args.Length();
    if (!argLen) {
        // This happens when we return a previously constructed
        // ArrayBufferView, e.g. from the call to <Type>Array.subset().
        // The V8DOMWrapper will set the internal pointer in the
        // created object. Unfortunately it doesn't look like it's
        // possible to distinguish between this case and that where
        // the user calls "new <Type>Array()" from JavaScript. We must
        // construct an empty view to avoid crashes when fetching the
        // length.
        RefPtr<ArrayClass> array = ArrayClass::create(0);
        // Transform the holder into a wrapper object for the array.
        V8DOMWrapper::setDOMWrapper(args.Holder(), type, array.get());
        // Do not call SetIndexedPropertiesToExternalArrayData on this
        // object. Not only is there no point from a performance
        // perspective, but doing so causes errors in the subset() case.
        return toV8(array.release(), args.Holder(), MarkIndependent);
    }

    // Supported constructors:
    // WebGL<T>Array(n) where n is an integer:
    //   -- create an empty array of n elements
    // WebGL<T>Array(arr) where arr is an array:
    //   -- create a WebGL<T>Array containing the contents of "arr"
    // WebGL<T>Array(buf, offset, length)
    //   -- create a WebGL<T>Array pointing to the ArrayBuffer
    //      "buf", starting at the specified offset, for the given
    //      length

    if (args[0]->IsNull()) {
        // Invalid first argument
        // FIXME: use forthcoming V8Proxy::throwTypeError().
        return V8Proxy::throwError(V8Proxy::TypeError, "Type error");
    }

    // See whether the first argument is a ArrayBuffer.
    if (V8ArrayBuffer::HasInstance(args[0]))
      return constructWebGLArrayWithArrayBufferArgument<ArrayClass, ElementType>(args, type, arrayType, true);

    uint32_t len = 0;
    v8::Handle<v8::Object> srcArray;
    bool doInstantiation = false;

    if (args[0]->IsObject()) {
        srcArray = args[0]->ToObject();
        if (srcArray.IsEmpty())
            return throwError("Could not convert argument 0 to an array");
        len = toUInt32(srcArray->Get(v8::String::New("length")));
        doInstantiation = true;
    } else {
        bool ok = false;
        int32_t tempLength = toInt32(args[0], ok); // NaN/+inf/-inf returns 0, this is intended by WebIDL
        if (ok && tempLength >= 0) {
            len = static_cast<uint32_t>(tempLength);
            doInstantiation = true;
        }
    }

    RefPtr<ArrayClass> array;
    if (doInstantiation)
        array = ArrayClass::create(len);
    if (!array.get())
        return throwError("ArrayBufferView size is not a small enough positive integer.", V8Proxy::RangeError);


    // Transform the holder into a wrapper object for the array.
    V8DOMWrapper::setDOMWrapper(args.Holder(), type, array.get());
    args.Holder()->SetIndexedPropertiesToExternalArrayData(array.get()->baseAddress(), arrayType, array.get()->length());

    if (!srcArray.IsEmpty())
        copyElements(args.Holder(), srcArray, 0);

    return toV8(array.release(), args.Holder(), MarkIndependent);
}

template <class CPlusPlusArrayType, class JavaScriptWrapperArrayType>
v8::Handle<v8::Value> setWebGLArrayHelper(const v8::Arguments& args)
{
    if (args.Length() < 1) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }

    CPlusPlusArrayType* impl = JavaScriptWrapperArrayType::toNative(args.Holder());

    if (JavaScriptWrapperArrayType::HasInstance(args[0])) {
        // void set(in WebGL<T>Array array, [Optional] in unsigned long offset);
        CPlusPlusArrayType* src = JavaScriptWrapperArrayType::toNative(args[0]->ToObject());
        uint32_t offset = 0;
        if (args.Length() == 2)
            offset = toUInt32(args[1]);
        if (!impl->set(src, offset))
            V8Proxy::setDOMException(INDEX_SIZE_ERR);
        return v8::Undefined();
    }

    if (args[0]->IsObject()) {
        // void set(in sequence<long> array, [Optional] in unsigned long offset);
        v8::Local<v8::Object> array = args[0]->ToObject();
        uint32_t offset = 0;
        if (args.Length() == 2)
            offset = toUInt32(args[1]);
        uint32_t length = toUInt32(array->Get(v8::String::New("length")));
        if (offset > impl->length()
            || offset + length > impl->length()
            || offset + length < offset)
            // Out of range offset or overflow
            V8Proxy::setDOMException(INDEX_SIZE_ERR);
        else {
            if (!fastSetInstalled(args.Holder())) {
                installFastSet(args.Holder());
                copyElements(args.Holder(), array, offset);
            } else {
                for (uint32_t i = 0; i < length; i++)
                    impl->set(offset + i, array->Get(i)->NumberValue());
            }
        }
        return v8::Undefined();
    }

    V8Proxy::setDOMException(SYNTAX_ERR);
    return notHandledByInterceptor();
}

}

#endif // V8ArrayBufferViewCustom_h
