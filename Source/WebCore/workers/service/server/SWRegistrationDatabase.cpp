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
#include <wtf/TZoneMallocInlines.h>
#include <wtf/persistence/PersistentCoders.h>
#include <wtf/persistence/PersistentDecoder.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SWRegistrationDatabase);

static constexpr auto scriptVersion = "V1"_s;
#define RECORDS_TABLE_SCHEMA_PREFIX "CREATE TABLE "

#define RECORDS_TABLE_SCHEMA_SUFFIX_V1 \
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
    ", preloadState BLOB NOT NULL ON CONFLICT FAIL"

#define RECORDS_TABLE_SCHEMA_SUFFIX_V2 \
    RECORDS_TABLE_SCHEMA_SUFFIX_V1 \
    ", routes BLOB NOT NULL ON CONFLICT FAIL"

static constexpr std::array<ASCIILiteral, 4> swRegistrationUpdatesV2 {
    "ALTER TABLE Records RENAME TO RecordsOld"_s,
    "CREATE TABLE Records(" RECORDS_TABLE_SCHEMA_SUFFIX_V2 ")"_s,
    "INSERT INTO Records SELECT key, origin, scopeURL, topOrigin, lastUpdateCheckTime, updateViaCache, scriptURL, workerType, contentSecurityPolicy, crossOriginEmbedderPolicy, referrerPolicy, scriptResourceMap, certificateInfo, preloadState, X'' FROM RecordsOld"_s,
    "DROP TABLE RecordsOld"_s,
};

#define RECORDS_TABLE_SCHEMA_SUFFIX RECORDS_TABLE_SCHEMA_SUFFIX_V2

static constexpr int currentSWRegistrationVersion = 2;

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
    return RECORDS_TABLE_SCHEMA_PREFIX "Records(" RECORDS_TABLE_SCHEMA_SUFFIX ")"_s;
}

static ASCIILiteral currentRecordsTableSchemaAlternate()
{
    return RECORDS_TABLE_SCHEMA_PREFIX "\"Records\"(" RECORDS_TABLE_SCHEMA_SUFFIX ")"_s;
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
        importedScripts.add(pair.key, ServiceWorkerContextData::ImportedScript { WTF::move(importedScript), WTF::move(pair.value.responseURL), WTF::move(pair.value.mimeType) });
    }
    return importedScripts;
}

