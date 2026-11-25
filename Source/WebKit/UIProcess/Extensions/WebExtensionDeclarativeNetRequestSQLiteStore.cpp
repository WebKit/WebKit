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
#include "WebExtensionDeclarativeNetRequestSQLiteStore.h"

#include "Logging.h"
#include "WebExtensionSQLiteHelpers.h"
#include <wtf/Borrow.h>
#include <wtf/CrossThreadCopier.h>
#include <wtf/JSONValues.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebKit {

#if ENABLE(WK_WEB_EXTENSIONS)

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebExtensionDeclarativeNetRequestSQLiteStore);

static const SchemaVersion currentDeclarativeNetRequestSchemaVersion = 2;

WebExtensionDeclarativeNetRequestSQLiteStore::WebExtensionDeclarativeNetRequestSQLiteStore(StringImpl* uniqueIdentifier, WebExtensionDeclarativeNetRequestStorageType storageType, StringImpl* directory, UsesInMemoryDatabase useInMemoryDatabase)
    : WebExtensionSQLiteStore(uniqueIdentifier, directory, static_cast<bool>(useInMemoryDatabase))
    , m_storageType(storageType)
{
    m_tableName = makeString(toString(m_storageType), "_rules"_s);
}

static Vector<RefPtr<StringImpl>> ruleIdMapToString(Vector<double> ruleIDs)
{
    return ruleIDs.map([](auto& id) {
        return makeString(id);
    });
}

void WebExtensionDeclarativeNetRequestSQLiteStore::updateRulesByRemovingIDs(Vector<double> ruleIDs, Ref<JSON::Array> rules, CompletionHandler<void(StringImpl* errorMessage)>&& completionHandler)
{
    deleteRules(ruleIDs, [protectedThis = Ref { *this }, ruleIDs = WTF::move(ruleIDs), rules = WTF::move(rules), completionHandler = WTF::move(completionHandler)](StringImpl* errorMessage) mutable {
        if (!isEmpty(errorMessage)) {
            completionHandler(errorMessage);
            return;
        }

        protectedThis->addRules(rules, [completionHandler = WTF::move(completionHandler)](StringImpl* errorMessage) mutable {
            completionHandler(errorMessage);
        });
    });
}

void WebExtensionDeclarativeNetRequestSQLiteStore::addRules(Ref<JSON::Array> rules, CompletionHandler<void(StringImpl* errorMessage)>&& completionHandler)
{
    if (!rules->length()) {
        completionHandler(nullptr);
        return;
    }

    Vector<std::pair<double, RefPtr<StringImpl>>> serializedRules;
    for (Ref ruleValue : rules.get()) {
        RefPtr rule = ruleValue->asObject();
        if (!rule || !rule->size()) {
            ASSERT_NOT_REACHED();
            continue;
        }

        auto ruleID = rule->getInteger("id"_si);
        ASSERT(ruleID);
        serializedRules.append({ *ruleID, rule->toJSONString() });
    }

    queue().dispatch([weakThis = ThreadSafeWeakPtr { *this }, serializedRules = crossThreadCopy(serializedRules), completionHandler = WTF::move(completionHandler)]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis) {
            WorkQueue::mainSingleton().dispatch([completionHandler = WTF::move(completionHandler)]() mutable {
                completionHandler(nullptr);
            });
            return;
        }

        RefPtr<StringImpl> errorMessage;
        if (!protectedThis->openDatabaseIfNecessary(errorMessage, true)) {
            WorkQueue::mainSingleton().dispatch([errorMessage = crossThreadCopy(errorMessage), completionHandler = WTF::move(completionHandler)]() mutable {
                completionHandler(errorMessage);
            });
            return;
        }

        ASSERT(isEmpty(errorMessage));

        Borrow borrowedSerializedRules = serializedRules;

        Vector<double> rulesIDs;
        for (auto& [ruleID, ruleData] : borrowedSerializedRules)
            rulesIDs.append(ruleID);

        ASSERT(rulesIDs.size());

        if (RefPtr rows = SQLiteDatabaseFetch(*(protectedThis->database()), makeString("SELECT id FROM "_s, protectedThis->m_tableName, " WHERE id in ("_s, makeStringByJoining(ruleIdMapToString(rulesIDs).span(), ", "_si), ")"_s))) {
            Vector<double> existingRuleIDs;
            RefPtr row = rows->next();

            while (row != nullptr) {
                existingRuleIDs.append(row->getInt64(0));
                row = rows->next();
            }

            if (existingRuleIDs.size() == 1)
                errorMessage = makeString("Failed to add "_s, toString(protectedThis->m_storageType), " rules. Rule "_s, existingRuleIDs.first(), " does not have a unique ID."_s);
            else if (existingRuleIDs.size() >= 2)
                errorMessage = makeString("Failed to add "_s, toString(protectedThis->m_storageType), " rules. Some rules do not have unique IDs ("_s, makeStringByJoining(ruleIdMapToString(rulesIDs).span(), ", "_si), ")"_s);

            if (!isEmpty(errorMessage)) {
                WorkQueue::mainSingleton().dispatch([errorMessage = crossThreadCopy(errorMessage), completionHandler = WTF::move(completionHandler)]() mutable {
                    completionHandler(errorMessage);
                });
                return;
            }
        }

        for (auto& [ruleID, ruleData] : borrowedSerializedRules) {
            errorMessage = protectedThis->insertRule(ruleID, ruleData, *(protectedThis->database()));
            if (!isEmpty(errorMessage))
                break;
        }

        WorkQueue::mainSingleton().dispatch([errorMessage = crossThreadCopy(errorMessage), completionHandler = WTF::move(completionHandler)]() mutable {
            completionHandler(errorMessage);
        });
    });
}

