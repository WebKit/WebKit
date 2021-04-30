/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Justin Haygood (jhaygood@reaktix.com)
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
#include "SQLiteDatabase.h"

#include "DatabaseAuthorizer.h"
#include "Logging.h"
#include "MemoryRelease.h"
#include "SQLiteDatabaseTracker.h"
#include "SQLiteFileSystem.h"
#include "SQLiteStatement.h"
#include <mutex>
#include <sqlite3.h>
#include <thread>
#include <wtf/FileSystem.h>
#include <wtf/Threading.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

static const char notOpenErrorMessage[] = "database is not open";

static void unauthorizedSQLFunction(sqlite3_context *context, int, sqlite3_value **)
{
    const char* functionName = (const char*)sqlite3_user_data(context);
    sqlite3_result_error(context, makeString("Function ", functionName, " is unauthorized").utf8().data(), -1);
}

static void initializeSQLiteIfNecessary()
{
    static std::once_flag flag;
    std::call_once(flag, [] {
        // It should be safe to call this outside of std::call_once, since it is documented to be
        // completely threadsafe. But in the past it was not safe, and the SQLite developers still
        // aren't confident that it really is, and we still support ancient versions of SQLite. So
        // std::call_once is used to stay on the safe side. See bug #143245.
        int ret = sqlite3_initialize();
        if (ret != SQLITE_OK) {
#if SQLITE_VERSION_NUMBER >= 3007015
            WTFLogAlways("Failed to initialize SQLite: %s", sqlite3_errstr(ret));
#else
            WTFLogAlways("Failed to initialize SQLite");
#endif
            CRASH();
        }
    });
}

static bool isDatabaseOpeningForbidden = false;
static Lock isDatabaseOpeningForbiddenMutex;

void SQLiteDatabase::setIsDatabaseOpeningForbidden(bool isForbidden)
{
    auto locker = holdLock(isDatabaseOpeningForbiddenMutex);
    isDatabaseOpeningForbidden = isForbidden;
}

SQLiteDatabase::SQLiteDatabase() = default;

SQLiteDatabase::~SQLiteDatabase()
{
    close();
}

bool SQLiteDatabase::open(const String& filename, OpenMode openMode)
{
    initializeSQLiteIfNecessary();

    close();

    {
        auto locker = holdLock(isDatabaseOpeningForbiddenMutex);
        if (isDatabaseOpeningForbidden) {
            m_openErrorMessage = "opening database is forbidden";
            return false;
        }

        int flags = SQLITE_OPEN_AUTOPROXY;
        switch (openMode) {
        case OpenMode::ReadOnly:
            flags |= SQLITE_OPEN_READONLY;
            break;
        case OpenMode::ReadWrite:
            flags |= SQLITE_OPEN_READWRITE;
            break;
        case OpenMode::ReadWriteCreate:
            flags |= SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
            break;
        }

        m_openError = sqlite3_open_v2(FileSystem::fileSystemRepresentation(filename).data(), &m_db, flags, nullptr);
        if (m_openError != SQLITE_OK) {
            m_openErrorMessage = m_db ? sqlite3_errmsg(m_db) : "sqlite_open returned null";
            LOG_ERROR("SQLite database failed to load from %s\nCause - %s", filename.ascii().data(),
                m_openErrorMessage.data());
            sqlite3_close(m_db);
            m_db = 0;
            return false;
        }
    }

    overrideUnauthorizedFunctions();

    m_openError = sqlite3_extended_result_codes(m_db, 1);
    if (m_openError != SQLITE_OK) {
        m_openErrorMessage = sqlite3_errmsg(m_db);
        LOG_ERROR("SQLite database error when enabling extended errors - %s", m_openErrorMessage.data());
        sqlite3_close(m_db);
        m_db = 0;
        return false;
    }

    if (isOpen())
        m_openingThread = &Thread::current();
    else
        m_openErrorMessage = "sqlite_open returned null";

    {
        SQLiteTransactionInProgressAutoCounter transactionCounter;
        if (!SQLiteStatement(*this, "PRAGMA temp_store = MEMORY;"_s).executeCommand())
            LOG_ERROR("SQLite database could not set temp_store to memory");
    }

    if (openMode != OpenMode::ReadOnly)
        useWALJournalMode();

    String shmFileName = makeString(filename, "-shm"_s);
    if (FileSystem::fileExists(shmFileName)) {
        if (!FileSystem::isSafeToUseMemoryMapForPath(shmFileName)) {
            RELEASE_LOG_FAULT(SQLDatabase, "Opened an SQLite database with a Class A -shm file. This may trigger a crash when the user locks the device. (%s)", shmFileName.latin1().data());
            FileSystem::makeSafeToUseMemoryMapForPath(shmFileName);
        }
    }

    return isOpen();
}

