/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "SWRegistrationDatabase.h"

#include "ContentSecurityPolicyResponseHeaders.h"
#include "CrossOriginEmbedderPolicy.h"
#include "Logging.h"
#include "SQLiteDatabase.h"
#include "SQLiteFileSystem.h"
#include "SQLiteStatement.h"
#include "SQLiteStatementAutoResetScope.h"
#include "SQLiteTransaction.h"
#include "SWScriptStorage.h"
#include "ServiceWorkerClientData.h"
#include "ServiceWorkerContextData.h"
#include "ServiceWorkerRegistrationKey.h"
#include "WebCorePersistentCoders.h"
#include "WorkerType.h"
#include <wtf/persistence/PersistentCoders.h>
#include <wtf/persistence/PersistentDecoder.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

static constexpr auto scriptVersion = "V1"_s;
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

static String databaseFilePath(const String& directory)
{
    if (directory.isEmpty())
        return emptyString();

    return FileSystem::pathByAppendingComponent(directory, makeString("ServiceWorkerRegistrations-"_s, SWRegistrationDatabase::schemaVersion, ".sqlite3"_s));
}

static String scriptDirectoryPath(const String& directory)
{
    if (directory.isEmpty())
        return emptyString();

    return FileSystem::pathByAppendingComponent(directory, "Scripts"_s);
}

static String scriptVersionDirectoryPath(const String& directory)
{
    auto scriptDirectory = scriptDirectoryPath(directory);
    if (scriptDirectory.isEmpty())
        return emptyString();

    return FileSystem::pathByAppendingComponent(scriptDirectory, scriptVersion);
}

static ASCIILiteral convertUpdateViaCacheToString(ServiceWorkerUpdateViaCache update)
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

static std::optional<ServiceWorkerUpdateViaCache> convertStringToUpdateViaCache(const String& update)
{
    if (update == "Imports"_s)
        return ServiceWorkerUpdateViaCache::Imports;
    if (update == "All"_s)
        return ServiceWorkerUpdateViaCache::All;
    if (update == "None"_s)
        return ServiceWorkerUpdateViaCache::None;

    return std::nullopt;
}

