/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 * 3.  Neither the name of Google, Inc. ("Google") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE AND ITS CONTRIBUTORS "AS IS" AND ANY
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
#include "Intent.h"

#include "ExceptionCode.h"
#include "MessagePort.h"
#include "SerializedScriptValue.h"

#include "V8Binding.h"
#include "V8DOMWrapper.h"
#include "V8Intent.h"
#include "V8MessagePort.h"
#include <wtf/ArrayBuffer.h>

namespace WebCore {

v8::Handle<v8::Value> V8Intent::constructorCallbackCustom(const v8::Arguments& args)
{
    if (args.Length() < 1)
        return throwNotEnoughArgumentsError(args.GetIsolate());
    if (args.Length() == 1) {
        // Use the dictionary constructor. This block will return if the
        // argument isn't a valid Dictionary.
        V8TRYCATCH(Dictionary, options, Dictionary(args[0], args.GetIsolate()));
        ExceptionCode ec = 0;
        RefPtr<Intent> impl = Intent::create(ScriptState::current(), options, ec);
        if (ec)
            return setDOMException(ec, args.GetIsolate());

        v8::Handle<v8::Object> wrapper = args.Holder();
        V8DOMWrapper::associateObjectWithWrapper(impl.release(), &info, wrapper, args.GetIsolate());
        return wrapper;
    }

    ExceptionCode ec = 0;
    V8TRYCATCH_FOR_V8STRINGRESOURCE(V8StringResource<>, action, MAYBE_MISSING_PARAMETER(args, 0, DefaultIsUndefined));
    V8TRYCATCH_FOR_V8STRINGRESOURCE(V8StringResource<>, type, MAYBE_MISSING_PARAMETER(args, 1, DefaultIsUndefined));
    MessagePortArray messagePortArrayTransferList;
    ArrayBufferArray arrayBufferArrayTransferList;
    if (args.Length() > 3) {
        if (!extractTransferables(args[3], messagePortArrayTransferList, arrayBufferArrayTransferList, args.GetIsolate()))
            return throwTypeError("Could not extract transferables", args.GetIsolate());
    }
    bool dataDidThrow = false;
    RefPtr<SerializedScriptValue> data = SerializedScriptValue::create(args[2], &messagePortArrayTransferList, &arrayBufferArrayTransferList, dataDidThrow);
    if (dataDidThrow)
        return setDOMException(DATA_CLONE_ERR, args.GetIsolate());

    RefPtr<Intent> impl = Intent::create(action, type, data, messagePortArrayTransferList, ec);
    if (ec)
        return setDOMException(ec, args.GetIsolate());

    v8::Handle<v8::Object> wrapper = args.Holder();
    V8DOMWrapper::associateObjectWithWrapper(impl.release(), &info, wrapper, args.GetIsolate());
    return wrapper;
}


} // namespace WebCore
