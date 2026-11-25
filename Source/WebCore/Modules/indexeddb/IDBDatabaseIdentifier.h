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

#include <WebCore/ClientOrigin.h>
#include <WebCore/SecurityOriginData.h>
#include <wtf/ArgumentCoder.h>

namespace WebCore {

class SecurityOrigin;

class IDBDatabaseIdentifier {
public:
    IDBDatabaseIdentifier() { }
    IDBDatabaseIdentifier(WTF::HashTableDeletedValueType)
        : m_databaseName(WTF::HashTableDeletedValue)
    {
    }

    WEBCORE_EXPORT IDBDatabaseIdentifier(StringImpl* databaseName, SecurityOriginData&& openingOrigin, SecurityOriginData&& mainFrameOrigin, bool isTransient = false);

    IDBDatabaseIdentifier isolatedCopy() const &;
    IDBDatabaseIdentifier isolatedCopy() &&;

    bool isHashTableDeletedValue() const
    {
        return m_databaseName.isHashTableDeletedValue();
    }

    bool isValid() const
    {
        return !!m_databaseName
            && !m_databaseName.isHashTableDeletedValue();
    }

    bool isEmpty() const
    {
        return !m_databaseName;
    }

    friend bool operator==(const IDBDatabaseIdentifier& a, const IDBDatabaseIdentifier& b)
    {
        return contentEquals(a.m_databaseName, b.m_databaseName)
            && a.m_origin == b.m_origin
            && a.m_isTransient == b.m_isTransient;
    }

    const RefPtr<StringImpl>& databaseName() const LIFETIME_BOUND { return m_databaseName; }
    const ClientOrigin& origin() const LIFETIME_BOUND { return m_origin; }
    bool isTransient() const { return m_isTransient; }

    RefPtr<StringImpl> databaseDirectoryRelativeToRoot(StringImpl* rootDirectory, ASCIILiteral versionString = "v1"_s) const;
    WEBCORE_EXPORT static RefPtr<StringImpl> databaseDirectoryRelativeToRoot(const ClientOrigin&, StringImpl* rootDirectory, ASCIILiteral versionString);
    WEBCORE_EXPORT static RefPtr<StringImpl> optionalDatabaseDirectoryRelativeToRoot(const ClientOrigin&, StringImpl* rootDirectory, ASCIILiteral versionString);

#if !LOG_DISABLED
    RefPtr<StringImpl> loggingString() const;
#endif

private:
    friend struct IPC::ArgumentCoder<IDBDatabaseIdentifier>;

    RefPtr<StringImpl> m_databaseName;
    ClientOrigin m_origin;
    bool m_isTransient { false };
};

inline void add(Hasher& hasher, const IDBDatabaseIdentifier& identifier)
{
    add(hasher, identifier.databaseName(), identifier.origin(), identifier.isTransient());
}

struct IDBDatabaseIdentifierHashTraits : SimpleClassHashTraits<IDBDatabaseIdentifier> {
    static const bool hasIsEmptyValueFunction = true;
    static const bool emptyValueIsZero = false;
    static bool isEmptyValue(const IDBDatabaseIdentifier& info) { return info.isEmpty(); }
};

} // namespace WebCore

namespace WTF {

template<> struct HashTraits<WebCore::IDBDatabaseIdentifier> : WebCore::IDBDatabaseIdentifierHashTraits { };

} // namespace WTF
