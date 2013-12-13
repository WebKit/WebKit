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

#ifndef UniqueIDBDatabaseIdentifier_h
#define UniqueIDBDatabaseIdentifier_h

#if ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)

#include "SecurityOriginData.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class UniqueIDBDatabaseIdentifier {
public:
    UniqueIDBDatabaseIdentifier();
    UniqueIDBDatabaseIdentifier(const String& databaseName, const SecurityOriginData& openingOrigin, const SecurityOriginData& mainFrameOrigin);

    bool isNull() const;

    const String& databaseName() const { return m_databaseName; }
    const SecurityOriginData& openingOrigin() const { return m_openingOrigin; }
    const SecurityOriginData& mainFrameOrigin() const { return m_mainFrameOrigin; }

    UniqueIDBDatabaseIdentifier(WTF::HashTableDeletedValueType);
    bool isHashTableDeletedValue() const;
    unsigned hash() const;

    UniqueIDBDatabaseIdentifier isolatedCopy() const;

private:
    String m_databaseName;
    SecurityOriginData m_openingOrigin;
    SecurityOriginData m_mainFrameOrigin;
};

bool operator==(const UniqueIDBDatabaseIdentifier&, const UniqueIDBDatabaseIdentifier&);

struct UniqueIDBDatabaseIdentifierHash {
    static unsigned hash(const UniqueIDBDatabaseIdentifier& identifier) { return identifier.hash(); }
    static bool equal(const UniqueIDBDatabaseIdentifier& a, const UniqueIDBDatabaseIdentifier& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = false;
};

struct UniqueIDBDatabaseIdentifierHashTraits : WTF::SimpleClassHashTraits<UniqueIDBDatabaseIdentifier> {
    static const bool hasIsEmptyValueFunction = true;
    static bool isEmptyValue(const UniqueIDBDatabaseIdentifier& info) { return info.isNull(); }
};

} // namespace WebKit

namespace WTF {

template<> struct HashTraits<WebKit::UniqueIDBDatabaseIdentifier> : WebKit::UniqueIDBDatabaseIdentifierHashTraits { };
template<> struct DefaultHash<WebKit::UniqueIDBDatabaseIdentifier> {
    typedef WebKit::UniqueIDBDatabaseIdentifierHash Hash;
};

} // namespaec WTF

#endif // ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)
#endif // UniqueIDBDatabaseIdentifier_h
