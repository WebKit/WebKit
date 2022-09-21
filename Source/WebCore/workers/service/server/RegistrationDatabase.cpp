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
#include "WebCorePersistentCoders.h"
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

#define RECORDS_TABLE_SCHEMA_PREFIX "CREATE TABLE "
#define RECORDS_TABLE_SCHEMA_SUFFIX "(" \
    "key TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE" \
    ", origin TEXT NOT NULL ON CONFLICT FAIL" \
    ", scopeURL TEXT NOT NULL ON CONFLICT FAIL" \
    ", topOrigin TEXT NOT NULL ON CONFLICT FAIL" \
    ", lastUpdateCheckTime DOUBLE NOT NULL ON CONFLICT FAIL" \
    ", updateViaCache TEXT NOT NULL ON CONFLICT FAIL" \
    ", scriptURL TEXT NOT NULL ON CONFLICT FAIL" \
    ", workerType TEXT NOT NULL ON CONFLICT FAIL" \
    ", contentSecurityPolicy BLOB NOT NULL ON CONFLICT FAIL" \
    ", crossOriginEmbedderPolicy BLOB NOT NULL ON CONFLICT FAIL" \
    ", referrerPolicy TEXT NOT NULL ON CONFLICT FAIL" \
    ", scriptResourceMap BLOB NOT NULL ON CONFLICT FAIL" \
    ", certificateInfo BLOB NOT NULL ON CONFLICT FAIL" \
    ", preloadState BLOB NOT NULL ON CONFLICT FAIL" \
    ")"_s;

static ASCIILiteral recordsTableSchema()
{
    return RECORDS_TABLE_SCHEMA_PREFIX "Records" RECORDS_TABLE_SCHEMA_SUFFIX;
}

static ASCIILiteral recordsTableSchemaAlternate()
{
    return RECORDS_TABLE_SCHEMA_PREFIX "\"Records\"" RECORDS_TABLE_SCHEMA_SUFFIX;
}

static inline String databaseFilenameFromVersion(uint64_t version)
{
    return makeString("ServiceWorkerRegistrations-", version, ".sqlite3");
}

static const String& databaseFilename()
{
    ASSERT(isMainThread());
    static NeverDestroyed<String> filename = databaseFilenameFromVersion(RegistrationDatabase::schemaVersion);
    return filename;
}

String serviceWorkerRegistrationDatabaseFilename(const String& databaseDirectory)
{
    return FileSystem::pathByAppendingComponent(databaseDirectory, databaseFilename());
}

static inline void cleanOldDatabases(const String& databaseDirectory)
{
    for (uint64_t version = 1; version < RegistrationDatabase::schemaVersion; ++version)
        SQLiteFileSystem::deleteDatabaseFile(FileSystem::pathByAppendingComponent(databaseDirectory, databaseFilenameFromVersion(version)));
}

static HashMap<URL, ImportedScriptAttributes> stripScriptSources(const MemoryCompactRobinHoodHashMap<URL, ServiceWorkerContextData::ImportedScript>& map)
{
    HashMap<URL, ImportedScriptAttributes> mapWithoutScripts;
    for (auto& pair : map)
        mapWithoutScripts.add(pair.key, ImportedScriptAttributes { pair.value.responseURL, pair.value.mimeType });
    return mapWithoutScripts;
}

static MemoryCompactRobinHoodHashMap<URL, ServiceWorkerContextData::ImportedScript> populateScriptSourcesFromDisk(SWScriptStorage& scriptStorage, const ServiceWorkerRegistrationKey& registrationKey, HashMap<URL, ImportedScriptAttributes>&& map)
{
    MemoryCompactRobinHoodHashMap<URL, ServiceWorkerContextData::ImportedScript> importedScripts;
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
        workQueue.construct(WorkQueue::create("ServiceWorker I/O Thread"));
    });
    return workQueue;
}

RegistrationDatabase::RegistrationDatabase(RegistrationStore& store, String&& databaseDirectory)
    : m_workQueue(registrationDatabaseWorkQueue())
    , m_store(store)
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
    m_workQueue->dispatch([protectedThis = Ref { *this }, task = WTFMove(task)]() mutable {
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
    return FileSystem::pathByAppendingComponents(m_databaseDirectory, { "Scripts"_s, "V1"_s });
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
            callOnMainThread([this, protectedThis = Ref { *this }] {
                databaseFailedToOpen();
            });
            return;
        }
    }

    callOnMainThread([this, protectedThis = Ref { *this }] {
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
        auto statement = m_database->prepareStatement("SELECT type, sql FROM sqlite_master WHERE tbl_name='Records'"_s);
        if (!statement)
            return "Unable to prepare statement to fetch schema for the Records table."_s;

        int sqliteResult = statement->step();

        // If there is no Records table at all, create it and then bail.
        if (sqliteResult == SQLITE_DONE) {
            if (!m_database->executeCommand(recordsTableSchema()))
                return makeString("Could not create Records table in database (", m_database->lastError(), ") - ", m_database->lastErrorMsg());
            return { };
        }

        if (sqliteResult != SQLITE_ROW)
            return "Error executing statement to fetch schema for the Records table."_s;

        currentSchema = statement->columnText(1);
    }

    ASSERT(!currentSchema.isEmpty());
    
    if (currentSchema == recordsTableSchema() || currentSchema == recordsTableSchemaAlternate())
        return { };

    return makeString("Unexpected schema: ", currentSchema);
}

