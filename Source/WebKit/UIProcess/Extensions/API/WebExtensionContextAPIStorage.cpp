/*
 * Copyright (C) 2024-2026 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Igalia S.L. All rights reserved.
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
#include "WebExtensionContext.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include "WebExtensionConstants.h"
#include "WebExtensionContextProxy.h"
#include "WebExtensionContextProxyMessages.h"
#include "WebExtensionDataType.h"
#include "WebExtensionPermission.h"
#include "WebExtensionStorageAccessLevel.h"
#include "WebExtensionStorageSQLiteStore.h"
#include "WebExtensionUtilities.h"
#include <wtf/text/MakeString.h>

namespace WebKit {

static WorkQueue& extensionStorageQueue()
{
    static NeverDestroyed<Ref<WorkQueue>> queue = WorkQueue::create("com.apple.WebKit.WebExtensionContextAPIStorage"_s);
    return queue.get().get();
}

bool WebExtensionContext::isStorageMessageAllowed(IPC::Decoder& message)
{
    return isLoaded() && (hasPermission(WebExtensionPermission::storage()) || hasPermission(WebExtensionPermission::unlimitedStorage()));
}

void WebExtensionContext::storageGet(WebPageProxyIdentifier webPageProxyIdentifier, WebExtensionDataType dataType, const Vector<String>& keys, CompletionHandler<void(Expected<Vector<String>, WebExtensionError>&&)>&& completionHandler)
{
    auto callingAPIName = makeString("browser.storage."_s, toAPIString(dataType), ".get()"_s);

    Ref storage = storageForType(dataType);
    storage->getValuesForKeys(keys, [callingAPIName, completionHandler = WTF::move(completionHandler)](HashMap<String, String> values, const String& errorMessage) mutable {
        if (!errorMessage.isEmpty())
            completionHandler(toWebExtensionError(callingAPIName, nullString(), errorMessage));
        else {
            Ref jsonObject = JSON::Object::create();
            for (auto entry : values)
                jsonObject->setString(entry.key, entry.value);

            Vector<String> serializedJSONStrings;
            serializeToMultipleJSONStrings(jsonObject, [&](String&& jsonChunk) {
                serializedJSONStrings.append(WTF::move(jsonChunk));
            });
            completionHandler(serializedJSONStrings);
        }
    });
}

void WebExtensionContext::storageGetKeys(WebPageProxyIdentifier webPageProxyIdentifier, WebExtensionDataType dataType, CompletionHandler<void(Expected<Vector<String>, WebExtensionError>&&)>&& completionHandler)
{
    auto callingAPIName = makeString("browser.storage."_s, toAPIString(dataType), ".getKeys()"_s);

    Ref storage = storageForType(dataType);
    storage->getAllKeys([callingAPIName, completionHandler = WTF::move(completionHandler)](Vector<String> keys, const String& errorMessage) mutable {
        if (!errorMessage.isEmpty())
            completionHandler(toWebExtensionError(callingAPIName, nullString(), errorMessage));
        else
            completionHandler(keys);
    });
}

void WebExtensionContext::storageGetBytesInUse(WebPageProxyIdentifier webPageProxyIdentifier, WebExtensionDataType dataType, const Vector<String>& keys, CompletionHandler<void(Expected<uint64_t, WebExtensionError>&&)>&& completionHandler)
{
    auto callingAPIName = makeString("browser.storage."_s, toAPIString(dataType), ".getBytesInUse()"_s);

    Ref storage = storageForType(dataType);
    storage->getStorageSizeForKeys(keys, [callingAPIName, completionHandler = WTF::move(completionHandler)](size_t size, const String& errorMessage) mutable {
        if (!errorMessage.isEmpty())
            completionHandler(toWebExtensionError(callingAPIName, nullString(), errorMessage));
        else
            completionHandler(size);
    });
}

void WebExtensionContext::storageSet(WebPageProxyIdentifier webPageProxyIdentifier, WebExtensionDataType dataType, const String& dataJSON, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    auto callingAPIName = makeString("browser.storage."_s, toAPIString(dataType), ".set()"_s);

    HashMap<String, String> data = { };
    if (RefPtr json = JSON::Value::parseJSON(dataJSON)) {
        if (RefPtr object = json->asObject()) {
            for (const auto& key : object->keys())
                data.set(key, object->getString(key));
        }
    }

    Ref storage = storageForType(dataType);
    storage->getStorageSizeForAllKeys(data, [this, protectedThis = Ref { *this }, callingAPIName, dataType, data = WTF::move(data), completionHandler = WTF::move(completionHandler)](size_t size, int numberOfKeys, HashMap<String, String> existingKeysAndValues, const String& errorMessage) mutable {
        if (!errorMessage.isEmpty()) {
            completionHandler(toWebExtensionError(callingAPIName, nullString(), errorMessage));
            return;
        }

        if (size > quotaForStorageType(dataType)) {
            completionHandler(toWebExtensionError(callingAPIName, nullString(), "exceeded storage quota"_s));
            return;
        }

        if (dataType == WebExtensionDataType::Sync && static_cast<size_t>(numberOfKeys) > webExtensionStorageAreaSyncMaximumItems) {
            completionHandler(toWebExtensionError(callingAPIName, nullString(), "exceeded maximum number of items"_s));
            return;
        }

        Ref storage = storageForType(dataType);
        storage->setKeyedData(data, [this, protectedThis = Ref { *this }, callingAPIName, data, dataType, existingKeysAndValues = WTF::move(existingKeysAndValues), completionHandler = WTF::move(completionHandler)](Vector<String> keysSuccessfullySet, const String& errorMessage) mutable {
            if (!errorMessage.isEmpty())
                completionHandler(toWebExtensionError(callingAPIName, nullString(), errorMessage));
            else
                completionHandler({ });

            // Only fire an onChanged event for the keys that were successfully set.
            if (!keysSuccessfullySet.size())
                return;

            if (keysSuccessfullySet.size() != data.size()) {
                for (const auto& key : data.keys()) {
                    if (!keysSuccessfullySet.contains(key))
                        data.remove(key);
                }
            }

            fireStorageChangedEventIfNeeded(existingKeysAndValues, data, dataType);
        });
    });
}

void WebExtensionContext::storageRemove(WebPageProxyIdentifier webPageProxyIdentifier, WebExtensionDataType dataType, const Vector<String>& keys, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    auto callingAPIName = makeString("browser.storage."_s, toAPIString(dataType), ".remove()"_s);

    Ref storage = storageForType(dataType);
    storage->getValuesForKeys(keys, [this, protectedThis = Ref { *this }, callingAPIName, keys, dataType, completionHandler = WTF::move(completionHandler)](HashMap<String, String> oldValuesAndKeys, const String& errorMessage) mutable {
        if (!errorMessage.isEmpty()) {
            completionHandler(toWebExtensionError(callingAPIName, nullString(), errorMessage));
            return;
        }

        Ref storage = storageForType(dataType);
        storage->deleteValuesForKeys(keys, [this, protectedThis = Ref { *this }, callingAPIName, dataType, oldValuesAndKeys = WTF::move(oldValuesAndKeys), completionHandler = WTF::move(completionHandler)](const String& errorMessage) mutable {
            if (!errorMessage.isEmpty()) {
                completionHandler(toWebExtensionError(callingAPIName, nullString(), errorMessage));
                return;
            }

            fireStorageChangedEventIfNeeded(oldValuesAndKeys, { }, dataType);

            completionHandler({ });
        });
    });
}

void WebExtensionContext::storageClear(WebPageProxyIdentifier webPageProxyIdentifier, WebExtensionDataType dataType, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    auto callingAPIName = makeString("browser.storage."_s, toAPIString(dataType), ".clear()"_s);

    Ref storage = storageForType(dataType);
    storage->getValuesForKeys({ }, [this, protectedThis = Ref { *this }, callingAPIName, dataType, completionHandler = WTF::move(completionHandler)](HashMap<String, String> oldValuesAndKeys, const String& errorMessage) mutable {
        if (!errorMessage.isEmpty()) {
            completionHandler(toWebExtensionError(callingAPIName, nullString(), errorMessage));
            return;
        }

        Ref storage = storageForType(dataType);
        storage->deleteDatabase([this, protectedThis = Ref { *this }, callingAPIName, oldValuesAndKeys = WTF::move(oldValuesAndKeys), dataType, completionHandler = WTF::move(completionHandler)](const String& errorMessage) mutable {
            if (!errorMessage.isEmpty()) {
                completionHandler(toWebExtensionError(callingAPIName, nullString(), errorMessage));
                return;
            }

            fireStorageChangedEventIfNeeded(oldValuesAndKeys, { }, dataType);

            completionHandler({ });
        });
    });
}

void WebExtensionContext::storageSetAccessLevel(WebPageProxyIdentifier webPageProxyIdentifier, WebExtensionDataType dataType, const WebExtensionStorageAccessLevel accessLevel, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    setSessionStorageAllowedInContentScripts(accessLevel == WebExtensionStorageAccessLevel::TrustedAndUntrustedContexts);

    completionHandler({ });
}

void WebExtensionContext::fireStorageChangedEventIfNeeded(HashMap<String, String> oldKeysAndValues, HashMap<String, String> newKeysAndValues, WebExtensionDataType dataType)
{
    static constexpr auto newValueKey = "newValue"_s;
    static constexpr auto oldValueKey = "oldValue"_s;

    if (!oldKeysAndValues.size() && !newKeysAndValues.size())
        return;

    // Dispatch this work to a background thread to avoid hangs on the main thread. See https://webkit.org/b/305347.
    Ref queue = extensionStorageQueue();
    queue->dispatch([this, protectedThis = Ref { *this }, oldKeysAndValues = crossThreadCopy(WTF::move(oldKeysAndValues)), newKeysAndValues = crossThreadCopy(WTF::move(newKeysAndValues)), dataType]() mutable {
        Ref changedData = JSON::Object::create();

        // Process new or changed keys
        for (auto& entry : newKeysAndValues) {
            const auto& key = entry.key;
            const auto& value = entry.value;

            String oldValue = oldKeysAndValues.get(key);

            if (oldValue.isEmpty() || oldValue != value) {
                RefPtr parsedNewValue = JSON::Value::parseJSON(value);
                RefPtr parsedOldValue = JSON::Value::parseJSON(oldValue);
                Ref data = JSON::Object::create();
                if (parsedOldValue)
                    data->setValue(oldValueKey, *parsedOldValue);
                if (parsedNewValue)
                    data->setValue(newValueKey, *parsedNewValue);
                changedData->setObject(key, data);
            }
        }

        // Process removed keys.
        for (auto& entry : oldKeysAndValues) {
            const auto& key = entry.key;
            const auto& value = entry.value;

            if (!newKeysAndValues.contains(key)) {
                RefPtr parsedNewValue = JSON::Value::parseJSON(value);
                Ref data = JSON::Object::create();
                if (parsedNewValue)
                    data->setValue(oldValueKey, *parsedNewValue);
                changedData->setObject(key, data);
            }
        }

        if (!changedData->size())
            return;

        constexpr auto type = WebExtensionEventListenerType::StorageOnChanged;

        Vector<String> serializedJSONStrings;
        serializeToMultipleJSONStrings(changedData, [&](String&& jsonChunk) {
            serializedJSONStrings.append(WTF::move(jsonChunk));
        });

        WorkQueue::mainSingleton().dispatch([this, protectedThis = Ref { *this }, serializedJSONStrings = crossThreadCopy(WTF::move(serializedJSONStrings)), dataType]() mutable {
            // Unlike other extension events which are only dispatched to the web process that hosts all the extension-related web views (background page, popup, full page extension content),
            // content scripts are allowed to listen to storage.onChanged events.
            sendToContentScriptProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchStorageChangedEvent(serializedJSONStrings, dataType, WebExtensionContentWorldType::ContentScript));

            wakeUpBackgroundContentIfNecessaryToFireEvents({ type }, [=, this, protectedThis = Ref { *this }] {
                sendToProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchStorageChangedEvent(serializedJSONStrings, dataType, WebExtensionContentWorldType::Main));
            });
        });
    });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)

