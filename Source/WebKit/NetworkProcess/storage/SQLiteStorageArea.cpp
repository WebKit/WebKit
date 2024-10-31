/*
 * Copyright (C) 2008-2021 Apple Inc. All rights reserved.
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
#include "SQLiteStorageArea.h"

#include "Logging.h"
#include <WebCore/SQLiteDatabase.h>
#include <WebCore/SQLiteFileSystem.h>
#include <WebCore/SQLiteStatement.h>
#include <WebCore/SQLiteStatementAutoResetScope.h>
#include <WebCore/SQLiteTransaction.h>
#include <WebCore/StorageMap.h>
#include <wtf/FileSystem.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SQLiteStorageArea);

constexpr Seconds transactionDuration { 500_ms };
constexpr unsigned maximumSizeForValuesKeptInMemory { 1 * KB };
constexpr auto createItemTableStatementAlternative = "CREATE TABLE IF NOT EXISTS ItemTable (key TEXT UNIQUE ON CONFLICT REPLACE, value BLOB NOT NULL ON CONFLICT FAIL)"_s;
constexpr auto createItemTableStatement = "CREATE TABLE ItemTable (key TEXT UNIQUE ON CONFLICT REPLACE, value BLOB NOT NULL ON CONFLICT FAIL)"_s;

ASCIILiteral SQLiteStorageArea::statementString(StatementType type) const
{
    switch (type) {
    case StatementType::CountItems:
        return "SELECT COUNT(*) FROM ItemTable"_s;
    case StatementType::DeleteItem:
        return "DELETE FROM ItemTable WHERE key=?"_s;
    case StatementType::DeleteAllItems:
        return "DELETE FROM ItemTable"_s;
    case StatementType::GetItem:
        return "SELECT value FROM ItemTable WHERE key=?"_s;
    case StatementType::GetAllItems:
        return "SELECT key, value FROM ItemTable"_s;
    case StatementType::SetItem:
        return "INSERT INTO ItemTable VALUES (?, ?)"_s;
    case StatementType::Invalid:
        break;
    }

    ASSERT_NOT_REACHED();
    return ""_s;
}

Ref<SQLiteStorageArea> SQLiteStorageArea::create(unsigned quota, const WebCore::ClientOrigin& origin, const String& path, Ref<WorkQueue>&& workQueue)
{
    return adoptRef(*new SQLiteStorageArea(quota, origin, path, WTFMove(workQueue)));
}

SQLiteStorageArea::SQLiteStorageArea(unsigned quota, const WebCore::ClientOrigin& origin, const String& path, Ref<WorkQueue>&& workQueue)
    : StorageAreaBase(quota, origin)
    , m_path(path)
    , m_queue(WTFMove(workQueue))
    , m_cachedStatements(static_cast<size_t>(StatementType::Invalid))
{
    ASSERT(!isMainRunLoop());
}

void SQLiteStorageArea::close()
{
    ASSERT(!isMainRunLoop());

    m_cache = std::nullopt;
    m_cacheSize = std::nullopt;
    commitTransactionIfNecessary();
    for (size_t i = 0; i < static_cast<size_t>(StatementType::Invalid); ++i)
        m_cachedStatements[i] = nullptr;
    m_database = nullptr;
}

SQLiteStorageArea::~SQLiteStorageArea()
{
    ASSERT(!isMainRunLoop());

    bool databaseIsEmpty = isEmpty();
    close();
    if (databaseIsEmpty)
        WebCore::SQLiteFileSystem::deleteDatabaseFile(m_path);
}

bool SQLiteStorageArea::isEmpty()
{
    ASSERT(!isMainRunLoop());

    if (m_cache)
        return m_cache->isEmpty();

    if (!prepareDatabase(ShouldCreateIfNotExists::No))
        return true;

    if (!m_database)
        return true;

    auto statement = cachedStatement(StatementType::CountItems);
    if (!statement || statement->step() != SQLITE_ROW) {
        RELEASE_LOG_ERROR(Storage, "SQLiteStorageArea::isEmpty failed on executing statement (%d) - %" PUBLIC_LOG_STRING, m_database->lastError(), m_database->lastErrorMsg());
        return true;
    }

    return !statement->columnInt(0);
}

void SQLiteStorageArea::clear()
{
    ASSERT(!isMainRunLoop());

    close();
    WebCore::SQLiteFileSystem::deleteDatabaseFile(m_path);
    notifyListenersAboutClear();
}

bool SQLiteStorageArea::createTableIfNecessary()
{
    if (!m_database)
        return false;

    String statement = m_database->tableSQL("ItemTable"_s);
    if (statement == createItemTableStatement || statement == createItemTableStatementAlternative)
        return true;

    // Table exists but statement is wrong; drop it.
    if (!statement.isEmpty()) {
        if (!m_database->executeCommand("DROP TABLE ItemTable"_s)) {
            RELEASE_LOG_ERROR(Storage, "SQLiteStorageArea::createTableIfNecessary failed to drop existing item table (%d) - %" PUBLIC_LOG_STRING, m_database->lastError(), m_database->lastErrorMsg());
            return false;
        }
    }

    // Table does not exist.
    if (!m_database->executeCommand(createItemTableStatement)) {
        RELEASE_LOG_ERROR(Storage, "SQLiteStorageArea::createTableIfNecessary failed to create item table (%d) - %" PUBLIC_LOG_STRING, m_database->lastError(), m_database->lastErrorMsg());
        return false;
    }

    return true;
}

bool SQLiteStorageArea::prepareDatabase(ShouldCreateIfNotExists shouldCreateIfNotExists)
{
    if (m_database && m_database->isOpen())
        return true;

    m_database = nullptr;
    bool databaseExists = FileSystem::fileExists(m_path);
    if (shouldCreateIfNotExists == ShouldCreateIfNotExists::No && !databaseExists)
        return true;

    m_database = makeUnique<WebCore::SQLiteDatabase>();
    FileSystem::makeAllDirectories(FileSystem::parentPath(m_path));
    auto openResult  = m_database->open(m_path, WebCore::SQLiteDatabase::OpenMode::ReadWriteCreate, WebCore::SQLiteDatabase::OpenOptions::CanSuspendWhileLocked);
    if (!openResult && handleDatabaseErrorIfNeeded(m_database->lastError()) == IsDatabaseDeleted::Yes) {
        databaseExists = false;
        if (shouldCreateIfNotExists == ShouldCreateIfNotExists::No)
            return true;

        m_database = makeUnique<WebCore::SQLiteDatabase>();
        openResult = m_database->open(m_path);
    }

    if (!openResult) {
        RELEASE_LOG_ERROR(Storage, "SQLiteStorageArea::prepareDatabase failed to open database at '%s'", m_path.utf8().data());
        m_database = nullptr;
        return false;
    }

    // Since a WorkQueue isn't bound to a specific thread, we need to disable threading check.
    // We will never access the database from different threads simultaneously.
    m_database->disableThreadingChecks();

    if (!createTableIfNecessary()) {
        m_database = nullptr;
        return false;
    }

    if (!databaseExists) {
        m_cache = HashMap<String, Value> { };
        m_cacheSize = 0;
    }

    return true;
}

void SQLiteStorageArea::startTransactionIfNecessary()
{
    ASSERT(m_database);

    if (!m_transaction || m_transaction->wasRolledBackBySqlite())
        m_transaction = makeUnique<WebCore::SQLiteTransaction>(*m_database);

    if (m_transaction->inProgress())
        return;
    m_transaction->begin();

    m_queue->dispatchAfter(transactionDuration, [weakThis = WeakPtr { *this }] {
        if (weakThis)
            weakThis->commitTransactionIfNecessary();
    });
}

WebCore::SQLiteStatementAutoResetScope SQLiteStorageArea::cachedStatement(StatementType type)
{
    ASSERT(m_database);
    ASSERT(type < StatementType::Invalid);

    auto index = static_cast<uint8_t>(type);
    if (!m_cachedStatements[index]) {
        if (auto result = m_database->prepareHeapStatement(statementString(type)))
            m_cachedStatements[index] = result.value().moveToUniquePtr();
    }

    return WebCore::SQLiteStatementAutoResetScope { m_cachedStatements[index].get() };
}

Expected<String, StorageError> SQLiteStorageArea::getItem(const String& key)
{
    if (m_cache) {
        auto iterator = m_cache->find(key);
        if (iterator == m_cache->end())
            return makeUnexpected(StorageError::ItemNotFound);

        if (auto* valueString = std::get_if<String>(&iterator->value)) {
            ASSERT(!valueString->isNull());
            return *valueString;
        }
    }

    return getItemFromDatabase(key);
}

Expected<String, StorageError> SQLiteStorageArea::getItemFromDatabase(const String& key)
{
    if (!prepareDatabase(ShouldCreateIfNotExists::No))
        return makeUnexpected(StorageError::Database);

    if (!m_database)
        return makeUnexpected(StorageError::ItemNotFound);

    int result = SQLITE_OK;
    // Ensure that statement goes out of scope before handleDatabaseErrorIfNeeded(). Otherwise, we may get a CheckedPtr verification failure.
    {
        auto statement = cachedStatement(StatementType::GetItem);
        if (!statement || statement->bindText(1, key)) {
            RELEASE_LOG_ERROR(Storage, "SQLiteStorageArea::getItemFromDatabase failed on creating statement (%d) - %" PUBLIC_LOG_STRING, m_database->lastError(), m_database->lastErrorMsg());
            return makeUnexpected(StorageError::Database);
        }

        result = statement->step();
        if (result == SQLITE_ROW)
            return statement->columnBlobAsString(0);
    }

    if (result != SQLITE_DONE) {
        RELEASE_LOG_ERROR(Storage, "SQLiteStorageArea::getItemFromDatabase failed on stepping statement (%d) - %" PUBLIC_LOG_STRING, m_database->lastError(), m_database->lastErrorMsg());
        handleDatabaseErrorIfNeeded(result);

        return makeUnexpected(StorageError::Database);
    }

    return makeUnexpected(StorageError::ItemNotFound);
}

HashMap<String, String> SQLiteStorageArea::allItems()
{
    ASSERT(!isMainRunLoop());

    if (!prepareDatabase(ShouldCreateIfNotExists::No) || !m_database)
        return HashMap<String, String> { };

    HashMap<String, String> items;
    if (m_cache) {
        items.reserveInitialCapacity(m_cache->size());
        for (auto& [key, value] : *m_cache) {
            if (auto* valueString = std::get_if<String>(&value)) {
                ASSERT(!valueString->isNull());
                items.add(key, *valueString);
                continue;
            }

            if (auto result = getItemFromDatabase(key))
                items.add(key, result.value());
        }
        return items;
    }

    // Import from database.
    int result = SQLITE_OK;
    // Ensure that statement goes out of scope before handleDatabaseErrorIfNeeded(). Otherwise, we may get a CheckedPtr verification failure.
    {
        auto statement = cachedStatement(StatementType::GetAllItems);
        if (!statement) {
            RELEASE_LOG_ERROR(Storage, "SQLiteStorageArea::allItems failed on creating statement (%d) - %" PUBLIC_LOG_STRING, m_database->lastError(), m_database->lastErrorMsg());
            return { };
        }

        m_cache = HashMap<String, Value> { };
        m_cacheSize = 0;
        result = statement->step();
        while (result == SQLITE_ROW) {
            String key = statement->columnText(0);
            String value = statement->columnBlobAsString(1);
            if (!key.isNull() && !value.isNull()) {
                items.add(key, value);
                updateCacheIfNeeded(WTFMove(key), WTFMove(value));
            }

            result = statement->step();
        }
    }

    if (result != SQLITE_DONE) {
        RELEASE_LOG_ERROR(Storage, "SQLiteStorageArea::allItems failed on executing statement (%d) - %" PUBLIC_LOG_STRING, m_database->lastError(), m_database->lastErrorMsg());
        handleDatabaseErrorIfNeeded(result);
    }

    return items;
}

Expected<void, StorageError> SQLiteStorageArea::setItem(IPC::Connection::UniqueID connection, StorageAreaImplIdentifier storageAreaImplID, String&& key, String&& value, const String& urlString)
{
    ASSERT(!isMainRunLoop());

    if (!prepareDatabase(ShouldCreateIfNotExists::Yes))
        return makeUnexpected(StorageError::Database);

    if (!requestSpace(key, value))
        return makeUnexpected(StorageError::QuotaExceeded);

    startTransactionIfNecessary();

    auto getItemResult = getItem(key);

    String oldValue;
    if (getItemResult.has_value()) {
        oldValue = getItemResult.value();
    } else if (getItemResult.error() != StorageError::ItemNotFound)
        return makeUnexpected(getItemResult.error()); // Database error.

    int result = SQLITE_OK;
    // Ensure that statement goes out of scope before handleDatabaseErrorIfNeeded(). Otherwise, we may get a CheckedPtr verification failure.
    {
        auto statement = cachedStatement(StatementType::SetItem);
        if (!statement || statement->bindText(1, key) || statement->bindBlob(2, value)) {
            RELEASE_LOG_ERROR(Storage, "SQLiteStorageArea::setItem failed on creating statement (%d) - %" PUBLIC_LOG_STRING, m_database->lastError(), m_database->lastErrorMsg());
            return makeUnexpected(StorageError::Database);
        }

        result = statement->step();
    }

    if (result != SQLITE_DONE) {
        RELEASE_LOG_ERROR(Storage, "SQLiteStorageArea::setItem failed on stepping statement (%d) - %" PUBLIC_LOG_STRING, m_database->lastError(), m_database->lastErrorMsg());
        handleDatabaseErrorIfNeeded(result);
        return makeUnexpected(StorageError::Database);
    }

    dispatchEvents(connection, storageAreaImplID, key, oldValue, value, urlString);
    updateCacheIfNeeded(key, value);

    return { };
}

Expected<void, StorageError> SQLiteStorageArea::removeItem(IPC::Connection::UniqueID connection, StorageAreaImplIdentifier storageAreaImplID, const String& key, const String& urlString)
{
    ASSERT(!isMainRunLoop());

    if (!prepareDatabase(ShouldCreateIfNotExists::No))
        return makeUnexpected(StorageError::Database);

    if (!m_database)
        return makeUnexpected(StorageError::ItemNotFound);

    startTransactionIfNecessary();
    String oldValue;
    if (auto valueOrError = getItem(key))
        oldValue = valueOrError.value();
    else
        return makeUnexpected(StorageError::ItemNotFound);

    int result = SQLITE_OK;
    // Ensure that statement goes out of scope before handleDatabaseErrorIfNeeded(). Otherwise, we may get a CheckedPtr verification failure.
    {
        auto statement = cachedStatement(StatementType::DeleteItem);
        if (!statement || statement->bindText(1, key)) {
            RELEASE_LOG_ERROR(Storage, "SQLiteStorageArea::removeItem failed on creating statement (%d) - %" PUBLIC_LOG_STRING, m_database->lastError(), m_database->lastErrorMsg());
            return makeUnexpected(StorageError::Database);
        }

        result = statement->step();
    }

    if (result != SQLITE_DONE) {
        RELEASE_LOG_ERROR(Storage, "SQLiteStorageArea::removeItem failed on executing statement (%d) - %" PUBLIC_LOG_STRING, m_database->lastError(), m_database->lastErrorMsg());
        handleDatabaseErrorIfNeeded(result);

        return makeUnexpected(StorageError::Database);
    }

    dispatchEvents(connection, storageAreaImplID, key, oldValue, String(), urlString);
    updateCacheIfNeeded(key, { });

    return { };
}

Expected<void, StorageError> SQLiteStorageArea::clear(IPC::Connection::UniqueID connection, StorageAreaImplIdentifier storageAreaImplID, const String& urlString)
{
    ASSERT(!isMainRunLoop());

    if (!prepareDatabase(ShouldCreateIfNotExists::No))
        return makeUnexpected(StorageError::Database);

    if (m_cache && m_cache->isEmpty())
        return makeUnexpected(StorageError::ItemNotFound);

    if (m_cache) {
        m_cache->clear();
        m_cacheSize = 0;
    }

    if (!m_database)
        return makeUnexpected(StorageError::ItemNotFound);

    startTransactionIfNecessary();

    int result = SQLITE_OK;
    // Ensure that statement goes out of scope before handleDatabaseErrorIfNeeded(). Otherwise, we may get a CheckedPtr verification failure.
    {
        auto statement = cachedStatement(StatementType::DeleteAllItems);
        if (!statement) {
            RELEASE_LOG_ERROR(Storage, "SQLiteStorageArea::clear failed on creating statement (%d) - %" PUBLIC_LOG_STRING, m_database->lastError(), m_database->lastErrorMsg());
            return makeUnexpected(StorageError::Database);
        }

        result = statement->step();
    }

    if (result != SQLITE_DONE) {
        RELEASE_LOG_ERROR(Storage, "SQLiteStorageArea::clear failed on executing statement (%d) - %" PUBLIC_LOG_STRING, m_database->lastError(), m_database->lastErrorMsg());
        handleDatabaseErrorIfNeeded(result);

        return makeUnexpected(StorageError::Database);
    }

    if (m_database->lastChanges() <= 0)
        return makeUnexpected(StorageError::ItemNotFound);

    dispatchEvents(connection, storageAreaImplID, String(), String(), String(), urlString);

    return { };
}

void SQLiteStorageArea::commitTransactionIfNecessary()
{
    if (auto transaction = std::exchange(m_transaction, nullptr))
        transaction->commit();
}

void SQLiteStorageArea::handleLowMemoryWarning()
{
    ASSERT(!isMainRunLoop());

    if (m_database && m_database->isOpen())
        m_database->releaseMemory();
}

SQLiteStorageArea::IsDatabaseDeleted SQLiteStorageArea::handleDatabaseErrorIfNeeded(int databaseError)
{
    if ((databaseError & 0XFF) == SQLITE_IOERR) {
        close();
        return IsDatabaseDeleted::No;
    }

    if (databaseError == SQLITE_CORRUPT || databaseError == SQLITE_NOTADB) {
        close();

        RELEASE_LOG_ERROR(Storage, "SQLiteStorageArea::handleDatabaseCorruption deletes corrupted database file '%s'", m_path.utf8().data());
        WebCore::SQLiteFileSystem::deleteDatabaseFile(m_path);
        return IsDatabaseDeleted::Yes;
    }

    return IsDatabaseDeleted::No;
}

void SQLiteStorageArea::updateCacheIfNeeded(const String& key, const String& value)
{
    if (!m_cache)
        return;

    ASSERT(m_cacheSize);
    auto iter = m_cache->find(key);
    bool itemExists = iter != m_cache->end();
    unsigned oldKeySize = 0;
    unsigned oldValueSize = 0;
    unsigned keySize = key.sizeInBytes();
    unsigned valueSize = value.sizeInBytes();
    if (itemExists) {
        oldKeySize = iter->key.sizeInBytes();
        WTF::switchOn(iter->value, [&](unsigned valueSize) {
            oldValueSize = valueSize;
        }, [&](const String& value) {
            oldValueSize = value.sizeInBytes();
        });
    }

    CheckedUint32 newCacheSize = *m_cacheSize;
    // Null value means to remove.
    if (value.isNull()) {
        m_cache->remove(key);
        newCacheSize -= oldKeySize;
        newCacheSize -= oldValueSize;
    } else {
        if (valueSize > maximumSizeForValuesKeptInMemory)
            m_cache->set(key, valueSize);
        else
            m_cache->set(key, value);
        newCacheSize -= oldKeySize;
        newCacheSize -= oldValueSize;
        newCacheSize += itemExists ? oldKeySize : keySize;
        newCacheSize += valueSize;
    }

    if (newCacheSize.hasOverflowed()) {
        RELEASE_LOG_ERROR(Storage, "SQLiteStorageArea::updateCacheIfNeeded newCacheSize has overflowed: cacheSize - %u, oldKeySize - %u, oldValueSize - %u, keySize - %u, valueSize - %u, will recompute", *m_cacheSize, oldKeySize, oldValueSize, keySize, valueSize);
        newCacheSize = 0;
        for (auto& value : m_cache->values()) {
            WTF::switchOn(value, [&](unsigned size) {
                newCacheSize += size;
            }, [&](const String& value) {
                newCacheSize += value.sizeInBytes();
            });
        }
    }

    m_cacheSize = newCacheSize;
}

bool SQLiteStorageArea::requestSpace(const String& key, const String& value)
{
    ASSERT(m_database && m_database->isOpen());
    if (!m_cache)
        return key.sizeInBytes() + value.sizeInBytes() <= quota();

    if (value.isNull())
        return true;

    ASSERT(m_cacheSize);
    CheckedUint32 newCacheSize = *m_cacheSize;
    auto iter = m_cache->find(key);
    if (iter == m_cache->end())
        newCacheSize += key.sizeInBytes();
    else {
        auto oldValueSize = WTF::switchOn(iter->value, [](unsigned valueSize) {
            return valueSize;
        }, [](const String& value) {
            return value.sizeInBytes();
        });
        newCacheSize -= oldValueSize;
    }
    newCacheSize += value.sizeInBytes();
    if (newCacheSize.hasOverflowed())
        return false;

    return newCacheSize <= quota();
}

} // namespace WebKit
