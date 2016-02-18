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

#ifndef LegacyUniqueIDBDatabaseIdentifier_h
#define LegacyUniqueIDBDatabaseIdentifier_h

#if ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)

#include <WebCore/SecurityOriginData.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class LegacyUniqueIDBDatabaseIdentifier {
public:
    LegacyUniqueIDBDatabaseIdentifier();
    LegacyUniqueIDBDatabaseIdentifier(const String& databaseName, const WebCore::SecurityOriginData& openingOrigin, const WebCore::SecurityOriginData& mainFrameOrigin);

    bool isNull() const;

    const String& databaseName() const { return m_databaseName; }
    const WebCore::SecurityOriginData& openingOrigin() const { return m_openingOrigin; }
    const WebCore::SecurityOriginData& mainFrameOrigin() const { return m_mainFrameOrigin; }

    LegacyUniqueIDBDatabaseIdentifier(WTF::HashTableDeletedValueType);
    bool isHashTableDeletedValue() const;
    unsigned hash() const;

    LegacyUniqueIDBDatabaseIdentifier isolatedCopy() const;

private:
    String m_databaseName;
    WebCore::SecurityOriginData m_openingOrigin;
    WebCore::SecurityOriginData m_mainFrameOrigin;
};

bool operator==(const LegacyUniqueIDBDatabaseIdentifier&, const LegacyUniqueIDBDatabaseIdentifier&);

struct LegacyUniqueIDBDatabaseIdentifierHash {
    static unsigned hash(const LegacyUniqueIDBDatabaseIdentifier& identifier) { return identifier.hash(); }
    static bool equal(const LegacyUniqueIDBDatabaseIdentifier& a, const LegacyUniqueIDBDatabaseIdentifier& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = false;
};

struct LegacyUniqueIDBDatabaseIdentifierHashTraits : WTF::SimpleClassHashTraits<LegacyUniqueIDBDatabaseIdentifier> {
    static const bool hasIsEmptyValueFunction = true;
    static bool isEmptyValue(const LegacyUniqueIDBDatabaseIdentifier& info) { return info.isNull(); }
};

} // namespace WebKit

namespace WTF {

template<> struct HashTraits<WebKit::LegacyUniqueIDBDatabaseIdentifier> : WebKit::LegacyUniqueIDBDatabaseIdentifierHashTraits { };
template<> struct DefaultHash<WebKit::LegacyUniqueIDBDatabaseIdentifier> {
    typedef WebKit::LegacyUniqueIDBDatabaseIdentifierHash Hash;
};

} // namespaec WTF

#endif // ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)
#endif // LegacyUniqueIDBDatabaseIdentifier_h
