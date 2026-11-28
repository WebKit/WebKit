/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Igalia S.L. All rights reserved.
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
#include "WebExtensionAPIStorageArea.h"

#include "WebExtensionAPIKeys.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include "APIData.h"
#include "APIObject.h"
#include "MessageSenderInlines.h"
#include "WebExtensionConstants.h"
#include "WebExtensionContextMessages.h"
#include "WebExtensionContextProxy.h"
#include "WebExtensionStorageAccessLevel.h"
#include "WebExtensionUtilities.h"
#include "WebProcess.h"
#include <wtf/Borrow.h>

namespace WebKit {

bool WebExtensionAPIStorageArea::isPropertyAllowed(const ASCIILiteral& propertyName, WebPage*)
{
    if (protect(extensionContext())->isUnsupportedAPI(propertyPath(), propertyName)) [[unlikely]]
        return false;

    static NeverDestroyed<HashSet<AtomString>> syncStorageProperties { HashSet { AtomString("QUOTA_BYTES_PER_ITEM"_s), AtomString("MAX_ITEMS"_s), AtomString("MAX_WRITE_OPERATIONS_PER_HOUR"_s), AtomString("MAX_WRITE_OPERATIONS_PER_MINUTE"_s) } };

    if (syncStorageProperties.get().contains(propertyName))
        return m_type == WebExtensionDataType::Sync;

    if (propertyName == "setAccessLevel"_s)
        return isForTrustedContext();

    ASSERT_NOT_REACHED();
    return false;
}

void WebExtensionAPIStorageArea::get(WebPageProxyIdentifier webPageProxyIdentifier, RefPtr<JSON::Value> items, Ref<WebExtensionCallbackHandler>&& callback, String& outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/storage/StorageArea/get

    if (!items)
        items = JSON::Value::null();

    if (!validateObject(items, "items"_s, {
        JSON::Value::Type::Object,
        JSON::Value::Type::String,
        JSON::Value::Type::Array,
        JSON::Value::Type::Null
    }, { JSON::Value::Type::String }, outExceptionString))
        return;

    Vector<String> keysVector;

    auto keysWithDefaultValues = items->asObject();
    if (keysWithDefaultValues) {
        if (!keysWithDefaultValues->size()) {
            callback->call(JSObjectMake(callback->globalContext(), nullptr, nullptr));
            return;
        }

        for (const auto& key : keysWithDefaultValues->keys())
            keysVector.append(key);
    }

    auto keys = items->asArray();
    if (keys) {
        if (!keys->length()) {
            callback->call(JSObjectMake(callback->globalContext(), nullptr, nullptr));
            return;
        }

        keysVector = makeStringVector(*keys);
    }

    String key;
    if (items->asString(key))
        keysVector = { key };

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::StorageGet(webPageProxyIdentifier, m_type, keysVector), [keysWithDefaultValues, protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<Vector<String>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        RefPtr deserializedData = JSON::Object::create();
        for (auto& serializedJSON : borrow(result.value())) {
            RefPtr data = JSON::Value::parseJSON(serializedJSON);
            if (RefPtr dataObject = data ? data->asObject() : nullptr; dataObject) {
                RefPtr mappedData = JSON::Object::create();
                for (const auto& stringValue : *dataObject) {
                    auto key = stringValue.key;
                    auto value = stringValue.value;
                    if (RefPtr parsed = JSON::Value::parseJSON(value->asString()); parsed)
                        mappedData->setValue(key, *parsed);
                }

                deserializedData = mergeJSON(mappedData, deserializedData);
            }
        }

        if (keysWithDefaultValues)
            deserializedData = mergeJSON(deserializedData, keysWithDefaultValues);

        callback->call(fromJSON(callback->globalContext(), deserializedData->asValue()));
    }, extensionContext().identifier());
}

void WebExtensionAPIStorageArea::getKeys(WebPageProxyIdentifier webPageProxyIdentifier, Ref<WebExtensionCallbackHandler>&& callback, String& outExceptionString)
{
    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::StorageGetKeys(webPageProxyIdentifier, m_type), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<Vector<String>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call(fromArray(callback->globalContext(), WTF::move(result.value())));
    }, extensionContext().identifier());
}

