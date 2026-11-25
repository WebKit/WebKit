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
#include "WebExtensionStorageSQLiteStore.h"

#include "Logging.h"
#include "WebExtensionDataType.h"
#include "WebExtensionSQLiteHelpers.h"
#include <wtf/CrossThreadCopier.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebKit {

#if ENABLE(WK_WEB_EXTENSIONS)

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebExtensionStorageSQLiteStore);

static const SchemaVersion currentSchemaVersion = 2;

static RefPtr<StringImpl> rowFilterStringFromRowKeys(Vector<RefPtr<StringImpl>> keys)
{
    const Vector escapedAndQuotedKeys = [&] {
        Vector<RefPtr<StringImpl>> escapedAndQuotedKeys;
        for (RefPtr<StringImpl> key : keys) {
            RefPtr<StringImpl> keyWithSingleQuotesEscaped = makeStringByReplacingAll(key, "'"_s, "''"_s);
            escapedAndQuotedKeys.append(makeString("'"_s, keyWithSingleQuotesEscaped, "'"_s));
        }
        return escapedAndQuotedKeys;
    }();

    return makeStringByJoining(escapedAndQuotedKeys.span(), ", "_si);
}

WebExtensionStorageSQLiteStore::WebExtensionStorageSQLiteStore(StringImpl* uniqueIdentifier, WebExtensionDataType storageType, StringImpl* directory, UsesInMemoryDatabase useInMemoryDatabase)
    : WebExtensionSQLiteStore(uniqueIdentifier, directory, static_cast<bool>(useInMemoryDatabase))
    , m_storageType(storageType)
{
}

void WebExtensionStorageSQLiteStore::getAllKeys(CompletionHandler<void(Vector<RefPtr<StringImpl>> keys, StringImpl* errorMessage)>&& completionHandler)
{
    queue().dispatch([weakThis = ThreadSafeWeakPtr { *this }, uniqueIdentifier = crossThreadCopy(uniqueIdentifier()), completionHandler = WTF::move(completionHandler)]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis) {
            RELEASE_LOG_ERROR(Extensions, "Failed to retrieve all keys for extension %s.", utf8(uniqueIdentifier));
            WorkQueue::mainSingleton().dispatch([completionHandler = WTF::move(completionHandler)]() mutable {
                completionHandler({ }, "Failed to retrieve all keys"_si);
            });
            return;
        }

        RefPtr<StringImpl> errorMessage;
        auto keysArray = protectedThis->getAllKeysWithErrorMessage(errorMessage);

        WorkQueue::mainSingleton().dispatch([keysArray = crossThreadCopy(keysArray), errorMessage = crossThreadCopy(errorMessage), completionHandler = WTF::move(completionHandler)]() mutable {
            completionHandler(keysArray, errorMessage);
        });
    });
}

void WebExtensionStorageSQLiteStore::getValuesForKeys(Vector<RefPtr<StringImpl>> keys, CompletionHandler<void(HashMap<RefPtr<StringImpl>, RefPtr<StringImpl>> results, StringImpl* errorMessage)>&& completionHandler)
{
    queue().dispatch([weakThis = ThreadSafeWeakPtr { *this }, uniqueIdentifier = crossThreadCopy(uniqueIdentifier()), keys = crossThreadCopy(keys), completionHandler = WTF::move(completionHandler)]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis) {
            RELEASE_LOG_ERROR(Extensions, "Failed to retrieve values for keys: %s for extension %s.", utf8(rowFilterStringFromRowKeys(keys)), utf8(uniqueIdentifier));
            WorkQueue::mainSingleton().dispatch([completionHandler = WTF::move(completionHandler)]() mutable {
                completionHandler({ }, "Failed to retrieve values for keys"_si);
            });
            return;
        }

        RefPtr<StringImpl> errorMessage;
        auto results = keys.size() ? protectedThis->getValuesForKeysWithErrorMessage(keys, errorMessage) : protectedThis->getValuesForAllKeys(errorMessage);

        WorkQueue::mainSingleton().dispatch([results = crossThreadCopy(results), errorMessage = crossThreadCopy(errorMessage), completionHandler = WTF::move(completionHandler)]() mutable {
            completionHandler(results, errorMessage);
        });
    });
}

