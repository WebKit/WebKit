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
#include "V8HTMLInputElement.h"

#include "HTMLInputElement.h"

#include "V8Binding.h"
#include "V8Proxy.h"

namespace WebCore {

v8::Handle<v8::Value> V8HTMLInputElement::selectionStartAccessorGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.HTMLInputElement.selectionStart._get");
    v8::Handle<v8::Object> holder = info.Holder();
    HTMLInputElement* imp = V8HTMLInputElement::toNative(holder);

    if (!imp->canHaveSelection())
        return throwError("Accessing selectionStart on an input element that cannot have a selection.");

    int v = imp->selectionStart();
    return v8::Integer::New(v);
}

void V8HTMLInputElement::selectionStartAccessorSetter(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.HTMLInputElement.selectionStart._set");
    v8::Handle<v8::Object> holder = info.Holder();
    HTMLInputElement* imp = V8HTMLInputElement::toNative(holder);

    if (!imp->canHaveSelection()) {
        throwError("Accessing selectionStart on an input element that cannot have a selection.");
        return;
    }
    imp->setSelectionStart(value->Int32Value());
}

v8::Handle<v8::Value> V8HTMLInputElement::selectionEndAccessorGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.HTMLInputElement.selectionEnd._get");
    v8::Handle<v8::Object> holder = info.Holder();
    HTMLInputElement* imp = V8HTMLInputElement::toNative(holder);

    if (!imp->canHaveSelection())
        return throwError("Accessing selectionEnd on an input element that cannot have a selection.");

    int v = imp->selectionEnd();
    return v8::Integer::New(v);
}

void V8HTMLInputElement::selectionEndAccessorSetter(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.HTMLInputElement.selectionEnd._set");
    v8::Handle<v8::Object> holder = info.Holder();
    HTMLInputElement* imp = V8HTMLInputElement::toNative(holder);

    if (!imp->canHaveSelection()) {
        throwError("Accessing selectionEnd on an input element that cannot have a selection.");
        return;
    }

    imp->setSelectionEnd(value->Int32Value());
}

v8::Handle<v8::Value> V8HTMLInputElement::setSelectionRangeCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.HTMLInputElement.setSelectionRange");
    v8::Handle<v8::Object> holder = args.Holder();
    HTMLInputElement* imp = V8HTMLInputElement::toNative(holder);

    if (!imp->canHaveSelection())
        return throwError("Calling setSelectionRange on an input element that cannot have a selection.");

    int start = args[0]->Int32Value();
    int end = args[1]->Int32Value();

    imp->setSelectionRange(start, end);
    return v8::Undefined();
}

} // namespace WebCore
