/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "PushDatabase.h"

#if ENABLE(SERVICE_WORKER)

#include "Logging.h"
#include "SQLValue.h"
#include "SQLiteFileSystem.h"
#include "SQLiteTransaction.h"
#include "SecurityOrigin.h"
#include <iterator>
#include <wtf/CrossThreadCopier.h>
#include <wtf/Expected.h>
#include <wtf/FileSystem.h>
#include <wtf/RunLoop.h>
#include <wtf/Scope.h>
#include <wtf/UniqueRef.h>
#include <wtf/text/StringConcatenateNumbers.h>

#define PUSHDB_RELEASE_LOG(fmt, ...) RELEASE_LOG(Push, "%p - PushDatabase::" fmt, this, ##__VA_ARGS__)
#define PUSHDB_RELEASE_LOG_ERROR(fmt, ...) RELEASE_LOG_ERROR(Push, "%p - PushDatabase::" fmt, this, ##__VA_ARGS__)

#define kPushRecordColumns " sub.rowID, ss.bundleID, ss.pushPartition, ss.dataStoreUUID, ss.securityOrigin, sub.scope, sub.endpoint, sub.topic, sub.serverVAPIDPublicKey, sub.clientPublicKey, sub.clientPrivateKey, sub.sharedAuthSecret, sub.expirationTime "

// This helper makers sure that we always bind the empty string rather than NULL for the
// subscription set identifier columns. This is because these columns are part of a unique index
// constraint. Since all NULL values are treated as distinct by SQLite, this would render the unique
// index constraint meaningless. 
#define bindSubscriptionSetParameters(ss) \
    !(ss).bundleIdentifier.isNull() ? (ss).bundleIdentifier : emptyString(), \
    !(ss).pushPartition.isNull() ? (ss).pushPartition : emptyString(), \
    uuidToSpan((ss).dataStoreIdentifier)