void WebExtensionAPIStorageArea::getBytesInUse(WebPageProxyIdentifier webPageProxyIdentifier, RefPtr<JSON::Value> keys, Ref<WebExtensionCallbackHandler>&& callback, String& outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/storage/StorageArea/getBytesInUse

    if (!keys)
        keys = JSON::Value::null();

    if (keys && !validateObject(keys, "keys"_s, {
        JSON::Value::Type::String,
        JSON::Value::Type::Array,
        JSON::Value::Type::Null,
    }, { JSON::Value::Type::String }, outExceptionString))
        return;

    Vector<String> keysVector;
    if (auto keysArray = keys->asArray(); keysArray)
        keysVector = makeStringVector(*keysArray);
    else if (String key; keys->asString(key))
        keysVector = { key };

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::StorageGetBytesInUse(webPageProxyIdentifier, m_type, keysVector), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<uint64_t, WebExtensionError>&& result) {
        if (!result)
            callback->reportError(result.error());
        else
            callback->call(JSValueMakeNumber(callback->globalContext(), result.value()));
    }, extensionContext().identifier());
}

void WebExtensionAPIStorageArea::set(WebPageProxyIdentifier webPageProxyIdentifier, JSContextRef context, JSValueRef items, Ref<WebExtensionCallbackHandler>&& callback, String& outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/storage/StorageArea/set

    if (!context || !items) {
        callback->call();
        return;
    }

    if (JSValueIsArray(context, items)) {
        outExceptionString = toErrorString(nullString(), "items"_s, "an object is expected"_s);
        return;
    }

    JSValueRef exception = nullptr;
    JSObjectRef object = JSValueToObject(context, items, &exception);
    if (!object || exception) {
        callback->call();
        return;
    }

    RefPtr<JSON::Object> serializedData = JSON::Object::create();
    JSPropertyNameArrayRef propertyNames = JSObjectCopyPropertyNames(context, object);
    size_t propertyNameCount = JSPropertyNameArrayGetCount(propertyNames);
    String keyWithError = nullString();
    for (size_t i = 0; i < propertyNameCount; ++i) {
        RefPtr propertyName = JSPropertyNameArrayGetNameAtIndex(propertyNames, i);
        if (!propertyName)
            continue;

        JSValueRef item = JSObjectGetProperty(context, object, propertyName.get(), 0);

        auto key = toString(propertyName.get());
        if (JSValueIsUndefined(context, item))
            continue;

        if (!keyWithError.isEmpty())
            continue;

        auto result = toJSONString(context, item);
        if (result.isEmpty()) {
            keyWithError = key;
            continue;
        }

        serializedData->setString(key, result);
    }

    JSPropertyNameArrayRelease(propertyNames);

    if (!keyWithError.isEmpty()) {
        outExceptionString = toErrorString(nullString(), makeString("items[`"_s, keyWithError, "`]"_s), "it is not JSON-serializable"_s);
        return;
    }

    if (m_type == WebExtensionDataType::Sync && anyItemsExceedQuota(serializedData, webExtensionStorageAreaSyncQuotaBytesPerItem, keyWithError)) {
        outExceptionString = toErrorString(nullString(), makeString("items[`"_s, keyWithError, "`]"_s), "it exceeded maximum size for a single item"_s);
        return;
    }

    if (storageSizeOf(serializedData) > storageMaximumBytesPerCall(extensionContext().inTestingMode())) {
        outExceptionString = toErrorString(nullString(), nullString(), "it exceeded maximum data size allowed per call"_s);
        return;
    }

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::StorageSet(webPageProxyIdentifier, m_type, serializedData->toJSONString()), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<void, WebExtensionError>&& result) {
        if (!result)
            callback->reportError(result.error());
        else
            callback->call();
    }, extensionContext().identifier());
}

