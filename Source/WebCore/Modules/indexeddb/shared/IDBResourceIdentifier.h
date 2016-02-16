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

#ifndef IDBResourceIdentifier_h
#define IDBResourceIdentifier_h

#if ENABLE(INDEXED_DATABASE)

#include <wtf/text/StringHash.h>

namespace WebCore {

namespace IDBClient {
class IDBConnectionToServer;
class IDBRequest;
}

namespace IDBServer {
class IDBConnectionToClient;
}

class IDBResourceIdentifier {
public:
    explicit IDBResourceIdentifier(const IDBClient::IDBConnectionToServer&);
    IDBResourceIdentifier(const IDBClient::IDBConnectionToServer&, const IDBClient::IDBRequest&);
    explicit IDBResourceIdentifier(const IDBServer::IDBConnectionToClient&);

    static IDBResourceIdentifier deletedValue();
    WEBCORE_EXPORT bool isHashTableDeletedValue() const;

    static IDBResourceIdentifier emptyValue();
    bool isEmpty() const
    {
        return !m_resourceNumber && !m_idbConnectionIdentifier;
    }

    unsigned hash() const
    {
        uint64_t hashCodes[2] = { m_idbConnectionIdentifier, m_resourceNumber };
        return StringHasher::hashMemory<sizeof(hashCodes)>(hashCodes);
    }
    
    bool operator==(const IDBResourceIdentifier& other) const
    {
        return m_idbConnectionIdentifier == other.m_idbConnectionIdentifier
            && m_resourceNumber == other.m_resourceNumber;
    }
    
    uint64_t connectionIdentifier() const { return m_idbConnectionIdentifier; }

    IDBResourceIdentifier isolatedCopy() const;

#ifndef NDEBUG
    String loggingString() const;
#endif

private:
    IDBResourceIdentifier() = delete;
    IDBResourceIdentifier(uint64_t connectionIdentifier, uint64_t resourceIdentifier);
    uint64_t m_idbConnectionIdentifier { 0 };
    uint64_t m_resourceNumber { 0 };
};

struct IDBResourceIdentifierHash {
    static unsigned hash(const IDBResourceIdentifier& a) { return a.hash(); }
    static bool equal(const IDBResourceIdentifier& a, const IDBResourceIdentifier& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = false;
};

struct IDBResourceIdentifierHashTraits : WTF::CustomHashTraits<IDBResourceIdentifier> {
    static const bool hasIsEmptyValueFunction = true;
    static const bool emptyValueIsZero = false;

    static IDBResourceIdentifier emptyValue()
    {
        return IDBResourceIdentifier::emptyValue();
    }

    static bool isEmptyValue(const IDBResourceIdentifier& identifier)
    {
        return identifier.isEmpty();
    }

    static void constructDeletedValue(IDBResourceIdentifier& identifier)
    {
        identifier = IDBResourceIdentifier::deletedValue();
    }

    static bool isDeletedValue(const IDBResourceIdentifier& identifier)
    {
        return identifier.isHashTableDeletedValue();
    }
};

} // namespace WebCore

namespace WTF {

template<> struct HashTraits<WebCore::IDBResourceIdentifier> : WebCore::IDBResourceIdentifierHashTraits { };
template<> struct DefaultHash<WebCore::IDBResourceIdentifier> {
    typedef WebCore::IDBResourceIdentifierHash Hash;
};

} // namespace WTF

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBResourceIdentifier_h
