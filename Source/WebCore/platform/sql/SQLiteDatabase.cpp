/*
 * Copyright (C) 2006-2023 Apple Inc. All rights reserved.
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
#include <wtf/FastMalloc.h>
#include <wtf/FileSystem.h>
#include <wtf/Lock.h>
#include <wtf/Scope.h>
#include <wtf/Threading.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringConcatenateNumbers.h>

#if !USE(SYSTEM_MALLOC)
#include <bmalloc/BPlatform.h>
#define ENABLE_SQLITE_FAST_MALLOC (BENABLE(MALLOC_SIZE) && BENABLE(MALLOC_GOOD_SIZE))
#endif

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

void SQLiteDatabase::useFastMalloc()
{
#if ENABLE(SQLITE_FAST_MALLOC)
    int returnCode = sqlite3_config(SQLITE_CONFIG_LOOKASIDE, 0, 0);
    RELEASE_LOG_ERROR_IF(returnCode != SQLITE_OK, SQLDatabase, "Unable to reduce lookaside buffer size: %d", returnCode);

    static sqlite3_mem_methods fastMallocMethods = {
        [](int n) { return fastMalloc(n); },
        fastFree,
        [](void *p, int n) { return fastRealloc(p, n); },
        [](void *p) { return static_cast<int>(fastMallocSize(p)); },
        [](int n) { return static_cast<int>(fastMallocGoodSize(n)); },
        [](void*) { return SQLITE_OK; },
        [](void*) { },
        nullptr
    };

    returnCode = sqlite3_config(SQLITE_CONFIG_MALLOC, &fastMallocMethods);
    RELEASE_LOG_ERROR_IF(returnCode != SQLITE_OK, SQLDatabase, "Unable to replace SQLite malloc: %d", returnCode);
#endif
}

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

    auto closeDatabase = makeScopeExit([&]() {
        if (!m_db)
            return;

        m_openingThread = nullptr;
        m_openErrorMessage = sqlite3_errmsg(m_db);
        m_openError = sqlite3_errcode(m_db);
        close();
    });

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

        int result = SQLITE_OK;
        {
            SQLiteTransactionInProgressAutoCounter transactionCounter;
            result = sqlite3_open_v2(FileSystem::fileSystemRepresentation(filename).data(), &m_db, flags, nullptr);
        }

        if (result != SQLITE_OK) {
            if (!m_db) {
                m_openError = result;
                m_openErrorMessage = "sqlite_open returned null";
            }
            return false;
        }
    }

    overrideUnauthorizedFunctions();

    m_openingThread = &Thread::current();
    if (sqlite3_extended_result_codes(m_db, 1) != SQLITE_OK)
        return false;

    {
        SQLiteTransactionInProgressAutoCounter transactionCounter;
        if (!executeCommand("PRAGMA temp_store = MEMORY;"_s))
            LOG_ERROR("SQLite database could not set temp_store to memory");
    }

    if (filename != inMemoryPath()) {
        if (openMode != OpenMode::ReadOnly && !useWALJournalMode())
            return false;

        auto shmFileName = makeString(filename, "-shm"_s);
        if (FileSystem::fileExists(shmFileName) && !FileSystem::isSafeToUseMemoryMapForPath(shmFileName)) {
            RELEASE_LOG_FAULT(SQLDatabase, "Opened an SQLite database with a Class A -shm file. This may trigger a crash when the user locks the device. (%s)", shmFileName.latin1().data());
            if (!FileSystem::makeSafeToUseMemoryMapForPath(shmFileName))
                return false;
        }
    }

    closeDatabase.release();
    return true;
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

bool SQLiteDatabase::useWALJournalMode()
{
    m_useWAL = true;
    {
        SQLiteTransactionInProgressAutoCounter transactionCounter;
        auto walStatement = prepareStatement("PRAGMA journal_mode=WAL;"_s);
        if (!walStatement)
            return false;

        int stepResult = walStatement->step();
        if (stepResult != SQLITE_ROW)
            return false;

#ifndef NDEBUG
        String mode = walStatement->columnText(0);
        if (!equalLettersIgnoringASCIICase(mode, "wal"_s)) {
            LOG_ERROR("SQLite database journal_mode should be 'WAL', but is '%s'", mode.utf8().data());
            return false;
        }
#endif
    }

    // The database can be used even if checkpoint fails, e.g. when there are multiple open database connections.
    checkpoint(CheckpointMode::Truncate);

    return true;
}

void SQLiteDatabase::close()
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
            RELEASE_LOG_ERROR(SQLDatabase, "SQLiteDatabase::close: Failed to close database (%d) - %" PUBLIC_LOG_STRING, closeResult, lastErrorMsg());
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

bool SQLiteDatabase::executeCommandSlow(StringView query)
{
    auto statement = prepareStatementSlow(query);
    return statement && statement->executeCommand();
}

bool SQLiteDatabase::executeCommand(ASCIILiteral query)
{
    auto statement = prepareStatement(query);
    return statement && statement->executeCommand();
}

bool SQLiteDatabase::tableExists(StringView tableName)
{
    return !tableSQL(tableName).isEmpty();
}

String SQLiteDatabase::tableSQL(StringView tableName)
{
    if (!isOpen())
        return { };

    auto statement = prepareStatement("SELECT sql FROM sqlite_master WHERE type = 'table' AND name = ?;"_s);
    if (!statement || statement->bindText(1, tableName) != SQLITE_OK || statement->step() != SQLITE_ROW)
        return { };

    return statement->columnText(0);
}

String SQLiteDatabase::indexSQL(StringView indexName)
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
        if (!executeCommandSlow(makeString("DROP TABLE ", table)))
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

    auto parameter1String = String::fromLatin1(parameter1);
    auto parameter2String = String::fromLatin1(parameter2);
    switch (actionCode) {
        case SQLITE_CREATE_INDEX:
            return auth->createIndex(parameter1String, parameter2String);
        case SQLITE_CREATE_TABLE:
            return auth->createTable(parameter1String);
        case SQLITE_CREATE_TEMP_INDEX:
            return auth->createTempIndex(parameter1String, parameter2String);
        case SQLITE_CREATE_TEMP_TABLE:
            return auth->createTempTable(parameter1String);
        case SQLITE_CREATE_TEMP_TRIGGER:
            return auth->createTempTrigger(parameter1String, parameter2String);
        case SQLITE_CREATE_TEMP_VIEW:
            return auth->createTempView(parameter1String);
        case SQLITE_CREATE_TRIGGER:
            return auth->createTrigger(parameter1String, parameter2String);
        case SQLITE_CREATE_VIEW:
            return auth->createView(parameter1String);
        case SQLITE_DELETE:
            return auth->allowDelete(parameter1String);
        case SQLITE_DROP_INDEX:
            return auth->dropIndex(parameter1String, parameter2String);
        case SQLITE_DROP_TABLE:
            return auth->dropTable(parameter1String);
        case SQLITE_DROP_TEMP_INDEX:
            return auth->dropTempIndex(parameter1String, parameter2String);
        case SQLITE_DROP_TEMP_TABLE:
            return auth->dropTempTable(parameter1String);
        case SQLITE_DROP_TEMP_TRIGGER:
            return auth->dropTempTrigger(parameter1String, parameter2String);
        case SQLITE_DROP_TEMP_VIEW:
            return auth->dropTempView(parameter1String);
        case SQLITE_DROP_TRIGGER:
            return auth->dropTrigger(parameter1String, parameter2String);
        case SQLITE_DROP_VIEW:
            return auth->dropView(parameter1String);
        case SQLITE_INSERT:
            return auth->allowInsert(parameter1String);
        case SQLITE_PRAGMA:
            return auth->allowPragma(parameter1String, parameter2String);
        case SQLITE_READ:
            return auth->allowRead(parameter1String, parameter2String);
        case SQLITE_SELECT:
            return auth->allowSelect();
        case SQLITE_TRANSACTION:
            return auth->allowTransaction();
        case SQLITE_UPDATE:
            return auth->allowUpdate(parameter1String, parameter2String);
        case SQLITE_ATTACH:
            return auth->allowAttach(parameter1String);
        case SQLITE_DETACH:
            return auth->allowDetach(parameter1String);
        case SQLITE_ALTER_TABLE:
            return auth->allowAlterTable(parameter1String, parameter2String);
        case SQLITE_REINDEX:
            return auth->allowReindex(parameter1String);
        case SQLITE_ANALYZE:
            return auth->allowAnalyze(parameter1String);
        case SQLITE_CREATE_VTABLE:
            return auth->createVTable(parameter1String, parameter2String);
        case SQLITE_DROP_VTABLE:
            return auth->dropVTable(parameter1String, parameter2String);
        case SQLITE_FUNCTION:
            return auth->allowFunction(parameter2String);
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
    int autoVacuumMode = AutoVacuumNone;
    {
        auto statement = prepareStatement("PRAGMA auto_vacuum"_s);
        if (!statement)
            return false;
        autoVacuumMode = statement->columnInt(0);
    }

    // Check if we got an error while trying to get the value of the auto_vacuum flag.
    // If we got a SQLITE_BUSY error, then there's probably another transaction in
    // progress on this database. In this case, keep the current value of the
    // auto_vacuum flag and try to set it to INCREMENTAL the next time we open this
    // database. If the error is not SQLITE_BUSY, then we probably ran into a more
    // serious problem and should return false (to log an error message).
    if (lastError() != SQLITE_ROW)
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
        return lastError() == SQLITE_OK;
    }
}

static void destroyCollationFunction(void* arg)
{
    auto f = static_cast<Function<int(int, const void*, int, const void*)>*>(arg);
    delete f;
}

static int callCollationFunction(void* arg, int aLength, const void* a, int bLength, const void* b)
{
    auto f = static_cast<Function<int(int, const void*, int, const void*)>*>(arg);
    return (*f)(aLength, a, bLength, b);
}

void SQLiteDatabase::setCollationFunction(const String& collationName, Function<int(int, const void*, int, const void*)>&& collationFunction)
{
    auto functionObject = new Function<int(int, const void*, int, const void*)>(WTFMove(collationFunction));
    sqlite3_create_collation_v2(m_db, collationName.utf8().data(), SQLITE_UTF8, functionObject, callCollationFunction, destroyCollationFunction);
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

Expected<SQLiteStatement, int> SQLiteDatabase::prepareStatementSlow(StringView queryString)
{
    CString query = queryString.stripWhiteSpace().utf8();
    auto sqlStatement = constructAndPrepareStatement(*this, query.data(), query.length());
    if (!sqlStatement) {
        RELEASE_LOG_ERROR(SQLDatabase, "SQLiteDatabase::prepareStatement: Failed to prepare statement %" PUBLIC_LOG_STRING, query.data());
        return makeUnexpected(sqlStatement.error());
    }
    return SQLiteStatement { *this, sqlStatement.value() };
}

Expected<SQLiteStatement, int> SQLiteDatabase::prepareStatement(ASCIILiteral query)
{
    auto sqlStatement = constructAndPrepareStatement(*this, query.characters(), query.length());
    if (!sqlStatement) {
        RELEASE_LOG_ERROR(SQLDatabase, "SQLiteDatabase::prepareStatement: Failed to prepare statement %" PUBLIC_LOG_STRING, query.characters());
        return makeUnexpected(sqlStatement.error());
    }
    return SQLiteStatement { *this, sqlStatement.value() };
}

Expected<UniqueRef<SQLiteStatement>, int> SQLiteDatabase::prepareHeapStatementSlow(StringView queryString)
{
    CString query = queryString.stripWhiteSpace().utf8();
    auto sqlStatement = constructAndPrepareStatement(*this, query.data(), query.length());
    if (!sqlStatement) {
        RELEASE_LOG_ERROR(SQLDatabase, "SQLiteDatabase::prepareHeapStatement: Failed to prepare statement %" PUBLIC_LOG_STRING, query.data());
        return makeUnexpected(sqlStatement.error());
    }
    return UniqueRef<SQLiteStatement>(*new SQLiteStatement(*this, sqlStatement.value()));
}

Expected<UniqueRef<SQLiteStatement>, int> SQLiteDatabase::prepareHeapStatement(ASCIILiteral query)
{
    auto sqlStatement = constructAndPrepareStatement(*this, query.characters(), query.length());
    if (!sqlStatement) {
        RELEASE_LOG_ERROR(SQLDatabase, "SQLiteDatabase::prepareHeapStatement: Failed to prepare statement %" PUBLIC_LOG_STRING, query.characters());
        return makeUnexpected(sqlStatement.error());
    }
    return UniqueRef<SQLiteStatement>(*new SQLiteStatement(*this, sqlStatement.value()));
}

} // namespace WebCore
