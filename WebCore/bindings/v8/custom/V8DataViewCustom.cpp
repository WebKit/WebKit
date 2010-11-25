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

#if ENABLE(3D_CANVAS) || ENABLE(BLOB)

#include "DataView.h"

#include "V8ArrayBufferViewCustom.h"
#include "V8Binding.h"
#include "V8BindingMacros.h"
#include "V8DataView.h"
#include "V8Proxy.h"

namespace WebCore {

v8::Handle<v8::Value> V8DataView::constructorCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.ArrayBuffer.Constructor");

    if (args[0]->IsNull() || !V8ArrayBuffer::HasInstance(args[0]))
        return V8Proxy::throwTypeError();
    return constructWebGLArrayWithArrayBufferArgument<DataView, char>(args, &info, v8::kExternalByteArray, false);
}

v8::Handle<v8::Value> toV8(DataView* impl)
{
    if (!impl)
        return v8::Null();
    return V8DataView::wrap(impl);
}

v8::Handle<v8::Value> V8DataView::getInt8Callback(const v8::Arguments& args)
{
    INC_STATS("DOM.DataView.getInt8");
    if (args.Length() < 1)
        return throwError("Not enough arguments", V8Proxy::SyntaxError);

    DataView* imp = V8DataView::toNative(args.Holder());
    ExceptionCode ec = 0;
    EXCEPTION_BLOCK(unsigned, byteOffset, toUInt32(args[0]));
    char result = imp->getInt8(byteOffset, ec);
    if (UNLIKELY(ec)) {
        V8Proxy::setDOMException(ec);
        return v8::Handle<v8::Value>();
    }
    return v8::Integer::New(result);
}

v8::Handle<v8::Value> V8DataView::getUint8Callback(const v8::Arguments& args)
{
    INC_STATS("DOM.DataView.getUint8");
    if (args.Length() < 1)
        return throwError("Not enough arguments", V8Proxy::SyntaxError);

    DataView* imp = V8DataView::toNative(args.Holder());
    ExceptionCode ec = 0;
    EXCEPTION_BLOCK(unsigned, byteOffset, toUInt32(args[0]));
    unsigned char result = imp->getUint8(byteOffset, ec);
    if (UNLIKELY(ec)) {
        V8Proxy::setDOMException(ec);
        return v8::Handle<v8::Value>();
    }
    return v8::Integer::New(result);
}

v8::Handle<v8::Value> V8DataView::setInt8Callback(const v8::Arguments& args)
{
    INC_STATS("DOM.DataView.setInt8");
    if (args.Length() < 2)
        return throwError("Not enough arguments", V8Proxy::SyntaxError);

    DataView* imp = V8DataView::toNative(args.Holder());
    ExceptionCode ec = 0;
    EXCEPTION_BLOCK(unsigned, byteOffset, toUInt32(args[0]));
    EXCEPTION_BLOCK(int, value, toInt32(args[1]));
    imp->setInt8(byteOffset, static_cast<char>(value), ec);
    if (UNLIKELY(ec))
        V8Proxy::setDOMException(ec);
    return v8::Handle<v8::Value>();
}

v8::Handle<v8::Value> V8DataView::setUint8Callback(const v8::Arguments& args)
{
    INC_STATS("DOM.DataView.setUint8");
    if (args.Length() < 2)
        return throwError("Not enough arguments", V8Proxy::SyntaxError);

    DataView* imp = V8DataView::toNative(args.Holder());
    ExceptionCode ec = 0;
    EXCEPTION_BLOCK(unsigned, byteOffset, toUInt32(args[0]));
    EXCEPTION_BLOCK(int, value, toInt32(args[1]));
    imp->setUint8(byteOffset, static_cast<unsigned char>(value), ec);
    if (UNLIKELY(ec))
        V8Proxy::setDOMException(ec);
    return v8::Handle<v8::Value>();
}

} // namespace WebCore

#endif // ENABLE(3D_CANVAS) || ENABLE(BLOB)
