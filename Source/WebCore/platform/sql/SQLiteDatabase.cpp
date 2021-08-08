/*
 * Copyright (C) 2006-2021 Apple Inc. All rights reserved.
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
#include <wtf/Lock.h>
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

static Lock isDatabaseOpeningForbiddenLock;
static bool isDatabaseOpeningForbidden WTF_GUARDED_BY_LOCK(isDatabaseOpeningForbiddenLock) { false };

void SQLiteDatabase::setIsDatabaseOpeningForbidden(bool isForbidden)
{
    Locker locker { isDatabaseOpeningForbiddenLock };
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
        Locker locker { isDatabaseOpeningForbiddenLock };
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

        {
            SQLiteTransactionInProgressAutoCounter transactionCounter;
            m_openError = sqlite3_open_v2(FileSystem::fileSystemRepresentation(filename).data(), &m_db, flags, nullptr);
        }
        if (m_openError != SQLITE_OK) {
            m_openErrorMessage = m_db ? sqlite3_errmsg(m_db) : "sqlite_open returned null";
            LOG_ERROR("SQLite database failed to load from %s\nCause - %s", filename.ascii().data(),
                m_openErrorMessage.data());
            close(ShouldSetErrorState::No);
            return false;
        }
    }

    overrideUnauthorizedFunctions();

    m_openError = sqlite3_extended_result_codes(m_db, 1);
    if (m_openError != SQLITE_OK) {
        m_openErrorMessage = sqlite3_errmsg(m_db);
        LOG_ERROR("SQLite database error when enabling extended errors - %s", m_openErrorMessage.data());
        close(ShouldSetErrorState::No);
        return false;
    }

    if (isOpen())
        m_openingThread = &Thread::current();
    else
        m_openErrorMessage = "sqlite_open returned null";

    {
        SQLiteTransactionInProgressAutoCounter transactionCounter;
        if (!executeCommand("PRAGMA temp_store = MEMORY;"_s))
            LOG_ERROR("SQLite database could not set temp_store to memory");
    }

    if (openMode != OpenMode::ReadOnly)
        useWALJournalMode();

    auto shmFileName = makeString(filename, "-shm"_s);
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

static int checkpointModeValue(SQLiteDatabase::CheckpointMode mode)
{
    switch (mode) {
    case SQLiteDatabase::CheckpointMode::Full:
        return SQLITE_CHECKPOINT_FULL;
    case SQLiteDatabase::CheckpointMode::Truncate:
        return SQLITE_CHECKPOINT_TRUNCATE;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

void SQLiteDatabase::checkpoint(CheckpointMode mode)
{
    SQLiteTransactionInProgressAutoCounter transactionCounter;
    int result = sqlite3_wal_checkpoint_v2(m_db, nullptr, checkpointModeValue(mode), nullptr, nullptr);
    if (result == SQLITE_OK)
        return;

    if (result == SQLITE_BUSY) {
        LOG(SQLDatabase, "SQLite database checkpoint is blocked");
        return;
    }

    LOG_ERROR("SQLite database failed to checkpoint: %s", lastErrorMsg());
}

void SQLiteDatabase::useWALJournalMode()
{
    m_useWAL = true;
    {
        auto walStatement = prepareStatement("PRAGMA journal_mode=WAL;"_s);
        if (walStatement && walStatement->step() == SQLITE_ROW) {
#ifndef NDEBUG
            String mode = walStatement->columnText(0);
            if (!equalLettersIgnoringASCIICase(mode, "wal"))
                LOG_ERROR("journal_mode of database should be 'WAL', but is '%s'", mode.utf8().data());
#endif
        } else
            LOG_ERROR("SQLite database failed to set journal_mode to WAL, error: %s", lastErrorMsg());
    }

    checkpoint(CheckpointMode::Truncate);
}

void SQLiteDatabase::close(ShouldSetErrorState shouldSetErrorState)
{
    if (m_db) {
        ASSERT_WITH_MESSAGE(!m_statementCount, "All SQLiteTransaction objects should be destroyed before closing the database");

        // FIXME: This is being called on the main thread during JS GC. <rdar://problem/5739818>
        // ASSERT(m_openingThread == &Thread::current());
        sqlite3* db = m_db;
        {
            Locker locker { m_databaseClosingMutex };
            m_db = 0;
        }

        int closeResult;
        if (m_useWAL) {
            // Close in the scope of counter as it may acquire lock of database.
            SQLiteTransactionInProgressAutoCounter transactionCounter;
            closeResult = sqlite3_close(db);
        } else
            closeResult = sqlite3_close(db);

        if (closeResult != SQLITE_OK)
            RELEASE_LOG_ERROR(SQLDatabase, "SQLiteDatabase::close: Failed to close database (%d) - %{public}s", closeResult, lastErrorMsg());
    }

    if (shouldSetErrorState == ShouldSetErrorState::Yes) {
        m_openingThread = nullptr;
        m_openError = SQLITE_ERROR;
        m_openErrorMessage = CString();
    }
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
        Locker locker { m_authorizerLock };
        enableAuthorizer(false);
        auto statement = prepareStatement("PRAGMA max_page_count"_s);
        maxPageCount = statement ? statement->columnInt64(0) : 0;
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
    
    Locker locker { m_authorizerLock };
    enableAuthorizer(false);

    auto statement = prepareStatementSlow(makeString("PRAGMA max_page_count = ", newMaxPageCount));
    if (!statement || statement->step() != SQLITE_ROW)
        LOG_ERROR("Failed to set maximum size of database to %lli bytes", static_cast<long long>(size));

    enableAuthorizer(true);

}

int SQLiteDatabase::pageSize()
{
    // Since the page size of a database is locked in at creation and therefore cannot be dynamic, 
    // we can cache the value for future use
    if (m_pageSize == -1) {
        Locker locker { m_authorizerLock };
        enableAuthorizer(false);
        
        auto statement = prepareStatement("PRAGMA page_size"_s);
        m_pageSize = statement ? statement->columnInt(0) : 0;
        
        enableAuthorizer(true);
    }

    return m_pageSize;
}

int64_t SQLiteDatabase::freeSpaceSize()
{
    int64_t freelistCount = 0;

    {
        Locker locker { m_authorizerLock };
        enableAuthorizer(false);
        // Note: freelist_count was added in SQLite 3.4.1.
        auto statement = prepareStatement("PRAGMA freelist_count"_s);
        freelistCount = statement ? statement->columnInt64(0) : 0;
        enableAuthorizer(true);
    }

    return freelistCount * pageSize();
}

int64_t SQLiteDatabase::totalSize()
{
    int64_t pageCount = 0;

    {
        Locker locker { m_authorizerLock };
        enableAuthorizer(false);
        auto statement = prepareStatement("PRAGMA page_count"_s);
        pageCount = statement ? statement->columnInt64(0) : 0;
        enableAuthorizer(true);
    }

    return pageCount * pageSize();
}

void SQLiteDatabase::setSynchronous(SynchronousPragma sync)
{
    executeCommandSlow(makeString("PRAGMA synchronous = ", static_cast<unsigned>(sync)));
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

bool SQLiteDatabase::executeCommandSlow(const String& query)
{
    auto statement = prepareStatementSlow(query);
    return statement && statement->executeCommand();
}

bool SQLiteDatabase::executeCommand(ASCIILiteral query)
{
    auto statement = prepareStatement(query);
    return statement && statement->executeCommand();
}

bool SQLiteDatabase::tableExists(const String& tableName)
{
    return !tableSQL(tableName).isEmpty();
}

String SQLiteDatabase::tableSQL(const String& tableName)
{
    if (!isOpen())
        return { };

    auto statement = prepareStatement("SELECT sql FROM sqlite_master WHERE type = 'table' AND name = ?;"_s);
    if (!statement || statement->bindText(1, tableName) != SQLITE_OK || statement->step() != SQLITE_ROW)
        return { };

    return statement->columnText(0);
}

String SQLiteDatabase::indexSQL(const String& indexName)
{
    if (!isOpen())
        return { };

    auto statement = prepareStatement("SELECT sql FROM sqlite_master WHERE type = 'index' AND name = ?;"_s);
    if (!statement || statement->bindText(1, indexName) != SQLITE_OK || statement->step() != SQLITE_ROW)
        return { };

    return statement->columnText(0);
}

void SQLiteDatabase::clearAllTables()
{
    auto statement = prepareStatement("SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%';"_s);
    if (!statement) {
        LOG(SQLDatabase, "Failed to prepare statement to retrieve list of tables from database");
        return;
    }
    Vector<String> tables;
    while (statement->step() == SQLITE_ROW)
        tables.append(statement->columnText(0));
    for (auto& table : tables) {
        if (!executeCommandSlow("DROP TABLE " + table))
            LOG(SQLDatabase, "Unable to drop table %s", table.ascii().data());
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
    Locker locker { m_authorizerLock };
    enableAuthorizer(false);

    if (!executeCommand("PRAGMA incremental_vacuum"_s))
        LOG(SQLDatabase, "Unable to run incremental vacuum - %s", lastErrorMsg());

    enableAuthorizer(true);
    return lastError();
}

void SQLiteDatabase::interrupt()
{
    Locker locker { m_databaseClosingMutex };
    if (m_db)
        sqlite3_interrupt(m_db);
}

int64_t SQLiteDatabase::lastInsertRowID()
{
    if (!m_db)
        return 0;
    return sqlite3_last_insert_rowid(m_db);
}

int SQLiteDatabase::lastChanges()
{
    if (!m_db)
        return 0;

    return sqlite3_changes(m_db);
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

    Locker locker { m_authorizerLock };

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
    int autoVacuumMode = 0;
    if (auto statement = prepareStatement("PRAGMA auto_vacuum"_s))
        autoVacuumMode = statement->columnInt(0);
    int error = lastError();

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

static Expected<sqlite3_stmt*, int> constructAndPrepareStatement(SQLiteDatabase& database, const char* query, size_t queryLength)
{
    Locker databaseLock { database.databaseMutex() };
    LOG(SQLDatabase, "SQL - prepare - %s", query);

    // Pass the length of the string including the null character to sqlite3_prepare_v2;
    // this lets SQLite avoid an extra string copy.
    size_t lengthIncludingNullCharacter = queryLength + 1;

    sqlite3_stmt* statement { nullptr };
    const char* tail = nullptr;
    int error = sqlite3_prepare_v2(database.sqlite3Handle(), query, lengthIncludingNullCharacter, &statement, &tail);
    if (error != SQLITE_OK)
        LOG(SQLDatabase, "sqlite3_prepare16 failed (%i)\n%s\n%s", error, query, sqlite3_errmsg(database.sqlite3Handle()));

    if (tail && *tail)
        error = SQLITE_ERROR;

    if (error != SQLITE_OK) {
        sqlite3_finalize(statement);
        return makeUnexpected(error);
    }

    // When the query is an empty string, sqlite3_prepare_v2() sets the statement to nullptr and yet doesn't return an error.
    if (!statement)
        return makeUnexpected(SQLITE_ERROR);

    return statement;
}

Expected<SQLiteStatement, int> SQLiteDatabase::prepareStatementSlow(const String& queryString)
{
    CString query = queryString.stripWhiteSpace().utf8();
    auto sqlStatement = constructAndPrepareStatement(*this, query.data(), query.length());
    if (!sqlStatement) {
        RELEASE_LOG_ERROR(SQLDatabase, "SQLiteDatabase::prepareStatement: Failed to prepare statement %{public}s", query.data());
        return makeUnexpected(sqlStatement.error());
    }
    return SQLiteStatement { *this, sqlStatement.value() };
}

Expected<SQLiteStatement, int> SQLiteDatabase::prepareStatement(ASCIILiteral query)
{
    auto sqlStatement = constructAndPrepareStatement(*this, query.characters(), query.length());
    if (!sqlStatement) {
        RELEASE_LOG_ERROR(SQLDatabase, "SQLiteDatabase::prepareStatement: Failed to prepare statement %{public}s", query.characters());
        return makeUnexpected(sqlStatement.error());
    }
    return SQLiteStatement { *this, sqlStatement.value() };
}

Expected<UniqueRef<SQLiteStatement>, int> SQLiteDatabase::prepareHeapStatementSlow(const String& queryString)
{
    CString query = queryString.stripWhiteSpace().utf8();
    auto sqlStatement = constructAndPrepareStatement(*this, query.data(), query.length());
    if (!sqlStatement) {
        RELEASE_LOG_ERROR(SQLDatabase, "SQLiteDatabase::prepareHeapStatement: Failed to prepare statement %{public}s", query.data());
        return makeUnexpected(sqlStatement.error());
    }
    return UniqueRef<SQLiteStatement>(*new SQLiteStatement(*this, sqlStatement.value()));
}

Expected<UniqueRef<SQLiteStatement>, int> SQLiteDatabase::prepareHeapStatement(ASCIILiteral query)
{
    auto sqlStatement = constructAndPrepareStatement(*this, query.characters(), query.length());
    if (!sqlStatement) {
        RELEASE_LOG_ERROR(SQLDatabase, "SQLiteDatabase::prepareHeapStatement: Failed to prepare statement %{public}s", query.characters());
        return makeUnexpected(sqlStatement.error());
    }
    return UniqueRef<SQLiteStatement>(*new SQLiteStatement(*this, sqlStatement.value()));
}

} // namespace WebCore
