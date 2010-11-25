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
#include "OptionsObject.h"
#include "V8Binding.h"

namespace WebCore {

OptionsObject::OptionsObject()
{
}

OptionsObject::OptionsObject(const v8::Local<v8::Value>& options)
    : m_options(options)
{
}

OptionsObject::~OptionsObject()
{
}

OptionsObject& OptionsObject::operator=(const OptionsObject& optionsObject)
{
    m_options = optionsObject.m_options;
    return *this;
}

bool OptionsObject::isUndefinedOrNull() const
{
    if (m_options.IsEmpty())
        return true;
    return WebCore::isUndefinedOrNull(m_options);
}

bool OptionsObject::getKeyBool(const String& key, bool& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    if (!v8Value->IsBoolean())
        return false;
    v8::Local<v8::Boolean> v8Bool = v8Value->ToBoolean();
    ASSERT(!v8Bool.IsEmpty());

    value = v8Bool->Value();
    return true;
}

bool OptionsObject::getKeyString(const String& key, String& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    if (!v8Value->IsString())
        return false;

    value = v8ValueToWebCoreString(v8Value);
    ASSERT(!value.isEmpty());
    return true;
}

bool OptionsObject::getKey(const String& key, v8::Local<v8::Value>& value) const
{
    if (isUndefinedOrNull())
        return false;
    v8::Local<v8::Object> options = m_options->ToObject();
    ASSERT(!options.IsEmpty());

    v8::Handle<v8::String> v8Key = v8String(key);
    if (!options->Has(v8Key))
        return false;
    value = options->Get(v8Key);
    ASSERT(!value.IsEmpty());
    return true;
}

} // namespace WebCore
