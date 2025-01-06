/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WebExtensionAPIStorageArea.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "APIData.h"
#import "APIObject.h"
#import "CocoaHelpers.h"
#import "MessageSenderInlines.h"
#import "WebExtensionConstants.h"
#import "WebExtensionContextMessages.h"
#import "WebExtensionContextProxy.h"
#import "WebExtensionUtilities.h"
#import "WebProcess.h"
#import <wtf/cocoa/VectorCocoa.h>

static NSString * const accessLevelKey = @"accessLevel";
static NSString * const accessLevelTrustedContexts = @"TRUSTED_CONTEXTS";
static NSString * const accessLevelTrustedAndUntrustedContexts = @"TRUSTED_AND_UNTRUSTED_CONTEXTS";

namespace WebKit {

bool WebExtensionAPIStorageArea::isPropertyAllowed(const ASCIILiteral& propertyName, WebPage*)
{
    if (UNLIKELY(extensionContext().isUnsupportedAPI(propertyPath(), propertyName)))
        return false;

    static NeverDestroyed<HashSet<AtomString>> syncStorageProperties { HashSet { AtomString("QUOTA_BYTES_PER_ITEM"_s), AtomString("MAX_ITEMS"_s), AtomString("MAX_WRITE_OPERATIONS_PER_HOUR"_s), AtomString("MAX_WRITE_OPERATIONS_PER_MINUTE"_s) } };

    if (syncStorageProperties.get().contains(propertyName))
        return m_type == WebExtensionDataType::Sync;

    if (propertyName == "setAccessLevel"_s)
        return m_type == WebExtensionDataType::Session;

    ASSERT_NOT_REACHED();
    return false;
}

void WebExtensionAPIStorageArea::get(WebPageProxyIdentifier webPageProxyIdentifier, id items, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/storage/StorageArea/get

    if (!items)
        items = NSNull.null;

    if (!validateObject(items, @"items", [NSOrderedSet orderedSetWithObjects:NSDictionary.class, NSString.class, @[ NSString.class ], NSNull.class, nil], outExceptionString))
        return;

    Vector<String> keysVector;
    NSDictionary *keysWithDefaultValues = dynamic_objc_cast<NSDictionary>(items);

    if (keysWithDefaultValues) {
        if (!keysWithDefaultValues.count) {
            callback->call(@{ });
            return;
        }

        keysVector = makeVector<String>(keysWithDefaultValues.allKeys);
    }

    if (NSArray *keys = dynamic_objc_cast<NSArray>(items)) {
        if (!keys.count) {
            callback->call(@{ });
            return;
        }

        keysVector = makeVector<String>(keys);
    }

    if (NSString *key = dynamic_objc_cast<NSString>(items))
        keysVector = { key };

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::StorageGet(webPageProxyIdentifier, m_type, keysVector), [keysWithDefaultValues, protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<String, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        NSDictionary *data = parseJSON(result.value());
        NSDictionary<NSString *, id> *deserializedData = mapObjects(data, ^id(NSString *key, NSString *jsonString) {
            return parseJSON(jsonString, JSONOptions::FragmentsAllowed);
        });

        deserializedData = keysWithDefaultValues ? mergeDictionaries(deserializedData, keysWithDefaultValues) : deserializedData;
        callback->call(deserializedData);
    }, extensionContext().identifier());
}

void WebExtensionAPIStorageArea::getKeys(WebPageProxyIdentifier webPageProxyIdentifier, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::StorageGetKeys(webPageProxyIdentifier, m_type), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<Vector<String>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call(createNSArray(result.value()).get());
    }, extensionContext().identifier());
}

void WebExtensionAPIStorageArea::getBytesInUse(WebPageProxyIdentifier webPageProxyIdentifier, id keys, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/storage/StorageArea/getBytesInUse

    if (!keys)
        keys = NSNull.null;

    if (keys && !validateObject(keys, @"keys", [NSOrderedSet orderedSetWithObjects:NSString.class, @[ NSString.class ], NSNull.class, nil], outExceptionString))
        return;

    Vector<String> keysVector;
    if (NSArray *keysArray = dynamic_objc_cast<NSArray>(keys))
        keysVector = makeVector<String>(keysArray);
    else if (NSString *key = dynamic_objc_cast<NSString>(keys))
        keysVector = { key };

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::StorageGetBytesInUse(webPageProxyIdentifier, m_type, keysVector), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<size_t, WebExtensionError>&& result) {
        if (!result)
            callback->reportError(result.error());
        else
            callback->call(@(result.value()));
    }, extensionContext().identifier());
}