static size_t storageSizeOf(HashMap<RefPtr<StringImpl>, RefPtr<StringImpl>> map)
{
    size_t totalStorage = 0;

    for (const auto& key : map.keys())
        totalStorage += sizeInBytes(key);

    for (const auto& value : map.values())
        totalStorage += sizeInBytes(value);

    return totalStorage;
}

void WebExtensionStorageSQLiteStore::getStorageSizeForKeys(Vector<RefPtr<StringImpl>> keys, CompletionHandler<void(size_t storageSize, StringImpl* errorMessage)>&& completionHandler)
{
    queue().dispatch([protectedThis = Ref { *this }, uniqueIdentifier = crossThreadCopy(uniqueIdentifier()), keys = crossThreadCopy(keys), completionHandler = WTF::move(completionHandler)]() mutable {
        RefPtr<StringImpl> errorMessage;
        if (!keys.isEmpty()) {
            auto keysAndValues = protectedThis->getValuesForKeysWithErrorMessage(keys, errorMessage);
            WorkQueue::mainSingleton().dispatch([keysAndValues = crossThreadCopy(keysAndValues), errorMessage = crossThreadCopy(errorMessage), completionHandler = WTF::move(completionHandler)]() mutable {
                completionHandler(storageSizeOf(keysAndValues), errorMessage);
            });

            return;
        }

        // Return storage size for all keys if no keys are specified.
        if (!protectedThis->openDatabaseIfNecessary(errorMessage, false)) {
            WorkQueue::mainSingleton().dispatch([errorMessage = crossThreadCopy(errorMessage), completionHandler = WTF::move(completionHandler)]() mutable {
                completionHandler(0, errorMessage);
            });
            return;
        }

        int64_t result = 0;
        RefPtr<API::Error> error;
        SQLiteDatabaseEnumerate(*(protectedThis->database()), error, "SELECT SUM(LENGTH(key) + LENGTH(value)) FROM extension_storage"_si, std::tie(result));

        WorkQueue::mainSingleton().dispatch([result, error, completionHandler = WTF::move(completionHandler)]() mutable {
            if (!error)
                completionHandler(result, nullptr);
            else {
                RELEASE_LOG_ERROR(Extensions, "Failed to calculate storage size for keys: %s", utf8(error->localizedDescription()));
                completionHandler(0, error->localizedDescription());
            }
        });
    });
}

static Vector<RefPtr<StringImpl>> toVector(HashMap<RefPtr<StringImpl>, RefPtr<StringImpl>> map, bool mapKeys)
{
    Vector<RefPtr<StringImpl>> result;
    if (mapKeys) {
        for (const auto& key : map.keys())
            result.append(key);
    } else {
        for (const auto& value : map.values())
            result.append(value);
    }

    return result;
}