static int walAutomaticTruncationHook(void* context, sqlite3* db, const char* dbName, int walPageCount)
{
    UNUSED_PARAM(context);

    static constexpr int checkpointThreshold = 1000; // matches SQLITE_DEFAULT_WAL_AUTOCHECKPOINT

    if (walPageCount >= checkpointThreshold) {
        int newWalPageCount = 0;
        int result = sqlite3_wal_checkpoint_v2(db, dbName, SQLITE_CHECKPOINT_TRUNCATE, &newWalPageCount, nullptr);

#if LOG_DISABLED
        UNUSED_VARIABLE(result);
#else
        if (result != SQLITE_OK || newWalPageCount) {
            LOG(SQLDatabase, "Can't fully checkpoint SQLite db %p", db);

            sqlite3_stmt* stmt = nullptr;
            while ((stmt = sqlite3_next_stmt(db, stmt))) {
                if (sqlite3_stmt_busy(stmt))
                    LOG(SQLDatabase, "SQLite db %p has busy stmt %p blocking checkpoint: %s", db, stmt, sqlite3_sql(stmt));
            }
        }
#endif
    }

    return SQLITE_OK;
}

void SQLiteDatabase::enableAutomaticWALTruncation()
{
    sqlite3_wal_hook(m_db, walAutomaticTruncationHook, nullptr);
}

void SQLiteDatabase::useWALJournalMode()
{
    m_useWAL = true;
    {
        SQLiteStatement walStatement(*this, "PRAGMA journal_mode=WAL;"_s);
        if (walStatement.prepareAndStep() == SQLITE_ROW) {
#ifndef NDEBUG
            String mode = walStatement.getColumnText(0);
            if (!equalLettersIgnoringASCIICase(mode, "wal"))
                LOG_ERROR("journal_mode of database should be 'WAL', but is '%s'", mode.utf8().data());
#endif
        } else
            LOG_ERROR("SQLite database failed to set journal_mode to WAL, error: %s", lastErrorMsg());
    }

    {
        SQLiteTransactionInProgressAutoCounter transactionCounter;
        SQLiteStatement checkpointStatement(*this, "PRAGMA wal_checkpoint(TRUNCATE)"_s);
        if (checkpointStatement.prepareAndStep() == SQLITE_ROW) {
            if (checkpointStatement.getColumnInt(0))
                LOG(SQLDatabase, "SQLite database checkpoint is blocked");
        } else
            LOG_ERROR("SQLite database failed to checkpoint: %s", lastErrorMsg());
    }
}

void SQLiteDatabase::close()
{
    if (m_db) {
        // FIXME: This is being called on the main thread during JS GC. <rdar://problem/5739818>
        // ASSERT(m_openingThread == &Thread::current());
        sqlite3* db = m_db;
        {
            LockHolder locker(m_databaseClosingMutex);
            m_db = 0;
        }
        if (m_useWAL) {
            SQLiteTransactionInProgressAutoCounter transactionCounter;
            sqlite3_close(db);
        } else
            sqlite3_close(db);
    }

    m_openingThread = nullptr;
    m_openError = SQLITE_ERROR;
    m_openErrorMessage = CString();
}

void SQLiteDatabase::overrideUnauthorizedFunctions()
{
    static const std::pair<const char*, int> functionParameters[] = {
        { "rtreenode", 2 },
        { "rtreedepth", 1 },
        { "eval", 1 },
        { "eval", 2 },
        { "printf", -1 },
        { "fts3_tokenizer", 1 },
        { "fts3_tokenizer", 2 },
    };

    for (auto& functionParameter : functionParameters)
        sqlite3_create_function(m_db, functionParameter.first, functionParameter.second, SQLITE_UTF8, const_cast<char*>(functionParameter.first), unauthorizedSQLFunction, 0, 0);
}

