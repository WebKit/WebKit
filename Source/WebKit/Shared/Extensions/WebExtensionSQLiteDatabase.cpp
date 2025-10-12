/*
 * Copyright (C) 2024 Igalia, S.L. All rights reserved.
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "WebExtensionSQLiteDatabase.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include "APIError.h"
#include "Logging.h"
#include "WebExtensionSQLiteHelpers.h"
#include <sqlite3.h>
#include <wtf/FileSystem.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMallocInlines.h>

static constexpr auto WebExtensionSQLiteErrorDomain = "com.apple.WebKit.SQLite"_s;
static constexpr auto WebExtensionSQLiteInMemoryDatabaseName = "file::memory:"_s;

using namespace WebKit;

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebExtensionSQLiteDatabase);

WebExtensionSQLiteDatabase::WebExtensionSQLiteDatabase(const URL& url, Ref<WorkQueue>&& queue)
    : m_queue(WTFMove(queue))
{
    ASSERT(url.protocolIsFile());

    m_url = url;
}

void WebExtensionSQLiteDatabase::assertQueue()
{
    assertIsCurrent(queue());
}

int WebExtensionSQLiteDatabase::close()
{
    assertQueue();

    int result = sqlite3_close_v2(m_db);
    if (result != SQLITE_OK) {
        RELEASE_LOG_ERROR(Extensions, "Failed to close database: %s (%d)", m_lastErrorMessage.data(), result);
        return result;
    }

    m_db = nullptr;
    return result;
}

void WebExtensionSQLiteDatabase::reportErrorWithCode(int errorCode, const String& query, RefPtr<API::Error>& outError)
{
    assertQueue();
    ASSERT(errorCode != SQLITE_OK);

    if (!query.isEmpty())
        RELEASE_LOG_ERROR(Extensions, "SQLite error (%d) occurred with query: %" PRIVATE_LOG_STRING, errorCode, query.utf8().data());
    else
        RELEASE_LOG_ERROR(Extensions, "SQLite error (%d) occurred", errorCode);

    outError = errorWithSQLiteErrorCode(errorCode);
}

void WebExtensionSQLiteDatabase::reportErrorWithCode(int errorCode, sqlite3_stmt* statement, RefPtr<API::Error>& outError)
{
    assertQueue();
    ASSERT(errorCode != SQLITE_OK);

    if (statement) {
        if (char* sql = sqlite3_expanded_sql(statement)) {
            reportErrorWithCode(errorCode, String::fromUTF8(sql), outError);
            sqlite3_free(sql);
            return;
        }
    }

    reportErrorWithCode(errorCode, emptyString(), outError);
}

RefPtr<API::Error> WebExtensionSQLiteDatabase::errorWithSQLiteErrorCode(int errorCode)
{
    if (errorCode == SQLITE_OK)
        return nullptr;

    auto errorMessage = String::fromUTF8(sqlite3_errstr(errorCode));
    return API::Error::create({ WebExtensionSQLiteErrorDomain, errorCode, m_url, errorMessage });
}

bool WebExtensionSQLiteDatabase::enableWAL(RefPtr<API::Error>& error)
{
    Ref<WebExtensionSQLiteDatabase> protectedThis(*this);

    // SQLite docs: The synchronous NORMAL setting is a good choice for most applications running in WAL mode.
    if (!SQLiteDatabaseExecuteAndReturnError(*this, error, "PRAGMA synchronous = NORMAL"_s))
        return false;
    return SQLiteDatabaseEnumerate(*this, error, "PRAGMA journal_mode = WAL"_s, std::tie(std::ignore));
}

bool WebExtensionSQLiteDatabase::openWithAccessType(AccessType accessType, RefPtr<API::Error>& outError, ProtectionType protectionType, const String& vfs)
{
    int flags = SQLITE_OPEN_NOMUTEX;

    switch (accessType) {
    case AccessType::ReadOnly:
        flags |= SQLITE_OPEN_READONLY;
        break;
    case AccessType::ReadWrite:
        flags |= SQLITE_OPEN_READWRITE;
        break;
    case AccessType::ReadWriteCreate:
        flags |= SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
        break;
    }

#if PLATFORM(IOS_FAMILY)
    switch (protectionType) {
    case ProtectionType::Default:
    case ProtectionType::CompleteUntilFirstUserAuthentication:
        flags |= SQLITE_OPEN_FILEPROTECTION_COMPLETEUNTILFIRSTUSERAUTHENTICATION;
        break;
    case ProtectionType::CompleteUnlessOpen:
        flags |= SQLITE_OPEN_FILEPROTECTION_COMPLETEUNLESSOPEN;
        break;
    case ProtectionType::Complete:
        flags |= SQLITE_OPEN_FILEPROTECTION_COMPLETE;
        break;
    }
#endif

    assertQueue();
    ASSERT(!m_db);

    String databasePath;
    if (m_url == inMemoryDatabaseURL())
        databasePath = WebExtensionSQLiteInMemoryDatabaseName;
    else if (m_url == privateOnDiskDatabaseURL())
        databasePath = ""_s;
    else {
        ASSERT(!m_url.isEmpty());

        databasePath = m_url.fileSystemPath();

        auto directory = m_url.truncatedForUseAsBase().fileSystemPath();
        if (!FileSystem::makeAllDirectories(directory) || FileSystem::fileType(directory) != FileSystem::FileType::Directory) {
            RELEASE_LOG_ERROR(Extensions, "Unable to create parent folder for database at path: %s", m_url.fileSystemPath().utf8().data());
            outError = errorWithSQLiteErrorCode(SQLITE_CANTOPEN);
            return false;
        }
    }

    int result = sqlite3_open_v2(FileSystem::fileSystemRepresentation(databasePath).data(), &m_db, flags, vfs.isEmpty() ? nullptr : vfs.utf8().data());
    if (result == SQLITE_OK)
        return true;

    // SQLite may return a valid database handle even if an error occurred. sqlite3_close silently
    // ignores calls with a null handle so we can call it here unconditionally.
    sqlite3_close_v2(m_db);
    m_db = nullptr;

    if (result == SQLITE_CANTOPEN && !(flags & SQLITE_OPEN_CREATE))
        return false;

    outError = errorWithSQLiteErrorCode(result);

    return false;
}

URL WebExtensionSQLiteDatabase::inMemoryDatabaseURL()
{
    return URL(WebExtensionSQLiteInMemoryDatabaseName);
}

URL WebExtensionSQLiteDatabase::privateOnDiskDatabaseURL()
{
    return URL("webkit::privateondisk"_s);
}

#endif // ENABLE(WK_WEB_EXTENSIONS)
