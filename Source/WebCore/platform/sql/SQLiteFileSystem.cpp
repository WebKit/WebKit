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

#include "Logging.h"
#include "SQLiteDatabase.h"
#include "SQLiteStatement.h"
#include <pal/crypto/CryptoDigest.h>
#include <sqlite3.h>
#include <wtf/FileSystem.h>
#include <wtf/HexNumber.h>
#include <wtf/text/MakeString.h>

#if PLATFORM(COCOA)
#include <pal/spi/cocoa/SQLite3SPI.h>
#endif

#if PLATFORM(COCOA)
#include <sys/xattr.h>
#endif

namespace WebCore {

static constexpr std::array<ASCIILiteral, 3> databaseFileSuffixes { ""_s, "-shm"_s, "-wal"_s };

SQLiteFileSystem::SQLiteFileSystem() = default;

String SQLiteFileSystem::appendDatabaseFileNameToPath(StringView path, StringView fileName)
{
    return FileSystem::pathByAppendingComponent(path, fileName);
}

bool SQLiteFileSystem::ensureDatabaseDirectoryExists(const String& path)
{
    if (path.isEmpty())
        return false;
    return FileSystem::makeAllDirectories(path);
}

bool SQLiteFileSystem::ensureDatabaseFileExists(const String& fileName, bool checkPathOnly)
{
    if (fileName.isEmpty())
        return false;

    if (checkPathOnly) {
        String dir = FileSystem::parentPath(fileName);
        return ensureDatabaseDirectoryExists(dir);
    }

    return FileSystem::fileExists(fileName);
}

bool SQLiteFileSystem::deleteEmptyDatabaseDirectory(const String& path)
{
    return FileSystem::deleteEmptyDirectory(path);
}

bool SQLiteFileSystem::deleteDatabaseFile(const String& filePath)
{
    bool fileExists = false;
    for (auto suffix : databaseFileSuffixes) {
        auto path = makeString(filePath, suffix);
        FileSystem::deleteFile(path);
        fileExists |= FileSystem::fileExists(path);
    }

    return !fileExists;
}

#if PLATFORM(COCOA)
void SQLiteFileSystem::setCanSuspendLockedFileAttribute(const String& filePath)
{
    for (auto suffix : databaseFileSuffixes) {
        auto path = makeString(filePath, suffix);
        char excluded = 0xff;
        auto result = setxattr(FileSystem::fileSystemRepresentation(path).data(), "com.apple.runningboard.can-suspend-locked", &excluded, sizeof(excluded), 0, 0);
        if (result < 0 && !strcmp(suffix, ""))
            RELEASE_LOG_ERROR(SQLDatabase, "SQLiteFileSystem::setCanSuspendLockedFileAttribute: setxattr failed: %" PUBLIC_LOG_STRING, strerror(errno));
    }
}
#endif

bool SQLiteFileSystem::moveDatabaseFile(const String& oldFilePath, const String& newFilePath)
{
    bool allMoved = true;
    for (auto suffix : databaseFileSuffixes)
        allMoved &= FileSystem::moveFile(makeString(oldFilePath, suffix), makeString(newFilePath, suffix));

    return allMoved;
}

#if PLATFORM(COCOA)
bool SQLiteFileSystem::truncateDatabaseFile(sqlite3* database)
{
    return sqlite3_file_control(database, 0, SQLITE_TRUNCATE_DATABASE, 0) == SQLITE_OK;
}
#endif
    
uint64_t SQLiteFileSystem::databaseFileSize(const String& filePath)
{
    uint64_t totalSize = 0;
    for (auto suffix : databaseFileSuffixes) {
        if (auto fileSize = FileSystem::fileSize(makeString(filePath, suffix)))
            totalSize += *fileSize;
    }

    return totalSize;
}

std::optional<WallTime> SQLiteFileSystem::databaseCreationTime(const String& fileName)
{
    return FileSystem::fileCreationTime(fileName);
}

std::optional<WallTime> SQLiteFileSystem::databaseModificationTime(const String& fileName)
{
    return FileSystem::fileModificationTime(fileName);
}
    
String SQLiteFileSystem::computeHashForFileName(StringView fileName)
{
    auto cryptoDigest = PAL::CryptoDigest::create(PAL::CryptoDigest::Algorithm::SHA_256);
    auto utf8FileName = fileName.utf8();
    cryptoDigest->addBytes(utf8FileName.span());
    return cryptoDigest->toHexString();
}

} // namespace WebCore