ASCIILiteral SWRegistrationDatabase::statementString(StatementType type) const
{
    switch (type) {
    case StatementType::GetAllRecords:
        return "SELECT * FROM Records;"_s;
    case StatementType::CountAllRecords:
        return "SELECT COUNT(*) FROM Records;"_s;
    case StatementType::InsertRecord:
        return "INSERT INTO Records VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"_s;
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

    auto index = std::to_underlying(type);
    if (!m_cachedStatements[index]) {
        if (auto statement = CheckedRef { *m_database }->prepareStatement(statementString(type)))
            m_cachedStatements[index] = WTF::move(statement);
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
    if (CheckedPtr database = m_database.get(); database && database->isOpen())
        return true;

    if (m_directory.isEmpty())
        return false;

    auto databasePath = databaseFilePath(m_directory);
    bool databaseExists = FileSystem::fileExists(databasePath);
    if (shouldCreateIfNotExists == ShouldCreateIfNotExists::No && !databaseExists)
        return true;

    m_database = makeUnique<SQLiteDatabase>();
    FileSystem::makeAllDirectories(m_directory);
    auto openResult  = protect(m_database)->open(databasePath, SQLiteDatabase::OpenMode::ReadWriteCreate, SQLiteDatabase::OpenOptions::CanSuspendWhileLocked);
    if (!openResult) {
        auto lastError = protect(m_database)->lastError();
        if (lastError == SQLITE_CORRUPT && lastError == SQLITE_NOTADB) {
            m_database = makeUnique<SQLiteDatabase>();
            SQLiteFileSystem::deleteDatabaseFile(databasePath);
            openResult  = protect(m_database)->open(databasePath);
        }
    }

    if (!openResult) {
        RELEASE_LOG_ERROR(ServiceWorker, "SWRegistrationDatabase::prepareDatabase failed to open database at '%s'", m_directory.utf8().data());
        m_database = nullptr;
        return false;
    }

    protect(m_database)->disableThreadingChecks();

    int version = 1;
    {
        auto sql = protect(m_database)->prepareStatement("PRAGMA user_version"_s);
        if (sql && sql->step() == SQLITE_ROW)
            version = sql->columnInt(0);
    }

    if (version < 0 || version > currentSWRegistrationVersion) {
        RELEASE_LOG_ERROR(ServiceWorker, "Found unexpected SWRegistrationDatabase version: %d (expected: %d) at path: %s", version, currentSWRegistrationVersion, databasePath.utf8().data());
        m_database = nullptr;
        return false;
    }

    if (version < currentSWRegistrationVersion) {
        SQLiteTransaction transaction(*m_database);
        transaction.begin();

        if (databaseExists) {
            for (auto statement : swRegistrationUpdatesV2) {
                if (!protect(m_database)->executeCommand(statement)) {
                    RELEASE_LOG_ERROR(ServiceWorker, "Error executing SWRegistrationDatabase statement update: %d", m_database->lastError());
                    return false;
                }
            }
        }

        if (!protect(m_database)->executeCommandSlow(makeString("PRAGMA user_version = "_s, currentSWRegistrationVersion)))
            RELEASE_LOG_ERROR(ServiceWorker, "Error setting SWRegistrationDatabase user version: %d", m_database->lastError());

        transaction.commit();
    }

    if (!ensureValidRecordsTable()) {
        m_database = nullptr;
        return false;
    }

    return true;
}

bool SWRegistrationDatabase::ensureValidRecordsTable()
{
    CheckedPtr database = m_database.get();
    if (!database || !database->isOpen())
        return false;

    String statement = database->tableSQL("Records"_s);
    if (statement == currentRecordsTableSchema() || statement == currentRecordsTableSchemaAlternate())
        return true;

    // Table exists but statement is wrong; drop it.
    if (!statement.isEmpty()) {
        if (!database->executeCommand("DROP TABLE Records"_s)) {
            RELEASE_LOG_ERROR(ServiceWorker, "SWRegistrationDatabase::ensureValidRecordsTable failed to drop existing table (%d) - %s", m_database->lastError(), m_database->lastErrorMsg());
            return false;
        }
    }

    // Table does not exist.
    if (!database->executeCommand(currentRecordsTableSchema())) {
        RELEASE_LOG_ERROR(ServiceWorker, "SWRegistrationDatabase::ensureValidRecordsTable failed to create table (%d) - %s", m_database->lastError(), m_database->lastErrorMsg());
        return false;
    }

    return true;
}

std::optional<Vector<ServiceWorkerContextData>> SWRegistrationDatabase::importRegistrations()
{
    auto result = importRegistrationsImpl();
    if (result && result->isEmpty() && m_database)
        deleteAllFiles();

    return result;
}

std::optional<Vector<ServiceWorkerContextData>> SWRegistrationDatabase::importRegistrationsImpl()
{
    if (!prepareDatabase(ShouldCreateIfNotExists::No))
        return std::nullopt;

    if (!m_database) {
        deleteAllFiles();
        return Vector<ServiceWorkerContextData> { };
    }

    auto sqlStatement = cachedStatement(StatementType::GetAllRecords);
    if (!sqlStatement) {
        RELEASE_LOG_ERROR(ServiceWorker, "SWRegistrationDatabase::importRegistrations failed on creating statement (%d) - %s", m_database->lastError(), m_database->lastErrorMsg());
        return std::nullopt;
    }
    CheckedPtr statement = sqlStatement.get();

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
            scriptResourceMap = populateScriptSourcesFromDisk(scriptStorage(), *key, WTF::move(*scriptResourceMapWithoutScripts));
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

        auto routesDataSpan = statement->columnBlobAsSpan(14);
        std::optional<Vector<ServiceWorkerRoute>> routes;

        if (routesDataSpan.size()) {
            WTF::Persistence::Decoder routesDecoder(routesDataSpan);
            routesDecoder >> routes;
            if (!routes) {
                RELEASE_LOG_ERROR(ServiceWorker, "SWRegistrationDatabase::importRegistrations failed to decode routes");
                continue;
            }
        } else
            routes = Vector<ServiceWorkerRoute> { };

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
        auto registration = ServiceWorkerRegistrationData { WTF::move(*key), registrationIdentifier, WTF::move(scopeURL), *updateViaCache, lastUpdateCheckTime, std::nullopt, std::nullopt, WTF::move(serviceWorkerData) };
        auto contextData = ServiceWorkerContextData { std::nullopt, WTF::move(registration), workerIdentifier, WTF::move(script), WTF::move(*certificateInfo), WTF::move(*contentSecurityPolicy), WTF::move(*coep), WTF::move(referrerPolicy), WTF::move(scriptURL), *workerType, true, LastNavigationWasAppInitiated::Yes, WTF::move(scriptResourceMap), std::nullopt, WTF::move(*navigationPreloadState), WTF::move(*routes) };

        registrations.append(WTF::move(contextData));
    }

    if (result != SQLITE_DONE)
        RELEASE_LOG_ERROR(Storage, "SWRegistrationDatabase::importRegistrations failed on executing statement (%d) - %s", m_database->lastError(), m_database->lastErrorMsg());

    return registrations;
}

std::optional<Vector<ServiceWorkerScripts>> SWRegistrationDatabase::updateRegistrations(const Vector<ServiceWorkerContextData>& registrationsToUpdate, const Vector<ServiceWorkerRegistrationKey>& registrationsToDelete)
{
    auto result = updateRegistrationsImpl(registrationsToUpdate, registrationsToDelete);
    if (auto count = recordsCount()) {
        if (!count.value())
            deleteAllFiles();
    }

    return result;
}

std::optional<Vector<ServiceWorkerScripts>> SWRegistrationDatabase::updateRegistrationsImpl(const Vector<ServiceWorkerContextData>& registrationsToUpdate, const Vector<ServiceWorkerRegistrationKey>& registrationsToDelete)
{
    if (!prepareDatabase(ShouldCreateIfNotExists::Yes))
        return std::nullopt;

    SQLiteTransaction transaction(*m_database);
    transaction.begin();

    for (auto& registration : registrationsToDelete) {
        auto sqlStatement = cachedStatement(StatementType::DeleteRecord);
        if (!sqlStatement) {
            RELEASE_LOG_ERROR(Storage, "SWRegistrationDatabase::updateRegistrations failed to delete record (%d) - %s", m_database->lastError(), m_database->lastErrorMsg());
            return std::nullopt;
        }
        CheckedPtr statement = sqlStatement.get();
        if (statement->bindText(1, registration.toDatabaseKey()) != SQLITE_OK || statement->step() != SQLITE_DONE) {
            RELEASE_LOG_ERROR(Storage, "SWRegistrationDatabase::updateRegistrations failed to delete record (%d) - %s", m_database->lastError(), m_database->lastErrorMsg());
            return std::nullopt;
        }
        scriptStorage().clear(registration);
    }

    for (auto&& data : registrationsToUpdate) {
        auto sqlStatement = cachedStatement(StatementType::InsertRecord);
        if (!sqlStatement) {
            RELEASE_LOG_ERROR(ServiceWorker, "SWRegistrationDatabase::updateRegistrations failed to prepare statement for inserting record (%d) - %s", m_database->lastError(), m_database->lastErrorMsg());
            return std::nullopt;
        }
        CheckedPtr statement = sqlStatement.get();

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

        WTF::Persistence::Encoder routesEncoder;
        routesEncoder << data.routes;

        if (statement->bindText(1, data.registration.key.toDatabaseKey()) != SQLITE_OK
            || statement->bindText(2, data.registration.scopeURL.protocolHostAndPort()) != SQLITE_OK
            || statement->bindText(3, data.registration.scopeURL.path().toString()) != SQLITE_OK
            || statement->bindText(4, data.registration.key.topOrigin().databaseIdentifier()) != SQLITE_OK
            || statement->bindDouble(5, data.registration.lastUpdateTime.secondsSinceEpoch().value()) != SQLITE_OK
            || statement->bindText(6, StringView { convertUpdateViaCacheToString(data.registration.updateViaCache) }) != SQLITE_OK
            || statement->bindText(7, data.scriptURL.string()) != SQLITE_OK
            || statement->bindText(8, StringView { convertWorkerTypeToString(data.workerType) }) != SQLITE_OK
            || statement->bindBlob(9, cspEncoder.span()) != SQLITE_OK
            || statement->bindBlob(10, coepEncoder.span()) != SQLITE_OK
            || statement->bindText(11, data.referrerPolicy) != SQLITE_OK
            || statement->bindBlob(12, scriptResourceMapEncoder.span()) != SQLITE_OK
            || statement->bindBlob(13, certificateInfoEncoder.span()) != SQLITE_OK
            || statement->bindBlob(14, navigationPreloadStateEncoder.span()) != SQLITE_OK
            || statement->bindBlob(15, routesEncoder.span()) != SQLITE_OK
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
        result.append(ServiceWorkerScripts { data.serviceWorkerIdentifier, WTF::move(mainScript), WTF::move(importedScripts) });
    }

    return result;
}

std::optional<uint64_t> SWRegistrationDatabase::recordsCount()
{
    if (!m_database)
        return std::nullopt;

    auto sqlStatement = cachedStatement(StatementType::CountAllRecords);
    if (!sqlStatement) {
        RELEASE_LOG_ERROR(ServiceWorker, "SWRegistrationDatabase::recordsCount failed on creating statement (%d) - %s", m_database->lastError(), m_database->lastErrorMsg());
        return std::nullopt;
    }

    CheckedPtr statement = sqlStatement.get();
    if (statement->step() != SQLITE_ROW) {
        RELEASE_LOG_ERROR(ServiceWorker, "SWRegistrationDatabase::recordsCount failed to count records (%d) - %s", m_database->lastError(), m_database->lastErrorMsg());
        return std::nullopt;
    }

    return statement->columnInt(0);
}

void SWRegistrationDatabase::deleteAllFiles()
{
    close();
    SQLiteFileSystem::deleteDatabaseFile(databaseFilePath(m_directory));
    FileSystem::deleteNonEmptyDirectory(scriptDirectoryPath(m_directory));
    FileSystem::deleteEmptyDirectory(m_directory);
}

} // namespace WebCore
