/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef IDBRequestIdentifier_h
#define IDBRequestIdentifier_h

#if ENABLE(INDEXED_DATABASE)

#include <wtf/text/StringHash.h>

namespace WebCore {

namespace IDBClient {
class IDBConnectionToServer;
class IDBRequest;
}

class IDBRequestIdentifier {
public:
    IDBRequestIdentifier(const IDBClient::IDBConnectionToServer&);
    IDBRequestIdentifier(const IDBClient::IDBConnectionToServer&, const IDBClient::IDBRequest&);

    static IDBRequestIdentifier deletedValue();
    bool isHashTableDeletedValue() const;

    static IDBRequestIdentifier emptyValue();
    bool isEmpty() const
    {
        return !m_requestNumber && !m_idbClientServerConnectionNumber;
    }

    unsigned hash() const
    {
        uint64_t hashCodes[2] = { reinterpret_cast<uint64_t>(m_idbClientServerConnectionNumber), static_cast<uint64_t>(m_requestNumber) };
        return StringHasher::hashMemory<sizeof(hashCodes)>(hashCodes);
    }
    
    bool operator==(const IDBRequestIdentifier& other) const
    {
        return m_idbClientServerConnectionNumber == other.m_idbClientServerConnectionNumber
            && m_requestNumber == other.m_requestNumber;
    }
    
    uint64_t connectionIdentifier() const { return m_idbClientServerConnectionNumber; }
    
private:
    IDBRequestIdentifier();
    uint64_t m_idbClientServerConnectionNumber { 0 };
    uint64_t m_requestNumber;
};

struct IDBRequestIdentifierHash {
    static unsigned hash(const IDBRequestIdentifier& a) { return a.hash(); }
    static bool equal(const IDBRequestIdentifier& a, const IDBRequestIdentifier& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = false;
};

struct IDBRequestIdentifierHashTraits : WTF::CustomHashTraits<IDBRequestIdentifier> {
    static const bool hasIsEmptyValueFunction = true;
    static const bool emptyValueIsZero = false;

    static IDBRequestIdentifier emptyValue()
    {
        return IDBRequestIdentifier::emptyValue();
    }

    static bool isEmptyValue(const IDBRequestIdentifier& identifier)
    {
        return identifier.isEmpty();
    }

    static void constructDeletedValue(IDBRequestIdentifier& identifier)
    {
        identifier = IDBRequestIdentifier::deletedValue();
    }

    static bool isDeletedValue(const IDBRequestIdentifier& identifier)
    {
        return identifier.isHashTableDeletedValue();
    }
};

} // namespace WebCore

namespace WTF {

template<> struct HashTraits<WebCore::IDBRequestIdentifier> : WebCore::IDBRequestIdentifierHashTraits { };
template<> struct DefaultHash<WebCore::IDBRequestIdentifier> {
    typedef WebCore::IDBRequestIdentifierHash Hash;
};

} // namespace WTF

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBRequestIdentifier_h
