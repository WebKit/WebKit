/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include <wtf/HashFunctions.h>
#include <wtf/HashTraits.h>
#include <wtf/Optional.h>
#include <wtf/text/TextStream.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

template<typename T> class MonotonicObjectIdentifier {
public:
    MonotonicObjectIdentifier() = default;

    MonotonicObjectIdentifier(WTF::HashTableDeletedValueType)
        : m_identifier(hashTableDeletedValue())
    { }

    bool isHashTableDeletedValue() const { return m_identifier == hashTableDeletedValue(); }

    template<typename Encoder> void encode(Encoder& encoder) const
    {
        ASSERT(isValidIdentifier(m_identifier));
        encoder << m_identifier;
    }
    template<typename Decoder> static Optional<MonotonicObjectIdentifier> decode(Decoder& decoder)
    {
        Optional<uint64_t> identifier;
        decoder >> identifier;
        if (!identifier)
            return WTF::nullopt;
        ASSERT(isValidIdentifier(*identifier));
        return MonotonicObjectIdentifier { *identifier };
    }

    bool operator==(const MonotonicObjectIdentifier& other) const
    {
        return m_identifier == other.m_identifier;
    }

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

    bool operator!=(const MonotonicObjectIdentifier& other) const
    {
        return m_identifier != other.m_identifier;
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
TextStream& operator<<(TextStream& ts, const MonotonicObjectIdentifier<T>& identifier)
{
    ts << identifier.toUInt64();
    return ts;
}

enum TransactionIDType { };
using TransactionID = MonotonicObjectIdentifier<TransactionIDType>;

}
