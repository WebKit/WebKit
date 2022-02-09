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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ClientOrigin.h"
#include "SecurityOriginData.h"

namespace WebCore {

class SecurityOrigin;

class IDBDatabaseIdentifier {
public:
    IDBDatabaseIdentifier() { }
    IDBDatabaseIdentifier(WTF::HashTableDeletedValueType)
        : m_databaseName(WTF::HashTableDeletedValue)
    {
    }

    WEBCORE_EXPORT IDBDatabaseIdentifier(const String& databaseName, SecurityOriginData&& openingOrigin, SecurityOriginData&& mainFrameOrigin, bool isTransient = false);

    IDBDatabaseIdentifier isolatedCopy() const;

    bool isHashTableDeletedValue() const
    {
        return m_databaseName.isHashTableDeletedValue();
    }

    bool isValid() const
    {
        return !m_databaseName.isNull()
            && !m_databaseName.isHashTableDeletedValue();
    }

    bool isEmpty() const
    {
        return m_databaseName.isNull();
    }

    bool operator==(const IDBDatabaseIdentifier& other) const
    {
        return other.m_databaseName == m_databaseName && other.m_origin == m_origin && other.m_isTransient == m_isTransient;
    }

    const String& databaseName() const { return m_databaseName; }
    const ClientOrigin& origin() const { return m_origin; }
    bool isTransient() const { return m_isTransient; }

    String databaseDirectoryRelativeToRoot(const String& rootDirectory, const String& versionString="v1") const;
    WEBCORE_EXPORT static String databaseDirectoryRelativeToRoot(const SecurityOriginData& topLevelOrigin, const SecurityOriginData& openingOrigin, const String& rootDirectory, const String& versionString);

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<IDBDatabaseIdentifier> decode(Decoder&);

#if !LOG_DISABLED
    String loggingString() const;
#endif

    bool isRelatedToOrigin(const SecurityOriginData& other) const { return m_origin.isRelated(other); }

private:
    String m_databaseName;
    ClientOrigin m_origin;
    bool m_isTransient { false };
};

inline void add(Hasher& hasher, const IDBDatabaseIdentifier& identifier)
{
    add(hasher, identifier.databaseName(), identifier.origin(), identifier.isTransient());
}

struct IDBDatabaseIdentifierHash {
    static unsigned hash(const IDBDatabaseIdentifier& a) { return computeHash(a); }
    static bool equal(const IDBDatabaseIdentifier& a, const IDBDatabaseIdentifier& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = false;
};

struct IDBDatabaseIdentifierHashTraits : SimpleClassHashTraits<IDBDatabaseIdentifier> {
    static const bool hasIsEmptyValueFunction = true;
    static const bool emptyValueIsZero = false;
    static bool isEmptyValue(const IDBDatabaseIdentifier& info) { return info.isEmpty(); }
};

template<class Encoder>
void IDBDatabaseIdentifier::encode(Encoder& encoder) const
{
    encoder << m_databaseName << m_origin << m_isTransient;
}

template<class Decoder>
std::optional<IDBDatabaseIdentifier> IDBDatabaseIdentifier::decode(Decoder& decoder)
{
    std::optional<String> databaseName;
    decoder >> databaseName;
    if (!databaseName)
        return std::nullopt;

    std::optional<ClientOrigin> origin;
    decoder >> origin;
    if (!origin)
        return std::nullopt;

    std::optional<bool> isTransient;
    decoder >> isTransient;
    if (!isTransient)
        return std::nullopt;

    IDBDatabaseIdentifier identifier;
    identifier.m_databaseName = WTFMove(*databaseName); // FIXME: When decoding from IPC, databaseName can be null, and the non-empty constructor asserts that this is not the case.
    identifier.m_origin = WTFMove(*origin);
    identifier.m_isTransient = *isTransient;
    return identifier;
}

} // namespace WebCore

namespace WTF {

template<> struct HashTraits<WebCore::IDBDatabaseIdentifier> : WebCore::IDBDatabaseIdentifierHashTraits { };
template<> struct DefaultHash<WebCore::IDBDatabaseIdentifier> : WebCore::IDBDatabaseIdentifierHash { };

} // namespace WTF
