/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "BufferSource.h"
#include "JSDOMConvertInterface.h"
#include "JSDOMWrapperCache.h"
#include "JSDynamicDowncast.h"
#include <runtime/JSTypedArrays.h>

namespace WebCore {

// FIXME: It is wrong that we treat buffer related types as interfaces, they should be their own IDL type. 

// Add adaptors to make ArrayBuffer / ArrayBufferView / typed arrays act like normal interfaces.

template<typename ImplementationClass> struct JSDOMWrapperConverterTraits;

template<> struct JSDOMWrapperConverterTraits<JSC::ArrayBuffer> {
    using WrapperClass = JSC::JSArrayBuffer;
    using ToWrappedReturnType = JSC::ArrayBuffer*;
};

template<> struct JSDOMWrapperConverterTraits<JSC::ArrayBufferView> {
    using WrapperClass = JSC::JSArrayBufferView;
    using ToWrappedReturnType = RefPtr<ArrayBufferView>;
};

template<typename Adaptor> struct JSDOMWrapperConverterTraits<JSC::GenericTypedArrayView<Adaptor>> {
    using WrapperClass = JSC::JSGenericTypedArrayView<Adaptor>;
    using ToWrappedReturnType = RefPtr<JSC::GenericTypedArrayView<Adaptor>>;
};


inline RefPtr<JSC::Int8Array> toPossiblySharedInt8Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toPossiblySharedNativeTypedView<JSC::Int8Adaptor>(vm, value); }
inline RefPtr<JSC::Int16Array> toPossiblySharedInt16Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toPossiblySharedNativeTypedView<JSC::Int16Adaptor>(vm, value); }
inline RefPtr<JSC::Int32Array> toPossiblySharedInt32Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toPossiblySharedNativeTypedView<JSC::Int32Adaptor>(vm, value); }
inline RefPtr<JSC::Uint8Array> toPossiblySharedUint8Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toPossiblySharedNativeTypedView<JSC::Uint8Adaptor>(vm, value); }
inline RefPtr<JSC::Uint8ClampedArray> toPossiblySharedUint8ClampedArray(JSC::VM& vm, JSC::JSValue value) { return JSC::toPossiblySharedNativeTypedView<JSC::Uint8ClampedAdaptor>(vm, value); }
inline RefPtr<JSC::Uint16Array> toPossiblySharedUint16Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toPossiblySharedNativeTypedView<JSC::Uint16Adaptor>(vm, value); }
inline RefPtr<JSC::Uint32Array> toPossiblySharedUint32Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toPossiblySharedNativeTypedView<JSC::Uint32Adaptor>(vm, value); }
inline RefPtr<JSC::Float32Array> toPossiblySharedFloat32Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toPossiblySharedNativeTypedView<JSC::Float32Adaptor>(vm, value); }
inline RefPtr<JSC::Float64Array> toPossiblySharedFloat64Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toPossiblySharedNativeTypedView<JSC::Float64Adaptor>(vm, value); }

inline RefPtr<JSC::Int8Array> toUnsharedInt8Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toUnsharedNativeTypedView<JSC::Int8Adaptor>(vm, value); }
inline RefPtr<JSC::Int16Array> toUnsharedInt16Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toUnsharedNativeTypedView<JSC::Int16Adaptor>(vm, value); }
inline RefPtr<JSC::Int32Array> toUnsharedInt32Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toUnsharedNativeTypedView<JSC::Int32Adaptor>(vm, value); }
inline RefPtr<JSC::Uint8Array> toUnsharedUint8Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toUnsharedNativeTypedView<JSC::Uint8Adaptor>(vm, value); }
inline RefPtr<JSC::Uint8ClampedArray> toUnsharedUint8ClampedArray(JSC::VM& vm, JSC::JSValue value) { return JSC::toUnsharedNativeTypedView<JSC::Uint8ClampedAdaptor>(vm, value); }
inline RefPtr<JSC::Uint16Array> toUnsharedUint16Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toUnsharedNativeTypedView<JSC::Uint16Adaptor>(vm, value); }
inline RefPtr<JSC::Uint32Array> toUnsharedUint32Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toUnsharedNativeTypedView<JSC::Uint32Adaptor>(vm, value); }
inline RefPtr<JSC::Float32Array> toUnsharedFloat32Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toUnsharedNativeTypedView<JSC::Float32Adaptor>(vm, value); }
inline RefPtr<JSC::Float64Array> toUnsharedFloat64Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toUnsharedNativeTypedView<JSC::Float64Adaptor>(vm, value); }

inline JSC::JSValue toJS(JSC::ExecState* state, JSDOMGlobalObject* globalObject, JSC::ArrayBuffer& buffer)
{
    if (auto result = getCachedWrapper(globalObject->world(), buffer))
        return result;

    // The JSArrayBuffer::create function will register the wrapper in finishCreation.
    return JSC::JSArrayBuffer::create(state->vm(), globalObject->arrayBufferStructure(buffer.sharingMode()), &buffer);
}

inline JSC::JSValue toJS(JSC::ExecState* state, JSC::JSGlobalObject* globalObject, JSC::ArrayBufferView& view)
{
    return view.wrap(state, globalObject);
}

inline JSC::JSValue toJS(JSC::ExecState* state, JSDOMGlobalObject* globalObject, JSC::ArrayBuffer* buffer)
{
    if (!buffer)
        return JSC::jsNull();
    return toJS(state, globalObject, *buffer);
}

inline JSC::JSValue toJS(JSC::ExecState* state, JSC::JSGlobalObject* globalObject, JSC::ArrayBufferView* view)
{
    if (!view)
        return JSC::jsNull();
    return toJS(state, globalObject, *view);
}

inline RefPtr<JSC::ArrayBufferView> toPossiblySharedArrayBufferView(JSC::VM& vm, JSC::JSValue value)
{
    auto* wrapper = jsDynamicDowncast<JSC::JSArrayBufferView*>(vm, value);
    if (!wrapper)
        return nullptr;
    return wrapper->possiblySharedImpl();
}

inline RefPtr<JSC::ArrayBufferView> toUnsharedArrayBufferView(JSC::VM& vm, JSC::JSValue value)
{
    auto result = toPossiblySharedArrayBufferView(vm, value);
    if (!result || result->isShared())
        return nullptr;
    return result;
}

} // namespace WebCore
