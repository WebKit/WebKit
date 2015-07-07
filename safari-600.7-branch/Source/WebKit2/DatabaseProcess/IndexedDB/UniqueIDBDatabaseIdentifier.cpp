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

#include "config.h"
#include "UniqueIDBDatabaseIdentifier.h"

#if ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)

#include <wtf/text/StringBuilder.h>

namespace WebKit {

UniqueIDBDatabaseIdentifier::UniqueIDBDatabaseIdentifier()
{
}

UniqueIDBDatabaseIdentifier::UniqueIDBDatabaseIdentifier(const String& databaseName, const SecurityOriginData& openingOrigin, const SecurityOriginData& mainFrameOrigin)
    : m_databaseName(databaseName)
    , m_openingOrigin(openingOrigin)
    , m_mainFrameOrigin(mainFrameOrigin)
{
    // While it is valid to have an empty database name, it is not valid to have a null one.
    ASSERT(!m_databaseName.isNull());
}

UniqueIDBDatabaseIdentifier::UniqueIDBDatabaseIdentifier(WTF::HashTableDeletedValueType)
    : m_databaseName(WTF::HashTableDeletedValue)
{
}

bool UniqueIDBDatabaseIdentifier::isHashTableDeletedValue() const
{
    return m_databaseName.isHashTableDeletedValue();
}

unsigned UniqueIDBDatabaseIdentifier::hash() const
{
    unsigned hashCodes[7] = {
        m_databaseName.impl() ? m_databaseName.impl()->hash() : 0,
        m_openingOrigin.protocol.impl() ? m_openingOrigin.protocol.impl()->hash() : 0,
        m_openingOrigin.host.impl() ? m_openingOrigin.host.impl()->hash() : 0,
        static_cast<unsigned>(m_openingOrigin.port),
        m_mainFrameOrigin.protocol.impl() ? m_mainFrameOrigin.protocol.impl()->hash() : 0,
        m_mainFrameOrigin.host.impl() ? m_mainFrameOrigin.host.impl()->hash() : 0,
        static_cast<unsigned>(m_mainFrameOrigin.port)
    };
    return StringHasher::hashMemory<sizeof(hashCodes)>(hashCodes);
}

bool UniqueIDBDatabaseIdentifier::isNull() const
{
    // Only a default constructed UniqueIDBDatabaseIdentifier can have a null database name.
    return m_databaseName.isNull();
}

UniqueIDBDatabaseIdentifier UniqueIDBDatabaseIdentifier::isolatedCopy() const
{
    UniqueIDBDatabaseIdentifier result;

    result.m_databaseName = m_databaseName.isolatedCopy();
    result.m_openingOrigin = m_openingOrigin.isolatedCopy();
    result.m_mainFrameOrigin = m_mainFrameOrigin.isolatedCopy();

    return result;
}

bool operator==(const UniqueIDBDatabaseIdentifier& a, const UniqueIDBDatabaseIdentifier& b)
{
    if (&a == &b)
        return true;

    return a.databaseName() == b.databaseName()
        && a.openingOrigin() == b.openingOrigin()
        && a.mainFrameOrigin() == b.mainFrameOrigin();
}

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)
