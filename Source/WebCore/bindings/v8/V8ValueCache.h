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

#ifndef V8ValueCache_h
#define V8ValueCache_h

#include <v8.h>
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class ExternalStringVisitor;
class MemoryObjectInfo;

class StringCache {
public:
    StringCache() { }

    v8::Local<v8::String> v8ExternalString(StringImpl* stringImpl, v8::Isolate* isolate)
    {
        if (m_lastStringImpl.get() == stringImpl) {
            ASSERT(!m_lastV8String.IsNearDeath());
            ASSERT(!m_lastV8String.IsEmpty());
            return v8::Local<v8::String>::New(m_lastV8String);
        }

        return v8ExternalStringSlow(stringImpl, isolate);
    }

    void clearOnGC() 
    {
        m_lastStringImpl = 0;
        m_lastV8String.Clear();
    }

    void remove(StringImpl*);
    void reportMemoryUsage(MemoryObjectInfo*) const;

private:
    v8::Local<v8::String> v8ExternalStringSlow(StringImpl*, v8::Isolate*);

    HashMap<StringImpl*, v8::String*> m_stringCache;
    v8::Persistent<v8::String> m_lastV8String;
    // Note: RefPtr is a must as we cache by StringImpl* equality, not identity
    // hence lastStringImpl might be not a key of the cache (in sense of identity)
    // and hence it's not refed on addition.
    RefPtr<StringImpl> m_lastStringImpl;
};

const int numberOfCachedSmallIntegers = 64;

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
        : m_plainString(string.string())
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

    void visitStrings(ExternalStringVisitor*);

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

class IntegerCache {
public:
     IntegerCache() : m_initialized(false) { };
    ~IntegerCache();

    v8::Handle<v8::Integer> v8Integer(int value)
    {
        if (!m_initialized)
            createSmallIntegers();
        if (0 <= value && value < numberOfCachedSmallIntegers)
            return m_smallIntegers[value];
        return v8::Integer::New(value);
    }

    v8::Handle<v8::Integer> v8UnsignedInteger(unsigned value)
    {
        if (!m_initialized)
            createSmallIntegers();
        if (value < static_cast<unsigned>(numberOfCachedSmallIntegers))
            return m_smallIntegers[value];
        return v8::Integer::NewFromUnsigned(value);
    }

private:
    void createSmallIntegers();

    v8::Persistent<v8::Integer> m_smallIntegers[numberOfCachedSmallIntegers];
    bool m_initialized;
};
    
} // namespace WebCore

#endif // V8ValueCache_h
