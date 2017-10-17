/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include <WebCore/DatabaseTracker.h>
#include <WebCore/FileSystem.h>
#include <WebCore/OriginLock.h>
#include <WebCore/SQLiteDatabase.h>
#include <WebCore/SQLiteStatement.h>
#include <WebCore/SecurityOriginData.h>

using namespace WebCore;

namespace TestWebKitAPI {

#if PLATFORM(IOS)

TEST(DatabaseTracker, DeleteDatabaseFileIfEmpty)
{
    PlatformFileHandle handle;
    String databaseFilePath = openTemporaryFile("tempEmptyDatabase", handle);
    closeFile(handle);

    long long fileSize;
    getFileSize(databaseFilePath, fileSize);
    EXPECT_EQ(0, fileSize);

    EXPECT_TRUE(DatabaseTracker::deleteDatabaseFileIfEmpty(databaseFilePath));

    bool fileStillExists = fileExists(databaseFilePath);
    EXPECT_FALSE(fileStillExists);

    if (fileStillExists)
        deleteFile(databaseFilePath);
}

#endif // PLATFORM(IOS)

#if PLATFORM(COCOA)

static void addToDatabasesTable(const String& databasePath, const SecurityOriginData& origin, const String& newDatabaseName, const String& newDatabasePath)
{
    SQLiteDatabase database;
    database.open(databasePath);

    SQLiteStatement addDatabaseStatement(database, "INSERT INTO Databases (origin, name, path) VALUES (?, ?, ?);");
    addDatabaseStatement.prepare();
    addDatabaseStatement.bindText(1, origin.databaseIdentifier());
    addDatabaseStatement.bindText(2, newDatabaseName);
    addDatabaseStatement.bindText(3, newDatabasePath);
    addDatabaseStatement.executeCommand();

    database.close();
}

static void removeDirectoryAndAllContents(const String& directoryPath)
{
    for (const auto& file : listDirectory(directoryPath, "*"))
        EXPECT_TRUE(deleteFile(file));

    if (fileExists(directoryPath))
        EXPECT_TRUE(deleteEmptyDirectory(directoryPath));
}

static void createFileAtPath(const String& path)
{
    PlatformFileHandle fileHandle = openFile(path, OpenForWrite);
    EXPECT_NE(-1, fileHandle);
    closeFile(fileHandle);
    EXPECT_TRUE(fileExists(path));
}

TEST(DatabaseTracker, DeleteOrigin)
{
    // Test the expected case. There is an entry in both the Origins and Databases
    // tables, and an actual database on disk.
    // In this case, we should remove the origin's information from both the Origins
    // and Databases tables, and remove the database from disk.
    NSString *webSQLDirectory = createTemporaryDirectory(@"WebSQL");
    String databaseDirectoryPath(webSQLDirectory.UTF8String);

    std::unique_ptr<DatabaseTracker> databaseTracker = DatabaseTracker::trackerWithDatabasePath(databaseDirectoryPath);
    SecurityOriginData origin("https", "webkit.org", 443);

    databaseTracker->setQuota(origin, 5242880);
    EXPECT_EQ((unsigned)1, databaseTracker->origins().size());

    String databasePath = pathByAppendingComponent(databaseDirectoryPath, "Databases.db");
    EXPECT_TRUE(fileExists(databasePath));

    String webDatabaseName = "database_name";
    addToDatabasesTable(databasePath, origin, webDatabaseName, "database.db");
    EXPECT_EQ((unsigned)1, databaseTracker->databaseNames(origin).size());

    String originPath = pathByAppendingComponent(databaseDirectoryPath, origin.databaseIdentifier());
    EXPECT_TRUE(makeAllDirectories(originPath));
    EXPECT_TRUE(fileExists(originPath));

    String fullWebDatabasePath = databaseTracker->fullPathForDatabase(origin, webDatabaseName, false);
    createFileAtPath(fullWebDatabasePath);

    EXPECT_TRUE(databaseTracker->deleteOrigin(origin));
    EXPECT_TRUE(databaseTracker->origins().isEmpty());
    EXPECT_TRUE(databaseTracker->databaseNames(origin).isEmpty());

#if PLATFORM(IOS)
    EXPECT_TRUE(DatabaseTracker::deleteDatabaseFileIfEmpty(fullWebDatabasePath));
    EXPECT_TRUE(deleteEmptyDirectory(originPath));
#else
    EXPECT_FALSE(fileExists(fullWebDatabasePath));
    EXPECT_FALSE(fileExists(originPath));
#endif

    removeDirectoryAndAllContents(databaseDirectoryPath);
    EXPECT_FALSE(fileExists(databaseDirectoryPath));
}

TEST(DatabaseTracker, DeleteOriginWhenDatabaseDoesNotExist)
{
    // Test the case where there is an entry in both the Origins and Databases tables,
    // but not an actual database on disk.
    // The information should still be removed from the tables.
    NSString *webSQLDirectory = createTemporaryDirectory(@"WebSQL");
    String databaseDirectoryPath(webSQLDirectory.UTF8String);

    std::unique_ptr<DatabaseTracker> databaseTracker = DatabaseTracker::trackerWithDatabasePath(databaseDirectoryPath);
    SecurityOriginData origin("https", "webkit.org", 443);

    databaseTracker->setQuota(origin, 5242880);
    EXPECT_EQ((unsigned)1, databaseTracker->origins().size());

    String databasePath = pathByAppendingComponent(databaseDirectoryPath, "Databases.db");
    EXPECT_TRUE(fileExists(databasePath));

    String webDatabaseName = "database_name";
    addToDatabasesTable(databasePath, origin, webDatabaseName, "database.db");
    EXPECT_EQ((unsigned)1, databaseTracker->databaseNames(origin).size());

    String webDatabaseFullPath = databaseTracker->fullPathForDatabase(origin, webDatabaseName, false);
    EXPECT_FALSE(fileExists(webDatabaseFullPath));

    EXPECT_TRUE(databaseTracker->deleteOrigin(origin));

    EXPECT_TRUE(databaseTracker->origins().isEmpty());
    EXPECT_TRUE(databaseTracker->databaseNames(origin).isEmpty());

    removeDirectoryAndAllContents(databaseDirectoryPath);
    EXPECT_FALSE(fileExists(databaseDirectoryPath));
}

TEST(DatabaseTracker, DeleteOriginWhenDeletingADatabaseFails)
{
    // Test the case where there are entries in both the Databases and Origins tables.
    // There are also databases on disk.
    // When we call deleteOrigin(), deleting one of these databases fails.
    // In this case, we shouldn't remove the information from either the Databases or
    // Origins tables.
    NSString *webSQLDirectory = createTemporaryDirectory(@"WebSQL");
    String databaseDirectoryPath(webSQLDirectory.UTF8String);

    std::unique_ptr<DatabaseTracker> databaseTracker = DatabaseTracker::trackerWithDatabasePath(databaseDirectoryPath);
    SecurityOriginData origin("https", "webkit.org", 443);

    databaseTracker->setQuota(origin, 5242880);
    EXPECT_EQ((unsigned)1, databaseTracker->origins().size());

    String databasePath = pathByAppendingComponent(databaseDirectoryPath, "Databases.db");
    EXPECT_TRUE(fileExists(databasePath));

    String webDatabaseName = "database_name";
    addToDatabasesTable(databasePath, origin, webDatabaseName, "database.db");
    EXPECT_EQ((unsigned)1, databaseTracker->databaseNames(origin).size());

    String originPath = pathByAppendingComponent(databaseDirectoryPath, origin.databaseIdentifier());
    EXPECT_TRUE(makeAllDirectories(originPath));
    EXPECT_TRUE(fileExists(originPath));

    String fullWebDatabasePath = databaseTracker->fullPathForDatabase(origin, webDatabaseName, false);
    createFileAtPath(fullWebDatabasePath);

    chmod(fullWebDatabasePath.utf8().data(), 555);

#if !PLATFORM(IOS)
    chflags(fullWebDatabasePath.utf8().data(), UF_IMMUTABLE);
#endif

    EXPECT_FALSE(databaseTracker->deleteOrigin(origin));

    EXPECT_TRUE(fileExists(fullWebDatabasePath));
    EXPECT_TRUE(fileExists(originPath));

    EXPECT_EQ((unsigned)1, databaseTracker->origins().size());
    EXPECT_EQ((unsigned)1, databaseTracker->databaseNames(origin).size());

    chmod(fullWebDatabasePath.utf8().data(), 666);

#if !PLATFORM(IOS)
    chflags(fullWebDatabasePath.utf8().data(), 0);
#endif

    EXPECT_TRUE(deleteFile(fullWebDatabasePath));
    EXPECT_TRUE(deleteEmptyDirectory(originPath));

    removeDirectoryAndAllContents(databaseDirectoryPath);
    EXPECT_FALSE(fileExists(databaseDirectoryPath));
}

TEST(DatabaseTracker, DeleteDatabase)
{
    // Test the expected case. There is an entry in the Databases table
    // and a database on disk. After the deletion, the database should be deleted
    // from disk, and the information should be gone from the Databases table.
    NSString *webSQLDirectory = createTemporaryDirectory(@"WebSQL");
    String databaseDirectoryPath(webSQLDirectory.UTF8String);

    std::unique_ptr<DatabaseTracker> databaseTracker = DatabaseTracker::trackerWithDatabasePath(databaseDirectoryPath);
    SecurityOriginData origin("https", "webkit.org", 443);

    databaseTracker->setQuota(origin, 5242880);
    EXPECT_EQ((unsigned)1, databaseTracker->origins().size());

    String databasePath = pathByAppendingComponent(databaseDirectoryPath, "Databases.db");
    EXPECT_TRUE(fileExists(databasePath));

    String webDatabaseName = "database_name";
    addToDatabasesTable(databasePath, origin, webDatabaseName, "database.db");
    EXPECT_EQ((unsigned)1, databaseTracker->databaseNames(origin).size());

    String originPath = pathByAppendingComponent(databaseDirectoryPath, origin.databaseIdentifier());
    EXPECT_TRUE(makeAllDirectories(originPath));
    EXPECT_TRUE(fileExists(originPath));

    String fullWebDatabasePath = databaseTracker->fullPathForDatabase(origin, webDatabaseName, false);
    createFileAtPath(fullWebDatabasePath);

    EXPECT_TRUE(databaseTracker->deleteDatabase(origin, webDatabaseName));
    EXPECT_TRUE(databaseTracker->databaseNames(origin).isEmpty());

#if PLATFORM(IOS)
    EXPECT_TRUE(DatabaseTracker::deleteDatabaseFileIfEmpty(fullWebDatabasePath));
#else
    EXPECT_FALSE(fileExists(fullWebDatabasePath));
#endif

    EXPECT_EQ((unsigned)1, databaseTracker->origins().size());
    EXPECT_TRUE(deleteEmptyDirectory(originPath));
    removeDirectoryAndAllContents(databaseDirectoryPath);
    EXPECT_FALSE(fileExists(databaseDirectoryPath));
}

TEST(DatabaseTracker, DeleteDatabaseWhenDatabaseDoesNotExist)
{
    // Test the case where we try to delete a database that doesn't exist on disk.
    // We should still remove the database information from the Databases table.
    NSString *webSQLDirectory = createTemporaryDirectory(@"WebSQL");
    String databaseDirectoryPath(webSQLDirectory.UTF8String);

    std::unique_ptr<DatabaseTracker> databaseTracker = DatabaseTracker::trackerWithDatabasePath(databaseDirectoryPath);
    SecurityOriginData origin("https", "webkit.org", 443);

    databaseTracker->setQuota(origin, 5242880);
    EXPECT_EQ((unsigned)1, databaseTracker->origins().size());

    String databasePath = pathByAppendingComponent(databaseDirectoryPath, "Databases.db");
    EXPECT_TRUE(fileExists(databasePath));

    String webDatabaseName = "database_name";
    addToDatabasesTable(databasePath, origin, webDatabaseName, "database.db");
    EXPECT_EQ((unsigned)1, databaseTracker->databaseNames(origin).size());

    String webDatabaseFullPath = databaseTracker->fullPathForDatabase(origin, webDatabaseName, false);
    EXPECT_FALSE(fileExists(webDatabaseFullPath));

    EXPECT_TRUE(databaseTracker->deleteDatabase(origin, webDatabaseName));
    EXPECT_TRUE(databaseTracker->databaseNames(origin).isEmpty());

    removeDirectoryAndAllContents(databaseDirectoryPath);
    EXPECT_FALSE(fileExists(databaseDirectoryPath));
}

#endif // PLATFORM(COCOA)

} // namespace TestWebKitAPI