static ASCIILiteral updateViaCacheToString(ServiceWorkerUpdateViaCache update)
{
    switch (update) {
    case ServiceWorkerUpdateViaCache::Imports:
        return "Imports"_s;
    case ServiceWorkerUpdateViaCache::All:
        return "All"_s;
    case ServiceWorkerUpdateViaCache::None:
        return "None"_s;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

static std::optional<ServiceWorkerUpdateViaCache> stringToUpdateViaCache(const String& update)
{
    if (update == "Imports"_s)
        return ServiceWorkerUpdateViaCache::Imports;
    if (update == "All"_s)
        return ServiceWorkerUpdateViaCache::All;
    if (update == "None"_s)
        return ServiceWorkerUpdateViaCache::None;

    return std::nullopt;
}

static ASCIILiteral workerTypeToString(WorkerType workerType)
{
    switch (workerType) {
    case WorkerType::Classic:
        return "Classic"_s;
    case WorkerType::Module:
        return "Module"_s;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

static std::optional<WorkerType> stringToWorkerType(const String& type)
{
    if (type == "Classic"_s)
        return WorkerType::Classic;
    if (type == "Module"_s)
        return WorkerType::Module;

    return std::nullopt;
}

void RegistrationDatabase::pushChanges(const HashMap<ServiceWorkerRegistrationKey, std::optional<ServiceWorkerContextData>>& changedRegistrations, CompletionHandler<void()>&& completionHandler)
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
    postTaskToWorkQueue([this, protectedThis = Ref { *this }, pushCounter, updatedRegistrations = WTFMove(updatedRegistrations), removedRegistrations = WTFMove(removedRegistrations), completionHandler = WTFMove(completionHandler)]() mutable {
        bool success = doPushChanges(updatedRegistrations, removedRegistrations);
        if (success) {
            updatedRegistrations.clear();
            removedRegistrations.clear();
        }
        callOnMainThread([this, protectedThis = WTFMove(protectedThis), success, pushCounter, updatedRegistrations = crossThreadCopy(WTFMove(updatedRegistrations)), removedRegistrations = crossThreadCopy(WTFMove(removedRegistrations)), completionHandler = WTFMove(completionHandler)]() mutable {
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

    auto insertStatement = m_database->prepareStatement("INSERT INTO Records VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"_s);
    if (!insertStatement) {
        RELEASE_LOG_ERROR(ServiceWorker, "Failed to prepare statement to store registration data into records table (%i) - %s", m_database->lastError(), m_database->lastErrorMsg());
        return false;
    }

    auto& scriptStorage = this->scriptStorage();
    for (auto& registration : removedRegistrations) {
        auto deleteStatement = m_database->prepareStatement("DELETE FROM Records WHERE key = ?"_s);
        if (!deleteStatement
            || deleteStatement->bindText(1, registration.toDatabaseKey()) != SQLITE_OK
            || deleteStatement->step() != SQLITE_DONE) {
            RELEASE_LOG_ERROR(ServiceWorker, "Failed to remove registration data from records table (%i) - %s", m_database->lastError(), m_database->lastErrorMsg());
            return false;
        }
        scriptStorage.clear(registration);
    }

    for (auto& data : updatedRegistrations) {
        WTF::Persistence::Encoder cspEncoder;
        cspEncoder << data.contentSecurityPolicy;

        WTF::Persistence::Encoder coepEncoder;
        coepEncoder << data.crossOriginEmbedderPolicy;

        // We don't actually encode the script sources to the database. They will be stored separately in the ScriptStorage.
        // As a result, we need to strip the script sources here before encoding the scriptResourceMap.
        WTF::Persistence::Encoder scriptResourceMapEncoder;
        scriptResourceMapEncoder << stripScriptSources(data.scriptResourceMap);

        WTF::Persistence::Encoder certificateInfoEncoder;
        certificateInfoEncoder << data.certificateInfo;

        WTF::Persistence::Encoder navigationPreloadStateEncoder;
        navigationPreloadStateEncoder << data.navigationPreloadState;

        if (insertStatement->bindText(1, data.registration.key.toDatabaseKey()) != SQLITE_OK
            || insertStatement->bindText(2, data.registration.scopeURL.protocolHostAndPort()) != SQLITE_OK
            || insertStatement->bindText(3, data.registration.scopeURL.path().toString()) != SQLITE_OK
            || insertStatement->bindText(4, data.registration.key.topOrigin().databaseIdentifier()) != SQLITE_OK
            || insertStatement->bindDouble(5, data.registration.lastUpdateTime.secondsSinceEpoch().value()) != SQLITE_OK
            || insertStatement->bindText(6, StringView { updateViaCacheToString(data.registration.updateViaCache) }) != SQLITE_OK
            || insertStatement->bindText(7, data.scriptURL.string()) != SQLITE_OK
            || insertStatement->bindText(8, StringView { workerTypeToString(data.workerType) }) != SQLITE_OK
            || insertStatement->bindBlob(9, Span { cspEncoder.buffer(), cspEncoder.bufferSize() }) != SQLITE_OK
            || insertStatement->bindBlob(10, Span { coepEncoder.buffer(), coepEncoder.bufferSize() }) != SQLITE_OK
            || insertStatement->bindText(11, data.referrerPolicy) != SQLITE_OK
            || insertStatement->bindBlob(12, Span { scriptResourceMapEncoder.buffer(), scriptResourceMapEncoder.bufferSize() }) != SQLITE_OK
            || insertStatement->bindBlob(13, Span { certificateInfoEncoder.buffer(), certificateInfoEncoder.bufferSize() }) != SQLITE_OK
            || insertStatement->bindBlob(14, Span { navigationPreloadStateEncoder.buffer(), navigationPreloadStateEncoder.bufferSize() }) != SQLITE_OK
            || insertStatement->step() != SQLITE_DONE) {
            RELEASE_LOG_ERROR(ServiceWorker, "Failed to store registration data into records table (%i) - %s", m_database->lastError(), m_database->lastErrorMsg());
            return false;
        }

        // Save scripts to disk.
        auto mainScript = scriptStorage.store(data.registration.key, data.scriptURL, data.script);
        ASSERT(mainScript);
        if (!mainScript)
            return false;
        MemoryCompactRobinHoodHashMap<URL, ScriptBuffer> importedScripts;
        for (auto& [scriptURL, script] : data.scriptResourceMap) {
            auto importedScript = scriptStorage.store(data.registration.key, scriptURL, script.script);
            ASSERT(importedScript);
            if (importedScript)
                importedScripts.add(crossThreadCopy(scriptURL), crossThreadCopy(WTFMove(importedScript)));
        }
        callOnMainThread([this, protectedThis = Ref { *this }, serviceWorkerIdentifier = data.serviceWorkerIdentifier, mainScript = crossThreadCopy(WTFMove(mainScript)), importedScripts = WTFMove(importedScripts)]() mutable {
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
    auto sql = m_database->prepareStatement("SELECT * FROM Records;"_s);
    if (!sql)
        return makeString("Failed to prepare statement to retrieve registrations from records table (", m_database->lastError(), ") - ", m_database->lastErrorMsg());

    int result = sql->step();

    for (; result == SQLITE_ROW; result = sql->step()) {
        RELEASE_LOG(ServiceWorker, "RegistrationDatabase::importRecords: Importing a registration from the database");
        auto key = ServiceWorkerRegistrationKey::fromDatabaseKey(sql->columnText(0));
        if (!key) {
            RELEASE_LOG_ERROR(ServiceWorker, "RegistrationDatabase::importRecords: Failed to decode service worker registration key");
            continue;
        }
        auto originURL = URL { sql->columnText(1) };
        auto scopePath = sql->columnText(2);
        auto scopeURL = URL { originURL, scopePath };
        auto topOrigin = SecurityOriginData::fromDatabaseIdentifier(sql->columnText(3));
        auto lastUpdateCheckTime = WallTime::fromRawSeconds(sql->columnDouble(4));
        auto updateViaCache = stringToUpdateViaCache(sql->columnText(5));
        auto scriptURL = URL { sql->columnText(6) };
        auto workerType = stringToWorkerType(sql->columnText(7));

        std::optional<ContentSecurityPolicyResponseHeaders> contentSecurityPolicy;
        auto contentSecurityPolicyDataSpan = sql->columnBlobAsSpan(8);
        if (contentSecurityPolicyDataSpan.size()) {
            WTF::Persistence::Decoder cspDecoder(contentSecurityPolicyDataSpan);
            cspDecoder >> contentSecurityPolicy;
            if (!contentSecurityPolicy) {
                RELEASE_LOG_ERROR(ServiceWorker, "RegistrationDatabase::importRecords: Failed to decode contentSecurityPolicy");
                continue;
            }
        }

        std::optional<CrossOriginEmbedderPolicy> coep;
        auto coepDataSpan = sql->columnBlobAsSpan(9);
        if (coepDataSpan.size()) {
            WTF::Persistence::Decoder coepDecoder(coepDataSpan);
            coepDecoder >> coep;
            if (!coep) {
                RELEASE_LOG_ERROR(ServiceWorker, "RegistrationDatabase::importRecords: Failed to decode crossOriginEmbedderPolicy");
                continue;
            }
        }

        auto referrerPolicy = sql->columnText(10);

        MemoryCompactRobinHoodHashMap<URL, ServiceWorkerContextData::ImportedScript> scriptResourceMap;
        auto scriptResourceMapDataSpan = sql->columnBlobAsSpan(11);
        if (scriptResourceMapDataSpan.size()) {
            WTF::Persistence::Decoder scriptResourceMapDecoder(scriptResourceMapDataSpan);
            std::optional<HashMap<URL, ImportedScriptAttributes>> scriptResourceMapWithoutScripts;
            scriptResourceMapDecoder >> scriptResourceMapWithoutScripts;
            if (!scriptResourceMapWithoutScripts) {
                RELEASE_LOG_ERROR(ServiceWorker, "RegistrationDatabase::importRecords: Failed to decode scriptResourceMapWithoutScripts");
                continue;
            }
            scriptResourceMap = populateScriptSourcesFromDisk(scriptStorage(), *key, WTFMove(*scriptResourceMapWithoutScripts));
        }

        auto certificateInfoDataSpan = sql->columnBlobAsSpan(12);
        std::optional<CertificateInfo> certificateInfo;

        WTF::Persistence::Decoder certificateInfoDecoder(certificateInfoDataSpan);
        certificateInfoDecoder >> certificateInfo;
        if (!certificateInfo) {
            RELEASE_LOG_ERROR(ServiceWorker, "RegistrationDatabase::importRecords: Failed to decode certificateInfo");
            continue;
        }

        auto navigationPreloadStateDataSpan = sql->columnBlobAsSpan(13);
        std::optional<NavigationPreloadState> navigationPreloadState;

        WTF::Persistence::Decoder navigationPreloadStateDecoder(navigationPreloadStateDataSpan);
        navigationPreloadStateDecoder >> navigationPreloadState;
        if (!navigationPreloadState) {
            RELEASE_LOG_ERROR(ServiceWorker, "RegistrationDatabase::importRecords: Failed to decode navigationPreloadState");
            continue;
        }

        // Validate the input for this registration.
        // If any part of this input is invalid, let's skip this registration.
        // FIXME: Should we return an error skipping *all* registrations?
        if (!originURL.isValid() || !topOrigin || !updateViaCache || !scriptURL.isValid() || !workerType || !scopeURL.isValid()) {
            RELEASE_LOG_ERROR(ServiceWorker, "RegistrationDatabase::importRecords: Failed to decode part of the registration");
            continue;
        }
        if (key->topOrigin() != *topOrigin) {
            RELEASE_LOG_ERROR(ServiceWorker, "RegistrationDatabase::importRecords: Inconsistent registration");
            continue;
        }

        auto script = scriptStorage().retrieve(*key, scriptURL);
        if (!script) {
            RELEASE_LOG_ERROR(ServiceWorker, "RegistrationDatabase::importRecords: Failed to retrieve main script for %s from disk", scriptURL.string().utf8().data());
            continue;
        }

        auto workerIdentifier = ServiceWorkerIdentifier::generate();
        auto registrationIdentifier = ServiceWorkerRegistrationIdentifier::generate();
        auto serviceWorkerData = ServiceWorkerData { workerIdentifier, scriptURL, ServiceWorkerState::Activated, *workerType, registrationIdentifier };
        auto registration = ServiceWorkerRegistrationData { WTFMove(*key), registrationIdentifier, WTFMove(scopeURL), *updateViaCache, lastUpdateCheckTime, std::nullopt, std::nullopt, WTFMove(serviceWorkerData) };
        auto contextData = ServiceWorkerContextData { std::nullopt, WTFMove(registration), workerIdentifier, WTFMove(script), WTFMove(*certificateInfo), WTFMove(*contentSecurityPolicy), WTFMove(*coep), WTFMove(referrerPolicy), WTFMove(scriptURL), *workerType, true, LastNavigationWasAppInitiated::Yes, WTFMove(scriptResourceMap), std::nullopt, WTFMove(*navigationPreloadState) };

        callOnMainThread([protectedThis = Ref { *this }, contextData = WTFMove(contextData).isolatedCopy()]() mutable {
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

#undef RECORDS_TABLE_SCHEMA_PREFIX
#undef RECORDS_TABLE_SCHEMA_SUFFIX

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
