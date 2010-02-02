/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "BoundObject.h"

#include "V8Proxy.h"

namespace WebKit {

BoundObject::BoundObject(v8::Handle<v8::Context> context, void* v8This, const char* objectName)
    : m_objectName(objectName)
    , m_context(context)
    , m_v8This(v8This)
{
    v8::Context::Scope contextScope(context);
    v8::Local<v8::FunctionTemplate> localTemplate = v8::FunctionTemplate::New(WebCore::V8Proxy::checkNewLegal);
    m_hostTemplate = v8::Persistent<v8::FunctionTemplate>::New(localTemplate);
    m_hostTemplate->SetClassName(v8::String::New(objectName));
}

BoundObject::~BoundObject()
{
    m_hostTemplate.Dispose();
}

void BoundObject::addProtoFunction(const char* name, v8::InvocationCallback callback)
{
    v8::Context::Scope contextScope(m_context);
    v8::Local<v8::Signature> signature = v8::Signature::New(m_hostTemplate);
    v8::Local<v8::ObjectTemplate> proto = m_hostTemplate->PrototypeTemplate();
    v8::Local<v8::External> v8This = v8::External::New(m_v8This);
    proto->Set(
        v8::String::New(name),
        v8::FunctionTemplate::New(
            callback,
            v8This,
            signature),
        static_cast<v8::PropertyAttribute>(v8::DontDelete));
}

void BoundObject::build()
{
    v8::Context::Scope contextScope(m_context);
    v8::Local<v8::Function> constructor = m_hostTemplate->GetFunction();
    v8::Local<v8::Object> boundObject = WebCore::SafeAllocation::newInstance(constructor);

    v8::Handle<v8::Object> global = m_context->Global();
    global->Set(v8::String::New(m_objectName), boundObject);
}

} // namespace WebKit
