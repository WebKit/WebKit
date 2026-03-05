/*
 * Copyright (C) 2025 Igalia, S.L. All rights reserved.
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
#include "WebExtensionRegisteredScriptsSQLiteStore.h"

#include "Logging.h"
#include "WebExtensionSQLiteHelpers.h"
#include <wtf/CrossThreadCopier.h>
#include <wtf/JSONValues.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebKit {

#if ENABLE(WK_WEB_EXTENSIONS)

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebExtensionRegisteredScriptsSQLiteStore);

static const SchemaVersion currentSchemaVersion = 2;

static constexpr auto idKey = "id"_s;
static constexpr auto persistAcrossSessionsKey = "persistAcrossSessions"_s;

static String rowFilterStringFromRowKeys(Vector<String> keys)
{
    Vector<String> escapedAndQuotedKeys;
    for (const auto& key : keys) {
        auto keyWithSingleQuotesEscaped = makeStringByReplacingAll(key, "'"_s, "''"_s);
        escapedAndQuotedKeys.append(makeString("'"_s, keyWithSingleQuotesEscaped, "'"_s));
    }

    return makeStringByJoining(escapedAndQuotedKeys.span(), ", "_s);
}

WebExtensionRegisteredScriptsSQLiteStore::WebExtensionRegisteredScriptsSQLiteStore(const String& uniqueIdentifier, const String& directory, bool useInMemoryDatabase)
    : WebExtensionSQLiteStore(uniqueIdentifier, directory, useInMemoryDatabase)
{
}

void WebExtensionRegisteredScriptsSQLiteStore::updateScripts(Vector<Ref<JSON::Object>> scripts, CompletionHandler<void(const String& errorMessage)>&& completionHandler)
{
    Vector<String> ids;
    for (const auto& script : scripts)
        ids.append(script->getString(idKey));

    deleteScriptsWithIDs(ids, [weakThis = ThreadSafeWeakPtr { *this }, ids = WTF::move(ids), scripts = WTF::move(scripts), completionHandler = WTF::move(completionHandler)](const String& errorMessage) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis) {
            completionHandler({ });
            return;
        }

        if (!errorMessage.isEmpty()) {
            completionHandler(errorMessage);
            return;
        }

        protectedThis->addScripts(scripts, [completionHandler = WTF::move(completionHandler)](const String& errorMessage) mutable {
            completionHandler(errorMessage);
        });
    });
}

void WebExtensionRegisteredScriptsSQLiteStore::deleteScriptsWithIDs(Vector<String> ids, CompletionHandler<void(const String& errorMessage)>&& completionHandler)
{
    if (ids.isEmpty()) {
        completionHandler({ });
        return;
    }

    queue().dispatch([weakThis = ThreadSafeWeakPtr { *this }, ids = crossThreadCopy(ids), completionHandler = WTF::move(completionHandler)]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis) {
            completionHandler({ });
            return;
        }

        String errorMessage;
        if (!protectedThis->openDatabaseIfNecessary(errorMessage, false)) {
            WorkQueue::mainSingleton().dispatch([errorMessage = crossThreadCopy(errorMessage), completionHandler = WTF::move(completionHandler)]() mutable {
                completionHandler(errorMessage);
            });
            return;
        }

        ASSERT(errorMessage.isEmpty());

        DatabaseResult result = SQLiteDatabaseExecute(*(protectedThis->database()), makeString("DELETE FROM registered_scripts WHERE key in ("_s, rowFilterStringFromRowKeys(ids), ")"_s));
        if (result != SQLITE_DONE) {
            RELEASE_LOG_ERROR(Extensions, "Failed to delete scripts for extension %s.", protectedThis->uniqueIdentifier().utf8().data());
            errorMessage = "Failed to delete scripts from registered content scripts storage."_s;
        }

        String deleteDatabaseErrorMessage = protectedThis->deleteDatabaseIfEmpty();

        WorkQueue::mainSingleton().dispatch([deleteDatabaseErrorMessage = crossThreadCopy(deleteDatabaseErrorMessage), errorMessage = crossThreadCopy(errorMessage), completionHandler = WTF::move(completionHandler)]() mutable {
            // Errors from opening the database or deleting keys take precedence over an error deleting the database.
            completionHandler(!errorMessage.isEmpty() ? errorMessage : deleteDatabaseErrorMessage);
        });
    });
}

void WebExtensionRegisteredScriptsSQLiteStore::addScripts(Vector<Ref<JSON::Object>> scripts, CompletionHandler<void(const String& errorMessage)>&& completionHandler)
{
    // Only save persistent scripts to storage
    Vector<Ref<JSON::Object>> persistentScripts;
    for (Ref script : scripts) {
        if (auto persistent = script->getBoolean(persistAcrossSessionsKey); persistent && persistent.value())
            persistentScripts.append(script);
    }

    if (persistentScripts.isEmpty()) {
        completionHandler({ });
        return;
    }

    queue().dispatch([weakThis = ThreadSafeWeakPtr { *this }, persistentScripts, completionHandler = WTF::move(completionHandler)]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis) {
            completionHandler({ });
            return;
        }

        String errorMessage;
        if (!protectedThis->openDatabaseIfNecessary(errorMessage, true)) {
            WorkQueue::mainSingleton().dispatch([errorMessage = crossThreadCopy(errorMessage), completionHandler = WTF::move(completionHandler)]() mutable {
                completionHandler(errorMessage);
            });
            return;
        }

        ASSERT(errorMessage.isEmpty());

        for (Ref script : persistentScripts)
            protectedThis->insertScript(script, *(protectedThis->database()), errorMessage);

        WorkQueue::mainSingleton().dispatch([errorMessage = crossThreadCopy(errorMessage), completionHandler = WTF::move(completionHandler)]() mutable {
            completionHandler(errorMessage);
        });
    });
}

void WebExtensionRegisteredScriptsSQLiteStore::getScripts(CompletionHandler<void(Vector<Ref<JSON::Object>> scripts, const String& errorMessage)>&& completionHandler)
{
    queue().dispatch([weakThis = ThreadSafeWeakPtr { *this }, completionHandler = WTF::move(completionHandler)]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis) {
            completionHandler({ }, nullString());
            return;
        }

        String errorMessage;
        auto scripts = protectedThis->getScriptsWithErrorMessage(errorMessage);
        WorkQueue::mainSingleton().dispatch([scripts = WTF::move(scripts), errorMessage = crossThreadCopy(errorMessage), completionHandler = WTF::move(completionHandler)]() mutable {
            completionHandler(scripts, errorMessage);
        });
    });
}

Vector<Ref<JSON::Object>> WebExtensionRegisteredScriptsSQLiteStore::getScriptsWithErrorMessage(String& errorMessage)
{
    assertIsCurrent(queue());

    if (!openDatabaseIfNecessary(errorMessage, false))
        return { };

    ASSERT(errorMessage.isEmpty());

    if (RefPtr rows = SQLiteDatabaseFetch(*database(), "SELECT * FROM registered_scripts"_s))
        return getKeysAndValuesFromRowIterator(*rows);
    return { };
}

Vector<Ref<JSON::Object>> WebExtensionRegisteredScriptsSQLiteStore::getKeysAndValuesFromRowIterator(Ref<WebExtensionSQLiteRowEnumerator> rows)
{
    Vector<Ref<JSON::Object>> results;

    RefPtr row = rows->next();
    while (row != nullptr) {
        auto script = row->getString(1);

        if (RefPtr value = JSON::Value::parseJSON(script)) {
            if (RefPtr object = value->asObject())
                results.append(*object);
            else
                RELEASE_LOG_ERROR(Extensions, "Failed to deserialize registered content scripts for extension %s", uniqueIdentifier().utf8().data());
        } else
            RELEASE_LOG_ERROR(Extensions, "Failed to parse JSON for registered content scripts for extension %s", uniqueIdentifier().utf8().data());

        row = rows->next();
    }

    return results;
}

void WebExtensionRegisteredScriptsSQLiteStore::insertScript(const JSON::Object& script, Ref<WebExtensionSQLiteDatabase> database, String& errorMessage)
{
    assertIsCurrent(queue());

    auto scriptID = script.getString(idKey);
    ASSERT(!scriptID.isEmpty());

    auto scriptData = script.toJSONString();
    DatabaseResult result = SQLiteDatabaseExecute(database, "INSERT INTO registered_scripts (key, script) VALUES (?, ?)"_s, scriptID, scriptData);
    if (result != SQLITE_DONE) {
        RELEASE_LOG_ERROR(Extensions, "Failed to insert registered content script for extension %s.", uniqueIdentifier().utf8().data());
        errorMessage = "Failed to add content script."_s;
        return;
    }
}

// MARK: Database Schema

SchemaVersion WebExtensionRegisteredScriptsSQLiteStore::currentDatabaseSchemaVersion()
{
    return currentSchemaVersion;
}

DatabaseResult WebExtensionRegisteredScriptsSQLiteStore::createFreshDatabaseSchema()
{
    assertIsCurrent(queue());
    ASSERT(database());

    DatabaseResult result = SQLiteDatabaseExecute(*database(), "CREATE TABLE registered_scripts (key TEXT PRIMARY KEY NOT NULL, script BLOB NOT NULL)"_s);
    if (result != SQLITE_DONE)
        RELEASE_LOG_ERROR(Extensions, "Failed to create registered_scripts database for extension %s: %s (%d)", uniqueIdentifier().utf8().data(), lastErrorMessage().data(), result);
    return result;
}

SchemaVersion WebExtensionRegisteredScriptsSQLiteStore::migrateToCurrentSchemaVersionIfNeeded()
{
    assertIsCurrent(queue());

    auto currentDatabaseSchemaVersion = databaseSchemaVersion();
    if (currentDatabaseSchemaVersion == 1) {
        // We need to migrate existing data to the format understood by the new C++ SQLite Store parser
        // Older data would be stored in a format dictated by NSKeyedArchiver/NSKeyedUnarchiver, and would need to be converted
        // to the JSON data that the new format expects.
        // We do bump the schema version, as it's technically a format change, but to avoid unnecessary data loss, we simply migrate the data
        // and return the new version without deleting the database.
        migrateData();

        setDatabaseSchemaVersion(currentSchemaVersion);
        return currentSchemaVersion;
    }

    return WebExtensionSQLiteStore::migrateToCurrentSchemaVersionIfNeeded();
}

DatabaseResult WebExtensionRegisteredScriptsSQLiteStore::resetDatabaseSchema()
{
    assertIsCurrent(queue());
    ASSERT(database());

    DatabaseResult result = SQLiteDatabaseExecute(*database(), "DROP TABLE IF EXISTS registered_scripts"_s);
    if (result != SQLITE_DONE)
        RELEASE_LOG_ERROR(Extensions, "Failed to reset registered_scripts database schema for extension %s: %s (%d)", uniqueIdentifier().utf8().data(), lastErrorMessage().data(), result);

    return result;
}

bool WebExtensionRegisteredScriptsSQLiteStore::isDatabaseEmpty()
{
    assertIsCurrent(queue());
    ASSERT(database());

    RefPtr rows = SQLiteDatabaseFetch(*database(), "SELECT COUNT(*) FROM registered_scripts"_s);
    if (RefPtr row = rows->next())
        return !row->getInt64(0);
    return true;
}

URL WebExtensionRegisteredScriptsSQLiteStore::databaseURL()
{
    if (useInMemoryDatabase())
        return WebExtensionSQLiteDatabase::inMemoryDatabaseURL();

    ASSERT(!directory().isEmpty());
    return URL(URL { makeString(directory().string(), "/"_s) }, "RegisteredContentScripts.db"_s);
}

#endif // ENABLE(WK_WEB_EXTENSIONS)

} // namespace WebKit
