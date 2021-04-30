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

#include "Logging.h"
#include "RegistrationStore.h"
#include "SQLiteDatabase.h"
#include "SQLiteFileSystem.h"
#include "SQLiteStatement.h"
#include "SQLiteTransaction.h"
#include "SWScriptStorage.h"
#include "SWServer.h"
#include "SecurityOrigin.h"
#include <wtf/CompletionHandler.h>
#include <wtf/CrossThreadCopier.h>
#include <wtf/FileSystem.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Scope.h>
#include <wtf/persistence/PersistentCoders.h>
#include <wtf/persistence/PersistentDecoder.h>
#include <wtf/persistence/PersistentEncoder.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

static const uint64_t schemaVersion = 6;

static const String recordsTableSchema(const String& tableName)
{
    return makeString("CREATE TABLE ", tableName, " ("
        "key TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE"
        ", origin TEXT NOT NULL ON CONFLICT FAIL"
        ", scopeURL TEXT NOT NULL ON CONFLICT FAIL"
        ", topOrigin TEXT NOT NULL ON CONFLICT FAIL"
        ", lastUpdateCheckTime DOUBLE NOT NULL ON CONFLICT FAIL"
        ", updateViaCache TEXT NOT NULL ON CONFLICT FAIL"
        ", scriptURL TEXT NOT NULL ON CONFLICT FAIL"
        ", workerType TEXT NOT NULL ON CONFLICT FAIL"
        ", contentSecurityPolicy BLOB NOT NULL ON CONFLICT FAIL"
        ", referrerPolicy TEXT NOT NULL ON CONFLICT FAIL"
        ", scriptResourceMap BLOB NOT NULL ON CONFLICT FAIL"
        ", certificateInfo BLOB NOT NULL ON CONFLICT FAIL"
        ")");
}

static const String recordsTableSchema()
{
    ASSERT(!isMainThread());
    static NeverDestroyed<String> schema(recordsTableSchema("Records"));
    return schema;
}

static const String recordsTableSchemaAlternate()
{
    ASSERT(!isMainThread());
    static NeverDestroyed<String> schema(recordsTableSchema("\"Records\""));
    return schema;
}

static inline String databaseFilenameFromVersion(uint64_t version)
{
    return makeString("ServiceWorkerRegistrations-", version, ".sqlite3");
}

static const String& databaseFilename()
{
    ASSERT(isMainThread());
    static NeverDestroyed<String> filename = databaseFilenameFromVersion(schemaVersion);
    return filename;
}

String serviceWorkerRegistrationDatabaseFilename(const String& databaseDirectory)
{
    return FileSystem::pathByAppendingComponent(databaseDirectory, databaseFilename());
}

static inline void cleanOldDatabases(const String& databaseDirectory)
{
    for (uint64_t version = 1; version < schemaVersion; ++version)
        SQLiteFileSystem::deleteDatabaseFile(FileSystem::pathByAppendingComponent(databaseDirectory, databaseFilenameFromVersion(version)));
}

struct ImportedScriptAttributes {
    URL responseURL;
    String mimeType;

    template<class Encoder> void encode(Encoder& encoder) const
    {
        encoder << responseURL << mimeType;
    }

    template<class Decoder> static Optional<ImportedScriptAttributes> decode(Decoder& decoder)
    {
        Optional<URL> responseURL;
        decoder >> responseURL;
        if (!responseURL)
            return WTF::nullopt;

        Optional<String> mimeType;
        decoder >> mimeType;
        if (!mimeType)
            return WTF::nullopt;

        return {{
            WTFMove(*responseURL),
            WTFMove(*mimeType)
        }};
    }
};

static HashMap<URL, ImportedScriptAttributes> stripScriptSources(const HashMap<URL, ServiceWorkerContextData::ImportedScript>& map)
{
    HashMap<URL, ImportedScriptAttributes> mapWithoutScripts;
    for (auto& pair : map)
        mapWithoutScripts.add(pair.key, ImportedScriptAttributes { pair.value.responseURL, pair.value.mimeType });
    return mapWithoutScripts;
}