namespace WebCore {

static constexpr ASCIILiteral pushDatabaseSchemaV1Statements[] = {
    "PRAGMA auto_vacuum=INCREMENTAL"_s,
};

static constexpr ASCIILiteral pushDatabaseSchemaV2Statements[] = {
    "CREATE TABLE SubscriptionSets("
    "  rowID INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  creationTime INT NOT NULL,"
    "  bundleID TEXT NOT NULL,"
    "  securityOrigin TEXT NOT NULL,"
    "  silentPushCount INT NOT NULL,"
    "  UNIQUE(bundleID, securityOrigin))"_s,
    "CREATE TABLE Subscriptions("
    "  rowID INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  creationTime INT NOT NULL,"
    "  subscriptionSetID INT NOT NULL,"
    "  scope TEXT NOT NULL,"
    "  endpoint TEXT NOT NULL,"
    "  topic TEXT NOT NULL UNIQUE,"
    "  serverVAPIDPublicKey BLOB NOT NULL,"
    "  clientPublicKey BLOB NOT NULL,"
    "  clientPrivateKey BLOB NOT NULL,"
    "  sharedAuthSecret BLOB NOT NULL,"
    "  expirationTime INT,"
    "  UNIQUE(scope, subscriptionSetID))"_s,
    "CREATE INDEX Subscriptions_SubscriptionSetID_Index ON Subscriptions(subscriptionSetID)"_s,
};

static constexpr ASCIILiteral pushDatabaseSchemaV3Statements[] = {
    "CREATE TABLE Metadata(key TEXT, value, UNIQUE(key))"_s,
};

static constexpr ASCIILiteral pushDatabaseSchemaV4Statements[] = {
    "ALTER TABLE SubscriptionSets ADD COLUMN state INT NOT NULL DEFAULT 0"_s,
};

static constexpr ASCIILiteral pushDatabaseSchemaV5Statements[] = {
    "ALTER TABLE SubscriptionSets RENAME TO SubscriptionSetsOld"_s,
    "CREATE TABLE SubscriptionSets("
    "  rowID INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  creationTime INT NOT NULL,"
    "  bundleID TEXT NOT NULL,"
    "  pushPartition TEXT NOT NULL,"
    "  dataStoreUUID BLOB NOT NULL,"
    "  securityOrigin TEXT NOT NULL,"
    "  silentPushCount INT NOT NULL,"
    "  state INT NOT NULL DEFAULT 0,"
    "  UNIQUE(bundleID, pushPartition, dataStoreUUID, securityOrigin))"_s,
    "INSERT INTO SubscriptionSets SELECT rowID, creationTime, bundleID, '', X'', securityOrigin, silentPushCount, state FROM SubscriptionSetsOld"_s,
    "DROP TABLE SubscriptionSetsOld"_s,
};

static constexpr Span<const ASCIILiteral> pushDatabaseSchemaStatements[] = {
    { pushDatabaseSchemaV1Statements },
    { pushDatabaseSchemaV2Statements },
    { pushDatabaseSchemaV3Statements },
    { pushDatabaseSchemaV4Statements },
    { pushDatabaseSchemaV5Statements },
};

static constexpr int currentPushDatabaseVersion = std::size(pushDatabaseSchemaStatements);

static constexpr ASCIILiteral publicTokenKey = "publicToken"_s;

PushRecord PushRecord::isolatedCopy() const &
{
    return {
        identifier,
        subscriptionSetIdentifier.isolatedCopy(),
        securityOrigin.isolatedCopy(),
        scope.isolatedCopy(),
        endpoint.isolatedCopy(),
        topic.isolatedCopy(),
        serverVAPIDPublicKey,
        clientPublicKey,
        clientPrivateKey,
        sharedAuthSecret,
        expirationTime
    };
}

PushRecord PushRecord::isolatedCopy() &&
{
    return {
        identifier,
        WTFMove(subscriptionSetIdentifier).isolatedCopy(),
        WTFMove(securityOrigin).isolatedCopy(),
        WTFMove(scope).isolatedCopy(),
        WTFMove(endpoint).isolatedCopy(),
        WTFMove(topic).isolatedCopy(),
        WTFMove(serverVAPIDPublicKey),
        WTFMove(clientPublicKey),
        WTFMove(clientPrivateKey),
        WTFMove(sharedAuthSecret),
        expirationTime
    };
}

RemovedPushRecord RemovedPushRecord::isolatedCopy() const &
{
    return { identifier, topic.isolatedCopy(), serverVAPIDPublicKey };
}

RemovedPushRecord RemovedPushRecord::isolatedCopy() &&
{
    return { identifier, WTFMove(topic).isolatedCopy(), WTFMove(serverVAPIDPublicKey) };
}

PushTopics PushTopics::isolatedCopy() const &
{
    return { crossThreadCopy(enabledTopics), crossThreadCopy(ignoredTopics) };
}

PushTopics PushTopics::isolatedCopy() &&
{
    return { crossThreadCopy(WTFMove(enabledTopics)), crossThreadCopy(WTFMove(ignoredTopics)) };
}

enum class ShouldDeleteAndRetry { No, Yes };

static Expected<UniqueRef<SQLiteDatabase>, ShouldDeleteAndRetry> openAndMigrateDatabaseImpl(const String& path)
{
    ASSERT(!RunLoop::isMain());

    if (path != ":memory:"_s && !FileSystem::fileExists(path) && !FileSystem::makeAllDirectories(FileSystem::parentPath(path))) {
        RELEASE_LOG_ERROR(Push, "Couldn't create PushDatabase parent directories for path %s", path.utf8().data());
        return makeUnexpected(ShouldDeleteAndRetry::No);
    }

    auto db = WTF::makeUniqueRef<SQLiteDatabase>();
    db->disableThreadingChecks();

    if (!db->open(path)) {
        RELEASE_LOG_ERROR(Push, "Couldn't open PushDatabase at path %s", path.utf8().data());
        return makeUnexpected(ShouldDeleteAndRetry::Yes);
    }

    int version = 0;
    {
        auto sql = db->prepareStatement("PRAGMA user_version"_s);
        if (!sql || sql->step() != SQLITE_ROW) {
            RELEASE_LOG_ERROR(Push, "Couldn't get PushDatabase version at path %s", path.utf8().data());
            return makeUnexpected(ShouldDeleteAndRetry::Yes);
        }
        version = sql->columnInt(0);
    }

    if (version < 0 || version > currentPushDatabaseVersion) {
        RELEASE_LOG_ERROR(Push, "Found unexpected PushDatabase version: %d (expected: %d) at path: %s", version, currentPushDatabaseVersion, path.utf8().data());
        return makeUnexpected(ShouldDeleteAndRetry::Yes);
    }

    if (version < currentPushDatabaseVersion) {
        FileSystem::setExcludedFromBackup(FileSystem::parentPath(path), true);

        SQLiteTransaction transaction(db);
        transaction.begin();

        for (auto i = version; i < currentPushDatabaseVersion; i++) {
            for (auto statement : pushDatabaseSchemaStatements[i]) {
                if (!db->executeCommand(statement)) {
                    RELEASE_LOG_ERROR(Push, "Error executing PushDatabase DDL statement %s at path %s: %d", statement.characters(), path.utf8().data(), db->lastError());
                    return makeUnexpected(ShouldDeleteAndRetry::Yes);
                }
            }
        }

        if (!db->executeCommandSlow(makeString("PRAGMA user_version = ", currentPushDatabaseVersion)))
            RELEASE_LOG_ERROR(Push, "Error setting user version for PushDatabase at path %s: %d", path.utf8().data(), db->lastError());

        transaction.commit();
    }

    return db;
}

static std::unique_ptr<SQLiteDatabase> openAndMigrateDatabase(const String& path)
{
    ASSERT(!RunLoop::isMain());

    auto result = openAndMigrateDatabaseImpl(path);
    if (!result && result.error() == ShouldDeleteAndRetry::Yes) {
        if (path == SQLiteDatabase::inMemoryPath() || !SQLiteFileSystem::deleteDatabaseFile(path)) {
            RELEASE_LOG_ERROR(Push, "Failed to delete PushDatabase at path %s; bailing on recreating from scratch", path.utf8().data());
            return nullptr;
        }

        RELEASE_LOG_ERROR(Push, "Deleted PushDatabase at path %s and recreating from scratch", path.utf8().data());
        result = openAndMigrateDatabaseImpl(path);
    }

    if (!result)
        return nullptr;

    auto database = WTFMove(*result);
    return database.moveToUniquePtr();
}

void PushDatabase::create(const String& path, CreationHandler&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    auto queue = WorkQueue::create("PushDatabase I/O Thread");
    queue->dispatch([queue, path = crossThreadCopy(path), completionHandler = WTFMove(completionHandler)]() mutable {
        auto database = openAndMigrateDatabase(path);
        WorkQueue::main().dispatch([queue = WTFMove(queue), database = WTFMove(database), completionHandler = WTFMove(completionHandler)]() mutable {
            if (!database) {
                completionHandler(nullptr);
                return;
            }

            completionHandler(std::unique_ptr<PushDatabase>(new PushDatabase(WTFMove(queue), makeUniqueRefFromNonNullUniquePtr(WTFMove(database)))));
        });
    });
}

PushDatabase::PushDatabase(Ref<WorkQueue>&& queue, UniqueRef<SQLiteDatabase>&& db)
    : m_queue(WTFMove(queue))
    , m_db(WTFMove(db))
{
}

PushDatabase::~PushDatabase()
{
    // In practice we aren't actually expecting this to run, since a NeverDestroyed<WebPushDaemon> instance
    // holds on to this object.
    //
    // If we intend to delete this object for real, we should probably make this object refcounted and make
    // the blocks on the queue protect the database object rather than using dispatchSync.
    ASSERT(RunLoop::isMain());

    // Flush any outstanding requests.
    m_queue->dispatchSync([]() { });

    // Finalize member variables on the queue, since they were are only meant to be used on the queue.
    m_queue->dispatchSync([db = WTFMove(m_db), statements = WTFMove(m_statements)]() mutable {
        statements.clear();
        db->close();
    });
}

void PushDatabase::dispatchOnWorkQueue(Function<void()>&& function)
{
    RELEASE_ASSERT(RunLoop::isMain());
    m_queue->dispatch(WTFMove(function));
}

SQLiteStatementAutoResetScope PushDatabase::cachedStatementOnQueue(ASCIILiteral query)
{
    ASSERT(!RunLoop::isMain());

    auto it = m_statements.find(query);
    if (it != m_statements.end())
        return SQLiteStatementAutoResetScope(it->value.ptr());

    auto result = m_db->prepareHeapStatement(query);
    if (!result) {
        PUSHDB_RELEASE_LOG_ERROR("Failed with %d preparing statement: %" PUBLIC_LOG_STRING, result.error(), query.characters());
        return SQLiteStatementAutoResetScope(nullptr);
    }

    auto ref = WTFMove(*result);
    auto statement = ref.ptr();
    m_statements.add(query, WTFMove(ref));
    return SQLiteStatementAutoResetScope(statement);
}

template<typename... Args>
WebCore::SQLiteStatementAutoResetScope PushDatabase::bindStatementOnQueue(ASCIILiteral query, Args&&... args)
{
    auto sql = cachedStatementOnQueue(query);
    if (!sql || !sql->bind(std::forward<Args>(args)...)) {
        PUSHDB_RELEASE_LOG_ERROR("Failed with %d (%" PUBLIC_LOG_STRING ") while binding statement: %" PUBLIC_LOG_STRING, m_db->lastError(), m_db->lastErrorMsg(), query.characters());
        return SQLiteStatementAutoResetScope(nullptr);
    }

    return sql;
}

static Span<const uint8_t> uuidToSpan(const std::optional<UUID>& uuid)
{
    if (!uuid) {
        // We store a null UUID as a zero-length blob rather than a SQL NULL. This is because the
        // data store UUID is part of a unique index, and storing the UUID as a SQL NULL doesn't
        // work well with a unique index (since all SQL NULL values are treated as distinct values).
        //
        // To do this, the span must have zero length and point to a non-nullptr value. This is
        // because calling sqlite3_bind_blob with a nullptr causes a SQL NULL to be stored in the
        // column rather than a zero-length blob.
        static const uint8_t junk = 0;
        return Span(&junk, static_cast<size_t>(0));
    }

    return uuid->toSpan();
}

static std::optional<UUID> uuidFromSpan(Span<const uint8_t> span)
{
    if (span.size() != 16)
        return std::nullopt;

    return UUID(span.first<16>());
}

static SQLValue expirationTimeToValue(std::optional<EpochTimeStamp> timestamp)
{
    if (!timestamp)
        return nullptr;

    return convertEpochTimeStampToSeconds(*timestamp);
}

static std::optional<EpochTimeStamp> expirationTimeFromValue(SQLValue value)
{
    if (std::holds_alternative<double>(value))
        return convertSecondsToEpochTimeStamp(std::get<double>(value));

    return std::nullopt;
}

template <class T, class U>
static void completeOnMainQueue(CompletionHandler<void(T)>&& completionHandler, U&& result)
{
    ASSERT(!RunLoop::isMain());
    WorkQueue::main().dispatch([completionHandler = WTFMove(completionHandler), result = crossThreadCopy(std::forward<U>(result))]() mutable {
        completionHandler(WTFMove(result));
    });
}

void PushDatabase::updatePublicToken(Span<const uint8_t> publicToken, CompletionHandler<void(PublicTokenChanged)>&& completionHandler)
{
    dispatchOnWorkQueue([this, newPublicToken = Vector<uint8_t> { publicToken }, completionHandler = WTFMove(completionHandler)]() mutable {
        SQLiteTransaction transaction(m_db);
        transaction.begin();

        auto result = PublicTokenChanged::No;
        Vector<uint8_t> currentPublicToken;
        auto scope = makeScopeExit([&completionHandler, &result] {
            completeOnMainQueue(WTFMove(completionHandler), result);
        });

        {
            auto sql = bindStatementOnQueue("SELECT value FROM Metadata WHERE key = ?"_s, publicTokenKey);
            if (!sql)
                return;

            if (sql->step() == SQLITE_ROW)
                currentPublicToken = sql->columnBlob(0);
        }

        if (currentPublicToken == newPublicToken)
            return;

        {
            auto sql = bindStatementOnQueue("REPLACE INTO Metadata(key, value) VALUES(?, ?)"_s, publicTokenKey, newPublicToken);
            if (!sql || sql->step() != SQLITE_DONE)
                return;
        }

        // If we are updating an old version of the database where currentPublicToken doesn't exist, just
        // save the initial publicToken without deleting all subscriptions and notifying the caller that
        // the token changed.
        if (!currentPublicToken.isEmpty()) {
            auto deleteSubscriptionSets = cachedStatementOnQueue("DELETE FROM SubscriptionSets"_s);
            auto deleteSubscriptions = cachedStatementOnQueue("DELETE FROM Subscriptions"_s);

            if (!deleteSubscriptionSets || !deleteSubscriptions || deleteSubscriptionSets->step() != SQLITE_DONE || deleteSubscriptions->step() != SQLITE_DONE)
                return;

            result = PublicTokenChanged::Yes;
        }

        scope.release();
        transaction.commit();
        completeOnMainQueue(WTFMove(completionHandler), result);
    });
}

void PushDatabase::getPublicToken(CompletionHandler<void(Vector<uint8_t>&&)>&& completionHandler)
{
    dispatchOnWorkQueue([this, completionHandler = WTFMove(completionHandler)]() mutable {
        SQLiteTransaction transaction(m_db);
        transaction.begin();

        auto sql = bindStatementOnQueue("SELECT value FROM Metadata WHERE key = ?"_s, publicTokenKey);
        if (!sql)
            return completeOnMainQueue(WTFMove(completionHandler), Vector<uint8_t> { });

        Vector<uint8_t> result;
        if (sql->step() == SQLITE_ROW)
            result = sql->columnBlob(0);

        transaction.commit();
        completeOnMainQueue(WTFMove(completionHandler), WTFMove(result));
    });
}

void PushDatabase::insertRecord(const PushRecord& record, CompletionHandler<void(std::optional<PushRecord>&&)>&& completionHandler)
{
    dispatchOnWorkQueue([this, record = crossThreadCopy(record), completionHandler = WTFMove(completionHandler)]() mutable {
        SQLiteTransaction transaction(m_db);
        transaction.begin();

        int64_t subscriptionSetID = 0;

        {
            auto sql = bindStatementOnQueue(
                "SELECT rowID FROM SubscriptionSets WHERE bundleID = ? AND pushPartition = ? AND dataStoreUUID = ? AND securityOrigin = ?"_s,
                bindSubscriptionSetParameters(record.subscriptionSetIdentifier),
                record.securityOrigin);
            if (!sql)
                return completeOnMainQueue(WTFMove(completionHandler), std::optional<PushRecord> { });

            if (sql->step() == SQLITE_ROW)
                subscriptionSetID = sql->columnInt64(0);
        }

        if (!subscriptionSetID) {
            auto sql = bindStatementOnQueue(
                "INSERT INTO SubscriptionSets VALUES(NULL, ?, ?, ?, ?, ?, 0, 0)"_s,
                time(nullptr),
                bindSubscriptionSetParameters(record.subscriptionSetIdentifier),
                record.securityOrigin);
            if (!sql || sql->step() != SQLITE_DONE)
                return completeOnMainQueue(WTFMove(completionHandler), std::optional<PushRecord> { });

            subscriptionSetID = m_db->lastInsertRowID();
        }

        {
            auto sql = bindStatementOnQueue(
                "INSERT INTO Subscriptions VALUES(NULL, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"_s,
                time(nullptr),
                subscriptionSetID,
                record.scope,
                record.endpoint,
                record.topic,
                record.serverVAPIDPublicKey,
                record.clientPublicKey,
                record.clientPrivateKey,
                record.sharedAuthSecret,
                expirationTimeToValue(record.expirationTime));
            if (!sql || sql->step() != SQLITE_DONE)
                return completeOnMainQueue(WTFMove(completionHandler), std::optional<PushRecord> { });

            record.identifier = makeObjectIdentifier<PushSubscriptionIdentifierType>(m_db->lastInsertRowID());
        }

        transaction.commit();

        completeOnMainQueue(WTFMove(completionHandler), WTFMove(record));
    });
}

void PushDatabase::removeRecordByIdentifier(PushSubscriptionIdentifier identifier, CompletionHandler<void(bool)>&& completionHandler)
{
    dispatchOnWorkQueue([this, rowIdentifier = identifier.toUInt64(), completionHandler = WTFMove(completionHandler)]() mutable {
        SQLiteTransaction transaction(m_db);
        transaction.begin();

        bool isLastSubscriptionInSet = false;
        int64_t subscriptionSetID = 0;

        // FIXME: Remove this and use RETURNING instead once EWS moves to a macOS build that supports it (SQLite >3.35.0).
        {
            auto sql = bindStatementOnQueue("SELECT subscriptionSetID FROM Subscriptions WHERE rowid = ?"_s, rowIdentifier);
            if (!sql || sql->step() != SQLITE_ROW)
                return completeOnMainQueue(WTFMove(completionHandler), false);

            subscriptionSetID = sql->columnInt(0);
        }

        {
            auto sql = bindStatementOnQueue("DELETE FROM Subscriptions WHERE rowid = ?"_s, rowIdentifier);
            if (!sql || sql->step() != SQLITE_DONE)
                return completeOnMainQueue(WTFMove(completionHandler), false);
        }

        {
            // Check if this was the last subscription in the subscription set.
            auto sql = bindStatementOnQueue("SELECT rowid FROM Subscriptions WHERE subscriptionSetID = ?"_s, subscriptionSetID);
            if (!sql)
                return completeOnMainQueue(WTFMove(completionHandler), false); 

            isLastSubscriptionInSet = (sql->step() == SQLITE_DONE);
        }

        if (isLastSubscriptionInSet) {
            // Delete the entire subscription set if it is no longer associated with any subscriptions.
            auto sql = bindStatementOnQueue("DELETE FROM SubscriptionSets WHERE rowid = ?"_s, subscriptionSetID);
            if (!sql || sql->step() != SQLITE_DONE)
                return completeOnMainQueue(WTFMove(completionHandler), false);
        }

        transaction.commit();

        completeOnMainQueue(WTFMove(completionHandler), true);
    });
}

static PushRecord makePushRecordFromRow(SQLiteStatementAutoResetScope& sql, int columnIndex)
{
    return PushRecord {
        .identifier = makeObjectIdentifier<PushSubscriptionIdentifierType>(sql->columnInt64(columnIndex)),
        .subscriptionSetIdentifier = {
            .bundleIdentifier = sql->columnText(columnIndex + 1),
            .pushPartition = sql->columnText(columnIndex + 2),
            .dataStoreIdentifier = uuidFromSpan(sql->columnBlobAsSpan(columnIndex + 3))
        },
        .securityOrigin = sql->columnText(columnIndex + 4),
        .scope = sql->columnText(columnIndex + 5),
        .endpoint = sql->columnText(columnIndex + 6),
        .topic = sql->columnText(columnIndex + 7),
        .serverVAPIDPublicKey = sql->columnBlob(columnIndex + 8),
        .clientPublicKey = sql->columnBlob(columnIndex + 9),
        .clientPrivateKey = sql->columnBlob(columnIndex + 10),
        .sharedAuthSecret = sql->columnBlob(columnIndex + 11),
        .expirationTime = expirationTimeFromValue(sql->columnValue(columnIndex + 12))
    };
}

void PushDatabase::getRecordByTopic(const String& topic, CompletionHandler<void(std::optional<PushRecord>&&)>&& completionHandler)
{
    dispatchOnWorkQueue([this, topic = crossThreadCopy(topic), completionHandler = WTFMove(completionHandler)]() mutable {
        // Force SQLite to consult the Subscriptions(scope) index first via CROSS JOIN.
        auto sql = bindStatementOnQueue(
            "SELECT " kPushRecordColumns
            "FROM Subscriptions sub "
            "CROSS JOIN SubscriptionSets ss "
            "ON sub.subscriptionSetID = ss.rowid "
            "WHERE sub.topic = ?"_s, topic);
        if (!sql || sql->step() != SQLITE_ROW)
            return completeOnMainQueue(WTFMove(completionHandler), std::optional<PushRecord> { });

        completeOnMainQueue(WTFMove(completionHandler), makePushRecordFromRow(sql, 0));
    });
}

void PushDatabase::getRecordBySubscriptionSetAndScope(const PushSubscriptionSetIdentifier& subscriptionSetIdentifier, const String& scope, CompletionHandler<void(std::optional<PushRecord>&&)>&& completionHandler)
{
    dispatchOnWorkQueue([this, subscriptionSetIdentifier = crossThreadCopy(subscriptionSetIdentifier), scope = crossThreadCopy(scope), completionHandler = WTFMove(completionHandler)]() mutable {
        // Force SQLite to consult the Subscriptions(scope) index first via CROSS JOIN.
        auto sql = bindStatementOnQueue(
            "SELECT " kPushRecordColumns
            "FROM Subscriptions sub "
            "CROSS JOIN SubscriptionSets ss "
            "ON sub.subscriptionSetID = ss.rowid "
            "WHERE sub.scope = ? AND ss.bundleID = ? AND ss.pushPartition = ? AND ss.dataStoreUUID = ?"_s,
            scope,
            bindSubscriptionSetParameters(subscriptionSetIdentifier));
        if (!sql || sql->step() != SQLITE_ROW)
            return completeOnMainQueue(WTFMove(completionHandler), std::optional<PushRecord> { });

        completeOnMainQueue(WTFMove(completionHandler), makePushRecordFromRow(sql, 0));
    });
}

void PushDatabase::getIdentifiers(CompletionHandler<void(HashSet<PushSubscriptionIdentifier>&&)>&& completionHandler)
{
    dispatchOnWorkQueue([this, completionHandler = WTFMove(completionHandler)]() mutable {
        HashSet<PushSubscriptionIdentifier> result;
        auto sql = cachedStatementOnQueue("SELECT rowid FROM Subscriptions"_s);
        while (sql && sql->step() == SQLITE_ROW)
            result.add(makeObjectIdentifier<PushSubscriptionIdentifierType>(sql->columnInt64(0)));

        completeOnMainQueue(WTFMove(completionHandler), WTFMove(result));
    });
}

void PushDatabase::getTopics(CompletionHandler<void(PushTopics&&)>&& completionHandler)
{
    dispatchOnWorkQueue([this, completionHandler = WTFMove(completionHandler)]() mutable {
        PushTopics topics;

        auto sql = cachedStatementOnQueue(
            "SELECT sub.topic, ss.state "
            "FROM Subscriptions sub "
            "JOIN SubscriptionSets ss "
            "ON sub.subscriptionSetID = ss.rowid"_s);
        if (!sql)
            return completeOnMainQueue(WTFMove(completionHandler), topics);

        while (sql->step() == SQLITE_ROW) {
            switch (static_cast<SubscriptionSetState>(sql->columnInt(1))) {
            case SubscriptionSetState::Enabled:
                topics.enabledTopics.append(sql->columnText(0));
                break;
            case SubscriptionSetState::Ignored:
                topics.ignoredTopics.append(sql->columnText(0));
                break;
            }
        }

        completeOnMainQueue(WTFMove(completionHandler), WTFMove(topics));
    });
}

void PushDatabase::incrementSilentPushCount(const PushSubscriptionSetIdentifier& subscriptionSetIdentifier, const String& securityOrigin, CompletionHandler<void(unsigned)>&& completionHandler)
{
    dispatchOnWorkQueue([this, subscriptionSetIdentifier = crossThreadCopy(subscriptionSetIdentifier), securityOrigin = crossThreadCopy(securityOrigin), completionHandler = WTFMove(completionHandler)]() mutable {
        int silentPushCount = 0;
        SQLiteTransaction transaction(m_db);
        transaction.begin();

        {
            auto sql = bindStatementOnQueue(
                "UPDATE SubscriptionSets "
                "SET silentPushCount = silentPushCount + 1 "
                "WHERE bundleID = ? AND pushPartition = ? AND dataStoreUUID = ? AND securityOrigin = ?"_s,
                bindSubscriptionSetParameters(subscriptionSetIdentifier),
                securityOrigin);
            if (!sql || sql->step() != SQLITE_DONE)
                return completeOnMainQueue(WTFMove(completionHandler), 0u);
        }

        // FIXME: Remove this and use RETURNING instead once EWS moves to a macOS build that supports it (SQLite >3.35.0).
        {
            auto sql = bindStatementOnQueue(
                "SELECT silentPushCount "
                "FROM SubscriptionSets "
                "WHERE bundleID = ? AND pushPartition = ? AND dataStoreUUID = ? AND securityOrigin = ?"_s,
                bindSubscriptionSetParameters(subscriptionSetIdentifier),
                securityOrigin);
            if (!sql || sql->step() != SQLITE_ROW)
                return completeOnMainQueue(WTFMove(completionHandler), 0u);

            silentPushCount = sql->columnInt(0);
        }

        transaction.commit();

        completeOnMainQueue(WTFMove(completionHandler), silentPushCount);
    });
}

void PushDatabase::removeRecordsBySubscriptionSet(const PushSubscriptionSetIdentifier& subscriptionSetIdentifier, CompletionHandler<void(Vector<RemovedPushRecord>&&)>&& completionHandler)
{
    dispatchOnWorkQueue([this, subscriptionSetIdentifier = crossThreadCopy(subscriptionSetIdentifier), completionHandler = WTFMove(completionHandler)]() mutable {
        auto scope = makeScopeExit([&completionHandler] {
            completeOnMainQueue(WTFMove(completionHandler), Vector<RemovedPushRecord> { });
        });

        Vector<RemovedPushRecord> removedPushRecords;
        SQLiteTransaction transaction(m_db);
        transaction.begin();

        {
            auto sql = bindStatementOnQueue(
                "SELECT sub.subscriptionSetID, sub.rowid, sub.topic, sub.serverVAPIDPublicKey "
                "FROM SubscriptionSets ss "
                "JOIN Subscriptions sub "
                "ON ss.rowid = sub.subscriptionSetID "
                "WHERE ss.bundleID = ? AND ss.pushPartition = ? AND ss.dataStoreUUID = ?"_s,
                bindSubscriptionSetParameters(subscriptionSetIdentifier));
            if (!sql)
                return;

            while (sql->step() == SQLITE_ROW) {
                auto identifier = makeObjectIdentifier<PushSubscriptionIdentifierType>(sql->columnInt(1));
                auto topic = sql->columnText(2);
                auto serverVAPIDPublicKey = sql->columnBlob(3);
                removedPushRecords.append({ identifier, WTFMove(topic), WTFMove(serverVAPIDPublicKey) });
            }
        }

        {
            auto sql = bindStatementOnQueue(
                "DELETE FROM Subscriptions "
                "WHERE subscriptionSetID IN ("
                "    SELECT rowid FROM SubscriptionSets "
                "    WHERE bundleID = ? AND pushPartition = ? AND dataStoreUUID = ? "
                ")"_s,
                bindSubscriptionSetParameters(subscriptionSetIdentifier));
            if (!sql || sql->step() != SQLITE_DONE)
                return;
        }

        {
            auto sql = bindStatementOnQueue(
                "DELETE FROM SubscriptionSets "
                "WHERE bundleID = ? AND pushPartition = ? AND dataStoreUUID = ?"_s,
                bindSubscriptionSetParameters(subscriptionSetIdentifier));
            if (!sql || sql->step() != SQLITE_DONE)
                return;
        }

        transaction.commit();

        scope.release();
        completeOnMainQueue(WTFMove(completionHandler), WTFMove(removedPushRecords));
    });
}


void PushDatabase::removeRecordsBySubscriptionSetAndSecurityOrigin(const PushSubscriptionSetIdentifier& subscriptionSetIdentifier, const String& securityOrigin, CompletionHandler<void(Vector<RemovedPushRecord>&&)>&& completionHandler)
{
    dispatchOnWorkQueue([this, subscriptionSetIdentifier = crossThreadCopy(subscriptionSetIdentifier), securityOrigin = crossThreadCopy(securityOrigin), completionHandler = WTFMove(completionHandler)]() mutable {
        auto scope = makeScopeExit([&completionHandler] {
            completeOnMainQueue(WTFMove(completionHandler), Vector<RemovedPushRecord> { });
        });

        Vector<RemovedPushRecord> removedPushRecords;
        SQLiteTransaction transaction(m_db);
        transaction.begin();

        int64_t subscriptionSetID = 0;

        {
            auto sql = bindStatementOnQueue(
                "SELECT sub.subscriptionSetID, sub.rowid, sub.topic, sub.serverVAPIDPublicKey "
                "FROM SubscriptionSets ss "
                "JOIN Subscriptions sub "
                "ON ss.rowid = sub.subscriptionSetID "
                "WHERE ss.bundleID = ? AND ss.pushPartition = ? AND ss.dataStoreUUID = ? AND ss.securityOrigin = ?"_s,
                bindSubscriptionSetParameters(subscriptionSetIdentifier),
                securityOrigin);
            if (!sql)
                return;

            while (sql->step() == SQLITE_ROW) {
                subscriptionSetID = sql->columnInt(0);
                auto identifier = makeObjectIdentifier<PushSubscriptionIdentifierType>(sql->columnInt(1));
                auto topic = sql->columnText(2);
                auto serverVAPIDPublicKey = sql->columnBlob(3);
                removedPushRecords.append({ identifier, WTFMove(topic), WTFMove(serverVAPIDPublicKey) });
            }
        }

        {
            auto sql = bindStatementOnQueue("DELETE FROM Subscriptions WHERE subscriptionSetID = ?"_s, subscriptionSetID);
            if (!sql || sql->step() != SQLITE_DONE)
                return;
        }

        {
            auto sql = bindStatementOnQueue("DELETE FROM SubscriptionSets WHERE rowid = ?"_s, subscriptionSetID);
            if (!sql || sql->step() != SQLITE_DONE)
                return;
        }

        transaction.commit();

        scope.release();
        completeOnMainQueue(WTFMove(completionHandler), WTFMove(removedPushRecords));
    });
}

void PushDatabase::setPushesEnabledForOrigin(const PushSubscriptionSetIdentifier& subscriptionSetIdentifier, const String& securityOrigin, bool enabled, CompletionHandler<void(bool recordsChanged)>&& completionHandler)
{
    dispatchOnWorkQueue([this, subscriptionSetIdentifier = crossThreadCopy(subscriptionSetIdentifier), securityOrigin = crossThreadCopy(securityOrigin), enabled, completionHandler = WTFMove(completionHandler)]() mutable {
        auto scope = makeScopeExit([&completionHandler] {
            completeOnMainQueue(WTFMove(completionHandler), false);
        });

        SQLiteTransaction transaction(m_db);
        transaction.begin();

        int64_t subscriptionSetID = 0;
        auto newState = enabled ? SubscriptionSetState::Enabled : SubscriptionSetState::Ignored;

        {
            auto sql = bindStatementOnQueue(
                "SELECT rowid, state "
                "FROM SubscriptionSets "
                "WHERE bundleID = ? AND pushPartition = ? AND dataStoreUUID = ? AND securityOrigin = ?"_s,
                bindSubscriptionSetParameters(subscriptionSetIdentifier),
                securityOrigin);
            if (!sql || sql->step() != SQLITE_ROW || static_cast<SubscriptionSetState>(sql->columnInt(1)) == newState)
                return;

            subscriptionSetID = sql->columnInt64(0);
        }

        {
            auto sql = bindStatementOnQueue("UPDATE SubscriptionSets SET state = ? WHERE rowid = ?"_s, static_cast<int>(newState), subscriptionSetID);
            if (!sql || sql->step() != SQLITE_DONE)
                return;
        }

        transaction.commit();

        scope.release();
        completeOnMainQueue(WTFMove(completionHandler), true);
    });
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