void WebExtensionAPIStorageArea::remove(WebPageProxyIdentifier webPageProxyIdentifier, RefPtr<JSON::Value> keys, Ref<WebExtensionCallbackHandler>&& callback, String& outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/storage/StorageArea/remove

    if (!validateObject(keys, "keys"_s, {
        JSON::Value::Type::String,
        JSON::Value::Type::Array
    }, { JSON::Value::Type::String }, outExceptionString))
        return;

    Vector<String> keysVector;
    if (RefPtr array = keys->asArray(); array)
        keysVector = makeStringVector(*array);
    else if (String key; keys->asString(key))
        keysVector = { key };

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::StorageRemove(webPageProxyIdentifier, m_type, keysVector), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<void, WebExtensionError>&& result) {
        if (!result)
            callback->reportError(result.error());
        else
            callback->call();
    }, extensionContext().identifier());
}

void WebExtensionAPIStorageArea::clear(WebPageProxyIdentifier webPageProxyIdentifier, Ref<WebExtensionCallbackHandler>&& callback)
{
    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::StorageClear(webPageProxyIdentifier, m_type), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<void, WebExtensionError>&& result) {
        if (!result)
            callback->reportError(result.error());
        else
            callback->call();
    }, extensionContext().identifier());
}

void WebExtensionAPIStorageArea::setAccessLevel(WebPageProxyIdentifier webPageProxyIdentifier, RefPtr<JSON::Value> accessOptions, Ref<WebExtensionCallbackHandler>&& callback, String& outExceptionString)
{
    if (!validateDictionary(accessOptions, "accessOptions"_s, { accessLevelKey }, {
        { accessLevelKey, JSON::Value::Type::String },
    }, outExceptionString))
        return;

    auto accessObject = accessOptions->asObject();
    if (!accessObject)
        return;

    String accessLevelString = accessObject->getString(accessLevelKey);
    if ((accessLevelString != accessLevelTrustedContexts) && (accessLevelString != accessLevelTrustedAndUntrustedContexts)) {
        outExceptionString = toErrorString(nullString(), "accessLevel"_s, "it must specify either 'TRUSTED_CONTEXTS' or 'TRUSTED_AND_UNTRUSTED_CONTEXTS'"_s);
        return;
    }

    WebExtensionStorageAccessLevel accessLevel = (accessLevelString == accessLevelTrustedContexts) ? WebExtensionStorageAccessLevel::TrustedContexts : WebExtensionStorageAccessLevel::TrustedAndUntrustedContexts;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::StorageSetAccessLevel(webPageProxyIdentifier, m_type, accessLevel), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<void, WebExtensionError>&& result) {
        if (!result)
            callback->reportError(result.error());
        else
            callback->call();
    }, extensionContext().identifier());
}

WebExtensionAPIEvent& WebExtensionAPIStorageArea::onChanged()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/storage/StorageArea/onChanged

    if (!m_onChanged)
        lazyInitialize(m_onChanged, WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::StorageOnChanged));

    return *m_onChanged;
}

double WebExtensionAPIStorageArea::quotaBytes()
{
    switch (m_type) {
    case WebExtensionDataType::Local:
        return webExtensionStorageAreaLocalQuotaBytes;
    case WebExtensionDataType::Sync:
        return webExtensionStorageAreaSyncQuotaBytes;
    case WebExtensionDataType::Session:
        return webExtensionStorageAreaSessionQuotaBytes;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

double WebExtensionAPIStorageArea::quotaBytesPerItem()
{
    ASSERT(m_type == WebExtensionDataType::Sync);
    return webExtensionStorageAreaSyncQuotaBytesPerItem;
}

double WebExtensionAPIStorageArea::maxItems()
{
    ASSERT(m_type == WebExtensionDataType::Sync);
    return webExtensionStorageAreaSyncMaximumItems;
}

double WebExtensionAPIStorageArea::maxWriteOperationsPerHour()
{
    ASSERT(m_type == WebExtensionDataType::Sync);
    return webExtensionStorageAreaSyncMaximumWriteOperationsPerHour;
}

double WebExtensionAPIStorageArea::maxWriteOperationsPerMinute()
{
    ASSERT(m_type == WebExtensionDataType::Sync);
    return webExtensionStorageAreaSyncMaximumWriteOperationsPerMinute;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
