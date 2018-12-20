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

#include <atomic>
#include <mutex>
#include <wtf/HashTraits.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/WTFString.h>

namespace WTF {

template<typename T> class ObjectIdentifier {
public:
    ObjectIdentifier() = default;

    ObjectIdentifier(HashTableDeletedValueType) : m_identifier(hashTableDeletedValue()) { }
    bool isHashTableDeletedValue() const { return m_identifier == hashTableDeletedValue(); }

    template<typename Encoder> void encode(Encoder& encoder) const
    {
        ASSERT(isValidIdentifier(m_identifier));
        encoder << m_identifier;
    }
    template<typename Decoder> static Optional<ObjectIdentifier> decode(Decoder& decoder)
    {
        Optional<uint64_t> identifier;
        decoder >> identifier;
        if (!identifier)
            return WTF::nullopt;
        ASSERT(isValidIdentifier(*identifier));
        return ObjectIdentifier { *identifier };
    }

    bool operator==(const ObjectIdentifier& other) const
    {
        return m_identifier == other.m_identifier;
    }

    bool operator!=(const ObjectIdentifier& other) const
    {
        return m_identifier != other.m_identifier;
    }

    uint64_t toUInt64() const { return m_identifier; }
    explicit operator bool() const { return m_identifier; }

    String loggingString() const
    {
        return String::number(m_identifier);
    }

private:
    template<typename U> friend ObjectIdentifier<U> generateObjectIdentifier();
    template<typename U> friend ObjectIdentifier<U> generateThreadSafeObjectIdentifier();
    template<typename U> friend ObjectIdentifier<U> makeObjectIdentifier(uint64_t);
    friend struct HashTraits<ObjectIdentifier>;
    template<typename U> friend struct ObjectIdentifierHash;

    static uint64_t hashTableDeletedValue() { return std::numeric_limits<uint64_t>::max(); }
    static bool isValidIdentifier(uint64_t identifier) { return identifier && identifier != hashTableDeletedValue(); }

    explicit ObjectIdentifier(uint64_t identifier)
        : m_identifier(identifier)
    {
    }

    uint64_t m_identifier { 0 };
};

template<typename T> inline ObjectIdentifier<T> generateObjectIdentifier()
{
    static uint64_t currentIdentifier;
    return ObjectIdentifier<T> { ++currentIdentifier };
}

template<typename T> inline ObjectIdentifier<T> generateThreadSafeObjectIdentifier()
{
    static LazyNeverDestroyed<std::atomic<uint64_t>> currentIdentifier;
    static std::once_flag initializeCurrentIdentifier;
    std::call_once(initializeCurrentIdentifier, [] {
        currentIdentifier.construct(0);
    });
    return ObjectIdentifier<T> { ++currentIdentifier.get() };
}

template<typename T> inline ObjectIdentifier<T> makeObjectIdentifier(uint64_t identifier)
{
    return ObjectIdentifier<T> { identifier };
}

template<typename T> struct ObjectIdentifierHash {
    static unsigned hash(const ObjectIdentifier<T>& identifier) { return intHash(identifier.m_identifier); }
    static bool equal(const ObjectIdentifier<T>& a, const ObjectIdentifier<T>& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

template<typename T> struct HashTraits<ObjectIdentifier<T>> : SimpleClassHashTraits<ObjectIdentifier<T>> { };

template<typename T> struct DefaultHash<ObjectIdentifier<T>> {
    typedef ObjectIdentifierHash<T> Hash;
};

} // namespace WTF

using WTF::ObjectIdentifier;
using WTF::generateObjectIdentifier;
using WTF::generateThreadSafeObjectIdentifier;
using WTF::makeObjectIdentifier;