void SQLiteDatabase::setFullsync(bool fsync)
{
    if (fsync) 
        executeCommand("PRAGMA fullfsync = 1;"_s);
    else
        executeCommand("PRAGMA fullfsync = 0;"_s);
}

int64_t SQLiteDatabase::maximumSize()
{
    int64_t maxPageCount = 0;

    {
        LockHolder locker(m_authorizerLock);
        enableAuthorizer(false);
        SQLiteStatement statement(*this, "PRAGMA max_page_count"_s);
        maxPageCount = statement.getColumnInt64(0);
        enableAuthorizer(true);
    }

    return maxPageCount * pageSize();
}

void SQLiteDatabase::setMaximumSize(int64_t size)
{
    if (size < 0)
        size = 0;
    
    int currentPageSize = pageSize();

    ASSERT(currentPageSize || !m_db);
    int64_t newMaxPageCount = currentPageSize ? size / currentPageSize : 0;
    
    LockHolder locker(m_authorizerLock);
    enableAuthorizer(false);

    SQLiteStatement statement(*this, makeString("PRAGMA max_page_count = ", newMaxPageCount));
    statement.prepare();
    if (statement.step() != SQLITE_ROW)
        LOG_ERROR("Failed to set maximum size of database to %lli bytes", static_cast<long long>(size));

    enableAuthorizer(true);

}

int SQLiteDatabase::pageSize()
{
    // Since the page size of a database is locked in at creation and therefore cannot be dynamic, 
    // we can cache the value for future use
    if (m_pageSize == -1) {
        LockHolder locker(m_authorizerLock);
        enableAuthorizer(false);
        
        SQLiteStatement statement(*this, "PRAGMA page_size"_s);
        m_pageSize = statement.getColumnInt(0);
        
        enableAuthorizer(true);
    }

    return m_pageSize;
}

int64_t SQLiteDatabase::freeSpaceSize()
{
    int64_t freelistCount = 0;

    {
        LockHolder locker(m_authorizerLock);
        enableAuthorizer(false);
        // Note: freelist_count was added in SQLite 3.4.1.
        SQLiteStatement statement(*this, "PRAGMA freelist_count"_s);
        freelistCount = statement.getColumnInt64(0);
        enableAuthorizer(true);
    }

    return freelistCount * pageSize();
}

int64_t SQLiteDatabase::totalSize()
{
    int64_t pageCount = 0;

    {
        LockHolder locker(m_authorizerLock);
        enableAuthorizer(false);
        SQLiteStatement statement(*this, "PRAGMA page_count"_s);
        pageCount = statement.getColumnInt64(0);
        enableAuthorizer(true);
    }

    return pageCount * pageSize();
}

void SQLiteDatabase::setSynchronous(SynchronousPragma sync)
{
    executeCommand(makeString("PRAGMA synchronous = ", static_cast<unsigned>(sync)));
}

void SQLiteDatabase::setBusyTimeout(int ms)
{
    if (m_db)
        sqlite3_busy_timeout(m_db, ms);
    else
        LOG(SQLDatabase, "BusyTimeout set on non-open database");
}

void SQLiteDatabase::setBusyHandler(int(*handler)(void*, int))
{
    if (m_db)
        sqlite3_busy_handler(m_db, handler, NULL);
    else
        LOG(SQLDatabase, "Busy handler set on non-open database");
}

bool SQLiteDatabase::executeCommand(const String& sql)
{
    return SQLiteStatement(*this, sql).executeCommand();
}

bool SQLiteDatabase::returnsAtLeastOneResult(const String& sql)
{
    return SQLiteStatement(*this, sql).returnsAtLeastOneResult();
}

bool SQLiteDatabase::tableExists(const String& tablename)
{
    if (!isOpen())
        return false;

    String statement = "SELECT name FROM sqlite_master WHERE type = 'table' AND name = '" + tablename + "';";

    SQLiteStatement sql(*this, statement);
    sql.prepare();
    return sql.step() == SQLITE_ROW;
}

void SQLiteDatabase::clearAllTables()
{
    String query = "SELECT name FROM sqlite_master WHERE type='table';"_s;
    Vector<String> tables;
    if (!SQLiteStatement(*this, query).returnTextResults(0, tables)) {
        LOG(SQLDatabase, "Unable to retrieve list of tables from database");
        return;
    }
    
    for (Vector<String>::iterator table = tables.begin(); table != tables.end(); ++table ) {
        if (*table == "sqlite_sequence")
            continue;
        if (!executeCommand("DROP TABLE " + *table))
            LOG(SQLDatabase, "Unable to drop table %s", (*table).ascii().data());
    }
}

