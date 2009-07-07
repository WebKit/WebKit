/*
 * Copyright (C) 2006, 2007, 2008, 2009 Google Inc. All rights reserved.
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
#include "V8Binding.h"

#include "AtomicString.h"
#include "CString.h"
#include "MathExtras.h"
#include "PlatformString.h"
#include "StringBuffer.h"

#include <v8.h>

namespace WebCore {

// WebCoreStringResource is a helper class for v8ExternalString. It is used
// to manage the life-cycle of the underlying buffer of the external string.
class WebCoreStringResource : public v8::String::ExternalStringResource {
public:
    explicit WebCoreStringResource(const String& string)
        : m_impl(string.impl())
    {
    }

    virtual ~WebCoreStringResource() { }

    const uint16_t* data() const
    {
        return reinterpret_cast<const uint16_t*>(m_impl.characters());
    }

    size_t length() const { return m_impl.length(); }

    String webcoreString() { return m_impl; }

private:
    // A shallow copy of the string. Keeps the string buffer alive until the V8 engine garbage collects it.
    String m_impl;
};

String v8StringToWebCoreString(v8::Handle<v8::String> v8String, bool externalize)
{
    WebCoreStringResource* stringResource = static_cast<WebCoreStringResource*>(v8String->GetExternalStringResource());
    if (stringResource)
        return stringResource->webcoreString();

    int length = v8String->Length();
    if (!length) {
        // Avoid trying to morph empty strings, as they do not have enough room to contain the external reference.
        return StringImpl::empty();
    }

    UChar* buffer;
    String result = String::createUninitialized(length, buffer);
    v8String->Write(reinterpret_cast<uint16_t*>(buffer), 0, length);

    if (externalize) {
        WebCoreStringResource* resource = new WebCoreStringResource(result);
        if (!v8String->MakeExternal(resource)) {
            // In case of a failure delete the external resource as it was not used.
            delete resource;
        }
    }
    return result;
}

String v8ValueToWebCoreString(v8::Handle<v8::Value> object)
{
    if (object->IsString()) 
        return v8StringToWebCoreString(v8::Handle<v8::String>::Cast(object), true);

    if (object->IsInt32()) {
        int value = object->Int32Value();
        // Most numbers used are <= 100. Even if they aren't used there's very little in using the space.
        const int kLowNumbers = 100;
        static AtomicString lowNumbers[kLowNumbers + 1];
        String webCoreString;
        if (0 <= value && value <= kLowNumbers) {
            webCoreString = lowNumbers[value];
            if (!webCoreString) {
                AtomicString valueString = AtomicString(String::number(value));
                lowNumbers[value] = valueString;
                webCoreString = valueString;
            }
        } else 
            webCoreString = String::number(value);
        return webCoreString;
    }

    v8::TryCatch block;
    v8::Handle<v8::String> v8String = object->ToString();
    // Check for empty handles to handle the case where an exception
    // is thrown as part of invoking toString on the objectect.
    if (v8String.IsEmpty())
        return StringImpl::empty();
    return v8StringToWebCoreString(v8String, false);
}

AtomicString v8StringToAtomicWebCoreString(v8::Handle<v8::String> v8String)
{
    String string = v8StringToWebCoreString(v8String, true);
    return AtomicString(string);
}

AtomicString v8ValueToAtomicWebCoreString(v8::Handle<v8::Value> v8String)
{
    String string = v8ValueToWebCoreString(v8String);
    return AtomicString(string);
}

v8::Handle<v8::String> v8String(const String& string)
{
    if (!string.length())
        return v8::String::Empty();
    return v8::String::NewExternal(new WebCoreStringResource(string));
}

v8::Local<v8::String> v8ExternalString(const String& string)
{
    if (!string.length())
        return v8::String::Empty();
    return v8::String::NewExternal(new WebCoreStringResource(string));
}

} // namespace WebCore
