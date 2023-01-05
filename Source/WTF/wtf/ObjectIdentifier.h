/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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
#include <wtf/Int128.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/TextStream.h>
#include <wtf/text/WTFString.h>

namespace WTF {

class ObjectIdentifierBase {
protected:
    WTF_EXPORT_PRIVATE static uint64_t generateIdentifierInternal();
    WTF_EXPORT_PRIVATE static uint64_t generateThreadSafeIdentifierInternal();
};

template<typename T> class ObjectIdentifier : private ObjectIdentifierBase {
public:
    static ObjectIdentifier generate()
    {
        RELEASE_ASSERT(!m_generationProtected);
        return ObjectIdentifier { generateIdentifierInternal() };
    }

    static ObjectIdentifier generateThreadSafe()
    {
        RELEASE_ASSERT(!m_generationProtected);
        return ObjectIdentifier { generateThreadSafeIdentifierInternal() };
    }

    static void enableGenerationProtection()
    {
        m_generationProtected = true;
    }

    ObjectIdentifier() = default;

    ObjectIdentifier(HashTableDeletedValueType) : m_identifier(hashTableDeletedValue()) { }
    bool isHashTableDeletedValue() const { return m_identifier == hashTableDeletedValue(); }
    bool isValid() const { return isValidIdentifier(m_identifier); }

    template<typename Encoder> void encode(Encoder& encoder) const
    {
        ASSERT(isValidIdentifier(m_identifier));
        encoder << m_identifier;
    }

    template<typename Decoder> static std::optional<ObjectIdentifier> decode(Decoder& decoder)
    {
        std::optional<uint64_t> identifier;
        decoder >> identifier;
        if (!identifier || !isValidIdentifier(*identifier))
            return std::nullopt;
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

    bool operator>(const ObjectIdentifier& other) const
    {
        return m_identifier > other.m_identifier;
    }

    bool operator>=(const ObjectIdentifier& other) const
    {
        return m_identifier >= other.m_identifier;
    }

    bool operator<(const ObjectIdentifier& other) const
    {
        return m_identifier < other.m_identifier;
    }

    bool operator<=(const ObjectIdentifier& other) const
    {
        return m_identifier <= other.m_identifier;
    }

    uint64_t toUInt64() const { return m_identifier; }
    explicit operator bool() const { return m_identifier; }

    String loggingString() const
    {
        return String::number(m_identifier);
    }

    struct MarkableTraits {
        static bool isEmptyValue(ObjectIdentifier identifier)
        {
            return !identifier.m_identifier;
        }

        static constexpr ObjectIdentifier emptyValue()
        {
            return ObjectIdentifier();
        }
    };

private:
    template<typename U> friend ObjectIdentifier<U> makeObjectIdentifier(UInt128);
    friend struct HashTraits<ObjectIdentifier>;
    template<typename U> friend struct ObjectIdentifierHash;

    static uint64_t hashTableDeletedValue() { return std::numeric_limits<uint64_t>::max(); }
    static bool isValidIdentifier(uint64_t identifier) { return identifier && identifier != hashTableDeletedValue(); }

    explicit constexpr ObjectIdentifier(uint64_t identifier)
        : m_identifier(identifier)
    {
    }

    uint64_t m_identifier { 0 };
    inline static bool m_generationProtected { false };
};

template<typename T> inline ObjectIdentifier<T> makeObjectIdentifier(UInt128 identifier)
{
    ASSERT(identifier == static_cast<uint64_t>(identifier));
    return ObjectIdentifier<T> { static_cast<uint64_t>(identifier) };
}

template<typename T> inline void add(Hasher& hasher, ObjectIdentifier<T> identifier)
{
    add(hasher, identifier.toUInt64());
}

template<typename T> struct ObjectIdentifierHash {
    static unsigned hash(const ObjectIdentifier<T>& identifier) { return intHash(identifier.m_identifier); }
    static bool equal(const ObjectIdentifier<T>& a, const ObjectIdentifier<T>& b) { return a == b; }
    static constexpr bool safeToCompareToEmptyOrDeleted = true;
};

template<typename T> struct HashTraits<ObjectIdentifier<T>> : SimpleClassHashTraits<ObjectIdentifier<T>> { };

template<typename T> struct DefaultHash<ObjectIdentifier<T>> : ObjectIdentifierHash<T> { };

template<typename T>
TextStream& operator<<(TextStream& ts, const ObjectIdentifier<T>& identifier)
{
    ts << identifier.toUInt64();
    return ts;
}

} // namespace WTF

using WTF::ObjectIdentifier;
using WTF::makeObjectIdentifier;
