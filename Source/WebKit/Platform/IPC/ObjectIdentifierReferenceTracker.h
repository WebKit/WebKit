/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include <utility>
#include <wtf/HashTraits.h>
#include <wtf/Hasher.h>
#include <wtf/Noncopyable.h>
#include <wtf/ObjectIdentifier.h>

namespace IPC {

// Type for the key in mapping from object identifier references to object state.
template<typename T>
class ObjectIdentifierReference {
public:
    ObjectIdentifierReference() = default;
    ObjectIdentifierReference(T identifier, uint64_t version)
        : m_identifier(identifier)
        , m_version(version)
    {
    }
    explicit ObjectIdentifierReference(WTF::HashTableDeletedValueType)
        : m_identifier(WTF::HashTableDeletedValue)
    {
    }
    T identifier() const { return m_identifier; }
    uint64_t version() const { return m_version; }
    static ObjectIdentifierReference<T> generateForAdd() { return { T::generate(), 0 }; }

    bool operator==(const ObjectIdentifierReference& other) const
    {
        return other.m_identifier == m_identifier && other.m_version == m_version;
    }
    bool isHashTableDeletedValue() const
    {
        return m_identifier.isHashTableDeletedValue();
    }
private:
    T m_identifier;
    uint64_t m_version { 0 };
};

template<typename T>
struct ObjectIdentifierReadReference {
public:
    using Reference = ObjectIdentifierReference<T>;
    ObjectIdentifierReadReference(Reference reference)
        : m_reference(reference)
    {
    }
    T identifier() const { return m_reference.identifier(); }
    uint64_t version() const { return m_reference.version(); }
    Reference reference() const { return m_reference; }

private:
    Reference m_reference;
};

template<typename T>
class ObjectIdentifierWriteReference {
public:
    using Reference = ObjectIdentifierReference<T>;

    ObjectIdentifierWriteReference(Reference reference, uint64_t pendingReads)
        : m_reference(reference)
        , m_pendingReads(pendingReads)
    {
    }
    T identifier() const { return m_reference.identifier(); }
    uint64_t version() const { return m_reference.version(); }
    uint64_t pendingReads() const { return m_pendingReads; }
    Reference reference() const { return m_reference; }
    ObjectIdentifierReference<T> retiredReference() const { return { identifier(), version() + 1 }; }

private:
    Reference m_reference;
    uint64_t m_pendingReads { 0 };
};

// Class to generate read and write references to an ObjectIdentifier.
template<typename T>
class ObjectIdentifierReferenceTracker {
    WTF_MAKE_NONCOPYABLE(ObjectIdentifierReferenceTracker);
public:
    using ReadReference = ObjectIdentifierReadReference<T>;
    using WriteReference = ObjectIdentifierWriteReference<T>;
    using Reference = ObjectIdentifierReference<T>;

    explicit ObjectIdentifierReferenceTracker(T identifier)
        : m_identifier(identifier)
    {
    }
    explicit ObjectIdentifierReferenceTracker(Reference reference)
        : m_identifier(reference.identifier())
        , m_version(reference.version())
    {
    }
    // Thread safe.
    ReadReference read() const
    {
        m_pendingReads++;
        return Reference { m_identifier, m_version };
    }
    // Not thread safe. Client must coordinate writes and reads anyway.
    WriteReference write() const
    {
        return { { m_identifier, m_version++ }, m_pendingReads.exchange(0) };
    }
    Reference add() const
    {
        return { m_identifier, m_version };
    }
    T identifier() const
    {
        return m_identifier;
    }
private:
    const T m_identifier;
    mutable std::atomic<uint64_t> m_pendingReads { 0 };
    mutable uint64_t m_version { 0 };
};

template<typename T> inline void add(Hasher& hasher, ObjectIdentifierReference<T> reference)
{
    add(hasher, reference.identifier());
    add(hasher, reference.version());
}

template<typename T>
TextStream& operator<<(TextStream& ts, const IPC::ObjectIdentifierReference<T>& reference)
{
    ts << "ObjectIdentifierReference(" << reference.identifier() << ", " << reference.version() << ")";
    return ts;
}

template<typename T>
TextStream& operator<<(TextStream& ts, const IPC::ObjectIdentifierReadReference<T>& reference)
{
    ts << "ObjectIdentifierReadReference(" << reference.identifier() << ", " << reference.version() << ")";
    return ts;
}

template<typename T>
TextStream& operator<<(TextStream& ts, const IPC::ObjectIdentifierWriteReference<T>& reference)
{
    ts << "ObjectIdentifierWriteReference(" << reference.identifier() << ", " << reference.version() << ", " << reference.pendingReads() << ")";
    return ts;
}

}

namespace WTF {

template<typename T> struct HashTraits<IPC::ObjectIdentifierReference<T>> : SimpleClassHashTraits<IPC::ObjectIdentifierReference<T>> {
    static constexpr bool emptyValueIsZero = HashTraits<T>::emptyValueIsZero;
};

template<typename T> struct DefaultHash<IPC::ObjectIdentifierReference<T>> {
    static unsigned hash(const IPC::ObjectIdentifierReference<T>& reference)
    {
        return computeHash(reference);
    }
    static bool equal(const IPC::ObjectIdentifierReference<T>& a, const IPC::ObjectIdentifierReference<T>& b)
    {
        return a == b;
    }
    static constexpr bool safeToCompareToEmptyOrDeleted = DefaultHash<T>::safeToCompareToEmptyOrDeleted;
};

} // namespace IPC