int SQLiteDatabase::runVacuumCommand()
{
    if (!executeCommand("VACUUM;"_s))
        LOG(SQLDatabase, "Unable to vacuum database - %s", lastErrorMsg());
    return lastError();
}

int SQLiteDatabase::runIncrementalVacuumCommand()
{
    LockHolder locker(m_authorizerLock);
    enableAuthorizer(false);

    if (!executeCommand("PRAGMA incremental_vacuum"_s))
        LOG(SQLDatabase, "Unable to run incremental vacuum - %s", lastErrorMsg());

    enableAuthorizer(true);
    return lastError();
}

void SQLiteDatabase::interrupt()
{
    LockHolder locker(m_databaseClosingMutex);
    if (m_db)
        sqlite3_interrupt(m_db);
}

int64_t SQLiteDatabase::lastInsertRowID()
{
    if (!m_db)
        return 0;
    return sqlite3_last_insert_rowid(m_db);
}

void SQLiteDatabase::updateLastChangesCount()
{
    if (!m_db)
        return;

    m_lastChangesCount = sqlite3_total_changes(m_db);
}

int SQLiteDatabase::lastChanges()
{
    if (!m_db)
        return 0;

    return sqlite3_total_changes(m_db) - m_lastChangesCount;
}

int SQLiteDatabase::lastError()
{
    return m_db ? sqlite3_errcode(m_db) : m_openError;
}

const char* SQLiteDatabase::lastErrorMsg()
{
    if (m_db)
        return sqlite3_errmsg(m_db);
    return m_openErrorMessage.isNull() ? notOpenErrorMessage : m_openErrorMessage.data();
}

#if ASSERT_ENABLED
void SQLiteDatabase::disableThreadingChecks()
{
    // Note that SQLite could be compiled with -DTHREADSAFE, or you may have turned off the mutexes.
    m_sharable = true;
}
#endif

int SQLiteDatabase::authorizerFunction(void* userData, int actionCode, const char* parameter1, const char* parameter2, const char* /*databaseName*/, const char* /*trigger_or_view*/)
{
    DatabaseAuthorizer* auth = static_cast<DatabaseAuthorizer*>(userData);
    ASSERT(auth);

    switch (actionCode) {
        case SQLITE_CREATE_INDEX:
            return auth->createIndex(parameter1, parameter2);
        case SQLITE_CREATE_TABLE:
            return auth->createTable(parameter1);
        case SQLITE_CREATE_TEMP_INDEX:
            return auth->createTempIndex(parameter1, parameter2);
        case SQLITE_CREATE_TEMP_TABLE:
            return auth->createTempTable(parameter1);
        case SQLITE_CREATE_TEMP_TRIGGER:
            return auth->createTempTrigger(parameter1, parameter2);
        case SQLITE_CREATE_TEMP_VIEW:
            return auth->createTempView(parameter1);
        case SQLITE_CREATE_TRIGGER:
            return auth->createTrigger(parameter1, parameter2);
        case SQLITE_CREATE_VIEW:
            return auth->createView(parameter1);
        case SQLITE_DELETE:
            return auth->allowDelete(parameter1);
        case SQLITE_DROP_INDEX:
            return auth->dropIndex(parameter1, parameter2);
        case SQLITE_DROP_TABLE:
            return auth->dropTable(parameter1);
        case SQLITE_DROP_TEMP_INDEX:
            return auth->dropTempIndex(parameter1, parameter2);
        case SQLITE_DROP_TEMP_TABLE:
            return auth->dropTempTable(parameter1);
        case SQLITE_DROP_TEMP_TRIGGER:
            return auth->dropTempTrigger(parameter1, parameter2);
        case SQLITE_DROP_TEMP_VIEW:
            return auth->dropTempView(parameter1);
        case SQLITE_DROP_TRIGGER:
            return auth->dropTrigger(parameter1, parameter2);
        case SQLITE_DROP_VIEW:
            return auth->dropView(parameter1);
        case SQLITE_INSERT:
            return auth->allowInsert(parameter1);
        case SQLITE_PRAGMA:
            return auth->allowPragma(parameter1, parameter2);
        case SQLITE_READ:
            return auth->allowRead(parameter1, parameter2);
        case SQLITE_SELECT:
            return auth->allowSelect();
        case SQLITE_TRANSACTION:
            return auth->allowTransaction();
        case SQLITE_UPDATE:
            return auth->allowUpdate(parameter1, parameter2);
        case SQLITE_ATTACH:
            return auth->allowAttach(parameter1);
        case SQLITE_DETACH:
            return auth->allowDetach(parameter1);
        case SQLITE_ALTER_TABLE:
            return auth->allowAlterTable(parameter1, parameter2);
        case SQLITE_REINDEX:
            return auth->allowReindex(parameter1);
        case SQLITE_ANALYZE:
            return auth->allowAnalyze(parameter1);
        case SQLITE_CREATE_VTABLE:
            return auth->createVTable(parameter1, parameter2);
        case SQLITE_DROP_VTABLE:
            return auth->dropVTable(parameter1, parameter2);
        case SQLITE_FUNCTION:
            return auth->allowFunction(parameter2);
        default:
            ASSERT_NOT_REACHED();
            return SQLAuthDeny;
    }
}