void WebExtensionAPIStorageArea::set(WebPageProxyIdentifier webPageProxyIdentifier, NSDictionary *items, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/storage/StorageArea/set

    if (!items.count) {
        callback->call();
        return;
    }

    __block NSString *keyWithError;

    auto *serializedData = mapObjects(items, ^NSString *(NSString *key, id object) {
        ASSERT([object isKindOfClass:JSValue.class]);

        if (((JSValue *)object).isUndefined)
            return nil;

        if (keyWithError)
            return nil;

        auto *result = encodeJSONString(object, JSONOptions::FragmentsAllowed);
        if (!result) {
            keyWithError = key;
            return nil;
        }

        return result;
    });

    if (keyWithError) {
        *outExceptionString = toErrorString(nil, [NSString stringWithFormat:@"items[`%@`]", keyWithError], @"it is not JSON-serializable");
        return;
    }

    if (m_type == WebExtensionDataType::Sync && anyItemsExceedQuota(serializedData, webExtensionStorageAreaSyncQuotaBytesPerItem, &keyWithError)) {
        *outExceptionString = toErrorString(nil, [NSString stringWithFormat:@"items[`%@`]", keyWithError], @"it exceeded maximum size for a single item");
        return;
    }

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::StorageSet(webPageProxyIdentifier, m_type, encodeJSONString(serializedData)), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<void, WebExtensionError>&& result) {
        if (!result)
            callback->reportError(result.error());
        else
            callback->call();
    }, extensionContext().identifier());
}

void WebExtensionAPIStorageArea::remove(WebPageProxyIdentifier webPageProxyIdentifier, id keys, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/storage/StorageArea/remove

    if (!validateObject(keys, @"keys", [NSOrderedSet orderedSetWithObjects:NSString.class, @[ NSString.class ], nil], outExceptionString))
        return;

    Vector<String> keysVector = [keys isKindOfClass:NSArray.class] ? makeVector<String>((NSArray *)keys) : Vector<String> { keys };

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::StorageRemove(webPageProxyIdentifier, m_type, keysVector), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<void, WebExtensionError>&& result) {
        if (!result)
            callback->reportError(result.error());
        else
            callback->call();
    }, extensionContext().identifier());
}

void WebExtensionAPIStorageArea::clear(WebPageProxyIdentifier webPageProxyIdentifier, Ref<WebExtensionCallbackHandler>&& callback)
{
    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::StorageClear(webPageProxyIdentifier, m_type), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<void, WebExtensionError>&& result) {
        if (!result)
            callback->reportError(result.error());
        else
            callback->call();
    }, extensionContext().identifier());
}

void WebExtensionAPIStorageArea::setAccessLevel(WebPageProxyIdentifier webPageProxyIdentifier, NSDictionary *accessOptions, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    static auto *requiredKeys = @[
        accessLevelKey,
    ];

    auto *keyTypes = @{
        accessLevelKey: NSString.class,
    };

    if (!validateDictionary(accessOptions, @"accessOptions", requiredKeys, keyTypes, outExceptionString))
        return;

    NSString *accessLevelString = accessOptions[accessLevelKey];
    if (![accessLevelString isEqualToString:accessLevelTrustedContexts] && ![accessLevelString isEqualToString:accessLevelTrustedAndUntrustedContexts]) {
        *outExceptionString = toErrorString(nil, @"accessLevel", @"it must specify either 'TRUSTED_CONTEXTS' or 'TRUSTED_AND_UNTRUSTED_CONTEXTS'");
        return;
    }

    WebExtensionStorageAccessLevel accessLevel = [accessLevelString isEqualToString:accessLevelTrustedContexts] ? WebExtensionStorageAccessLevel::TrustedContexts : WebExtensionStorageAccessLevel::TrustedAndUntrustedContexts;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::StorageSetAccessLevel(webPageProxyIdentifier, m_type, accessLevel), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<void, WebExtensionError>&& result) {
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
        m_onChanged = WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::StorageOnChanged);

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
