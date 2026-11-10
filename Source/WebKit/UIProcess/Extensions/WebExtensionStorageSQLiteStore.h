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

    void getAllKeys(CompletionHandler<void(Vector<RefPtr<StringImpl>> keys, StringImpl* errorMessage)>&&);
    void getValuesForKeys(Vector<RefPtr<StringImpl>> keys, CompletionHandler<void(HashMap<RefPtr<StringImpl>, RefPtr<StringImpl>> results, StringImpl* errorMessage)>&&);
    void getStorageSizeForKeys(Vector<RefPtr<StringImpl>> keys, CompletionHandler<void(size_t storageSize, StringImpl* errorMessage)>&&);
    void getStorageSizeForAllKeys(HashMap<RefPtr<StringImpl>, RefPtr<StringImpl>> additionalKeyedData, CompletionHandler<void(size_t storageSize, int numberOfKeysIncludingAdditionalKeyedData, HashMap<RefPtr<StringImpl>, RefPtr<StringImpl>> existingKeysAndValues, StringImpl* errorMessage)>&&);
    void setKeyedData(HashMap<RefPtr<StringImpl>, RefPtr<StringImpl>> keyedData, CompletionHandler<void(Vector<RefPtr<StringImpl>> keysSuccessfullySet, StringImpl* errorMessage)>&&);
    void deleteValuesForKeys(Vector<RefPtr<StringImpl>> keys, CompletionHandler<void(StringImpl* errorMessage)>&&);

    enum class UsesInMemoryDatabase : bool {
        No = false,
        Yes = true,
    };

protected:
    SchemaVersion migrateToCurrentSchemaVersionIfNeeded() override;

    DatabaseResult createFreshDatabaseSchema() override;
    DatabaseResult resetDatabaseSchema() override;
    bool isDatabaseEmpty() override;
    SchemaVersion currentDatabaseSchemaVersion() override;
    URL databaseURL() override;

private:
    WebExtensionStorageSQLiteStore(StringImpl* uniqueIdentifier, WebExtensionDataType storageType, StringImpl* directory, UsesInMemoryDatabase useInMemoryDatabase);

    RefPtr<StringImpl> insertOrUpdateValue(StringImpl* value, StringImpl* key, Ref<WebExtensionSQLiteDatabase>);
    HashMap<RefPtr<StringImpl>, RefPtr<StringImpl>> getValuesForAllKeys(RefPtr<StringImpl>& errorMessage);
    HashMap<RefPtr<StringImpl>, RefPtr<StringImpl>> getValuesForKeysWithErrorMessage(Vector<RefPtr<StringImpl>> keys, RefPtr<StringImpl>& errorMessage);
    HashMap<RefPtr<StringImpl>, RefPtr<StringImpl>> getKeysAndValuesFromRowIterator(Ref<WebExtensionSQLiteRowEnumerator> rows);
    Vector<RefPtr<StringImpl>> getAllKeysWithErrorMessage(RefPtr<StringImpl>& errorMessage);

    WebExtensionDataType m_storageType;
};

} // namespace WebKit