void WebExtensionStorageSQLiteStore::getStorageSizeForAllKeys(HashMap<RefPtr<StringImpl>, RefPtr<StringImpl>> additionalKeyedData, CompletionHandler<void(size_t storageSize, int numberOfKeysIncludingAdditionalKeyedData, HashMap<RefPtr<StringImpl>, RefPtr<StringImpl>> existingKeysAndValues, StringImpl* errorMessage)>&& completionHandler)
{
    getStorageSizeForKeys({ }, [this, protectedThis = Ref { *this }, additionalKeyedData, completionHandler = WTF::move(completionHandler)](size_t storageSize, StringImpl* errorMessage) mutable {
        if (!isEmpty(errorMessage)) {
            completionHandler(0.0, 0, { }, errorMessage);
            return;
        }

        queue().dispatch([weakThis = ThreadSafeWeakPtr { *this }, uniqueIdentifier = crossThreadCopy(uniqueIdentifier()), storageSize, additionalKeyedData = crossThreadCopy(additionalKeyedData), completionHandler = WTF::move(completionHandler)]() mutable {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis) {
                RELEASE_LOG_ERROR(Extensions, "Failed to calculate storage size for extension %s.", utf8(uniqueIdentifier));
                WorkQueue::mainSingleton().dispatch([completionHandler = WTF::move(completionHandler)]() mutable {
                    completionHandler(0.0, 0, { }, makeString("Failed to calculate storage size"_s));
                });
                return;
            }

            RefPtr<StringImpl> errorMessage;
            auto oldValuesForAdditionalKeys = protectedThis->getValuesForKeysWithErrorMessage(toVector(additionalKeyedData, true), errorMessage);
            auto oldStorageSizeForAdditionalKeys = storageSizeOf(oldValuesForAdditionalKeys);
            auto newStorageSizeForAdditionalKeys = storageSizeOf(additionalKeyedData);
            auto updatedStorageSize = storageSize - oldStorageSizeForAdditionalKeys + newStorageSizeForAdditionalKeys;

            auto existingAndAdditionalKeys = protectedThis->getAllKeysWithErrorMessage(errorMessage);
            existingAndAdditionalKeys.appendVector(toVector(additionalKeyedData, true));

            WorkQueue::mainSingleton().dispatch([updatedStorageSize, existingAndAdditionalKeys = crossThreadCopy(existingAndAdditionalKeys), oldValuesForAdditionalKeys = crossThreadCopy(oldValuesForAdditionalKeys), errorMessage = crossThreadCopy(errorMessage), completionHandler = WTF::move(completionHandler)]() mutable {
                completionHandler(updatedStorageSize, existingAndAdditionalKeys.size(), oldValuesForAdditionalKeys, errorMessage);
            });
        });
    });
}

void WebExtensionStorageSQLiteStore::setKeyedData(HashMap<RefPtr<StringImpl>, RefPtr<StringImpl>> keyedData, CompletionHandler<void(Vector<RefPtr<StringImpl>> keysSuccessfullySet, StringImpl* errorMessage)>&& completionHandler)
{
    queue().dispatch([weakThis = ThreadSafeWeakPtr { *this }, keyedData = crossThreadCopy(keyedData), completionHandler = WTF::move(completionHandler)]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis) {
            WorkQueue::mainSingleton().dispatch([keyedData = WTF::move(keyedData), completionHandler = WTF::move(completionHandler)]() mutable {
                completionHandler({ }, makeString("Failed to set keys: "_s, rowFilterStringFromRowKeys(toVector(keyedData, true))));
            });
            return;
        }

        RefPtr<StringImpl> errorMessage;
        if (!protectedThis->openDatabaseIfNecessary(errorMessage, true)) {
            WorkQueue::mainSingleton().dispatch([errorMessage = crossThreadCopy(errorMessage), completionHandler = WTF::move(completionHandler)]() mutable {
                completionHandler({ }, errorMessage);
            });
            return;
        }

        ASSERT(isEmpty(errorMessage));

        Vector<RefPtr<StringImpl>> keysSuccessfullySet;
        for (auto key : keyedData.keys()) {
            errorMessage = protectedThis->insertOrUpdateValue(keyedData.get(key), key, *(protectedThis->database()));
            if (!isEmpty(errorMessage))
                break;

            keysSuccessfullySet.append(key);
        }

        WorkQueue::mainSingleton().dispatch([keysSuccessfullySet = crossThreadCopy(keysSuccessfullySet), errorMessage = crossThreadCopy(errorMessage), completionHandler = WTF::move(completionHandler)]() mutable {
            completionHandler(keysSuccessfullySet, errorMessage);
        });
    });
}

