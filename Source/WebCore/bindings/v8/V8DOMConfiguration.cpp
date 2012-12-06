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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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
#include "V8DOMConfiguration.h"

#include "V8Binding.h"

namespace WebCore {

void V8DOMConfiguration::batchConfigureAttributes(v8::Handle<v8::ObjectTemplate> instance, 
                                                  v8::Handle<v8::ObjectTemplate> prototype, 
                                                  const BatchedAttribute* attributes, 
                                                  size_t attributeCount)
{
    for (size_t i = 0; i < attributeCount; ++i)
        configureAttribute(instance, prototype, attributes[i]);
}

void V8DOMConfiguration::batchConfigureConstants(v8::Handle<v8::FunctionTemplate> functionDescriptor,
                                                 v8::Handle<v8::ObjectTemplate> prototype,
                                                 const BatchedConstant* constants,
                                                 size_t constantCount)
{
    for (size_t i = 0; i < constantCount; ++i) {
        const BatchedConstant* constant = &constants[i];
        functionDescriptor->Set(v8::String::NewSymbol(constant->name), deprecatedV8Integer(constant->value), v8::ReadOnly);
        prototype->Set(v8::String::NewSymbol(constant->name), deprecatedV8Integer(constant->value), v8::ReadOnly);
    }
}

void V8DOMConfiguration::batchConfigureCallbacks(v8::Handle<v8::ObjectTemplate> prototype, 
                                                 v8::Handle<v8::Signature> signature, 
                                                 v8::PropertyAttribute attributes,
                                                 const BatchedCallback* callbacks,
                                                 size_t callbackCount)
{
    for (size_t i = 0; i < callbackCount; ++i)
        prototype->Set(v8::String::NewSymbol(callbacks[i].name), v8::FunctionTemplate::New(callbacks[i].callback, v8Undefined(), signature), attributes);
}

v8::Local<v8::Signature> V8DOMConfiguration::configureTemplate(v8::Persistent<v8::FunctionTemplate> functionDescriptor,
                                                               const char* interfaceName,
                                                               v8::Persistent<v8::FunctionTemplate> parentClass,
                                                               size_t fieldCount,
                                                               const BatchedAttribute* attributes, 
                                                               size_t attributeCount,
                                                               const BatchedCallback* callbacks,
                                                               size_t callbackCount)
{
    functionDescriptor->SetClassName(v8::String::NewSymbol(interfaceName));
    v8::Local<v8::ObjectTemplate> instance = functionDescriptor->InstanceTemplate();
    instance->SetInternalFieldCount(fieldCount);
    if (!parentClass.IsEmpty())
        functionDescriptor->Inherit(parentClass);
    if (attributeCount)
        batchConfigureAttributes(instance, functionDescriptor->PrototypeTemplate(), attributes, attributeCount);
    v8::Local<v8::Signature> defaultSignature = v8::Signature::New(functionDescriptor);
    if (callbackCount)
        batchConfigureCallbacks(functionDescriptor->PrototypeTemplate(), defaultSignature, static_cast<v8::PropertyAttribute>(v8::DontDelete), callbacks, callbackCount);
    return defaultSignature;
}

} // namespace WebCore
