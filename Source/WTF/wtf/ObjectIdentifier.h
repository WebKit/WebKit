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

#include <wtf/Compiler.h>
#include <wtf/HashTraits.h>
#include <wtf/Hasher.h>
#include <wtf/text/WTFString.h>

namespace WTF {

class PrintStream;

struct ObjectIdentifierThreadSafeAccessTraits {
    WTF_EXPORT_PRIVATE static uint64_t generateIdentifierInternal();
};

struct ObjectIdentifierMainThreadAccessTraits {
    WTF_EXPORT_PRIVATE static uint64_t NODELETE generateIdentifierInternal();
};

// Extracted from ObjectIdentifierGeneric to avoid binary bloat.
class ObjectIdentifierGenericBase {
public:
    bool isHashTableDeletedValue() const { return m_identifier == hashTableDeletedValue(); }
    static constexpr bool safeToCompareToHashTableEmptyOrDeletedValue = true;

    uint64_t toUInt64() const { return m_identifier; }

    String loggingString() const
    {
        return String::number(m_identifier);
    }

    static constexpr bool isValidIdentifier(uint64_t identifier) { return identifier && identifier != hashTableDeletedValue(); }

protected:
    explicit constexpr ObjectIdentifierGenericBase(uint64_t identifier)
        : m_identifier(identifier)
    {
    }

    ObjectIdentifierGenericBase() = default;
    ~ObjectIdentifierGenericBase() = default;
    ObjectIdentifierGenericBase(HashTableDeletedValueType) : m_identifier(hashTableDeletedValue()) { }

    static constexpr uint64_t hashTableDeletedValue() { return std::numeric_limits<uint64_t>::max(); }

private:
    uint64_t m_identifier { 0 };
};

template<typename T, typename ThreadSafety>
class ObjectIdentifierGeneric : public ObjectIdentifierGenericBase {
public:
    static ObjectIdentifierGeneric generate()
    {
        RELEASE_ASSERT(!m_generationProtected);
        return ObjectIdentifierGeneric { ThreadSafety::generateIdentifierInternal(), AssumeValidIdValue };
    }

    static void enableGenerationProtection()
    {
        m_generationProtected = true;
    }

    explicit constexpr ObjectIdentifierGeneric(uint64_t identifier)
        : ObjectIdentifierGenericBase(identifier)
    {
        RELEASE_ASSERT(ObjectIdentifierGenericBase::isValidIdentifier(identifier));
    }

    bool isHashTableEmptyValue() const { return !ObjectIdentifierGenericBase::toUInt64(); }

    // Do not call this constructor explicitly, it should only be used by the Hashtable implementation.
    ObjectIdentifierGeneric(HashTableDeletedValueType) : ObjectIdentifierGenericBase(HashTableDeletedValue) { }

private:
    friend struct HashTraits<ObjectIdentifierGeneric>;
    friend struct MarkableTraits<ObjectIdentifierGeneric>;
    template<typename U, typename V> friend struct ObjectIdentifierGenericHash;

    enum AssumeValidId { AssumeValidIdValue };
    explicit constexpr ObjectIdentifierGeneric(uint64_t identifier, AssumeValidId)
        : ObjectIdentifierGenericBase(identifier)
    {
        ASSERT(!!identifier);
    }

    enum InvalidId { InvalidIdValue };
    ObjectIdentifierGeneric(InvalidId)
    {
    }

    inline static bool m_generationProtected { false };
};

template<typename T, typename ThreadSafety>
struct MarkableTraits<ObjectIdentifierGeneric<T, ThreadSafety>> {
    static bool isEmptyValue(ObjectIdentifierGeneric<T, ThreadSafety> identifier) { return !identifier.toUInt64(); }
    static constexpr ObjectIdentifierGeneric<T, ThreadSafety> emptyValue() { return ObjectIdentifierGeneric<T, ThreadSafety>::InvalidIdValue; }
};

template<typename T> using ObjectIdentifier = ObjectIdentifierGeneric<T, ObjectIdentifierMainThreadAccessTraits>;
template<typename T> using AtomicObjectIdentifier = ObjectIdentifierGeneric<T, ObjectIdentifierThreadSafeAccessTraits>;

inline void add(Hasher& hasher, const ObjectIdentifierGenericBase& identifier)
{
    add(hasher, identifier.toUInt64());
}

template<typename T, typename U> struct HashTraits<ObjectIdentifierGeneric<T, U>> : SimpleClassHashTraits<ObjectIdentifierGeneric<T, U>> {
    using ValueType = ObjectIdentifierGeneric<T, U>;
    using PeekType = std::optional<ValueType>;
    using TakeType = std::optional<ValueType>;

    static ValueType emptyValue() { return ValueType { ValueType::InvalidIdValue }; }
    static bool isEmptyValue(ValueType value) { return value.isHashTableEmptyValue(); }

    static PeekType peek(ValueType identifier)
    {
        if (isEmptyValue(identifier))
            return std::nullopt;
        return identifier;
    }

    static TakeType take(ValueType identifier)
    {
        if (isEmptyValue(identifier))
            return std::nullopt;
        return identifier;
    }
};

WTF_EXPORT_PRIVATE void printInternal(PrintStream&, const ObjectIdentifierGenericBase&);

class ObjectIdentifierGenericBaseStringTypeAdapter {
public:
    unsigned length() const { return lengthOfIntegerAsString(m_identifier); }
    bool is8Bit() const { return true; }
    template<typename CharacterType> void writeTo(std::span<CharacterType> destination) const { writeIntegerToBuffer(m_identifier, destination); }
protected:
    explicit ObjectIdentifierGenericBaseStringTypeAdapter(uint64_t identifier)
        : m_identifier(identifier) { }
private:
    uint64_t m_identifier;
};

template<typename T, typename ThreadSafety>
class StringTypeAdapter<ObjectIdentifierGeneric<T, ThreadSafety>> : public ObjectIdentifierGenericBaseStringTypeAdapter {
public:
    explicit StringTypeAdapter(ObjectIdentifierGeneric<T, ThreadSafety> identifier)
        : ObjectIdentifierGenericBaseStringTypeAdapter(identifier.toUInt64()) { }
};

template<typename T, typename ThreadSafety>
bool operator==(const ObjectIdentifierGeneric<T, ThreadSafety>& a, const ObjectIdentifierGeneric<T, ThreadSafety>& b)
{
    return a.toUInt64() == b.toUInt64();
}

template<typename T, typename ThreadSafety>
std::strong_ordering operator<=>(const ObjectIdentifierGeneric<T, ThreadSafety>& a, const ObjectIdentifierGeneric<T, ThreadSafety>& b)
{
    return a.toUInt64() <=> b.toUInt64();
}

} // namespace WTF

using WTF::AtomicObjectIdentifier;
using WTF::ObjectIdentifierGenericBase;
using WTF::ObjectIdentifierGeneric;
using WTF::ObjectIdentifier;
