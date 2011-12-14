/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SQLiteFileSystem.h"

#include "PlatformBridge.h"
#include "SQLiteDatabase.h"
#include <sqlite3.h>
#include <wtf/text/CString.h>

// SQLiteFileSystem::registerSQLiteVFS() is implemented in the
// platform-specific files SQLiteFileSystemChromium{Win|Posix}.cpp
namespace WebCore {

SQLiteFileSystem::SQLiteFileSystem()
{
}

int SQLiteFileSystem::openDatabase(const String& fileName, sqlite3** database, bool forWebSQLDatabase)
{
    if (!forWebSQLDatabase) {
        String path = fileName;
        return sqlite3_open16(path.charactersWithNullTermination(), database);
    }

    return sqlite3_open_v2(fileName.utf8().data(), database, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, "chromium_vfs");
}

String SQLiteFileSystem::getFileNameForNewDatabase(
  const String&, const String& dbName, const String &originIdentifier, SQLiteDatabase*)
{
    // Not used by Chromium's DatabaseTracker implementation
    ASSERT_NOT_REACHED();
    return String();
}

String SQLiteFileSystem::appendDatabaseFileNameToPath(const String&, const String& fileName)
{
    // Not used by Chromium's DatabaseTracker implementation
    ASSERT_NOT_REACHED();
    return String();
}

bool SQLiteFileSystem::ensureDatabaseDirectoryExists(const String&)
{
    // Not used by Chromium's DatabaseTracker implementation
    ASSERT_NOT_REACHED();
    return false;
}

bool SQLiteFileSystem::ensureDatabaseFileExists(const String&, bool)
{
    // Not used by Chromium's DatabaseTracker implementation
    ASSERT_NOT_REACHED();
    return false;
}

bool SQLiteFileSystem::deleteEmptyDatabaseDirectory(const String&)
{
    // Not used by Chromium's DatabaseTracker implementation
    ASSERT_NOT_REACHED();
    return false;
}

bool SQLiteFileSystem::deleteDatabaseFile(const String& fileName)
{
    return (PlatformBridge::databaseDeleteFile(fileName) == SQLITE_OK);
}

long long SQLiteFileSystem::getDatabaseFileSize(const String& fileName)
{
    return PlatformBridge::databaseGetFileSize(fileName);
}

} // namespace WebCore
