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

#pragma once

#include "WebExtensionSQLiteRow.h"
#include "WebExtensionSQLiteStore.h"
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/WeakPtr.h>

namespace WebKit {

enum class WebExtensionDeclarativeNetRequestStorageType : uint8_t {
    Dynamic,
    Session
};

class WebExtensionDeclarativeNetRequestSQLiteStore final : public WebExtensionSQLiteStore {
    WTF_MAKE_TZONE_ALLOCATED(WebExtensionDeclarativeNetRequestSQLiteStore);

public:
    template<typename... Args>
    static Ref<WebExtensionDeclarativeNetRequestSQLiteStore> create(Args&&... args)
    {
        return adoptRef(*new WebExtensionDeclarativeNetRequestSQLiteStore(std::forward<Args>(args)...));
    }
    virtual ~WebExtensionDeclarativeNetRequestSQLiteStore() = default;

    enum class UsesInMemoryDatabase : bool {
        No = false,
        Yes = true,
    };

    void getRulesWithRuleIDs(Vector<double> ruleIDs, CompletionHandler<void(RefPtr<JSON::Array> rules, const String& errorMessage)>&&);
    void updateRulesByRemovingIDs(Vector<double> ruleIDs, Ref<JSON::Array> rules, CompletionHandler<void(const String& errorMessage)>&&);

    void addRules(Ref<JSON::Array> rules, CompletionHandler<void(const String& errorMessage)>&&);
    void deleteRules(Vector<double> ruleIDs, CompletionHandler<void(const String& errorMessage)>&&);

protected:
    SchemaVersion migrateToCurrentSchemaVersionIfNeeded() override;

    DatabaseResult createFreshDatabaseSchema() override;
    DatabaseResult resetDatabaseSchema() override;
    bool isDatabaseEmpty() override;
    SchemaVersion currentDatabaseSchemaVersion() override;
    URL databaseURL() override;

private:
    WebExtensionDeclarativeNetRequestSQLiteStore(const String& uniqueIdentifier, WebExtensionDeclarativeNetRequestStorageType, const String& directory, UsesInMemoryDatabase useInMemoryDatabase);

    RefPtr<JSON::Array> getRulesWithRuleIDsInternal(Vector<double> ruleIDs, String& errorMessage);
    Ref<JSON::Array> getKeysAndValuesFromRowIterator(Ref<WebExtensionSQLiteRowEnumerator> rows);
    String insertRule(const JSON::Object& rule, Ref<WebExtensionSQLiteDatabase>);

    WebExtensionDeclarativeNetRequestStorageType m_storageType;
    String m_tableName;

    void migrateData();
};

} // namespace WebKit
