/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef IDBIdentifier_h
#define IDBIdentifier_h

#if ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)

#include <wtf/HashTraits.h>
#include <wtf/StringHasher.h>

namespace WebKit {

class DatabaseProcessIDBConnection;

class IDBIdentifier {
public:
    IDBIdentifier()
        : m_connection(nullptr)
        , m_identifier(0)
    {
    }

    IDBIdentifier(DatabaseProcessIDBConnection& connection, int64_t identifier)
        : m_connection(&connection)
        , m_identifier(identifier)
    {
    }

    IDBIdentifier isolatedCopy() const
    {
        return *this;
    }

    bool isEmpty() const
    {
        return !m_connection && !m_identifier;
    }

    unsigned hash() const
    {
        uint64_t hashCodes[2] = { reinterpret_cast<uint64_t>(m_connection), static_cast<uint64_t>(m_identifier) };
        return StringHasher::hashMemory<sizeof(hashCodes)>(hashCodes);
    }

    bool operator==(const IDBIdentifier& other) const
    {
        return m_connection == other.m_connection && m_identifier == other.m_identifier;
    }

    IDBIdentifier(WTF::HashTableDeletedValueType)
        : m_connection(nullptr)
        , m_identifier(-1)
    {
    }

    bool isHashTableDeletedValue() const
    {
        return !m_connection && m_identifier == -1;
    }

    DatabaseProcessIDBConnection& connection() const
    {
        ASSERT(m_connection);
        return *m_connection;
    }

    int64_t id() const { return m_identifier; }

private:
    // If any members are added that cannot be safely copied across threads, isolatedCopy() must be updated.
    DatabaseProcessIDBConnection* m_connection;
    int64_t m_identifier;
};

struct IDBIdentifierHash {
    static unsigned hash(const IDBIdentifier& a) { return a.hash(); }
    static bool equal(const IDBIdentifier& a, const IDBIdentifier& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = false;
};

struct IDBIdentifierHashTraits : WTF::SimpleClassHashTraits<IDBIdentifier> {
    static const bool hasIsEmptyValueFunction = true;
    static bool isEmptyValue(const IDBIdentifier& info) { return info.isEmpty(); }
};

} // namespace WebKit

namespace WTF {

template<> struct HashTraits<WebKit::IDBIdentifier> : WebKit::IDBIdentifierHashTraits { };
template<> struct DefaultHash<WebKit::IDBIdentifier> {
    typedef WebKit::IDBIdentifierHash Hash;
};

} // namespace WTF

#endif // ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)
#endif // IDBIdentifier_h
