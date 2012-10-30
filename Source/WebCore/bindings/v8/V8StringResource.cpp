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

template<class StringClass> struct StringTraits {
    static const StringClass& fromStringResource(WebCoreStringResourceBase*);
    static bool is16BitAtomicString(StringClass&);
    template<bool ascii>
    static StringClass fromV8String(v8::Handle<v8::String>, int);
};

template<>
struct StringTraits<String> {
    static const String& fromStringResource(WebCoreStringResourceBase* resource)
    {
        return resource->webcoreString();
    }
    static bool is16BitAtomicString(String& string)
    {
        return false;
    }
    template<bool ascii>
    static String fromV8String(v8::Handle<v8::String>, int);
};

template<>
struct StringTraits<AtomicString> {
    static const AtomicString& fromStringResource(WebCoreStringResourceBase* resource)
    {
        return resource->atomicString();
    }
    static bool is16BitAtomicString(AtomicString& string)
    {
        return !string.string().is8Bit();
    }
    template<bool ascii>
    static AtomicString fromV8String(v8::Handle<v8::String>, int);
};

template<>
String StringTraits<String>::fromV8String<false>(v8::Handle<v8::String> v8String, int length)
{
    ASSERT(v8String->Length() == length);
    UChar* buffer;
    String result = String::createUninitialized(length, buffer);
    v8String->Write(reinterpret_cast<uint16_t*>(buffer), 0, length);
    return result;
}

template<>
AtomicString StringTraits<AtomicString>::fromV8String<false>(v8::Handle<v8::String> v8String, int length)
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

template<>
String StringTraits<String>::fromV8String<true>(v8::Handle<v8::String> v8String, int length)
{
    ASSERT(v8String->Length() == length);
    LChar* buffer;
    String result = String::createUninitialized(length, buffer);
    v8String->WriteAscii(reinterpret_cast<char*>(buffer), 0, length, v8::String::PRESERVE_ASCII_NULL);
    return result;
}

template<>
inline AtomicString StringTraits<AtomicString>::fromV8String<true>(v8::Handle<v8::String> v8String, int length)
{
    // FIXME: There is no inline fast path for 8 bit atomic strings.
    String result = StringTraits<String>::fromV8String<true>(v8String, length);
    return AtomicString(result);
}

template<typename StringType>
StringType v8StringToWebCoreString(v8::Handle<v8::String> v8String, ExternalMode external)
{
    {
        // A lot of WebCoreStringResourceBase::toWebCoreStringResourceBase is copied here by hand for performance reasons.
        // This portion of this function is very hot in certain Dromeao benchmarks.
        v8::String::Encoding encoding;
        v8::String::ExternalStringResourceBase* resource = v8String->GetExternalStringResourceBase(&encoding);
        if (LIKELY(!!resource)) {
            WebCoreStringResourceBase* base;
            if (encoding == v8::String::ASCII_ENCODING)
                base = static_cast<WebCoreStringResource8*>(resource);
            else
                base = static_cast<WebCoreStringResource16*>(resource);
            return StringTraits<StringType>::fromStringResource(base);
        }
    }

    int length = v8String->Length();
    if (UNLIKELY(!length))
        return String("");

    bool nonAscii = v8String->MayContainNonAscii();
    StringType result(nonAscii ? StringTraits<StringType>::template fromV8String<false>(v8String, length) : StringTraits<StringType>::template fromV8String<true>(v8String, length));

    if (external != Externalize || !v8String->CanMakeExternal())
        return result;

    if (!nonAscii && !StringTraits<StringType>::is16BitAtomicString(result)) {
        WebCoreStringResource8* stringResource = new WebCoreStringResource8(result);
        if (UNLIKELY(!v8String->MakeExternal(stringResource)))
            delete stringResource;
    } else {
        WebCoreStringResource16* stringResource = new WebCoreStringResource16(result);
        if (UNLIKELY(!v8String->MakeExternal(stringResource)))
            delete stringResource;
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
