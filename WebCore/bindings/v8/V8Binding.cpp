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
#include "StdLibExtras.h"
#include "StringBuffer.h"
#include "StringHash.h"
#include "Threading.h"
#include "V8Proxy.h"

#include <v8.h>

namespace WebCore {

// WebCoreStringResource is a helper class for v8ExternalString. It is used
// to manage the life-cycle of the underlying buffer of the external string.
class WebCoreStringResource : public v8::String::ExternalStringResource {
public:
    explicit WebCoreStringResource(const String& string)
        : m_plainString(string)
    {
#ifndef NDEBUG
        m_threadId = WTF::currentThread();
#endif
        ASSERT(!string.isNull());
        v8::V8::AdjustAmountOfExternalAllocatedMemory(2 * string.length());
    }

    explicit WebCoreStringResource(const AtomicString& string)
        : m_plainString(string)
        , m_atomicString(string)
    {
#ifndef NDEBUG
        m_threadId = WTF::currentThread();
#endif
        ASSERT(!string.isNull());
        v8::V8::AdjustAmountOfExternalAllocatedMemory(2 * string.length());
    }

    virtual ~WebCoreStringResource()
    {
#ifndef NDEBUG
        ASSERT(m_threadId == WTF::currentThread());
#endif
        int reducedExternalMemory = -2 * m_plainString.length();
        if (m_plainString.impl() != m_atomicString.impl() && !m_atomicString.isNull())
            reducedExternalMemory *= 2;
        v8::V8::AdjustAmountOfExternalAllocatedMemory(reducedExternalMemory);
    }

    virtual const uint16_t* data() const
    {
        return reinterpret_cast<const uint16_t*>(m_plainString.impl()->characters());
    }

    virtual size_t length() const { return m_plainString.impl()->length(); }

    String webcoreString() { return m_plainString; }

    AtomicString atomicString()
    {
#ifndef NDEBUG
        ASSERT(m_threadId == WTF::currentThread());
#endif
        if (m_atomicString.isNull()) {
            m_atomicString = AtomicString(m_plainString);
            ASSERT(!m_atomicString.isNull());
            if (m_plainString.impl() != m_atomicString.impl())
                v8::V8::AdjustAmountOfExternalAllocatedMemory(2 * m_atomicString.length());
        }
        return m_atomicString;
    }

    // Returns right string type based on a dummy parameter.
    String string(String) { return webcoreString(); }
    AtomicString string(AtomicString) { return atomicString(); }

    static WebCoreStringResource* toStringResource(v8::Handle<v8::String> v8String)
    {
        return static_cast<WebCoreStringResource*>(v8String->GetExternalStringResource());
    }

private:
    // A shallow copy of the string. Keeps the string buffer alive until the V8 engine garbage collects it.
    String m_plainString;
    // If this string is atomic or has been made atomic earlier the
    // atomic string is held here. In the case where the string starts
    // off non-atomic and becomes atomic later it is necessary to keep
    // the original string alive because v8 may keep derived pointers
    // into that string.
    AtomicString m_atomicString;

#ifndef NDEBUG
    WTF::ThreadIdentifier m_threadId;
#endif
};

enum ExternalMode {
    Externalize,
    DoNotExternalize
};

template <typename StringType>
static StringType v8StringToWebCoreString(v8::Handle<v8::String> v8String, ExternalMode external)
{
    WebCoreStringResource* stringResource = WebCoreStringResource::toStringResource(v8String);
    if (stringResource)
        return stringResource->string(StringType());

    int length = v8String->Length();
    if (!length) {
        // Avoid trying to morph empty strings, as they do not have enough room to contain the external reference.
        return StringImpl::empty();
    }

    StringType result;
    static const int inlineBufferSize = 16;
    if (length <= inlineBufferSize) {
        UChar inlineBuffer[inlineBufferSize];
        v8String->Write(reinterpret_cast<uint16_t*>(inlineBuffer), 0, length);
        result = StringType(inlineBuffer, length);
    } else {
        UChar* buffer;
        String tmp = String::createUninitialized(length, buffer);
        v8String->Write(reinterpret_cast<uint16_t*>(buffer), 0, length);
        result = StringType(tmp);
    }

    if (external == Externalize && v8String->CanMakeExternal()) {
        stringResource = new WebCoreStringResource(result);
        if (!v8String->MakeExternal(stringResource)) {
            // In case of a failure delete the external resource as it was not used.
            delete stringResource;
        }
    }
    return result;
}

String v8StringToWebCoreString(v8::Handle<v8::String> v8String)
{
    return v8StringToWebCoreString<String>(v8String, Externalize);
}

AtomicString v8StringToAtomicWebCoreString(v8::Handle<v8::String> v8String)
{
    return v8StringToWebCoreString<AtomicString>(v8String, Externalize);
}

String v8NonStringValueToWebCoreString(v8::Handle<v8::Value> object)
{
    ASSERT(!object->IsString());
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
    // Handle the case where an exception is thrown as part of invoking toString on the object.
    if (block.HasCaught()) {
        throwError(block.Exception());
        return StringImpl::empty();
    }
    return v8StringToWebCoreString<String>(v8String, DoNotExternalize);
}

AtomicString v8NonStringValueToAtomicWebCoreString(v8::Handle<v8::Value> object)
{
    ASSERT(!object->IsString());
    return AtomicString(v8NonStringValueToWebCoreString(object));
}

static bool stringImplCacheEnabled = false;

void enableStringImplCache()
{
    stringImplCacheEnabled = true;
}

static v8::Local<v8::String> makeExternalString(const String& string)
{
    WebCoreStringResource* stringResource = new WebCoreStringResource(string);
    v8::Local<v8::String> newString = v8::String::NewExternal(stringResource);
    if (newString.IsEmpty())
        delete stringResource;

    return newString;
}

typedef HashMap<StringImpl*, v8::String*> StringCache;

static StringCache& getStringCache()
{
    ASSERT(WTF::isMainThread());
    DEFINE_STATIC_LOCAL(StringCache, mainThreadStringCache, ());
    return mainThreadStringCache;
}

static void cachedStringCallback(v8::Persistent<v8::Value> wrapper, void* parameter)
{
    ASSERT(WTF::isMainThread());
    StringImpl* stringImpl = static_cast<StringImpl*>(parameter);
    ASSERT(getStringCache().contains(stringImpl));
    getStringCache().remove(stringImpl);
    wrapper.Dispose();
    stringImpl->deref();
}

v8::Local<v8::String> v8ExternalString(const String& string)
{
    if (!string.length())
        return v8::String::Empty();

    if (!stringImplCacheEnabled)
        return makeExternalString(string);

    StringImpl* stringImpl = string.impl();
    StringCache& stringCache = getStringCache();
    v8::String* cachedV8String = stringCache.get(stringImpl);
    if (cachedV8String)
        return v8::Local<v8::String>(cachedV8String);

    v8::Local<v8::String> newString = makeExternalString(string);
    if (newString.IsEmpty())
        return newString;

    v8::Persistent<v8::String> wrapper = v8::Persistent<v8::String>::New(newString);
    if (wrapper.IsEmpty())
        return newString;

    stringImpl->ref();
    wrapper.MakeWeak(stringImpl, cachedStringCallback);
    stringCache.set(stringImpl, *wrapper);

    return newString;
}

} // namespace WebCore
