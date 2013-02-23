/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DataView.h"

#include "V8ArrayBufferViewCustom.h"
#include "V8Binding.h"
#include "V8DataView.h"

namespace WebCore {

v8::Handle<v8::Value> V8DataView::constructorCallbackCustom(const v8::Arguments& args)
{
    if (!args.Length()) {
        // see constructWebGLArray -- we don't seem to be able to distingish between
        // 'new DataView()' and the call used to construct the cached DataView object.
        RefPtr<DataView> dataView = DataView::create(0);
        v8::Handle<v8::Object> wrapper = args.Holder();
        V8DOMWrapper::associateObjectWithWrapper(dataView.release(), &info, wrapper, args.GetIsolate(), WrapperConfiguration::Dependent);
        return wrapper;
    }
    if (args[0]->IsNull() || !V8ArrayBuffer::HasInstance(args[0], args.GetIsolate()))
        return throwTypeError(0, args.GetIsolate());
    return constructWebGLArrayWithArrayBufferArgument<DataView, char>(args, &info, v8::kExternalByteArray, false);
}

// FIXME: Don't need this override.
v8::Handle<v8::Object> wrap(DataView* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    ASSERT(impl);
    return V8DataView::createWrapper(impl, creationContext, isolate);
}

v8::Handle<v8::Value> V8DataView::getInt8MethodCustom(const v8::Arguments& args)
{
    if (args.Length() < 1)
        return throwNotEnoughArgumentsError(args.GetIsolate());

    DataView* imp = V8DataView::toNative(args.Holder());
    ExceptionCode ec = 0;
    V8TRYCATCH(unsigned, byteOffset, toUInt32(args[0]));
    int8_t result = imp->getInt8(byteOffset, ec);
    if (UNLIKELY(ec))
        return setDOMException(ec, args.GetIsolate());
    return v8Integer(result, args.GetIsolate());
}

v8::Handle<v8::Value> V8DataView::getUint8MethodCustom(const v8::Arguments& args)
{
    if (args.Length() < 1)
        return throwNotEnoughArgumentsError(args.GetIsolate());

    DataView* imp = V8DataView::toNative(args.Holder());
    ExceptionCode ec = 0;
    V8TRYCATCH(unsigned, byteOffset, toUInt32(args[0]));
    uint8_t result = imp->getUint8(byteOffset, ec);
    if (UNLIKELY(ec))
        return setDOMException(ec, args.GetIsolate());
    return v8Integer(result, args.GetIsolate());
}

v8::Handle<v8::Value> V8DataView::setInt8MethodCustom(const v8::Arguments& args)
{
    if (args.Length() < 2)
        return throwNotEnoughArgumentsError(args.GetIsolate());

    DataView* imp = V8DataView::toNative(args.Holder());
    ExceptionCode ec = 0;
    V8TRYCATCH(unsigned, byteOffset, toUInt32(args[0]));
    V8TRYCATCH(int, value, toInt32(args[1]));
    imp->setInt8(byteOffset, static_cast<int8_t>(value), ec);
    if (UNLIKELY(ec))
        return setDOMException(ec, args.GetIsolate());
    return v8Undefined();
}

v8::Handle<v8::Value> V8DataView::setUint8MethodCustom(const v8::Arguments& args)
{
    if (args.Length() < 2)
        return throwNotEnoughArgumentsError(args.GetIsolate());

    DataView* imp = V8DataView::toNative(args.Holder());
    ExceptionCode ec = 0;
    V8TRYCATCH(unsigned, byteOffset, toUInt32(args[0]));
    V8TRYCATCH(int, value, toInt32(args[1]));
    imp->setUint8(byteOffset, static_cast<uint8_t>(value), ec);
    if (UNLIKELY(ec))
        return setDOMException(ec, args.GetIsolate());
    return v8Undefined();
}

} // namespace WebCore
