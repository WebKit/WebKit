/*
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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

#include <wtf/ArgumentCoder.h>
#include <wtf/HashFunctions.h>
#include <wtf/HashTraits.h>
#include <wtf/text/TextStream.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

// MonotonicObjectIdentifier is similar to ObjectIdentifier but it can be monotonically
// increased in place.
// FIXME: merge ObjectIdentifier and MonotonicObjectIdentifier in one class.
template<typename T> class MonotonicObjectIdentifier {
public:
    MonotonicObjectIdentifier() = default;

    MonotonicObjectIdentifier(WTF::HashTableDeletedValueType)
        : m_identifier(hashTableDeletedValue())
    { }

    bool isHashTableDeletedValue() const { return m_identifier == hashTableDeletedValue(); }

    friend bool operator==(MonotonicObjectIdentifier, MonotonicObjectIdentifier) = default;

    bool operator>(const MonotonicObjectIdentifier& other) const
    {
        return m_identifier > other.m_identifier;
    }

    bool operator>=(const MonotonicObjectIdentifier& other) const
    {
        return m_identifier >= other.m_identifier;
    }

    bool operator<(const MonotonicObjectIdentifier& other) const
    {
        return m_identifier < other.m_identifier;
    }

    bool operator<=(const MonotonicObjectIdentifier& other) const
    {
        return m_identifier <= other.m_identifier;
    }

    MonotonicObjectIdentifier& increment()
    {
        ++m_identifier;
        return *this;
    }

    MonotonicObjectIdentifier next() const
    {
        return MonotonicObjectIdentifier(m_identifier + 1);
    }

    uint64_t toUInt64() const { return m_identifier; }
    explicit operator bool() const { return m_identifier; }

    String loggingString() const
    {
        return String::number(m_identifier);
    }

private:
    friend struct IPC::ArgumentCoder<MonotonicObjectIdentifier, void>;
    template<typename U> friend MonotonicObjectIdentifier<U> makeMonotonicObjectIdentifier(uint64_t);
    friend struct HashTraits<MonotonicObjectIdentifier>;
    template<typename U> friend struct MonotonicObjectIdentifierHash;

    static uint64_t hashTableDeletedValue() { return std::numeric_limits<uint64_t>::max(); }
    static bool isValidIdentifier(uint64_t identifier) { return identifier != hashTableDeletedValue(); }

    explicit MonotonicObjectIdentifier(uint64_t identifier)
        : m_identifier(identifier)
    {
    }

    uint64_t m_identifier { 0 };
};

template<typename T>
inline int64_t operator-(const MonotonicObjectIdentifier<T>& a, const MonotonicObjectIdentifier<T>& b)
{
    CheckedInt64 result = CheckedInt64(a.toUInt64() - b.toUInt64());
    return result.hasOverflowed() ? 0 : result.value();
}

template<typename T>
TextStream& operator<<(TextStream& ts, const MonotonicObjectIdentifier<T>& identifier)
{
    ts << identifier.toUInt64();
    return ts;
}

} // namespace WebKit