void WebExtensionStorageSQLiteStore::deleteValuesForKeys(Vector<RefPtr<StringImpl>> keys, CompletionHandler<void(StringImpl* errorMessage)>&& completionHandler)
{
    queue().dispatch([weakThis = ThreadSafeWeakPtr { *this }, keys = crossThreadCopy(keys), completionHandler = WTF::move(completionHandler)]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis) {
            WorkQueue::mainSingleton().dispatch([keys = WTF::move(keys), completionHandler = WTF::move(completionHandler)]() mutable {
                completionHandler(makeString("Failed to delete keys: "_s, rowFilterStringFromRowKeys(keys)));
            });
            return;
        }

        RefPtr<StringImpl> errorMessage;
        if (!protectedThis->openDatabaseIfNecessary(errorMessage, false)) {
            WorkQueue::mainSingleton().dispatch([errorMessage = crossThreadCopy(errorMessage), completionHandler = WTF::move(completionHandler)]() mutable {
                completionHandler(errorMessage);
            });
            return;
        }

        ASSERT(isEmpty(errorMessage));

        DatabaseResult result = SQLiteDatabaseExecute(*(protectedThis->database()), makeString("DELETE FROM extension_storage WHERE key in ("_s, rowFilterStringFromRowKeys(keys), ")"_s));
        if (result != SQLITE_DONE) {
            RELEASE_LOG_ERROR(Extensions, "Failed to delete keys %s for extension %s.", utf8(rowFilterStringFromRowKeys(keys)), utf8(protectedThis->uniqueIdentifier()));
            errorMessage = makeString("Failed to delete keys "_s, rowFilterStringFromRowKeys(keys));
        }

        auto deleteDatabaseErrorMessage = protectedThis->deleteDatabaseIfEmpty();

        WorkQueue::mainSingleton().dispatch([deleteDatabaseErrorMessage = crossThreadCopy(deleteDatabaseErrorMessage), errorMessage = crossThreadCopy(errorMessage), completionHandler = WTF::move(completionHandler)]() mutable {
            // Errors from opening the database or deleting keys take precedence over an error deleting the database.
            completionHandler(!isEmpty(errorMessage) ? errorMessage : deleteDatabaseErrorMessage);
        });
    });
}

RefPtr<StringImpl> WebExtensionStorageSQLiteStore::insertOrUpdateValue(StringImpl* value, StringImpl* key, Ref<WebExtensionSQLiteDatabase> database)
{
    assertIsCurrent(queue());

    DatabaseResult result = SQLiteDatabaseExecute(database, "INSERT OR REPLACE INTO extension_storage (key, value) VALUES (?, ?)"_si, key, value);
    if (result != SQLITE_DONE) {
        RELEASE_LOG_ERROR(Extensions, "Failed to insert value %s for key %s for extension %s.", utf8(value), utf8(key), utf8(uniqueIdentifier()));
        return makeString("Failed to insert value "_s, value, " for key "_s, key);
    }

    return nullString();
}

HashMap<RefPtr<StringImpl>, RefPtr<StringImpl>> WebExtensionStorageSQLiteStore::getValuesForAllKeys(RefPtr<StringImpl>& errorMessage)
{
    assertIsCurrent(queue());

    if (!openDatabaseIfNecessary(errorMessage, false))
        return { };

    ASSERT(isEmpty(errorMessage));

    if (RefPtr rows = SQLiteDatabaseFetch(*database(), "SELECT * FROM extension_storage"_si))
        return getKeysAndValuesFromRowIterator(*rows);
    return { };
}

HashMap<RefPtr<StringImpl>, RefPtr<StringImpl>> WebExtensionStorageSQLiteStore::getKeysAndValuesFromRowIterator(Ref<WebExtensionSQLiteRowEnumerator> rows)
{
    HashMap<RefPtr<StringImpl>, RefPtr<StringImpl>> results;

    RefPtr row = rows->next();
    while (row != nullptr) {
        auto key = row->getString(0);
        auto value = row->getString(1);

        results.set(key, value);

        row = rows->next();
    }

    return results;
}

Vector<RefPtr<StringImpl>> WebExtensionStorageSQLiteStore::getAllKeysWithErrorMessage(RefPtr<StringImpl>& errorMessage)
{
    assertIsCurrent(queue());

    if (!openDatabaseIfNecessary(errorMessage, false))
        return { };

    ASSERT(isEmpty(errorMessage));

    Vector<RefPtr<StringImpl>> keys;
    if (RefPtr rows = SQLiteDatabaseFetch(*database(), "SELECT key FROM extension_storage"_si)) {
        RefPtr row = rows->next();
        while (row != nullptr) {
            keys.append(row->getString(0));

            row = rows->next();
        }
    }

    return keys;
}

