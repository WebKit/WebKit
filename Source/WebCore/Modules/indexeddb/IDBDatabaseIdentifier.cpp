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

#include "config.h"
#include "IDBDatabaseIdentifier.h"

#if ENABLE(INDEXED_DATABASE)

#include "SecurityOrigin.h"
#include <wtf/FileSystem.h>
#include <wtf/Ref.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

IDBDatabaseIdentifier::IDBDatabaseIdentifier(const String& databaseName, const PAL::SessionID& sessionID, SecurityOriginData&& openingOrigin, SecurityOriginData&& mainFrameOrigin)
    : m_databaseName(databaseName)
    , m_sessionID(sessionID)
    , m_origin { WTFMove(openingOrigin), WTFMove(mainFrameOrigin) }
{
    // The empty string is a valid database name, but a null string is not.
    ASSERT(!databaseName.isNull());
}

IDBDatabaseIdentifier IDBDatabaseIdentifier::isolatedCopy() const
{
    IDBDatabaseIdentifier identifier;

    identifier.m_databaseName = m_databaseName.isolatedCopy();
    identifier.m_sessionID = m_sessionID.isolatedCopy();
    identifier.m_origin = m_origin.isolatedCopy();

    return identifier;
}

String IDBDatabaseIdentifier::databaseDirectoryRelativeToRoot(const String& rootDirectory) const
{
    return databaseDirectoryRelativeToRoot(m_origin.topOrigin, m_origin.clientOrigin, rootDirectory);
}

String IDBDatabaseIdentifier::databaseDirectoryRelativeToRoot(const SecurityOriginData& topLevelOrigin, const SecurityOriginData& openingOrigin, const String& rootDirectory)
{
    String mainFrameDirectory = FileSystem::pathByAppendingComponent(rootDirectory, topLevelOrigin.databaseIdentifier());

    // If the opening origin and main frame origins are the same, there is no partitioning.
    if (openingOrigin == topLevelOrigin)
        return mainFrameDirectory;

    return FileSystem::pathByAppendingComponent(mainFrameDirectory, openingOrigin.databaseIdentifier());
}

#if !LOG_DISABLED
String IDBDatabaseIdentifier::debugString() const
{
    return makeString(m_databaseName, "@", m_origin.topOrigin.debugString(), ":", m_origin.clientOrigin.debugString());
}
#endif

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