void WebExtensionDeclarativeNetRequestSQLiteStore::deleteRules(Vector<double> ruleIDs, CompletionHandler<void(StringImpl* errorMessage)>&& completionHandler)
{
    if (!ruleIDs.size()) {
        completionHandler(nullptr);
        return;
    }

    queue().dispatch([weakThis = ThreadSafeWeakPtr { *this }, ruleIDs = crossThreadCopy(ruleIDs), completionHandler = WTF::move(completionHandler)]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis) {
            WorkQueue::mainSingleton().dispatch([completionHandler = WTF::move(completionHandler)]() mutable {
                completionHandler(nullptr);
            });
            return;
        }

        RefPtr<StringImpl> errorMessage;
        if (!protectedThis->openDatabaseIfNecessary(errorMessage, true)) {
            WorkQueue::mainSingleton().dispatch([errorMessage = crossThreadCopy(errorMessage), completionHandler = WTF::move(completionHandler)]() mutable {
                completionHandler(errorMessage);
            });
            return;
        }

        ASSERT(isEmpty(errorMessage));
        ASSERT(protectedThis->database());

        DatabaseResult result = SQLiteDatabaseExecute(*(protectedThis->database()), makeString("DELETE FROM "_s, protectedThis->m_tableName, " WHERE id IN ("_s, makeStringByJoining(ruleIdMapToString(ruleIDs).span(), ", "_si), ")"_s));
        if (result != SQLITE_DONE) {
            RELEASE_LOG_ERROR(Extensions, "Failed to delete rules for extension %s.", utf8(protectedThis->uniqueIdentifier()));
            errorMessage = makeString("Failed to delete rules from "_s, toString(protectedThis->m_storageType), " rules storage."_s);
        }

        RefPtr<StringImpl> deleteDatabaseErrorMessage = protectedThis->deleteDatabaseIfEmpty();

        WorkQueue::mainSingleton().dispatch([errorMessage = crossThreadCopy(errorMessage), deleteDatabaseErrorMessage = crossThreadCopy(deleteDatabaseErrorMessage), completionHandler = WTF::move(completionHandler)]() mutable {
            completionHandler(isEmpty(errorMessage) ? deleteDatabaseErrorMessage : errorMessage);
        });
    });
}

void WebExtensionDeclarativeNetRequestSQLiteStore::getRulesWithRuleIDs(Vector<double> ruleIDs, CompletionHandler<void(RefPtr<JSON::Array> rules, StringImpl* errorMessage)>&& completionHandler)
{
    queue().dispatch([weakThis = ThreadSafeWeakPtr { *this }, ruleIDs = crossThreadCopy(ruleIDs), completionHandler = WTF::move(completionHandler)]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis) {
            WorkQueue::mainSingleton().dispatch([completionHandler = WTF::move(completionHandler)]() mutable {
                completionHandler({ }, nullString());
            });
            return;
        }

        RefPtr<StringImpl> errorMessage;
        RefPtr rules = protectedThis->getRulesWithRuleIDsInternal(ruleIDs, errorMessage);
        WorkQueue::mainSingleton().dispatch([rules = WTF::move(rules), errorMessage = crossThreadCopy(errorMessage), completionHandler = WTF::move(completionHandler)]() mutable {
            completionHandler(rules, errorMessage);
        });
    });
}

RefPtr<JSON::Array> WebExtensionDeclarativeNetRequestSQLiteStore::getRulesWithRuleIDsInternal(Vector<double> ruleIDs, RefPtr<StringImpl>& errorMessage)
{
    assertIsCurrent(queue());

    if (!openDatabaseIfNecessary(errorMessage, true))
        return { };

    ASSERT(isEmpty(errorMessage));
    ASSERT(database());

    RefPtr<WebExtensionSQLiteRowEnumerator> rows;

    if (ruleIDs.size()) {
        const Vector bindings = [&] {
            Vector<RefPtr<StringImpl>> bindings;
            for (size_t i = 0; i < ruleIDs.size(); i++)
                bindings.append("?"_s);
            return bindings;
        }();

        auto joinedBindings = makeStringByJoining(bindings.span(), ", "_si);
        auto query = makeString("SELECT * FROM "_s, m_tableName, " WHERE id IN ("_s, joinedBindings, ")"_s);
        RefPtr<API::Error> statementError;
        RefPtr statement = WebExtensionSQLiteStatement::create(*(database()), query, statementError);
        if (!statement || statementError)
            return { };

        Borrow borrowedRuleIDs = ruleIDs;
        for (size_t i = 0; i < ruleIDs.size(); i++)
            statement->bind(borrowedRuleIDs[i], i + 1);

        rows = statement->fetch();
    } else
        rows = SQLiteDatabaseFetch(*(database()), makeString("SELECT * FROM "_s, m_tableName));

    return getKeysAndValuesFromRowIterator(*rows);
}

