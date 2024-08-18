/*
 * Copyright (C) 2017-2023 Apple Inc. All rights reserved.
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

#include <wtf/HashTraits.h>
#include <wtf/UUID.h>
#include <wtf/text/TextStream.h>
#include <wtf/text/WTFString.h>

namespace WTF {

template<typename RawValue>
struct ObjectIdentifierThreadSafeAccessTraits {
};

template<>
struct ObjectIdentifierThreadSafeAccessTraits<uint64_t> {
    WTF_EXPORT_PRIVATE static uint64_t generateIdentifierInternal();
};

template<>
struct ObjectIdentifierThreadSafeAccessTraits<UUID> {
    WTF_EXPORT_PRIVATE static UUID generateIdentifierInternal();
};

template<typename RawValue>
struct ObjectIdentifierMainThreadAccessTraits {
};

template<>
struct ObjectIdentifierMainThreadAccessTraits<uint64_t> {
    WTF_EXPORT_PRIVATE static uint64_t generateIdentifierInternal();
};

template<>
struct ObjectIdentifierMainThreadAccessTraits<UUID> {
    WTF_EXPORT_PRIVATE static UUID generateIdentifierInternal();
};

// Extracted from ObjectIdentifierGeneric to avoid binary bloat.
template<typename RawValue>
class ObjectIdentifierGenericBase {
};

template<>
class ObjectIdentifierGenericBase<uint64_t> {
public:
    using RawValue = uint64_t;

    bool isHashTableDeletedValue() const { return m_identifier == hashTableDeletedValue(); }

    RawValue toUInt64() const { return toRawValue(); } // Use `toRawValue` instead.
    RawValue toRawValue() const { return m_identifier; }

    String loggingString() const
    {
        return String::number(m_identifier);
    }

    static bool isValidIdentifier(RawValue identifier) { return identifier && identifier != hashTableDeletedValue(); }

protected:
    explicit constexpr ObjectIdentifierGenericBase(RawValue identifier)
        : m_identifier(identifier)
    {
    }

    ObjectIdentifierGenericBase() = default;
    ~ObjectIdentifierGenericBase() = default;
    ObjectIdentifierGenericBase(HashTableDeletedValueType) : m_identifier(hashTableDeletedValue()) { }

    static RawValue hashTableDeletedValue() { return std::numeric_limits<RawValue>::max(); }

private:
    RawValue m_identifier { 0 };
};

template<>
class ObjectIdentifierGenericBase<UUID> {
public:
    using RawValue = UUID;

    bool isHashTableDeletedValue() const { return m_identifier == hashTableDeletedValue(); }

    RawValue toRawValue() const { return m_identifier; }

    String loggingString() const
    {
        return m_identifier.toString();
    }

    static bool isValidIdentifier(RawValue identifier) { return identifier && identifier != hashTableDeletedValue(); }

protected:
    explicit constexpr ObjectIdentifierGenericBase(RawValue identifier)
        : m_identifier(identifier)
    {
    }

    ObjectIdentifierGenericBase() = default;
    ~ObjectIdentifierGenericBase() = default;
    ObjectIdentifierGenericBase(HashTableDeletedValueType) : m_identifier(hashTableDeletedValue()) { }

    static RawValue hashTableDeletedValue() { return UUID { HashTableDeletedValue }; }

private:
    RawValue m_identifier { UUID::MarkableTraits::emptyValue() };
};

template<typename T, typename ThreadSafety, typename RawValue, SupportsObjectIdentifierNullState supportsNullState>
class ObjectIdentifierGeneric : public ObjectIdentifierGenericBase<RawValue> {
public:
    static ObjectIdentifierGeneric generate()
    {
        RELEASE_ASSERT(!m_generationProtected);
        return ObjectIdentifierGeneric { ThreadSafety::generateIdentifierInternal() };
    }

    static void enableGenerationProtection()
    {
        m_generationProtected = true;
    }

    explicit constexpr ObjectIdentifierGeneric(RawValue identifier)
        : ObjectIdentifierGenericBase<RawValue>(identifier)
    {
        ASSERT(supportsNullState == SupportsObjectIdentifierNullState::Yes || !!identifier);
    }

    bool isValid() const requires(supportsNullState == SupportsObjectIdentifierNullState::Yes) { return ObjectIdentifierGenericBase<RawValue>::isValidIdentifier(ObjectIdentifierGenericBase<RawValue>::toRawValue()); }
    explicit operator bool() const requires(supportsNullState == SupportsObjectIdentifierNullState::Yes) { return ObjectIdentifierGenericBase<RawValue>::toRawValue(); }

    ObjectIdentifierGeneric() requires (supportsNullState == SupportsObjectIdentifierNullState::Yes) = default;

    ObjectIdentifierGeneric(HashTableDeletedValueType) : ObjectIdentifierGenericBase<RawValue>(HashTableDeletedValue) { }

    struct MarkableTraits {
        static bool isEmptyValue(ObjectIdentifierGeneric identifier) { return !identifier.toRawValue(); }
        static constexpr ObjectIdentifierGeneric emptyValue() { return ObjectIdentifierGeneric(InvalidIdValue); }
    };

private:
    friend struct HashTraits<ObjectIdentifierGeneric>;
    template<typename U, typename V> friend struct ObjectIdentifierGenericHash;

    enum InvalidId { InvalidIdValue };
    ObjectIdentifierGeneric(InvalidId)
    {
    }

    inline static bool m_generationProtected { false };
};

template<typename T, typename RawValue> using LegacyNullableObjectIdentifier = ObjectIdentifierGeneric<T, ObjectIdentifierMainThreadAccessTraits<RawValue>, RawValue, SupportsObjectIdentifierNullState::Yes>;
template<typename T, typename RawValue> using LegacyNullableAtomicObjectIdentifier = ObjectIdentifierGeneric<T, ObjectIdentifierThreadSafeAccessTraits<RawValue>, RawValue, SupportsObjectIdentifierNullState::Yes>;
template<typename T, typename RawValue> using ObjectIdentifier = ObjectIdentifierGeneric<T, ObjectIdentifierMainThreadAccessTraits<RawValue>, RawValue, SupportsObjectIdentifierNullState::No>;
template<typename T, typename RawValue> using AtomicObjectIdentifier = ObjectIdentifierGeneric<T, ObjectIdentifierThreadSafeAccessTraits<RawValue>, RawValue, SupportsObjectIdentifierNullState::No>;

inline void add(Hasher& hasher, const ObjectIdentifierGenericBase<uint64_t>& identifier)
{
    add(hasher, identifier.toRawValue());
}

inline void add(Hasher& hasher, const ObjectIdentifierGenericBase<UUID>& identifier)
{
    add(hasher, identifier.toRawValue());
}

template<typename RawValue>
struct ObjectIdentifierGenericBaseHash {
};

template<>
struct ObjectIdentifierGenericBaseHash<uint64_t> {
    static unsigned hash(const ObjectIdentifierGenericBase<uint64_t>& identifier) { return intHash(identifier.toUInt64()); }
    static bool equal(const ObjectIdentifierGenericBase<uint64_t>& a, const ObjectIdentifierGenericBase<uint64_t>& b) { return a.toUInt64() == b.toUInt64(); }
    static constexpr bool safeToCompareToEmptyOrDeleted = true;
};

template<>
struct ObjectIdentifierGenericBaseHash<UUID> {
    static unsigned hash(const ObjectIdentifierGenericBase<UUID>& identifier) { return UUIDHash::hash(identifier.toRawValue()); }
    static bool equal(const ObjectIdentifierGenericBase<UUID>& a, const ObjectIdentifierGenericBase<UUID>& b) { return UUIDHash::equal(a.toRawValue(), b.toRawValue()); }
    static constexpr bool safeToCompareToEmptyOrDeleted = true;
};

template<typename T, typename U, typename V, SupportsObjectIdentifierNullState supportsNullState> struct HashTraits<ObjectIdentifierGeneric<T, U, V, supportsNullState>> : SimpleClassHashTraits<ObjectIdentifierGeneric<T, U, V, supportsNullState>> {
    static ObjectIdentifierGeneric<T, U, V, supportsNullState> emptyValue() { return ObjectIdentifierGeneric<T, U, V, supportsNullState>(ObjectIdentifierGeneric<T, U, V, supportsNullState>::InvalidIdValue); }
};

template<typename T, typename U, typename V, SupportsObjectIdentifierNullState supportsNullState> struct DefaultHash<ObjectIdentifierGeneric<T, U, V, supportsNullState>> : ObjectIdentifierGenericBaseHash<V> { };

WTF_EXPORT_PRIVATE TextStream& operator<<(TextStream&, const ObjectIdentifierGenericBase<uint64_t>&);

WTF_EXPORT_PRIVATE TextStream& operator<<(TextStream&, const ObjectIdentifierGenericBase<UUID>&);

template<typename RawValue>
class ObjectIdentifierGenericBaseStringTypeAdapter {
};

template<>
class ObjectIdentifierGenericBaseStringTypeAdapter<uint64_t> {
public:
    unsigned length() const { return lengthOfIntegerAsString(m_identifier); }
    bool is8Bit() const { return true; }
    template<typename CharacterType> void writeTo(CharacterType* destination) const { writeIntegerToBuffer(m_identifier, destination); }
protected:
    explicit ObjectIdentifierGenericBaseStringTypeAdapter(uint64_t identifier)
        : m_identifier(identifier) { }
private:
    uint64_t m_identifier;
};

template<typename T, typename ThreadSafety, SupportsObjectIdentifierNullState supportsNullState>
class StringTypeAdapter<ObjectIdentifierGeneric<T, ThreadSafety, uint64_t, supportsNullState>> : public ObjectIdentifierGenericBaseStringTypeAdapter<uint64_t> {
public:
    explicit StringTypeAdapter(ObjectIdentifierGeneric<T, ThreadSafety, uint64_t, supportsNullState> identifier)
        : ObjectIdentifierGenericBaseStringTypeAdapter(identifier.toRawValue()) { }
};

template<typename T, typename ThreadSafety, SupportsObjectIdentifierNullState supportsNullState>
class StringTypeAdapter<ObjectIdentifierGeneric<T, ThreadSafety, UUID, supportsNullState>> : public StringTypeAdapter<UUID> {
public:
    explicit StringTypeAdapter(ObjectIdentifierGeneric<T, ThreadSafety, UUID, supportsNullState> identifier)
        : StringTypeAdapter<UUID>(identifier.toRawValue()) { }
};

template<typename T, typename ThreadSafety, SupportsObjectIdentifierNullState supportsNullState>
bool operator==(const ObjectIdentifierGeneric<T, ThreadSafety, UUID, supportsNullState>& a, const ObjectIdentifierGeneric<T, ThreadSafety, UUID, supportsNullState>& b)
{
    return a.toRawValue() == b.toRawValue();
}

template<typename T, typename ThreadSafety, SupportsObjectIdentifierNullState supportsNullState>
bool operator==(const ObjectIdentifierGeneric<T, ThreadSafety, uint64_t, supportsNullState>& a, const ObjectIdentifierGeneric<T, ThreadSafety, uint64_t, supportsNullState>& b)
{
    return a.toRawValue() == b.toRawValue();
}

template<typename T, typename ThreadSafety, SupportsObjectIdentifierNullState supportsNullState>
bool operator>(const ObjectIdentifierGeneric<T, ThreadSafety, uint64_t, supportsNullState>& a, const ObjectIdentifierGeneric<T, ThreadSafety, uint64_t, supportsNullState>& b)
{
    return a.toRawValue() > b.toRawValue();
}

template<typename T, typename ThreadSafety, SupportsObjectIdentifierNullState supportsNullState>
bool operator>=(const ObjectIdentifierGeneric<T, ThreadSafety, uint64_t, supportsNullState>& a, const ObjectIdentifierGeneric<T, ThreadSafety, uint64_t, supportsNullState>& b)
{
    return a.toRawValue() >= b.toRawValue();
}

template<typename T, typename ThreadSafety, SupportsObjectIdentifierNullState supportsNullState>
bool operator<(const ObjectIdentifierGeneric<T, ThreadSafety, uint64_t, supportsNullState>& a, const ObjectIdentifierGeneric<T, ThreadSafety, uint64_t, supportsNullState>& b)
{
    return a.toRawValue() < b.toRawValue();
}

template<typename T, typename ThreadSafety, SupportsObjectIdentifierNullState supportsNullState>
bool operator<=(const ObjectIdentifierGeneric<T, ThreadSafety, uint64_t, supportsNullState>& a, const ObjectIdentifierGeneric<T, ThreadSafety, uint64_t, supportsNullState>& b)
{
    return a.toRawValue() <= b.toRawValue();
}

} // namespace WTF

using WTF::AtomicObjectIdentifier;
using WTF::LegacyNullableAtomicObjectIdentifier;
using WTF::LegacyNullableObjectIdentifier;
using WTF::ObjectIdentifierGenericBase;
using WTF::ObjectIdentifierGeneric;
using WTF::ObjectIdentifier;