static HashMap<URL, ServiceWorkerContextData::ImportedScript> populateScriptSourcesFromDisk(SWScriptStorage& scriptStorage, const ServiceWorkerRegistrationKey& registrationKey, HashMap<URL, ImportedScriptAttributes>&& map)
{
    HashMap<URL, ServiceWorkerContextData::ImportedScript> importedScripts;
    for (auto& pair : map) {
        auto importedScript = scriptStorage.retrieve(registrationKey, pair.key);
        if (!importedScript) {
            RELEASE_LOG_ERROR(ServiceWorker, "RegistrationDatabase::populateScriptSourcesFromDisk: Failed to retrieve imported script for %s from disk", pair.key.string().utf8().data());
            continue;
        }
        importedScripts.add(pair.key, ServiceWorkerContextData::ImportedScript { WTFMove(importedScript), WTFMove(pair.value.responseURL), WTFMove(pair.value.mimeType) });
    }
    return importedScripts;
}

static Ref<WorkQueue> registrationDatabaseWorkQueue()
{
    static LazyNeverDestroyed<Ref<WorkQueue>> workQueue;
    static std::once_flag onceKey;
    std::call_once(onceKey, [] {
        workQueue.construct(WorkQueue::create("ServiceWorker I/O Thread", WorkQueue::Type::Serial));
    });
    return workQueue;
}

RegistrationDatabase::RegistrationDatabase(RegistrationStore& store, String&& databaseDirectory)
    : m_workQueue(registrationDatabaseWorkQueue())
    , m_store(makeWeakPtr(store))
    , m_databaseDirectory(WTFMove(databaseDirectory))
    , m_databaseFilePath(FileSystem::pathByAppendingComponent(m_databaseDirectory, databaseFilename()))
{
    ASSERT(isMainThread());

    postTaskToWorkQueue([this] {
        importRecordsIfNecessary();
    });
}

RegistrationDatabase::~RegistrationDatabase()
{
    ASSERT(isMainThread());

    // The database & scriptStorage need to be destroyed on the background thread.
    if (m_database || m_scriptStorage)
        m_workQueue->dispatch([database = WTFMove(m_database), scriptStorage = WTFMove(m_scriptStorage)] { });
}

void RegistrationDatabase::postTaskToWorkQueue(Function<void()>&& task)
{
    ASSERT(isMainThread());

    ++m_pushCounter;
    m_workQueue->dispatch([protectedThis = makeRef(*this), task = WTFMove(task)]() mutable {
        task();
    });
}

bool RegistrationDatabase::openSQLiteDatabase(const String& fullFilename)
{
    ASSERT(!isMainThread());
    ASSERT(!m_database);

    auto databaseDirectory = this->databaseDirectoryIsolatedCopy();
    cleanOldDatabases(databaseDirectory);

    LOG(ServiceWorker, "ServiceWorker RegistrationDatabase opening file %s", fullFilename.utf8().data());

    SQLiteFileSystem::ensureDatabaseDirectoryExists(databaseDirectory);

    m_database = makeUnique<SQLiteDatabase>();
    if (!m_database->open(fullFilename)) {
        RELEASE_LOG_ERROR(ServiceWorker, "Failed to open Service Worker registration database");
        m_database = nullptr;
        return false;
    }

    // Disable threading checks. We always access the database from our serial WorkQueue. Such accesses
    // are safe since work queue tasks are guaranteed to run one after another. However, tasks will not
    // necessary run on the same thread every time (as per GCD documentation).
    m_database->disableThreadingChecks();
    
    auto doRecoveryAttempt = [&] {
        // Delete the database and files. We will try recreating it when flushing changes.
        m_database = nullptr;
        SQLiteFileSystem::deleteDatabaseFile(fullFilename);
    };
    
    String errorMessage = ensureValidRecordsTable();
    if (!errorMessage.isNull()) {
        RELEASE_LOG_ERROR(ServiceWorker, "ensureValidRecordsTable failed, reason: %" PUBLIC_LOG_STRING, errorMessage.utf8().data());
        doRecoveryAttempt();
        return false;
    }
    
    errorMessage = importRecords();
    if (!errorMessage.isNull()) {
        RELEASE_LOG_ERROR(ServiceWorker, "importRecords failed, reason: %" PUBLIC_LOG_STRING, errorMessage.utf8().data());
        doRecoveryAttempt();
        return false;
    }
    return true;
}

