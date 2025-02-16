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

enum class WebExtensionDataType : uint8_t;

class WebExtensionStorageSQLiteStore final : public WebExtensionSQLiteStore {
    WTF_MAKE_TZONE_ALLOCATED(WebExtensionStorageSQLiteStore);

public:
    template<typename... Args>
    static Ref<WebExtensionStorageSQLiteStore> create(Args&&... args)
    {
        return adoptRef(*new WebExtensionStorageSQLiteStore(std::forward<Args>(args)...));
    }
    virtual ~WebExtensionStorageSQLiteStore() = default;

    void getAllKeys(CompletionHandler<void(Vector<String> keys, const String& errorMessage)>&&);
    void getValuesForKeys(Vector<String> keys, CompletionHandler<void(HashMap<String, String> results, const String& errorMessage)>&&);
    void getStorageSizeForKeys(Vector<String> keys, CompletionHandler<void(size_t storageSize, const String& errorMessage)>&&);
    void getStorageSizeForAllKeys(HashMap<String, String> additionalKeyedData, CompletionHandler<void(size_t storageSize, int numberOfKeysIncludingAdditionalKeyedData, HashMap<String, String> existingKeysAndValues, const String& errorMessage)>&&);
    void setKeyedData(HashMap<String, String> keyedData, CompletionHandler<void(Vector<String> keysSuccessfullySet, const String& errorMessage)>&&);
    void deleteValuesForKeys(Vector<String> keys, CompletionHandler<void(const String& errorMessage)>&&);

    enum class UsesInMemoryDatabase : bool {
        No = false,
        Yes = true,
    };

protected:
    SchemaVersion migrateToCurrentSchemaVersionIfNeeded();

    DatabaseResult createFreshDatabaseSchema() override;
    DatabaseResult resetDatabaseSchema() override;
    bool isDatabaseEmpty() override;
    SchemaVersion currentDatabaseSchemaVersion() override;
    URL databaseURL() override;

private:
    WebExtensionStorageSQLiteStore(const String& uniqueIdentifier, WebExtensionDataType storageType, const String& directory, UsesInMemoryDatabase useInMemoryDatabase);

    String insertOrUpdateValue(const String& value, const String& key, Ref<WebExtensionSQLiteDatabase>);
    HashMap<String, String> getValuesForAllKeys(String& errorMessage);
    HashMap<String, String> getValuesForKeysWithErrorMessage(Vector<String> keys, String& errorMessage);
    HashMap<String, String> getKeysAndValuesFromRowIterator(Ref<WebExtensionSQLiteRowEnumerator> rows);
    Vector<String> getAllKeysWithErrorMessage(String& errorMessage);

    WebExtensionDataType m_storageType;
};

} // namespace WebKit
