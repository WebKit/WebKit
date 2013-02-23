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
#include "V8History.h"

#include "BindingState.h"
#include "ExceptionCode.h"
#include "History.h"
#include "SerializedScriptValue.h"
#include "V8Binding.h"
#include "V8DOMWindow.h"
#include "V8HiddenPropertyName.h"

namespace WebCore {

v8::Handle<v8::Value> V8History::stateAttrGetterCustom(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    History* history = V8History::toNative(info.Holder());

    v8::Handle<v8::Value> value = info.Holder()->GetHiddenValue(V8HiddenPropertyName::state());

    if (!value.IsEmpty() && !history->stateChanged())
        return value;

    RefPtr<SerializedScriptValue> serialized = history->state();
    value = serialized ? serialized->deserialize(info.GetIsolate()) : v8::Handle<v8::Value>(v8Null(info.GetIsolate()));
    info.Holder()->SetHiddenValue(V8HiddenPropertyName::state(), value);

    return value;
}

v8::Handle<v8::Value> V8History::pushStateMethodCustom(const v8::Arguments& args)
{
    bool didThrow = false;
    RefPtr<SerializedScriptValue> historyState = SerializedScriptValue::create(args[0], 0, 0, didThrow, args.GetIsolate());
    if (didThrow)
        return v8::Undefined();

    V8TRYCATCH_FOR_V8STRINGRESOURCE(V8StringResource<WithUndefinedOrNullCheck>, title, args[1]);
    V8TRYCATCH_FOR_V8STRINGRESOURCE(V8StringResource<WithUndefinedOrNullCheck>, url, argumentOrNull(args, 2));

    ExceptionCode ec = 0;
    History* history = V8History::toNative(args.Holder());
    history->stateObjectAdded(historyState.release(), title, url, History::StateObjectPush, ec);
    args.Holder()->DeleteHiddenValue(V8HiddenPropertyName::state());
    return setDOMException(ec, args.GetIsolate());
}

v8::Handle<v8::Value> V8History::replaceStateMethodCustom(const v8::Arguments& args)
{
    bool didThrow = false;
    RefPtr<SerializedScriptValue> historyState = SerializedScriptValue::create(args[0], 0, 0, didThrow, args.GetIsolate());
    if (didThrow)
        return v8::Undefined();

    V8TRYCATCH_FOR_V8STRINGRESOURCE(V8StringResource<WithUndefinedOrNullCheck>, title, args[1]);
    V8TRYCATCH_FOR_V8STRINGRESOURCE(V8StringResource<WithUndefinedOrNullCheck>, url, argumentOrNull(args, 2));

    ExceptionCode ec = 0;
    History* history = V8History::toNative(args.Holder());
    history->stateObjectAdded(historyState.release(), title, url, History::StateObjectReplace, ec);
    args.Holder()->DeleteHiddenValue(V8HiddenPropertyName::state());
    return setDOMException(ec, args.GetIsolate());
}

bool V8History::indexedSecurityCheck(v8::Local<v8::Object> host, uint32_t index, v8::AccessType type, v8::Local<v8::Value>)
{
    History* history = V8History::toNative(host);
    return BindingSecurity::shouldAllowAccessToFrame(BindingState::instance(), history->frame(), DoNotReportSecurityError);
}

bool V8History::namedSecurityCheck(v8::Local<v8::Object> host, v8::Local<v8::Value> key, v8::AccessType type, v8::Local<v8::Value>)
{
    History* history = V8History::toNative(host);
    return BindingSecurity::shouldAllowAccessToFrame(BindingState::instance(), history->frame(), DoNotReportSecurityError);
}

} // namespace WebCore
