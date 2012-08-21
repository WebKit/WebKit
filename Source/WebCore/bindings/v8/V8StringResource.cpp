/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "V8StringResource.h"

#include "V8Binding.h"

namespace WebCore {

template <class S> struct StringTraits {
    static S fromStringResource(WebCoreStringResource*);
    static S fromV8String(v8::Handle<v8::String>, int);
};

template<>
struct StringTraits<String> {
    static String fromStringResource(WebCoreStringResource* resource)
    {
        return resource->webcoreString();
    }

    static String fromV8String(v8::Handle<v8::String> v8String, int length)
    {
        ASSERT(v8String->Length() == length);
        // NOTE: as of now, String(const UChar*, int) performs String::createUninitialized
        // anyway, so no need to optimize like we do for AtomicString below.
        UChar* buffer;
        String result = String::createUninitialized(length, buffer);
        v8String->Write(reinterpret_cast<uint16_t*>(buffer), 0, length);
        return result;
    }
};

template<>
struct StringTraits<AtomicString> {
    static AtomicString fromStringResource(WebCoreStringResource* resource)
    {
        return resource->atomicString();
    }

    static AtomicString fromV8String(v8::Handle<v8::String> v8String, int length)
    {
        ASSERT(v8String->Length() == length);
        static const int inlineBufferSize = 16;
        if (length <= inlineBufferSize) {
            UChar inlineBuffer[inlineBufferSize];
            v8String->Write(reinterpret_cast<uint16_t*>(inlineBuffer), 0, length);
            return AtomicString(inlineBuffer, length);
        }
        UChar* buffer;
        String string = String::createUninitialized(length, buffer);
        v8String->Write(reinterpret_cast<uint16_t*>(buffer), 0, length);
        return AtomicString(string);
    }
};

template <typename StringType>
StringType v8StringToWebCoreString(v8::Handle<v8::String> v8String, ExternalMode external)
{
    WebCoreStringResource* stringResource = WebCoreStringResource::toStringResource(v8String);
    if (stringResource)
        return StringTraits<StringType>::fromStringResource(stringResource);

    int length = v8String->Length();
    if (!length) {
        // Avoid trying to morph empty strings, as they do not have enough room to contain the external reference.
        return String();
    }

    StringType result(StringTraits<StringType>::fromV8String(v8String, length));

    if (external == Externalize && v8String->CanMakeExternal()) {
        stringResource = new WebCoreStringResource(result);
        if (!v8String->MakeExternal(stringResource)) {
            // In case of a failure delete the external resource as it was not used.
            delete stringResource;
        }
    }
    return result;
}
    
// Explicitly instantiate the above template with the expected parameterizations,
// to ensure the compiler generates the code; otherwise link errors can result in GCC 4.4.
template String v8StringToWebCoreString<String>(v8::Handle<v8::String>, ExternalMode);
template AtomicString v8StringToWebCoreString<AtomicString>(v8::Handle<v8::String>, ExternalMode);

// Fast but non thread-safe version.
String int32ToWebCoreStringFast(int value)
{
    // Caching of small strings below is not thread safe: newly constructed AtomicString
    // are not safely published.
    ASSERT(isMainThread());

    // Most numbers used are <= 100. Even if they aren't used there's very little cost in using the space.
    const int kLowNumbers = 100;
    DEFINE_STATIC_LOCAL(Vector<AtomicString>, lowNumbers, (kLowNumbers + 1));
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

String int32ToWebCoreString(int value)
{
    // If we are on the main thread (this should always true for non-workers), call the faster one.
    if (isMainThread())
        return int32ToWebCoreStringFast(value);
    return String::number(value);
}

} // namespace WebCore
