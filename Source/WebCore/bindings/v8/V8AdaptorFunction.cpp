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

#include "config.h"
#include "V8AdaptorFunction.h"

#include "V8PerIsolateData.h"
#include <wtf/Vector.h>

#if ENABLE(CUSTOM_ELEMENTS)

namespace WebCore {

WrapperTypeInfo V8AdaptorFunction::info = { V8AdaptorFunction::getTemplate, 0, 0, 0, 0, 0, 0, WrapperTypeObjectPrototype };

v8::Persistent<v8::FunctionTemplate> V8AdaptorFunction::getTemplate(v8::Isolate* isolate, WrapperWorldType worldType)
{
    ASSERT(isolate);
    V8PerIsolateData* data = V8PerIsolateData::from(isolate);
    V8PerIsolateData::TemplateMap::iterator result = data->rawTemplateMap().find(&info);
    if (result != data->rawTemplateMap().end())
        return result->value;
    // The lifetime is of newTemplate is delegated to the TemplateMap thus this won't be leaked.
    v8::Persistent<v8::FunctionTemplate> newTemplate = v8::Persistent<v8::FunctionTemplate>::New(isolate, v8::FunctionTemplate::New());
    data->rawTemplateMap().add(&info, configureTemplate(newTemplate));
    return newTemplate;
}

v8::Persistent<v8::FunctionTemplate> V8AdaptorFunction::configureTemplate(v8::Persistent<v8::FunctionTemplate> functionTemplate)
{
    functionTemplate->SetCallHandler(&V8AdaptorFunction::invocationCallback);
    return functionTemplate;
}

v8::Handle<v8::Value> V8AdaptorFunction::invocationCallback(const v8::Arguments& args)
{
    v8::Handle<v8::Object> wrapped = v8::Handle<v8::Object>::Cast(args.Callee()->GetHiddenValue(V8HiddenPropertyName::adaptorFunctionPeer()));
    // FIXME: This can be faster if we can access underlying native callback directly.
    // We won't need this once https://bugs.webkit.org/show_bug.cgi?id=108138 is addressed.
    Vector<v8::Handle<v8::Value> > argArray(args.Length());
    for (int i = 0; i < args.Length(); ++i)
        argArray.append(args[i]);
    if (args.IsConstructCall())
        return wrapped->CallAsConstructor(argArray.size(), argArray.data());
    return wrapped->CallAsFunction(args.This(), argArray.size(), argArray.data());
}

v8::Handle<v8::Function> V8AdaptorFunction::wrap(v8::Handle<v8::Object> object, const AtomicString& name, v8::Isolate* isolate)
{
    if (object.IsEmpty() || !object->IsObject())
        return v8::Handle<v8::Function>();
    v8::Handle<v8::Function> adaptor = v8::Handle<v8::Function>::Cast(getTemplate(isolate, worldType(isolate))->GetFunction());
    if (adaptor.IsEmpty())
        return v8::Handle<v8::Function>();
    adaptor->SetName(v8String(name.string(), isolate));
    adaptor->SetHiddenValue(V8HiddenPropertyName::adaptorFunctionPeer(), object);
    object->SetHiddenValue(V8HiddenPropertyName::adaptorFunctionPeer(), adaptor);
    return adaptor;
}

} // namespace WebCore

#endif // ENABLE(CUSTOM_ELEMENTS)
