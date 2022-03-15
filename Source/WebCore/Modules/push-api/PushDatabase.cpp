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
#include <wtf/CrossThreadCopier.h>
#include <wtf/Expected.h>
#include <wtf/FileSystem.h>
#include <wtf/RunLoop.h>
#include <wtf/UniqueRef.h>

#define PUSHDB_RELEASE_LOG(fmt, ...) RELEASE_LOG(Push, "%p - PushDatabase::" fmt, this, ##__VA_ARGS__)
#define PUSHDB_RELEASE_LOG_ERROR(fmt, ...) RELEASE_LOG_ERROR(Push, "%p - PushDatabase::" fmt, this, ##__VA_ARGS__)
#define PUSHDB_RELEASE_LOG_BIND_ERROR() PUSHDB_RELEASE_LOG_ERROR("Failed to bind statement (%d): %s", m_db->lastError(), m_db->lastErrorMsg())

namespace WebCore {

static constexpr int currentPushDatabaseVersion = 1;

static const ASCIILiteral pushDatabaseSchemaV1Statements[] = {
    "PRAGMA auto_vacuum=INCREMENTAL"_s,
    "CREATE TABLE SubscriptionSets("
    "  creationTime INT NOT NULL,"
    "  bundleID TEXT NOT NULL,"
    "  securityOrigin TEXT NOT NULL,"
    "  wakeState INT NOT NULL,"
    "  UNIQUE(bundleID, securityOrigin))"_s,
    "CREATE TABLE Subscriptions("
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
    "PRAGMA user_version = 1"_s
};

PushRecord PushRecord::isolatedCopy() const &
{
    return {
        identifier,
        bundleID.isolatedCopy(),
        securityOrigin.isolatedCopy(),
        scope.isolatedCopy(),
        endpoint.isolatedCopy(),
        topic.isolatedCopy(),
        serverVAPIDPublicKey,
        clientPublicKey,
        clientPrivateKey,
        sharedAuthSecret,
        expirationTime,
        wakeState
    };
}

PushRecord PushRecord::isolatedCopy() &&
{
    return {
        identifier,
        WTFMove(bundleID).isolatedCopy(),
        WTFMove(securityOrigin).isolatedCopy(),
        WTFMove(scope).isolatedCopy(),
        WTFMove(endpoint).isolatedCopy(),
        WTFMove(topic).isolatedCopy(),
        WTFMove(serverVAPIDPublicKey),
        WTFMove(clientPublicKey),
        WTFMove(clientPrivateKey),
        WTFMove(sharedAuthSecret),
        expirationTime,
        wakeState
    };
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

    if (version && version != currentPushDatabaseVersion) {
        // FIXME: add migration when we need it.
        RELEASE_LOG_ERROR(Push, "Found unexpected PushDatabase version: %d (expected: %d) at path: %s", version, currentPushDatabaseVersion, path.utf8().data());
        return makeUnexpected(ShouldDeleteAndRetry::Yes);
    }

    if (!version) {
        SQLiteTransaction transaction(db);
        transaction.begin();

        for (auto& statement : pushDatabaseSchemaV1Statements) {
            if (!db->executeCommand(statement)) {
                RELEASE_LOG_ERROR(Push, "Error executing PushDatabase DDL statement %s at path %s", statement.characters(), path.utf8().data());
                return makeUnexpected(ShouldDeleteAndRetry::Yes);
            }
        }

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
            RELEASE_LOG(Push, "Failed to delete PushDatabase at path %s; bailing on recreating from scratch", path.utf8().data());
            return nullptr;
        }

        RELEASE_LOG(Push, "Deleted PushDatabase at path %s and recreating from scratch", path.utf8().data());
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

static int bindExpirationTime(SQLiteStatement* sql, int index, std::optional<EpochTimeStamp> timestamp)
{
    return timestamp ? sql->bindInt64(index, convertEpochTimeStampToSeconds(*timestamp)) : sql->bindNull(index);
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
    WorkQueue::main().dispatch([completionHandler = WTFMove(completionHandler), result = crossThreadCopy(WTFMove(result))]() mutable {
        completionHandler(WTFMove(result));
    });
}

void PushDatabase::insertRecord(const PushRecord& record, CompletionHandler<void(std::optional<PushRecord>&&)>&& completionHandler)
{
    dispatchOnWorkQueue([this, record = crossThreadCopy(record), completionHandler = WTFMove(completionHandler)]() mutable {
        SQLiteTransaction transaction(m_db);
        transaction.begin();

        int64_t subscriptionSetID = 0;

        {
            auto sql = cachedStatementOnQueue("SELECT rowid, wakeState FROM SubscriptionSets WHERE bundleID = ? AND securityOrigin = ?"_s);
            if (!sql
                || sql->bindText(1, record.bundleID) != SQLITE_OK
                || sql->bindText(2, record.securityOrigin) != SQLITE_OK) {
                PUSHDB_RELEASE_LOG_BIND_ERROR();
                completeOnMainQueue(WTFMove(completionHandler), std::optional<PushRecord> { });
                return;
            }

            if (sql->step() == SQLITE_ROW) {
                subscriptionSetID = sql->columnInt64(0);
                record.wakeState = static_cast<PushWakeState>(sql->columnInt(1));
            }
        }

        if (!subscriptionSetID) {
            auto sql = cachedStatementOnQueue("INSERT INTO SubscriptionSets VALUES(?, ?, ?, ?)"_s);
            if (!sql
                || sql->bindInt64(1, time(nullptr)) != SQLITE_OK
                || sql->bindText(2, record.bundleID) != SQLITE_OK
                || sql->bindText(3, record.securityOrigin) != SQLITE_OK
                || sql->bindInt(4, static_cast<int>(record.wakeState)) != SQLITE_OK) {
                PUSHDB_RELEASE_LOG_BIND_ERROR();
                completeOnMainQueue(WTFMove(completionHandler), std::optional<PushRecord> { });
                return;
            }

            if (sql->step() != SQLITE_DONE) {
                completeOnMainQueue(WTFMove(completionHandler), std::optional<PushRecord> { });
                return;
            }

            subscriptionSetID = m_db->lastInsertRowID();
        }

        {
            auto sql = cachedStatementOnQueue("INSERT INTO Subscriptions VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"_s);
            if (!sql
                || sql->bindInt64(1, time(nullptr)) != SQLITE_OK
                || sql->bindInt64(2, subscriptionSetID) != SQLITE_OK
                || sql->bindText(3, record.scope) != SQLITE_OK
                || sql->bindText(4, record.endpoint) != SQLITE_OK
                || sql->bindText(5, record.topic) != SQLITE_OK
                || sql->bindBlob(6, record.serverVAPIDPublicKey) != SQLITE_OK
                || sql->bindBlob(7, record.clientPublicKey) != SQLITE_OK
                || sql->bindBlob(8, record.clientPrivateKey) != SQLITE_OK
                || sql->bindBlob(9, record.sharedAuthSecret) != SQLITE_OK
                || bindExpirationTime(sql.get(), 10, record.expirationTime) != SQLITE_OK) {
                PUSHDB_RELEASE_LOG_BIND_ERROR();
                completeOnMainQueue(WTFMove(completionHandler), std::optional<PushRecord> { });
                return;
            }

            if (sql->step() != SQLITE_DONE) {
                completeOnMainQueue(WTFMove(completionHandler), std::optional<PushRecord> { });
                return;
            }

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

        {
            auto sql = cachedStatementOnQueue("SELECT subscriptionSetID FROM Subscriptions WHERE rowid = ?"_s);

            if (!sql || sql->bindInt64(1, rowIdentifier) != SQLITE_OK) {
                PUSHDB_RELEASE_LOG_BIND_ERROR();
                completeOnMainQueue(WTFMove(completionHandler), false);
                return;
            }

            if (sql->step() != SQLITE_ROW) {
                completeOnMainQueue(WTFMove(completionHandler), false);
                return;
            }

            subscriptionSetID = sql->columnInt64(0);
        }

        {
            // TODO: on SQLite >3.35.0, we could use RETURNING to avoid the SELECT above.
            auto sql = cachedStatementOnQueue("DELETE FROM Subscriptions WHERE rowid = ?"_s);

            if (!sql || sql->bindInt64(1, rowIdentifier) != SQLITE_OK) {
                PUSHDB_RELEASE_LOG_BIND_ERROR();
                completeOnMainQueue(WTFMove(completionHandler), false);
                return;
            }

            if (sql->step() != SQLITE_DONE) {
                completeOnMainQueue(WTFMove(completionHandler), false);
                return;
            }
        }

        {
            // Check if this was the last subscription in the subscription set.
            auto sql = cachedStatementOnQueue("SELECT rowid FROM Subscriptions WHERE subscriptionSetID = ?"_s);

            if (!sql || sql->bindInt64(1, subscriptionSetID) != SQLITE_OK) {
                PUSHDB_RELEASE_LOG_BIND_ERROR();
                completeOnMainQueue(WTFMove(completionHandler), false);
                return;
            }

            isLastSubscriptionInSet = (sql->step() == SQLITE_DONE);
        }

        if (isLastSubscriptionInSet) {
            // Delete the entire subscription set if it is no longer associated with any subscriptions.
            auto sql = cachedStatementOnQueue("DELETE FROM SubscriptionSets WHERE rowid = ?"_s);
            if (!sql || sql->bindInt64(1, subscriptionSetID) != SQLITE_OK) {
                PUSHDB_RELEASE_LOG_BIND_ERROR();
                completeOnMainQueue(WTFMove(completionHandler), false);
                return;
            }

            if (sql->step() != SQLITE_DONE) {
                completeOnMainQueue(WTFMove(completionHandler), false);
                return;
            }
        }

        transaction.commit();

        completeOnMainQueue(WTFMove(completionHandler), true);
    });
}

static PushRecord makePushRecordFromRow(SQLiteStatementAutoResetScope& sql)
{
    PushRecord record;
    record.identifier = makeObjectIdentifier<PushSubscriptionIdentifierType>(sql->columnInt64(0));
    record.bundleID = sql->columnText(1);
    record.securityOrigin = sql->columnText(2);
    record.scope = sql->columnText(3);
    record.endpoint = sql->columnText(4);
    record.topic = sql->columnText(5);
    record.serverVAPIDPublicKey = sql->columnBlob(6);
    record.clientPublicKey = sql->columnBlob(7);
    record.clientPrivateKey = sql->columnBlob(8);
    record.sharedAuthSecret = sql->columnBlob(9);
    record.expirationTime = expirationTimeFromValue(sql->columnValue(10));
    record.wakeState = static_cast<PushWakeState>(sql->columnInt(11));

    return record;
}

void PushDatabase::getRecordByTopic(const String& topic, CompletionHandler<void(std::optional<PushRecord>&&)>&& completionHandler)
{
    dispatchOnWorkQueue([this, topic = crossThreadCopy(topic), completionHandler = WTFMove(completionHandler)]() mutable {
        // Force SQLite to consult the Subscriptions(scope) index first via CROSS JOIN.
        auto sql = cachedStatementOnQueue(
            "SELECT "
            "  sub.rowID, ss.bundleID, ss.securityOrigin, sub.scope, sub.endpoint, sub.topic, "
            "  sub.serverVAPIDPublicKey, sub.clientPublicKey, sub.clientPrivateKey, "
            "  sub.sharedAuthSecret, sub.expirationTime, ss.wakeState "
            "FROM Subscriptions sub "
            "CROSS JOIN SubscriptionSets ss "
            "ON sub.subscriptionSetID = ss.rowid "
            "WHERE sub.topic = ?"_s);

        if (!sql || sql->bindText(1, topic) != SQLITE_OK) {
            PUSHDB_RELEASE_LOG_BIND_ERROR();
            completeOnMainQueue(WTFMove(completionHandler), std::optional<PushRecord> { });
            return;
        }

        if (sql->step() != SQLITE_ROW) {
            completeOnMainQueue(WTFMove(completionHandler), std::optional<PushRecord> { });
            return;
        }

        completeOnMainQueue(WTFMove(completionHandler), makePushRecordFromRow(sql));
    });
}

void PushDatabase::getRecordByBundleIdentifierAndScope(const String& bundleID, const String& scope, CompletionHandler<void(std::optional<PushRecord>&&)>&& completionHandler)
{
    dispatchOnWorkQueue([this, bundleID = crossThreadCopy(bundleID), scope = crossThreadCopy(scope), completionHandler = WTFMove(completionHandler)]() mutable {
        // Force SQLite to consult the Subscriptions(scope) index first via CROSS JOIN.
        auto sql = cachedStatementOnQueue(
            "SELECT "
            "  sub.rowID, ss.bundleID, ss.securityOrigin, sub.scope, sub.endpoint, sub.topic, "
            "  sub.serverVAPIDPublicKey, sub.clientPublicKey, sub.clientPrivateKey, "
            "  sub.sharedAuthSecret, sub.expirationTime, ss.wakeState "
            "FROM Subscriptions sub "
            "CROSS JOIN SubscriptionSets ss "
            "ON sub.subscriptionSetID = ss.rowid "
            "WHERE sub.scope = ? AND ss.bundleID = ?"_s);

        if (!sql || sql->bindText(1, scope) != SQLITE_OK || sql->bindText(2, bundleID) != SQLITE_OK) {
            PUSHDB_RELEASE_LOG_BIND_ERROR();
            completeOnMainQueue(WTFMove(completionHandler), std::optional<PushRecord> { });
            return;
        }

        if (sql->step() != SQLITE_ROW) {
            completeOnMainQueue(WTFMove(completionHandler), std::optional<PushRecord> { });
            return;
        }

        completeOnMainQueue(WTFMove(completionHandler), makePushRecordFromRow(sql));
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

void PushDatabase::getTopicsByWakeState(CompletionHandler<void(PushWakeStateToTopicMap&&)>&& completionHandler)
{
    dispatchOnWorkQueue([this, completionHandler = WTFMove(completionHandler)]() mutable {
        constexpr int numberOfWakeStates = static_cast<int>(PushWakeState::NumberOfStates);
        Vector<Vector<String>, numberOfWakeStates> topicsByWakeState(numberOfWakeStates); // used because HashMap::isolatedCopy doesn't exist

        auto sql = cachedStatementOnQueue(
            "SELECT ss.wakeState, sub.topic "
            "FROM Subscriptions sub "
            "JOIN SubscriptionSets ss "
            "ON sub.subscriptionSetID = ss.rowid"_s);
        if (!sql) {
            PUSHDB_RELEASE_LOG_BIND_ERROR();
            WorkQueue::main().dispatch([completionHandler = WTFMove(completionHandler)]() mutable {
                completionHandler({ });
            });
            return;
        }

        while (sql->step() == SQLITE_ROW) {
            auto pushWakeState = sql->columnInt(0);
            auto topic = sql->columnText(1);
            if (pushWakeState >= 0 && pushWakeState < numberOfWakeStates)
                topicsByWakeState[pushWakeState].append(WTFMove(topic));
        }

        WorkQueue::main().dispatch([completionHandler = WTFMove(completionHandler), topicsByWakeState = crossThreadCopy(WTFMove(topicsByWakeState))]() mutable {
            PushWakeStateToTopicMap result;
            for (int i = 0; i < static_cast<int>(PushWakeState::NumberOfStates); ++i)
                result.add(static_cast<PushWakeState>(i), WTFMove(topicsByWakeState[i]));
            completionHandler(WTFMove(result));
        });
    });
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
