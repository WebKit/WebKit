/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef V8AdaptorFunction_h
#define V8AdaptorFunction_h

#include "V8Binding.h"
#include "V8HiddenPropertyName.h"
#include "WrapperTypeInfo.h"
#include <wtf/PassRefPtr.h>

#if ENABLE(CUSTOM_ELEMENTS)

namespace WebCore {

//
// FIXME(https://bugs.webkit.org/show_bug.cgi?id=108138):
// V8AdaptorFunction class and V8WrapAsFunction IDL attribute are needed for two reasons:
// - 1) V8 doesn't allow expanding the internal field of function objects. https://code.google.com/p/v8/issues/detail?id=837
//      WebKit need it to associate each wrapper to its backing C++ object. We store it in a hidden property of the wrapped object.
// - 2) Binding codebase assumes every wrapper object is created through ObjectTemplate::NewInstance(), not FunctionTemplate::GetFunction().
// Once 1) is addresssed, we could attack 2) with it. 
//
class V8AdaptorFunction {
public:
    static WrapperTypeInfo info;
    static v8::Handle<v8::Object> unwrap(v8::Handle<v8::Function>);
    static v8::Handle<v8::Function> wrap(v8::Handle<v8::Object>, const AtomicString& name, v8::Isolate*);
    static v8::Handle<v8::Function> get(v8::Handle<v8::Object>);

    static v8::Persistent<v8::FunctionTemplate> getTemplate(v8::Isolate*, WrapperWorldType);
    static v8::Persistent<v8::FunctionTemplate> configureTemplate(v8::Persistent<v8::FunctionTemplate>);
    static v8::Handle<v8::Value> invocationCallback(const v8::Arguments&);
};

inline v8::Handle<v8::Object> V8AdaptorFunction::unwrap(v8::Handle<v8::Function> function)
{
    v8::Handle<v8::Value> wrapped = function->GetHiddenValue(V8HiddenPropertyName::adaptorFunctionPeer());
    ASSERT(!wrapped.IsEmpty());
    ASSERT(wrapped->IsObject());
    return v8::Handle<v8::Object>::Cast(wrapped);
}

inline v8::Handle<v8::Function> V8AdaptorFunction::get(v8::Handle<v8::Object> object)
{
    v8::Handle<v8::Value> adaptorFunction = object->GetHiddenValue(V8HiddenPropertyName::adaptorFunctionPeer());
    ASSERT(!adaptorFunction.IsEmpty());
    ASSERT(adaptorFunction->IsFunction());
    return v8::Handle<v8::Function>::Cast(adaptorFunction);
}

} // namespace WebCore

#endif // ENABLE(CUSTOM_ELEMENTS)
#endif // V8AdaptorFunction_h