HashMap<RefPtr<StringImpl>, RefPtr<StringImpl>> WebExtensionStorageSQLiteStore::getValuesForKeysWithErrorMessage(Vector<RefPtr<StringImpl>> keys, RefPtr<StringImpl>& errorMessage)
{
    assertIsCurrent(queue());

    if (!openDatabaseIfNecessary(errorMessage, false))
        return { };

    ASSERT(isEmpty(errorMessage));

    if (RefPtr rows = SQLiteDatabaseFetch(*database(), makeString("SELECT * FROM extension_storage WHERE key in ("_s, rowFilterStringFromRowKeys(keys), ")"_s)))
        return getKeysAndValuesFromRowIterator(*rows);
    return { };
}

// MARK: Database Schema

SchemaVersion WebExtensionStorageSQLiteStore::currentDatabaseSchemaVersion()
{
    return currentSchemaVersion;
}

DatabaseResult WebExtensionStorageSQLiteStore::createFreshDatabaseSchema()
{
    assertIsCurrent(queue());
    ASSERT(database());

    DatabaseResult result = SQLiteDatabaseExecute(*database(), "CREATE TABLE extension_storage (key TEXT PRIMARY KEY NOT NULL, value TEXT NOT NULL)"_si);
    if (result != SQLITE_DONE)
        RELEASE_LOG_ERROR(Extensions, "Failed to create the extension_storage table for extension %s: %s (%d)", utf8(uniqueIdentifier()), lastErrorMessage(), result);
    return result;
}

SchemaVersion WebExtensionStorageSQLiteStore::migrateToCurrentSchemaVersionIfNeeded()
{
    assertIsCurrent(queue());

    auto databaseSchemaVersion = currentDatabaseSchemaVersion();
    if (databaseSchemaVersion == 1 && currentSchemaVersion == 2) {
        // Safari shipped with a database schema version of 2, but when migrating to WebKit, the version was
        // incorrectly marked as 1. This mismatch would normally trigger a database schema reset, erasing
        // all storage data. However, since the schema for version 2 (Safari) and version 1 (WebKit) are
        // identical, we simply set the version and return the current version to avoid unnecessary data loss.

        setDatabaseSchemaVersion(currentSchemaVersion);
    }

    return WebExtensionSQLiteStore::migrateToCurrentSchemaVersionIfNeeded();
}

DatabaseResult WebExtensionStorageSQLiteStore::resetDatabaseSchema()
{
    assertIsCurrent(queue());
    ASSERT(database());

    DatabaseResult result = SQLiteDatabaseExecute(*database(), "DROP TABLE IF EXISTS extension_storage"_si);
    if (result != SQLITE_DONE)
        RELEASE_LOG_ERROR(Extensions, "Failed to reset database schema for extension %s: %s (%d)", utf8(uniqueIdentifier()), lastErrorMessage(), result);

    return result;
}

bool WebExtensionStorageSQLiteStore::isDatabaseEmpty()
{
    assertIsCurrent(queue());
    ASSERT(database());

    RefPtr rows = SQLiteDatabaseFetch(*database(), "SELECT COUNT(*) FROM extension_storage"_si);
    if (RefPtr row = rows->next())
        return !row->getInt64(0);
    return true;
}

URL WebExtensionStorageSQLiteStore::databaseURL()
{
    if (useInMemoryDatabase())
        return WebExtensionSQLiteDatabase::inMemoryDatabaseURL();

    RefPtr<StringImpl> databaseName;
    switch (m_storageType) {
    case WebExtensionDataType::Local:
        databaseName = "LocalStorage.db"_si;
        break;
    case WebExtensionDataType::Sync:
        databaseName = "SyncStorage.db"_si;
        break;
    case WebExtensionDataType::Session:
        // Session storage is kept in memory only.
        ASSERT_NOT_REACHED();
        return { };
    }

    ASSERT(!directory().isEmpty());

    return URL(URL { makeString(directory().string(), "/"_s) }, databaseName);
}

#endif // ENABLE(WK_WEB_EXTENSIONS)

} // namespace WebKit