void SQLiteDatabase::setAuthorizer(DatabaseAuthorizer& authorizer)
{
    if (!m_db) {
        LOG_ERROR("Attempt to set an authorizer on a non-open SQL database");
        ASSERT_NOT_REACHED();
        return;
    }

    LockHolder locker(m_authorizerLock);

    m_authorizer = &authorizer;
    
    enableAuthorizer(true);
}

void SQLiteDatabase::enableAuthorizer(bool enable)
{
    if (m_authorizer && enable)
        sqlite3_set_authorizer(m_db, SQLiteDatabase::authorizerFunction, m_authorizer.get());
    else
        sqlite3_set_authorizer(m_db, NULL, 0);
}

bool SQLiteDatabase::isAutoCommitOn() const
{
    return sqlite3_get_autocommit(m_db);
}

bool SQLiteDatabase::turnOnIncrementalAutoVacuum()
{
    SQLiteStatement statement(*this, "PRAGMA auto_vacuum"_s);
    int autoVacuumMode = statement.getColumnInt(0);
    int error = lastError();
    statement.finalize();

    // Check if we got an error while trying to get the value of the auto_vacuum flag.
    // If we got a SQLITE_BUSY error, then there's probably another transaction in
    // progress on this database. In this case, keep the current value of the
    // auto_vacuum flag and try to set it to INCREMENTAL the next time we open this
    // database. If the error is not SQLITE_BUSY, then we probably ran into a more
    // serious problem and should return false (to log an error message).
    if (error != SQLITE_ROW)
        return false;

    switch (autoVacuumMode) {
    case AutoVacuumIncremental:
        return true;
    case AutoVacuumFull:
        return executeCommand("PRAGMA auto_vacuum = 2"_s);
    case AutoVacuumNone:
    default:
        if (!executeCommand("PRAGMA auto_vacuum = 2"_s))
            return false;
        runVacuumCommand();
        error = lastError();
        return (error == SQLITE_OK);
    }
}

static void destroyCollationFunction(void* arg)
{
    auto f = static_cast<WTF::Function<int(int, const void*, int, const void*)>*>(arg);
    delete f;
}

static int callCollationFunction(void* arg, int aLength, const void* a, int bLength, const void* b)
{
    auto f = static_cast<WTF::Function<int(int, const void*, int, const void*)>*>(arg);
    return (*f)(aLength, a, bLength, b);
}

void SQLiteDatabase::setCollationFunction(const String& collationName, WTF::Function<int(int, const void*, int, const void*)>&& collationFunction)
{
    auto functionObject = new WTF::Function<int(int, const void*, int, const void*)>(WTFMove(collationFunction));
    sqlite3_create_collation_v2(m_db, collationName.utf8().data(), SQLITE_UTF8, functionObject, callCollationFunction, destroyCollationFunction);
}

void SQLiteDatabase::removeCollationFunction(const String& collationName)
{
    sqlite3_create_collation_v2(m_db, collationName.utf8().data(), SQLITE_UTF8, nullptr, nullptr, nullptr);
}

void SQLiteDatabase::releaseMemory()
{
    if (!m_db)
        return;

    sqlite3_db_release_memory(m_db);
}

} // namespace WebCore
