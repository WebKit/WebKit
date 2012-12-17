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

#include "ScopedPersistent.h"
#include <v8.h>
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class StringCache {
public:
    StringCache() { }

    v8::Local<v8::String> v8ExternalString(StringImpl* stringImpl, v8::Isolate* isolate)
    {
        if (m_lastStringImpl.get() == stringImpl && m_lastV8String.IsWeak(isolate))
            return v8::Local<v8::String>::New(isolate, m_lastV8String);

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

class IntegerCache {
public:
    IntegerCache();

    v8::Handle<v8::Integer> v8Integer(int value, v8::Isolate* isolate)
    {
        if (0 <= value && value < numberOfCachedSmallIntegers)
            return m_smallIntegers[value].get();
        return v8::Integer::New(value, isolate);
    }

    v8::Handle<v8::Integer> v8UnsignedInteger(unsigned value, v8::Isolate* isolate)
    {
        if (value < static_cast<unsigned>(numberOfCachedSmallIntegers))
            return m_smallIntegers[value].get();
        return v8::Integer::NewFromUnsigned(value, isolate);
    }

private:
    ScopedPersistent<v8::Integer> m_smallIntegers[numberOfCachedSmallIntegers];
};

} // namespace WebCore

#endif // V8ValueCache_h
