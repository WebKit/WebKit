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
#include "RegistrationDatabase.h"

#if ENABLE(SERVICE_WORKER)

#include "FileSystem.h"
#include "Logging.h"
#include "RegistrationStore.h"
#include "SQLiteDatabase.h"
#include "SQLiteFileSystem.h"
#include "SQLiteStatement.h"
#include "SQLiteTransaction.h"
#include "SWServer.h"
#include "SecurityOrigin.h"
#include <wtf/CompletionHandler.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Scope.h>
#include <wtf/persistence/PersistentCoders.h>
#include <wtf/persistence/PersistentDecoder.h>
#include <wtf/persistence/PersistentEncoder.h>

namespace WebCore {

static const int schemaVersion = 1;

static const String v1RecordsTableSchema(const String& tableName)
{
    return makeString("CREATE TABLE ", tableName, " (key TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE, origin TEXT NOT NULL ON CONFLICT FAIL, scopeURL TEXT NOT NULL ON CONFLICT FAIL, topOrigin TEXT NOT NULL ON CONFLICT FAIL, lastUpdateCheckTime DOUBLE NOT NULL ON CONFLICT FAIL, updateViaCache TEXT NOT NULL ON CONFLICT FAIL, scriptURL TEXT NOT NULL ON CONFLICT FAIL, script TEXT NOT NULL ON CONFLICT FAIL, workerType TEXT NOT NULL ON CONFLICT FAIL, contentSecurityPolicy BLOB NOT NULL ON CONFLICT FAIL)");
}

static const String v1RecordsTableSchema()
{
    ASSERT(!isMainThread());
    static NeverDestroyed<WTF::String> schema(v1RecordsTableSchema("Records"));
    return schema;
}

static const String v1RecordsTableSchemaAlternate()
{
    ASSERT(!isMainThread());
    static NeverDestroyed<WTF::String> schema(v1RecordsTableSchema("\"Records\""));
    return schema;
}

static const String& databaseFilename()
{
    ASSERT(isMainThread());
    static NeverDestroyed<String> filename = makeString("ServiceWorkerRegistrations-", String::number(schemaVersion), ".sqlite3");
    return filename;
}

String serviceWorkerRegistrationDatabaseFilename(const String& databaseDirectory)
{
    return FileSystem::pathByAppendingComponent(databaseDirectory, databaseFilename());
}

RegistrationDatabase::RegistrationDatabase(RegistrationStore& store, const String& databaseDirectory)
    : CrossThreadTaskHandler("ServiceWorker I/O Thread")
    , m_store(store)
    , m_databaseDirectory(databaseDirectory)
    , m_databaseFilePath(FileSystem::pathByAppendingComponent(m_databaseDirectory, databaseFilename()))
{
    ASSERT(isMainThread());

    postTask(CrossThreadTask([this] {
        importRecordsIfNecessary();
    }));
}

RegistrationDatabase::~RegistrationDatabase()
{
    ASSERT(!m_database);
    ASSERT(isMainThread());
}

void RegistrationDatabase::openSQLiteDatabase(const String& fullFilename)
{
    ASSERT(!isMainThread());
    ASSERT(!m_database);

    LOG(ServiceWorker, "ServiceWorker RegistrationDatabase opening file %s", fullFilename.utf8().data());

    String errorMessage;
    auto scopeExit = makeScopeExit([&, errorMessage = &errorMessage] {
        ASSERT_UNUSED(errorMessage, !errorMessage->isNull());

#if RELEASE_LOG_DISABLED
        LOG_ERROR("Failed to open Service Worker registration database: %s", errorMessage->utf8().data());
#else
        RELEASE_LOG_ERROR(ServiceWorker, "Failed to open Service Worker registration database: %{public}s", errorMessage->utf8().data());
#endif

        m_database = nullptr;
        postTaskReply(createCrossThreadTask(*this, &RegistrationDatabase::databaseFailedToOpen));
    });

    SQLiteFileSystem::ensureDatabaseDirectoryExists(m_databaseDirectory);

    m_database = std::make_unique<SQLiteDatabase>();
    if (!m_database->open(fullFilename)) {
        errorMessage = "Failed to open registration database";
        return;
    }
    
    errorMessage = ensureValidRecordsTable();
    if (!errorMessage.isNull())
        return;
    
    errorMessage = importRecords();
    if (!errorMessage.isNull())
        return;

    scopeExit.release();
}

void RegistrationDatabase::importRecordsIfNecessary()
{
    ASSERT(!isMainThread());

    if (FileSystem::fileExists(m_databaseFilePath))
        openSQLiteDatabase(m_databaseFilePath);

    postTaskReply(createCrossThreadTask(*this, &RegistrationDatabase::databaseOpenedAndRecordsImported));
}

String RegistrationDatabase::ensureValidRecordsTable()
{
    ASSERT(!isMainThread());
    ASSERT(m_database);
    ASSERT(m_database->isOpen());

    String currentSchema;
    {
        // Fetch the schema for an existing records table.
        SQLiteStatement statement(*m_database, "SELECT type, sql FROM sqlite_master WHERE tbl_name='Records'");
        if (statement.prepare() != SQLITE_OK)
            return "Unable to prepare statement to fetch schema for the Records table.";

        int sqliteResult = statement.step();

        // If there is no Records table at all, create it and then bail.
        if (sqliteResult == SQLITE_DONE) {
            if (!m_database->executeCommand(v1RecordsTableSchema()))
                return String::format("Could not create Records table in database (%i) - %s", m_database->lastError(), m_database->lastErrorMsg());
            return { };
        }

        if (sqliteResult != SQLITE_ROW)
            return "Error executing statement to fetch schema for the Records table.";

        currentSchema = statement.getColumnText(1);
    }

    ASSERT(!currentSchema.isEmpty());
    
    if (currentSchema == v1RecordsTableSchema() || currentSchema == v1RecordsTableSchemaAlternate())
        return { };

    // This database has a Records table but it is not a schema we expect.
    // Trying to recover by deleting the data contained within is dangerous so
    // we should consider this an unrecoverable error.
    RELEASE_ASSERT_NOT_REACHED();
}

static String updateViaCacheToString(ServiceWorkerUpdateViaCache update)
{
    switch (update) {
    case ServiceWorkerUpdateViaCache::Imports:
        return "Imports";
    case ServiceWorkerUpdateViaCache::All:
        return "All";
    case ServiceWorkerUpdateViaCache::None:
        return "None";
    }

    RELEASE_ASSERT_NOT_REACHED();
}

static std::optional<ServiceWorkerUpdateViaCache> stringToUpdateViaCache(const String& update)
{
    if (update == "Imports")
        return ServiceWorkerUpdateViaCache::Imports;
    if (update == "All")
        return ServiceWorkerUpdateViaCache::All;
    if (update == "None")
        return ServiceWorkerUpdateViaCache::None;

    return std::nullopt;
}

static String workerTypeToString(WorkerType workerType)
{
    switch (workerType) {
    case WorkerType::Classic:
        return "Classic";
    case WorkerType::Module:
        return "Module";
    }

    RELEASE_ASSERT_NOT_REACHED();
}

static std::optional<WorkerType> stringToWorkerType(const String& type)
{
    if (type == "Classic")
        return WorkerType::Classic;
    if (type == "Module")
        return WorkerType::Module;

    return std::nullopt;
}

void RegistrationDatabase::pushChanges(Vector<ServiceWorkerContextData>&& datas, WTF::CompletionHandler<void()>&& completionHandler)
{
    postTask(CrossThreadTask([this, datas = crossThreadCopy(datas), completionHandler = WTFMove(completionHandler)]() mutable {
        doPushChanges(WTFMove(datas));

        if (!completionHandler)
            return;

        postTaskReply(CrossThreadTask([completionHandler = WTFMove(completionHandler)] {
            completionHandler();
        }));
    }));
}

void RegistrationDatabase::clearAll(WTF::CompletionHandler<void()>&& completionHandler)
{
    postTask(CrossThreadTask([this, completionHandler = WTFMove(completionHandler)]() mutable {
        m_database = nullptr;

        SQLiteFileSystem::deleteDatabaseFile(m_databaseFilePath);
        SQLiteFileSystem::deleteEmptyDatabaseDirectory(m_databaseDirectory);

        postTaskReply(CrossThreadTask([completionHandler = WTFMove(completionHandler)] {
            completionHandler();
        }));
    }));
}

void RegistrationDatabase::doPushChanges(Vector<ServiceWorkerContextData>&& datas)
{
    if (!m_database) {
        openSQLiteDatabase(m_databaseFilePath);
        if (!m_database)
            return;
    }

    SQLiteTransaction transaction(*m_database);
    transaction.begin();

    SQLiteStatement sql(*m_database, ASCIILiteral("INSERT INTO Records VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    if (sql.prepare() != SQLITE_OK) {
        RELEASE_LOG_ERROR(ServiceWorker, "Failed to prepare statement to store registration data into records table (%i) - %s", m_database->lastError(), m_database->lastErrorMsg());
        return;
    }

    for (auto& data : datas) {
        if (data.registration.identifier == ServiceWorkerRegistrationIdentifier()) {
            SQLiteStatement sql(*m_database, "DELETE FROM Records WHERE key = ?");
            if (sql.prepare() != SQLITE_OK
                || sql.bindText(1, data.registration.key.toDatabaseKey()) != SQLITE_OK
                || sql.step() != SQLITE_DONE) {
                RELEASE_LOG_ERROR(ServiceWorker, "Failed to remove registration data from records table (%i) - %s", m_database->lastError(), m_database->lastErrorMsg());
                return;
            }

            continue;
        }

        WTF::Persistence::Encoder encoder;
        data.contentSecurityPolicy.encode(encoder);

        if (sql.bindText(1, data.registration.key.toDatabaseKey()) != SQLITE_OK
            || sql.bindText(2, data.registration.scopeURL.protocolHostAndPort()) != SQLITE_OK
            || sql.bindText(3, data.registration.scopeURL.path()) != SQLITE_OK
            || sql.bindText(4, data.registration.key.topOrigin().databaseIdentifier()) != SQLITE_OK
            || sql.bindDouble(5, data.registration.lastUpdateTime.secondsSinceEpoch().value()) != SQLITE_OK
            || sql.bindText(6, updateViaCacheToString(data.registration.updateViaCache)) != SQLITE_OK
            || sql.bindText(7, data.scriptURL.string()) != SQLITE_OK
            || sql.bindText(8, data.script) != SQLITE_OK
            || sql.bindText(9, workerTypeToString(data.workerType)) != SQLITE_OK
            || sql.bindBlob(10, encoder.buffer(), encoder.bufferSize()) != SQLITE_OK
            || sql.step() != SQLITE_DONE) {
            RELEASE_LOG_ERROR(ServiceWorker, "Failed to store registration data into records table (%i) - %s", m_database->lastError(), m_database->lastErrorMsg());
            return;
        }
    }

    transaction.commit();

    LOG(ServiceWorker, "Pushed %zu changes to ServiceWorker registration database", datas.size());
}

String RegistrationDatabase::importRecords()
{
    ASSERT(!isMainThread());

    SQLiteStatement sql(*m_database, ASCIILiteral("SELECT * FROM Records;"));
    if (sql.prepare() != SQLITE_OK)
        return String::format("Failed to prepare statement to retrieve registrations from records table (%i) - %s", m_database->lastError(), m_database->lastErrorMsg());

    int result = sql.step();

    for (; result == SQLITE_ROW; result = sql.step()) {
        auto key = ServiceWorkerRegistrationKey::fromDatabaseKey(sql.getColumnText(0));
        auto originURL = URL { URL(), sql.getColumnText(1) };
        auto scopePath = sql.getColumnText(2);
        auto topOrigin = SecurityOriginData::fromDatabaseIdentifier(sql.getColumnText(3));
        auto lastUpdateCheckTime = WallTime::fromRawSeconds(sql.getColumnDouble(4));
        auto updateViaCache = stringToUpdateViaCache(sql.getColumnText(5));
        auto scriptURL = URL { URL(), sql.getColumnText(6) };
        auto script = sql.getColumnText(7);
        auto workerType = stringToWorkerType(sql.getColumnText(8));

        Vector<uint8_t> contentSecurityPolicyData;
        sql.getColumnBlobAsVector(9, contentSecurityPolicyData);
        WTF::Persistence::Decoder decoder(contentSecurityPolicyData.data(), contentSecurityPolicyData.size());
        ContentSecurityPolicyResponseHeaders contentSecurityPolicy;
        if (contentSecurityPolicyData.size() && !ContentSecurityPolicyResponseHeaders::decode(decoder, contentSecurityPolicy))
            continue;

        // Validate the input for this registration.
        // If any part of this input is invalid, let's skip this registration.
        // FIXME: Should we return an error skipping *all* registrations?
        if (!key || !originURL.isValid() || !topOrigin || !updateViaCache || !scriptURL.isValid() || !workerType)
            continue;

        auto workerIdentifier = generateObjectIdentifier<ServiceWorkerIdentifierType>();
        auto registrationIdentifier = generateObjectIdentifier<ServiceWorkerRegistrationIdentifierType>();
        auto serviceWorkerData = ServiceWorkerData { workerIdentifier, scriptURL, ServiceWorkerState::Activated, *workerType, registrationIdentifier };
        auto registration = ServiceWorkerRegistrationData { WTFMove(*key), registrationIdentifier, URL(originURL, scopePath), *updateViaCache, lastUpdateCheckTime, std::nullopt, std::nullopt, WTFMove(serviceWorkerData) };
        auto contextData = ServiceWorkerContextData { std::nullopt, WTFMove(registration), workerIdentifier, WTFMove(script), WTFMove(contentSecurityPolicy), WTFMove(scriptURL), *workerType, m_store.server().sessionID(), true };

        postTaskReply(createCrossThreadTask(*this, &RegistrationDatabase::addRegistrationToStore, WTFMove(contextData)));
    }

    if (result != SQLITE_DONE)
        return String::format("Failed to import at least one registration from records table (%i) - %s", m_database->lastError(), m_database->lastErrorMsg());

    return { };
}

void RegistrationDatabase::addRegistrationToStore(ServiceWorkerContextData&& context)
{
    m_store.addRegistrationFromDatabase(WTFMove(context));
}

void RegistrationDatabase::databaseFailedToOpen()
{
    m_store.databaseFailedToOpen();
}

void RegistrationDatabase::databaseOpenedAndRecordsImported()
{
    m_store.databaseOpenedAndRecordsImported();
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