Ref<JSON::Array> WebExtensionDeclarativeNetRequestSQLiteStore::getKeysAndValuesFromRowIterator(Ref<WebExtensionSQLiteRowEnumerator> rows)
{
    Ref results = JSON::Array::create();

    RefPtr row = rows->next();
    while (row != nullptr) {
        auto rule = row->getData(1);

        if (!rule)
            continue;

        auto ruleObject = JSON::Value::optionalParseJSON(byteCast<Latin1Character>(rule->span()));
        if (ruleObject)
            results->pushValue(ruleObject->get());

        row = rows->next();
    }

    return results;
}

RefPtr<StringImpl> WebExtensionDeclarativeNetRequestSQLiteStore::insertRule(double ruleID, StringImpl* ruleData, Ref<WebExtensionSQLiteDatabase> database)
{
    assertIsCurrent(queue());

    DatabaseResult result = SQLiteDatabaseExecute(database, makeString("INSERT INTO "_s, m_tableName, " (id, rule) VALUES (?, ?)"_s), ruleID, ruleData);
    if (result != SQLITE_DONE) {
        RELEASE_LOG_ERROR(Extensions, "Failed to insert %s rule for extension %s", utf8(toString(m_storageType)), utf8(uniqueIdentifier()));
        return makeString("Failed to add "_s, toString(m_storageType), " rule."_s);
    }

    return nullString();
}

// MARK: Database Schema

SchemaVersion WebExtensionDeclarativeNetRequestSQLiteStore::currentDatabaseSchemaVersion()
{
    return currentDeclarativeNetRequestSchemaVersion;
}

URL WebExtensionDeclarativeNetRequestSQLiteStore::databaseURL()
{
    if (useInMemoryDatabase())
        return WebExtensionSQLiteDatabase::inMemoryDatabaseURL();

    ASSERT(m_storageType == WebExtensionDeclarativeNetRequestStorageType::Dynamic);

    RefPtr<StringImpl> databaseName = "DeclarativeNetRequestRules.db"_si;

    ASSERT(!directory().isEmpty());

    return URL(URL { makeString(directory().string(), "/"_s) }, databaseName);
}

DatabaseResult WebExtensionDeclarativeNetRequestSQLiteStore::createFreshDatabaseSchema()
{
    assertIsCurrent(queue());
    ASSERT(database());

    DatabaseResult result = SQLiteDatabaseExecute(*database(), makeString("CREATE TABLE "_s, m_tableName, " (id INTEGER PRIMARY KEY NOT NULL, rule BLOB NOT NULL)"_s));
    if (result != SQLITE_DONE)
        RELEASE_LOG_ERROR(Extensions, "Failed to create %s database for extension %s: %s (%d)", utf8(m_tableName), utf8(uniqueIdentifier()), lastErrorMessage(), result);
    return result;
}

SchemaVersion WebExtensionDeclarativeNetRequestSQLiteStore::migrateToCurrentSchemaVersionIfNeeded()
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

        setDatabaseSchemaVersion(currentDeclarativeNetRequestSchemaVersion);
        return currentDeclarativeNetRequestSchemaVersion;
    }
    return WebExtensionSQLiteStore::migrateToCurrentSchemaVersionIfNeeded();
}

DatabaseResult WebExtensionDeclarativeNetRequestSQLiteStore::resetDatabaseSchema()
{
    assertIsCurrent(queue());
    ASSERT(database());

    DatabaseResult result = SQLiteDatabaseExecute(*database(), makeString("DROP TABLE IF EXISTS "_s, m_tableName));
    if (result != SQLITE_DONE)
        RELEASE_LOG_ERROR(Extensions, "Failed to reset %s database schema for extension %s: %s (%d)", utf8(m_tableName), utf8(uniqueIdentifier()), lastErrorMessage(), result);

    return result;
}

bool WebExtensionDeclarativeNetRequestSQLiteStore::isDatabaseEmpty()
{
    assertIsCurrent(queue());
    ASSERT(database());

    RefPtr rows = SQLiteDatabaseFetch(*database(), makeString("SELECT COUNT(*) FROM "_s, m_tableName));
    if (RefPtr row = rows->next())
        return !row->getInt64(0);
    return true;
}

#endif // ENABLE(WK_WEB_EXTENSIONS)

} // namespace WebKit
