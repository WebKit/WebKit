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

namespace WebKit {

class WebExtensionRegisteredScriptsSQLiteStore final : public WebExtensionSQLiteStore {
    WTF_MAKE_TZONE_ALLOCATED(WebExtensionRegisteredScriptsSQLiteStore);

public:
    template<typename... Args>
    static Ref<WebExtensionRegisteredScriptsSQLiteStore> create(Args&&... args)
    {
        return adoptRef(*new WebExtensionRegisteredScriptsSQLiteStore(std::forward<Args>(args)...));
    }
    virtual ~WebExtensionRegisteredScriptsSQLiteStore() = default;

    void addScripts(Vector<Ref<JSON::Object>> scripts, CompletionHandler<void(const String& errorMessage)>&&);
    void deleteScriptsWithIDs(Vector<String> ids, CompletionHandler<void(const String& errorMessage)>&&);
    void getScripts(CompletionHandler<void(Vector<Ref<JSON::Object>> scripts, const String& errorMessage)>&&);
    void updateScripts(Vector<Ref<JSON::Object>> scripts, CompletionHandler<void(const String& errorMessage)>&&);

protected:
    SchemaVersion migrateToCurrentSchemaVersionIfNeeded() override;

    DatabaseResult createFreshDatabaseSchema() override;
    DatabaseResult resetDatabaseSchema() override;
    bool isDatabaseEmpty() override;
    SchemaVersion currentDatabaseSchemaVersion() override;
    URL databaseURL() override;

private:
    WebExtensionRegisteredScriptsSQLiteStore(const String& uniqueIdentifier, const String& directory, bool useInMemoryDatabase);

    Vector<Ref<JSON::Object>> getScriptsWithErrorMessage(String& errorMessage);
    Vector<Ref<JSON::Object>> getKeysAndValuesFromRowIterator(Ref<WebExtensionSQLiteRowEnumerator> rows);
    void insertScript(const JSON::Object& script, Ref<WebExtensionSQLiteDatabase>, String& errorMessage);

    void migrateData();
};

} // namespace WebKit
