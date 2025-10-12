/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#pragma once

#include <JavaScriptCore/JSRetainPtr.h>
#include <JavaScriptCore/JSValueRef.h>
#include <wtf/HashTraits.h>

namespace WebKit {

template<typename T>
class Protected {
public:
    Protected() = default;

    Protected(JSGlobalContextRef context, T value)
        : m_context(context)
        , m_value(value)
    {
        ASSERT(context);
        ASSERT(value);
        protectIfNonNull();
    }

    Protected(Protected&& other) { moveFrom(WTFMove(other)); }

    Protected(const Protected& other) { copyFrom(other); }

    Protected& operator=(Protected&& other)
    {
        moveFrom(WTFMove(other));
        return *this;
    }

    Protected& operator=(const Protected& other)
    {
        copyFrom(other);
        return *this;
    }

    ~Protected() { unprotectIfNonNull(); }

    T get() const { return m_value; }

    explicit operator bool() const { return !!m_value; }

    bool operator==(const Protected& other) const { return m_value == other.m_value; }

    Protected(WTF::HashTableDeletedValueType)
        : m_value(reinterpret_cast<T>(-1)) { }

    bool isHashTableDeletedValue() const { return m_value == reinterpret_cast<T>(-1); }

private:
    void moveFrom(Protected&& other)
    {
        unprotectIfNonNull();
        m_context = std::exchange(other.m_context, nullptr);
        m_value = std::exchange(other.m_value, nullptr);
    }

    void copyFrom(const Protected& other)
    {
        unprotectIfNonNull();
        m_context = other.m_context;
        m_value = other.m_value;
        protectIfNonNull();
    }

    void protectIfNonNull()
    {
        if (m_value)
            JSValueProtect(m_context.get(), m_value);
    }

    void unprotectIfNonNull()
    {
        if (T value = std::exchange(m_value, nullptr))
            JSValueUnprotect(m_context.get(), value);
        m_context = nullptr;
    }

    JSRetainPtr<JSGlobalContextRef> m_context;
    T m_value { nullptr };
};

}

namespace WTF {
template<typename T> struct HashTraits<WebKit::Protected<T>> : SimpleClassHashTraits<WebKit::Protected<T>> { };
template<typename T> struct DefaultHash<WebKit::Protected<T>> {
    static unsigned hash(const WebKit::Protected<T>& key) { return IntHash<uintptr_t>::hash(reinterpret_cast<uintptr_t>(key.get())); }
    static bool equal(const WebKit::Protected<T>& a, const WebKit::Protected<T>& b) { return a.get() == b.get(); }
    static constexpr bool safeToCompareToEmptyOrDeleted = true;
};
}
