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

#include "SecurityOrigin.h"
#include <wtf/FileSystem.h>
#include <wtf/Ref.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

IDBDatabaseIdentifier::IDBDatabaseIdentifier(const String& databaseName, SecurityOriginData&& openingOrigin, SecurityOriginData&& mainFrameOrigin, bool isTransient)
    : m_databaseName(databaseName)
    , m_origin { WTFMove(mainFrameOrigin), WTFMove(openingOrigin) }
    , m_isTransient(isTransient)
{
    // The empty string is a valid database name, but a null string is not.
    ASSERT(!databaseName.isNull());
}

IDBDatabaseIdentifier IDBDatabaseIdentifier::isolatedCopy() const &
{
    IDBDatabaseIdentifier identifier;
    identifier.m_databaseName = m_databaseName.isolatedCopy();
    identifier.m_origin = m_origin.isolatedCopy();
    identifier.m_isTransient = m_isTransient;
    return identifier;
}

IDBDatabaseIdentifier IDBDatabaseIdentifier::isolatedCopy() &&
{
    IDBDatabaseIdentifier identifier;
    identifier.m_databaseName = WTFMove(m_databaseName).isolatedCopy();
    identifier.m_origin = WTFMove(m_origin).isolatedCopy();
    identifier.m_isTransient = m_isTransient;
    return identifier;
}

String IDBDatabaseIdentifier::databaseDirectoryRelativeToRoot(const String& rootDirectory, ASCIILiteral versionString) const
{
    return databaseDirectoryRelativeToRoot(m_origin, rootDirectory, versionString);
}

String IDBDatabaseIdentifier::databaseDirectoryRelativeToRoot(const ClientOrigin& origin, const String& rootDirectory, ASCIILiteral versionString)
{
    String versionDirectory = FileSystem::pathByAppendingComponent(rootDirectory, StringView { versionString });
    String mainFrameDirectory = FileSystem::pathByAppendingComponent(versionDirectory, origin.topOrigin.databaseIdentifier());

    // If the opening origin and main frame origins are the same, there is no partitioning.
    if (origin.topOrigin == origin.clientOrigin)
        return mainFrameDirectory;

    return FileSystem::pathByAppendingComponent(mainFrameDirectory, origin.clientOrigin.databaseIdentifier());
}

#if !LOG_DISABLED
String IDBDatabaseIdentifier::loggingString() const
{
    return makeString(m_databaseName, "@", m_origin.topOrigin.debugString(), ":", m_origin.clientOrigin.debugString(), m_isTransient ? ", transient" : "");
}
#endif

} // namespace WebCore
