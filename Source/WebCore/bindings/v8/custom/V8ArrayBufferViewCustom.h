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

#include <wtf/ArrayBuffer.h>
#include "ExceptionCode.h"

#include "V8ArrayBuffer.h"
#include "V8Binding.h"
#include "V8Proxy.h"

namespace WebCore {

// Copy the elements from the source array to the typed destination array.
// Returns true if it succeeded, otherwise returns false.
bool copyElements(v8::Handle<v8::Object> destArray, v8::Handle<v8::Object> srcArray, uint32_t length, uint32_t offset, v8::Isolate*);

template<class ArrayClass>
v8::Handle<v8::Value> wrapArrayBufferView(const v8::Arguments& args, WrapperTypeInfo* type, ArrayClass array, v8::ExternalArrayType arrayType, bool hasIndexer)
{
    // Transform the holder into a wrapper object for the array.
    V8DOMWrapper::setDOMWrapper(args.Holder(), type, array.get());
    if (hasIndexer)
        args.Holder()->SetIndexedPropertiesToExternalArrayData(array.get()->baseAddress(), arrayType, array.get()->length());
    v8::Handle<v8::Object> wrapper = args.Holder();
    v8::Persistent<v8::Object> wrapperHandle = V8DOMWrapper::setJSWrapperForDOMObject(array.release(), wrapper);
    wrapperHandle.MarkIndependent();
    return wrapper;
}

// Template function used by the ArrayBufferView*Constructor callbacks.
template<class ArrayClass, class ElementType>
v8::Handle<v8::Value> constructWebGLArrayWithArrayBufferArgument(const v8::Arguments& args, WrapperTypeInfo* type, v8::ExternalArrayType arrayType, bool hasIndexer)
{
    ArrayBuffer* buf = V8ArrayBuffer::toNative(args[0]->ToObject());
    if (!buf)
        return throwTypeError("Could not convert argument 0 to a ArrayBuffer", args.GetIsolate());
    bool ok;
    uint32_t offset = 0;
    int argLen = args.Length();
    if (argLen > 1) {
        offset = toUInt32(args[1], ok);
        if (!ok)
            return throwTypeError("Could not convert argument 1 to a number", args.GetIsolate());
    }
    uint32_t length = 0;
    if (argLen > 2) {
        length = toUInt32(args[2], ok);
        if (!ok)
            return throwTypeError("Could not convert argument 2 to a number", args.GetIsolate());
    } else {
        if ((buf->byteLength() - offset) % sizeof(ElementType))
            return throwError(RangeError, "ArrayBuffer length minus the byteOffset is not a multiple of the element size.", args.GetIsolate());
        length = (buf->byteLength() - offset) / sizeof(ElementType);
    }
    RefPtr<ArrayClass> array = ArrayClass::create(buf, offset, length);
    if (!array)
        return setDOMException(INDEX_SIZE_ERR, args.GetIsolate());

    return wrapArrayBufferView(args, type, array, arrayType, hasIndexer);
}

static const char* notSmallEnoughSize = "ArrayBufferView size is not a small enough positive integer.";

// Template function used by the ArrayBufferView*Constructor callbacks.
template<class ArrayClass, class JavaScriptWrapperArrayType, class ElementType>
v8::Handle<v8::Value> constructWebGLArray(const v8::Arguments& args, WrapperTypeInfo* type, v8::ExternalArrayType arrayType)
{
    if (!args.IsConstructCall())
        return throwTypeError("DOM object constructor cannot be called as a function.", args.GetIsolate());

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
        // Do not call SetIndexedPropertiesToExternalArrayData on this
        // object. Not only is there no point from a performance
        // perspective, but doing so causes errors in the subset() case.
        return wrapArrayBufferView(args, type, array, arrayType, false);
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
        return throwTypeError(0, args.GetIsolate());
    }

    // See whether the first argument is a ArrayBuffer.
    if (V8ArrayBuffer::HasInstance(args[0]))
      return constructWebGLArrayWithArrayBufferArgument<ArrayClass, ElementType>(args, type, arrayType, true);

    // See whether the first argument is the same type as impl. In that case,
    // we can simply memcpy data from source to impl.
    if (JavaScriptWrapperArrayType::HasInstance(args[0])) {
        ArrayClass* source = JavaScriptWrapperArrayType::toNative(args[0]->ToObject());
        uint32_t length = source->length();
        RefPtr<ArrayClass> array = ArrayClass::createUninitialized(length);
        if (!array.get())
            return throwError(RangeError, notSmallEnoughSize, args.GetIsolate());

        memcpy(array->baseAddress(), source->baseAddress(), length * sizeof(ElementType));

        return wrapArrayBufferView(args, type, array, arrayType, true);
    }

    uint32_t len = 0;
    v8::Handle<v8::Object> srcArray;
    bool doInstantiation = false;

    if (args[0]->IsObject()) {
        srcArray = args[0]->ToObject();
        if (srcArray.IsEmpty())
            return throwTypeError("Could not convert argument 0 to an array", args.GetIsolate());
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
    if (doInstantiation) {
        if (srcArray.IsEmpty())
            array = ArrayClass::create(len);
        else
            array = ArrayClass::createUninitialized(len);
    }

    if (!array.get())
        return throwError(RangeError, notSmallEnoughSize, args.GetIsolate());


    // Transform the holder into a wrapper object for the array.
    V8DOMWrapper::setDOMWrapper(args.Holder(), type, array.get());
    args.Holder()->SetIndexedPropertiesToExternalArrayData(array.get()->baseAddress(), arrayType, array.get()->length());

    if (!srcArray.IsEmpty()) {
        bool copied = copyElements(args.Holder(), srcArray, len, 0, args.GetIsolate());
        if (!copied) {
            for (unsigned i = 0; i < len; i++)
                array->set(i, srcArray->Get(i)->NumberValue());
        }
    }

    v8::Handle<v8::Object> wrapper = args.Holder();
    v8::Persistent<v8::Object> wrapperHandle = V8DOMWrapper::setJSWrapperForDOMObject(array.release(), wrapper);
    wrapperHandle.MarkIndependent();
    return wrapper;
}

template <class CPlusPlusArrayType, class JavaScriptWrapperArrayType>
v8::Handle<v8::Value> setWebGLArrayHelper(const v8::Arguments& args)
{
    if (args.Length() < 1)
        return setDOMException(SYNTAX_ERR, args.GetIsolate());

    CPlusPlusArrayType* impl = JavaScriptWrapperArrayType::toNative(args.Holder());

    if (JavaScriptWrapperArrayType::HasInstance(args[0])) {
        // void set(in WebGL<T>Array array, [Optional] in unsigned long offset);
        CPlusPlusArrayType* src = JavaScriptWrapperArrayType::toNative(args[0]->ToObject());
        uint32_t offset = 0;
        if (args.Length() == 2)
            offset = toUInt32(args[1]);
        if (!impl->set(src, offset))
            return setDOMException(INDEX_SIZE_ERR, args.GetIsolate());
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
            || offset + length < offset) {
            // Out of range offset or overflow
            return setDOMException(INDEX_SIZE_ERR, args.GetIsolate());
        }
        bool copied = copyElements(args.Holder(), array, length, offset, args.GetIsolate());
        if (!copied) {
            for (uint32_t i = 0; i < length; i++)
                impl->set(offset + i, array->Get(i)->NumberValue());
        }
        return v8::Undefined();
    }

    return setDOMException(SYNTAX_ERR, args.GetIsolate());
}

}

#endif // V8ArrayBufferViewCustom_h