String RegistrationDatabase::scriptStorageDirectory() const
{
    return FileSystem::pathByAppendingComponents(m_databaseDirectory, { "Scripts", "V1" });
}

SWScriptStorage& RegistrationDatabase::scriptStorage()
{
    ASSERT(!isMainThread());
    if (!m_scriptStorage)
        m_scriptStorage = makeUnique<SWScriptStorage>(scriptStorageDirectory());
    return *m_scriptStorage;
}

void RegistrationDatabase::importRecordsIfNecessary()
{
    ASSERT(!isMainThread());

    if (FileSystem::fileExists(m_databaseFilePath)) {
        if (!openSQLiteDatabase(m_databaseFilePath)) {
            callOnMainThread([this, protectedThis = makeRef(*this)] {
                databaseFailedToOpen();
            });
            return;
        }
    }

    callOnMainThread([this, protectedThis = makeRef(*this)] {
        databaseOpenedAndRecordsImported();
    });
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
            if (!m_database->executeCommand(recordsTableSchema()))
                return makeString("Could not create Records table in database (", m_database->lastError(), ") - ", m_database->lastErrorMsg());
            return { };
        }

        if (sqliteResult != SQLITE_ROW)
            return "Error executing statement to fetch schema for the Records table.";

        currentSchema = statement.getColumnText(1);
    }

    ASSERT(!currentSchema.isEmpty());
    
    if (currentSchema == recordsTableSchema() || currentSchema == recordsTableSchemaAlternate())
        return { };

    return makeString("Unexpected schema: ", currentSchema);
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