static ASCIILiteral convertWorkerTypeToString(WorkerType workerType)
{
    switch (workerType) {
    case WorkerType::Classic:
        return "Classic"_s;
    case WorkerType::Module:
        return "Module"_s;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

static std::optional<WorkerType> convertStringToWorkerType(const String& type)
{
    if (type == "Classic"_s)
        return WorkerType::Classic;
    if (type == "Module"_s)
        return WorkerType::Module;

    return std::nullopt;
}

static ASCIILiteral currentRecordsTableSchema()
{
    return RECORDS_TABLE_SCHEMA_PREFIX "Records" RECORDS_TABLE_SCHEMA_SUFFIX;
}

static ASCIILiteral currentRecordsTableSchemaAlternate()
{
    return RECORDS_TABLE_SCHEMA_PREFIX "\"Records\"" RECORDS_TABLE_SCHEMA_SUFFIX;
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

ASCIILiteral SWRegistrationDatabase::statementString(StatementType type) const
{
    switch (type) {
    case StatementType::GetAllRecords:
        return "SELECT * FROM Records;"_s;
    case StatementType::InsertRecord:
        return "INSERT INTO Records VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"_s;
    case StatementType::DeleteRecord:
        return "DELETE FROM Records WHERE key = ?"_s;
    case StatementType::Invalid:
        break;
    }

    ASSERT_NOT_REACHED();
    return ""_s;
}

SQLiteStatementAutoResetScope SWRegistrationDatabase::cachedStatement(StatementType type)
{
    ASSERT(m_database);
    ASSERT(type < StatementType::Invalid);

    auto index = enumToUnderlyingType(type);
    if (!m_cachedStatements[index]) {
        if (auto result = m_database->prepareHeapStatement(statementString(type)))
            m_cachedStatements[index] = result.value().moveToUniquePtr();
    }

    return SQLiteStatementAutoResetScope { m_cachedStatements[index].get() };
}

SWRegistrationDatabase::SWRegistrationDatabase(const String& path)
    : m_directory(path)
    , m_cachedStatements(static_cast<size_t>(StatementType::Invalid))
{
    ASSERT(!isMainRunLoop());
    ASSERT(!m_directory.isEmpty());
}

SWRegistrationDatabase::~SWRegistrationDatabase()
{
    close();
}

void SWRegistrationDatabase::close()
{
    ASSERT(!isMainRunLoop());

    for (size_t i = 0; i < static_cast<size_t>(StatementType::Invalid); ++i)
        m_cachedStatements[i] = nullptr;
    m_database = nullptr;
    m_scriptStorage = nullptr;
}

SWScriptStorage& SWRegistrationDatabase::scriptStorage()
{
    if (!m_scriptStorage)
        m_scriptStorage = makeUnique<SWScriptStorage>(scriptVersionDirectoryPath(m_directory));
        
    return *m_scriptStorage;
}

bool SWRegistrationDatabase::prepareDatabase(ShouldCreateIfNotExists shouldCreateIfNotExists)
{
    if (m_database && m_database->isOpen())
        return true;

    if (m_directory.isEmpty())
        return false;

    auto databasePath = databaseFilePath(m_directory);
    bool databaseExists = FileSystem::fileExists(databasePath);
    if (shouldCreateIfNotExists == ShouldCreateIfNotExists::No && !databaseExists)
        return true;

    m_database = makeUnique<SQLiteDatabase>();
    FileSystem::makeAllDirectories(m_directory);
#if PLATFORM(MAC)
    auto openResult  = m_database->open(databasePath, SQLiteDatabase::OpenMode::ReadWriteCreate, SQLiteDatabase::OpenOptions::CanSuspendWhileLocked);
#else
    auto openResult  = m_database->open(databasePath);
#endif
    if (!openResult) {
        auto lastError = m_database->lastError();
        if (lastError == SQLITE_CORRUPT && lastError == SQLITE_NOTADB) {
            m_database = makeUnique<SQLiteDatabase>();
            SQLiteFileSystem::deleteDatabaseFile(databasePath);
            openResult  = m_database->open(databasePath);
        }
    }

    if (!openResult) {
        RELEASE_LOG_ERROR(ServiceWorker, "SWRegistrationDatabase::prepareDatabase failed to open database at '%s'", m_directory.utf8().data());
        m_database = nullptr;
        return false;
    }

    m_database->disableThreadingChecks();

    if (!ensureValidRecordsTable()) {
        m_database = nullptr;
        return false;
    }

    return true;
}

bool SWRegistrationDatabase::ensureValidRecordsTable()
{
    if (!m_database || !m_database->isOpen())
        return false;

    String statement = m_database->tableSQL("Records"_s);
    if (statement == currentRecordsTableSchema() || statement == currentRecordsTableSchemaAlternate())
        return true;

    // Table exists but statement is wrong; drop it.
    if (!statement.isEmpty()) {
        if (!m_database->executeCommand("DROP TABLE Records"_s)) {
            RELEASE_LOG_ERROR(ServiceWorker, "SWRegistrationDatabase::ensureValidRecordsTable failed to drop existing table (%d) - %s", m_database->lastError(), m_database->lastErrorMsg());
            return false;
        }
    }

    // Table does not exist.
    if (!m_database->executeCommand(currentRecordsTableSchema())) {
        RELEASE_LOG_ERROR(ServiceWorker, "SWRegistrationDatabase::ensureValidRecordsTable failed to create table (%d) - %s", m_database->lastError(), m_database->lastErrorMsg());
        return false;
    }

    return true;
}

std::optional<Vector<ServiceWorkerContextData>> SWRegistrationDatabase::importRegistrations()
{
    if (!prepareDatabase(ShouldCreateIfNotExists::No))
        return std::nullopt;

    if (!m_database) {
        clearAllRegistrations();
        return Vector<ServiceWorkerContextData> { };
    }

    auto statement = cachedStatement(StatementType::GetAllRecords);
    if (!statement) {
        RELEASE_LOG_ERROR(ServiceWorker, "SWRegistrationDatabase::importRegistrations failed on creating statement (%d) - %s", m_database->lastError(), m_database->lastErrorMsg());
        return std::nullopt;
    }

    Vector<ServiceWorkerContextData> registrations;
    int result = statement->step();
    for (; result == SQLITE_ROW; result = statement->step()) {
        auto key = ServiceWorkerRegistrationKey::fromDatabaseKey(statement->columnText(0));
        if (!key) {
            RELEASE_LOG_ERROR(ServiceWorker, "SWRegistrationDatabase::importRegistrations failed to decode service worker registration key");
            continue;
        }
    
        auto originURL = URL { statement->columnText(1) };
        auto scopePath = statement->columnText(2);
        auto scopeURL = URL { originURL, scopePath };
        auto topOrigin = SecurityOriginData::fromDatabaseIdentifier(statement->columnText(3));
        auto lastUpdateCheckTime = WallTime::fromRawSeconds(statement->columnDouble(4));
        auto updateViaCache = convertStringToUpdateViaCache(statement->columnText(5));
        auto scriptURL = URL { statement->columnText(6) };
        auto workerType = convertStringToWorkerType(statement->columnText(7));

        std::optional<ContentSecurityPolicyResponseHeaders> contentSecurityPolicy;
        auto contentSecurityPolicyDataSpan = statement->columnBlobAsSpan(8);
        if (contentSecurityPolicyDataSpan.size()) {
            WTF::Persistence::Decoder cspDecoder(contentSecurityPolicyDataSpan);
            cspDecoder >> contentSecurityPolicy;
            if (!contentSecurityPolicy) {
                RELEASE_LOG_ERROR(ServiceWorker, "SWRegistrationDatabase::importRegistrations failed to decode contentSecurityPolicy");
                continue;
            }
        }

        std::optional<CrossOriginEmbedderPolicy> coep;
        auto coepDataSpan = statement->columnBlobAsSpan(9);
        if (coepDataSpan.size()) {
            WTF::Persistence::Decoder coepDecoder(coepDataSpan);
            coepDecoder >> coep;
            if (!coep) {
                RELEASE_LOG_ERROR(ServiceWorker, "SWRegistrationDatabase::importRegistrations failed to decode crossOriginEmbedderPolicy");
                continue;
            }
        }

        auto referrerPolicy = statement->columnText(10);

        MemoryCompactRobinHoodHashMap<URL, ServiceWorkerContextData::ImportedScript> scriptResourceMap;
        auto scriptResourceMapDataSpan = statement->columnBlobAsSpan(11);
        if (scriptResourceMapDataSpan.size()) {
            WTF::Persistence::Decoder scriptResourceMapDecoder(scriptResourceMapDataSpan);
            std::optional<HashMap<URL, ImportedScriptAttributes>> scriptResourceMapWithoutScripts;
            scriptResourceMapDecoder >> scriptResourceMapWithoutScripts;
            if (!scriptResourceMapWithoutScripts) {
                RELEASE_LOG_ERROR(ServiceWorker, "SWRegistrationDatabase::importRegistrations failed to decode scriptResourceMapWithoutScripts");
                continue;
            }
            scriptResourceMap = populateScriptSourcesFromDisk(scriptStorage(), *key, WTFMove(*scriptResourceMapWithoutScripts));
        }

        auto certificateInfoDataSpan = statement->columnBlobAsSpan(12);
        std::optional<CertificateInfo> certificateInfo;
        WTF::Persistence::Decoder certificateInfoDecoder(certificateInfoDataSpan);
        certificateInfoDecoder >> certificateInfo;
        if (!certificateInfo) {
            RELEASE_LOG_ERROR(ServiceWorker, "SWRegistrationDatabase::importRegistrations failed to decode certificateInfo");
            continue;
        }

        auto navigationPreloadStateDataSpan = statement->columnBlobAsSpan(13);
        std::optional<NavigationPreloadState> navigationPreloadState;

        WTF::Persistence::Decoder navigationPreloadStateDecoder(navigationPreloadStateDataSpan);
        navigationPreloadStateDecoder >> navigationPreloadState;
        if (!navigationPreloadState) {
            RELEASE_LOG_ERROR(ServiceWorker, "SWRegistrationDatabase::importRegistrations failed to decode navigationPreloadState");
            continue;
        }

        // Validate the input for this registration.
        // If any part of this input is invalid, let's skip this registration.
        // FIXME: Should we return an error skipping *all* registrations?
        if (!originURL.isValid() || !topOrigin || !updateViaCache || !scriptURL.isValid() || !workerType || !scopeURL.isValid()) {
            RELEASE_LOG_ERROR(ServiceWorker, "SWRegistrationDatabase::importRegistrations failed to decode part of the registration");
            continue;
        }
        if (key->topOrigin() != *topOrigin) {
            RELEASE_LOG_ERROR(ServiceWorker, "SWRegistrationDatabase::importRegistrations found inconsistent registration");
            continue;
        }

        auto script = scriptStorage().retrieve(*key, scriptURL);
        if (!script) {
            RELEASE_LOG_ERROR(ServiceWorker, "SWRegistrationDatabase::importRegistrations failed to retrieve main script for %s from disk", scriptURL.string().utf8().data());
            continue;
        }

        auto workerIdentifier = ServiceWorkerIdentifier::generate();
        auto registrationIdentifier = ServiceWorkerRegistrationIdentifier::generate();
        auto serviceWorkerData = ServiceWorkerData { workerIdentifier, registrationIdentifier, scriptURL, ServiceWorkerState::Activated, *workerType };
        auto registration = ServiceWorkerRegistrationData { WTFMove(*key), registrationIdentifier, WTFMove(scopeURL), *updateViaCache, lastUpdateCheckTime, std::nullopt, std::nullopt, WTFMove(serviceWorkerData) };
        auto contextData = ServiceWorkerContextData { std::nullopt, WTFMove(registration), workerIdentifier, WTFMove(script), WTFMove(*certificateInfo), WTFMove(*contentSecurityPolicy), WTFMove(*coep), WTFMove(referrerPolicy), WTFMove(scriptURL), *workerType, true, LastNavigationWasAppInitiated::Yes, WTFMove(scriptResourceMap), std::nullopt, WTFMove(*navigationPreloadState) };

        registrations.append(WTFMove(contextData));
    }

    if (result != SQLITE_DONE)
        RELEASE_LOG_ERROR(Storage, "SWRegistrationDatabase::importRegistrations failed on executing statement (%d) - %s", m_database->lastError(), m_database->lastErrorMsg());

    return registrations;
}

std::optional<Vector<ServiceWorkerScripts>> SWRegistrationDatabase::updateRegistrations(const Vector<ServiceWorkerContextData>& registrationsToUpdate, const Vector<ServiceWorkerRegistrationKey>& registrationsToDelete)
{
    if (!prepareDatabase(ShouldCreateIfNotExists::Yes))
        return std::nullopt;

    SQLiteTransaction transaction(*m_database);
    transaction.begin();

    for (auto& registration : registrationsToDelete) {
        auto statement = cachedStatement(StatementType::DeleteRecord);
        if (!statement || statement->bindText(1, registration.toDatabaseKey()) != SQLITE_OK || statement->step() != SQLITE_DONE) {
            RELEASE_LOG_ERROR(Storage, "SWRegistrationDatabase::updateRegistrations failed to delete record (%d) - %s", m_database->lastError(), m_database->lastErrorMsg());
            return std::nullopt;
        }
        scriptStorage().clear(registration);
    }

    for (auto&& data : registrationsToUpdate) {
        auto statement = cachedStatement(StatementType::InsertRecord);
        if (!statement) {
            RELEASE_LOG_ERROR(ServiceWorker, "SWRegistrationDatabase::updateRegistrations failed to prepare statement for inserting record (%d) - %s", m_database->lastError(), m_database->lastErrorMsg());
            return std::nullopt;
        }

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

        if (statement->bindText(1, data.registration.key.toDatabaseKey()) != SQLITE_OK
            || statement->bindText(2, data.registration.scopeURL.protocolHostAndPort()) != SQLITE_OK
            || statement->bindText(3, data.registration.scopeURL.path().toString()) != SQLITE_OK
            || statement->bindText(4, data.registration.key.topOrigin().databaseIdentifier()) != SQLITE_OK
            || statement->bindDouble(5, data.registration.lastUpdateTime.secondsSinceEpoch().value()) != SQLITE_OK
            || statement->bindText(6, StringView { convertUpdateViaCacheToString(data.registration.updateViaCache) }) != SQLITE_OK
            || statement->bindText(7, data.scriptURL.string()) != SQLITE_OK
            || statement->bindText(8, StringView { convertWorkerTypeToString(data.workerType) }) != SQLITE_OK
            || statement->bindBlob(9, std::span(cspEncoder.buffer(), cspEncoder.bufferSize())) != SQLITE_OK
            || statement->bindBlob(10, std::span(coepEncoder.buffer(), coepEncoder.bufferSize())) != SQLITE_OK
            || statement->bindText(11, data.referrerPolicy) != SQLITE_OK
            || statement->bindBlob(12, std::span(scriptResourceMapEncoder.buffer(), scriptResourceMapEncoder.bufferSize())) != SQLITE_OK
            || statement->bindBlob(13, std::span(certificateInfoEncoder.buffer(), certificateInfoEncoder.bufferSize())) != SQLITE_OK
            || statement->bindBlob(14, std::span(navigationPreloadStateEncoder.buffer(), navigationPreloadStateEncoder.bufferSize())) != SQLITE_OK
            || statement->step() != SQLITE_DONE) {
            RELEASE_LOG_ERROR(ServiceWorker, "SWRegistrationDatabase::updateRegistrations failed to insert record (%i) - %s", m_database->lastError(), m_database->lastErrorMsg());
            return std::nullopt;
        }
    }
    
    transaction.commit();
    RELEASE_LOG(ServiceWorker, "SWRegistrationDatabase::updateRegistrations added/updated %zu registrations and removed %zu registrations", registrationsToUpdate.size(), registrationsToDelete.size());

    for (auto& registrationKey : registrationsToDelete)
        scriptStorage().clear(registrationKey);

    Vector<ServiceWorkerScripts> result;
    for (auto& data : registrationsToUpdate) {
        auto mainScript = scriptStorage().store(data.registration.key, data.scriptURL, data.script);
        if (!mainScript)
            continue;
        MemoryCompactRobinHoodHashMap<URL, ScriptBuffer> importedScripts;
        
        for (auto& [scriptURL, script] : data.scriptResourceMap) {
            auto importedScript = scriptStorage().store(data.registration.key, scriptURL, script.script);
            if (importedScript)
                importedScripts.add(scriptURL, importedScript);
        }
        result.append(ServiceWorkerScripts { data.serviceWorkerIdentifier, WTFMove(mainScript), WTFMove(importedScripts) });
    }

    return result;
}

void SWRegistrationDatabase::clearAllRegistrations()
{
    close();
    SQLiteFileSystem::deleteDatabaseFile(databaseFilePath(m_directory));
    FileSystem::deleteNonEmptyDirectory(scriptDirectoryPath(m_directory));
    FileSystem::deleteEmptyDirectory(m_directory);
}

} // namespace WebCore