static Optional<ServiceWorkerUpdateViaCache> stringToUpdateViaCache(const String& update)
{
    if (update == "Imports")
        return ServiceWorkerUpdateViaCache::Imports;
    if (update == "All")
        return ServiceWorkerUpdateViaCache::All;
    if (update == "None")
        return ServiceWorkerUpdateViaCache::None;

    return WTF::nullopt;
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

static Optional<WorkerType> stringToWorkerType(const String& type)
{
    if (type == "Classic")
        return WorkerType::Classic;
    if (type == "Module")
        return WorkerType::Module;

    return WTF::nullopt;
}

void RegistrationDatabase::pushChanges(const HashMap<ServiceWorkerRegistrationKey, Optional<ServiceWorkerContextData>>& changedRegistrations, CompletionHandler<void()>&& completionHandler)
{
    Vector<ServiceWorkerContextData> updatedRegistrations;
    Vector<ServiceWorkerRegistrationKey> removedRegistrations;
    for (auto& keyValue : changedRegistrations) {
        if (keyValue.value)
            updatedRegistrations.append(keyValue.value->isolatedCopy());
        else
            removedRegistrations.append(keyValue.key.isolatedCopy());
    }
    schedulePushChanges(WTFMove(updatedRegistrations), WTFMove(removedRegistrations), ShouldRetry::Yes, WTFMove(completionHandler));
}

void RegistrationDatabase::schedulePushChanges(Vector<ServiceWorkerContextData>&& updatedRegistrations, Vector<ServiceWorkerRegistrationKey>&& removedRegistrations, ShouldRetry shouldRetry, CompletionHandler<void()>&& completionHandler)
{
    auto pushCounter = shouldRetry == ShouldRetry::Yes ? m_pushCounter : 0;
    postTaskToWorkQueue([this, protectedThis = makeRef(*this), pushCounter, updatedRegistrations = WTFMove(updatedRegistrations), removedRegistrations = WTFMove(removedRegistrations), completionHandler = WTFMove(completionHandler)]() mutable {
        bool success = doPushChanges(updatedRegistrations, removedRegistrations);
        if (success) {
            updatedRegistrations.clear();
            removedRegistrations.clear();
        }
        callOnMainThread([this, protectedThis = WTFMove(protectedThis), success, pushCounter, updatedRegistrations = WTFMove(updatedRegistrations).isolatedCopy(), removedRegistrations = WTFMove(removedRegistrations).isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
            if (!success && (pushCounter + 1) == m_pushCounter) {
                // We retry writing once if no other change was pushed.
                schedulePushChanges(WTFMove(updatedRegistrations), WTFMove(removedRegistrations), ShouldRetry::No, WTFMove(completionHandler));
                return;
            }
            if (completionHandler)
                completionHandler();
        });
    });
}

void RegistrationDatabase::close(CompletionHandler<void()>&& completionHandler)
{
    postTaskToWorkQueue([this, completionHandler = WTFMove(completionHandler)]() mutable {
        m_database = nullptr;
        callOnMainThread(WTFMove(completionHandler));
    });
}

void RegistrationDatabase::clearAll(CompletionHandler<void()>&& completionHandler)
{
    postTaskToWorkQueue([this, completionHandler = WTFMove(completionHandler)]() mutable {
        m_database = nullptr;
        m_scriptStorage = nullptr;

        SQLiteFileSystem::deleteDatabaseFile(m_databaseFilePath);
        FileSystem::deleteNonEmptyDirectory(scriptStorageDirectory());
        SQLiteFileSystem::deleteEmptyDatabaseDirectory(databaseDirectoryIsolatedCopy());

        callOnMainThread(WTFMove(completionHandler));
    });
}

bool RegistrationDatabase::doPushChanges(const Vector<ServiceWorkerContextData>& updatedRegistrations, const Vector<ServiceWorkerRegistrationKey>& removedRegistrations)
{
    if (!m_database) {
        openSQLiteDatabase(m_databaseFilePath);
        if (!m_database)
            return false;
    }

    SQLiteTransaction transaction(*m_database);
    transaction.begin();

    SQLiteStatement sql(*m_database, "INSERT INTO Records VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"_s);
    if (sql.prepare() != SQLITE_OK) {
        RELEASE_LOG_ERROR(ServiceWorker, "Failed to prepare statement to store registration data into records table (%i) - %s", m_database->lastError(), m_database->lastErrorMsg());
        return false;
    }

    auto& scriptStorage = this->scriptStorage();
    for (auto& registration : removedRegistrations) {
        SQLiteStatement sql(*m_database, "DELETE FROM Records WHERE key = ?");
        if (sql.prepare() != SQLITE_OK
            || sql.bindText(1, registration.toDatabaseKey()) != SQLITE_OK
            || sql.step() != SQLITE_DONE) {
            RELEASE_LOG_ERROR(ServiceWorker, "Failed to remove registration data from records table (%i) - %s", m_database->lastError(), m_database->lastErrorMsg());
            return false;
        }
        scriptStorage.clear(registration);
    }

    for (auto& data : updatedRegistrations) {
        WTF::Persistence::Encoder cspEncoder;
        data.contentSecurityPolicy.encode(cspEncoder);

        // We don't actually encode the script sources to the database. They will be stored separately in the ScriptStorage.
        // As a result, we need to strip the script sources here before encoding the scriptResourceMap.
        WTF::Persistence::Encoder scriptResourceMapEncoder;
        scriptResourceMapEncoder << stripScriptSources(data.scriptResourceMap);

        WTF::Persistence::Encoder certificateInfoEncoder;
        certificateInfoEncoder << data.certificateInfo;

        if (sql.bindText(1, data.registration.key.toDatabaseKey()) != SQLITE_OK
            || sql.bindText(2, data.registration.scopeURL.protocolHostAndPort()) != SQLITE_OK
            || sql.bindText(3, data.registration.scopeURL.path().toString()) != SQLITE_OK
            || sql.bindText(4, data.registration.key.topOrigin().databaseIdentifier()) != SQLITE_OK
            || sql.bindDouble(5, data.registration.lastUpdateTime.secondsSinceEpoch().value()) != SQLITE_OK
            || sql.bindText(6, updateViaCacheToString(data.registration.updateViaCache)) != SQLITE_OK
            || sql.bindText(7, data.scriptURL.string()) != SQLITE_OK
            || sql.bindText(8, workerTypeToString(data.workerType)) != SQLITE_OK
            || sql.bindBlob(9, cspEncoder.buffer(), cspEncoder.bufferSize()) != SQLITE_OK
            || sql.bindText(10, data.referrerPolicy) != SQLITE_OK
            || sql.bindBlob(11, scriptResourceMapEncoder.buffer(), scriptResourceMapEncoder.bufferSize()) != SQLITE_OK
            || sql.bindBlob(12, certificateInfoEncoder.buffer(), certificateInfoEncoder.bufferSize()) != SQLITE_OK
            || sql.step() != SQLITE_DONE) {
            RELEASE_LOG_ERROR(ServiceWorker, "Failed to store registration data into records table (%i) - %s", m_database->lastError(), m_database->lastErrorMsg());
            return false;
        }

        // Save scripts to disk.
        auto mainScript = scriptStorage.store(data.registration.key, data.scriptURL, data.script);
        ASSERT(mainScript);
        if (!mainScript)
            return false;
        HashMap<URL, ScriptBuffer> importedScripts;
        for (auto& pair : data.scriptResourceMap) {
            auto importedScript = scriptStorage.store(data.registration.key, pair.key, pair.value.script);
            ASSERT(importedScript);
            if (importedScript)
                importedScripts.add(crossThreadCopy(pair.key), crossThreadCopy(importedScript));
        }
        callOnMainThread([this, protectedThis = makeRef(*this), serviceWorkerIdentifier = data.serviceWorkerIdentifier, mainScript = crossThreadCopy(mainScript), importedScripts = WTFMove(importedScripts)]() mutable {
            if (m_store)
                m_store->didSaveWorkerScriptsToDisk(serviceWorkerIdentifier, WTFMove(mainScript), WTFMove(importedScripts));
        });
    }

    transaction.commit();
    RELEASE_LOG(ServiceWorker, "Updated ServiceWorker registration database (%zu added/updated registrations and %zu removed registrations", updatedRegistrations.size(), removedRegistrations.size());
    return true;
}

String RegistrationDatabase::importRecords()
{
    ASSERT(!isMainThread());

    RELEASE_LOG(ServiceWorker, "RegistrationDatabase::importRecords:");
    SQLiteStatement sql(*m_database, "SELECT * FROM Records;"_s);
    if (sql.prepare() != SQLITE_OK)
        return makeString("Failed to prepare statement to retrieve registrations from records table (", m_database->lastError(), ") - ", m_database->lastErrorMsg());

    int result = sql.step();

    for (; result == SQLITE_ROW; result = sql.step()) {
        RELEASE_LOG(ServiceWorker, "RegistrationDatabase::importRecords: Importing a registration from the database");
        auto key = ServiceWorkerRegistrationKey::fromDatabaseKey(sql.getColumnText(0));
        auto originURL = URL { URL(), sql.getColumnText(1) };
        auto scopePath = sql.getColumnText(2);
        auto scopeURL = URL { originURL, scopePath };
        auto topOrigin = SecurityOriginData::fromDatabaseIdentifier(sql.getColumnText(3));
        auto lastUpdateCheckTime = WallTime::fromRawSeconds(sql.getColumnDouble(4));
        auto updateViaCache = stringToUpdateViaCache(sql.getColumnText(5));
        auto scriptURL = URL { URL(), sql.getColumnText(6) };
        auto workerType = stringToWorkerType(sql.getColumnText(7));

        Vector<uint8_t> contentSecurityPolicyData;
        sql.getColumnBlobAsVector(8, contentSecurityPolicyData);
        WTF::Persistence::Decoder cspDecoder(contentSecurityPolicyData.data(), contentSecurityPolicyData.size());
        Optional<ContentSecurityPolicyResponseHeaders> contentSecurityPolicy;
        if (contentSecurityPolicyData.size()) {
            cspDecoder >> contentSecurityPolicy;
            if (!contentSecurityPolicy) {
                RELEASE_LOG_ERROR(ServiceWorker, "RegistrationDatabase::importRecords: Failed to decode contentSecurityPolicy");
                continue;
            }
        }

        auto referrerPolicy = sql.getColumnText(9);

        Vector<uint8_t> scriptResourceMapData;
        sql.getColumnBlobAsVector(10, scriptResourceMapData);
        HashMap<URL, ServiceWorkerContextData::ImportedScript> scriptResourceMap;

        WTF::Persistence::Decoder scriptResourceMapDecoder(scriptResourceMapData.data(), scriptResourceMapData.size());
        if (scriptResourceMapData.size()) {
            Optional<HashMap<URL, ImportedScriptAttributes>> scriptResourceMapWithoutScripts;
            scriptResourceMapDecoder >> scriptResourceMapWithoutScripts;
            if (!scriptResourceMapWithoutScripts) {
                RELEASE_LOG_ERROR(ServiceWorker, "RegistrationDatabase::importRecords: Failed to decode scriptResourceMapWithoutScripts");
                continue;
            }
            scriptResourceMap = populateScriptSourcesFromDisk(scriptStorage(), *key, WTFMove(*scriptResourceMapWithoutScripts));
        }

        Vector<uint8_t> certificateInfoData;
        sql.getColumnBlobAsVector(11, certificateInfoData);
        Optional<CertificateInfo> certificateInfo;

        WTF::Persistence::Decoder certificateInfoDecoder(certificateInfoData.data(), certificateInfoData.size());
        certificateInfoDecoder >> certificateInfo;
        if (!certificateInfo) {
            RELEASE_LOG_ERROR(ServiceWorker, "RegistrationDatabase::importRecords: Failed to decode certificateInfo");
            continue;
        }

        // Validate the input for this registration.
        // If any part of this input is invalid, let's skip this registration.
        // FIXME: Should we return an error skipping *all* registrations?
        if (!key || !originURL.isValid() || !topOrigin || !updateViaCache || !scriptURL.isValid() || !workerType || !scopeURL.isValid()) {
            RELEASE_LOG_ERROR(ServiceWorker, "RegistrationDatabase::importRecords: Failed to decode part of the registration");
            continue;
        }

        auto script = scriptStorage().retrieve(*key, scriptURL);
        ASSERT(script);
        if (!script) {
            RELEASE_LOG_ERROR(ServiceWorker, "RegistrationDatabase::importRecords: Failed to retrieve main script for %s from disk", scriptURL.string().utf8().data());
            continue;
        }

        auto workerIdentifier = ServiceWorkerIdentifier::generate();
        auto registrationIdentifier = ServiceWorkerRegistrationIdentifier::generate();
        auto serviceWorkerData = ServiceWorkerData { workerIdentifier, scriptURL, ServiceWorkerState::Activated, *workerType, registrationIdentifier };
        auto registration = ServiceWorkerRegistrationData { WTFMove(*key), registrationIdentifier, WTFMove(scopeURL), *updateViaCache, lastUpdateCheckTime, WTF::nullopt, WTF::nullopt, WTFMove(serviceWorkerData) };
        auto contextData = ServiceWorkerContextData { WTF::nullopt, WTFMove(registration), workerIdentifier, WTFMove(script), WTFMove(*certificateInfo), WTFMove(*contentSecurityPolicy), WTFMove(referrerPolicy), WTFMove(scriptURL), *workerType, true, WTFMove(scriptResourceMap) };

        callOnMainThread([protectedThis = makeRef(*this), contextData = contextData.isolatedCopy()]() mutable {
            protectedThis->addRegistrationToStore(WTFMove(contextData));
        });
    }

    if (result != SQLITE_DONE)
        return makeString("Failed to import at least one registration from records table (", m_database->lastError(), ") - ", m_database->lastErrorMsg());

    return { };
}

void RegistrationDatabase::addRegistrationToStore(ServiceWorkerContextData&& context)
{
    if (m_store)
        m_store->addRegistrationFromDatabase(WTFMove(context));
}

void RegistrationDatabase::databaseFailedToOpen()
{
    if (m_store)
        m_store->databaseFailedToOpen();
}

void RegistrationDatabase::databaseOpenedAndRecordsImported()
{
    if (m_store)
        m_store->databaseOpenedAndRecordsImported();
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
