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

bool WebExtensionAPIStorageArea::isPropertyAllowed(ASCIILiteral propertyName, WebPage*)
{
    static NeverDestroyed<HashSet<AtomString>> syncStorageProperties { HashSet { AtomString("QUOTA_BYTES_PER_ITEM"_s), AtomString("MAX_ITEMS"_s), AtomString("MAX_WRITE_OPERATIONS_PER_HOUR"_s), AtomString("MAX_WRITE_OPERATIONS_PER_MINUTE"_s) } };

    if (syncStorageProperties.get().contains(propertyName))
        return m_type == StorageType::Sync;

    if (propertyName == "setAccessLevel"_s)
        return m_type == StorageType::Session;

    ASSERT_NOT_REACHED();
    return false;
}

void WebExtensionAPIStorageArea::get(id items, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/storage/StorageArea/get

    auto returnEmptyResult = ^() {
        callback->call(@{ });
    };

    auto validateJSONAndReturnData = ^(id items, JSONOptionSet options, NSString **outExceptionString) {
        if (!isValidJSONObject(items, options)) {
            *outExceptionString = toErrorString(nil, @"items", @"it is not JSON-serializable");
            return;
        }

        NSData *data = encodeJSONData(items, options);
        if (!data) {
            *outExceptionString = toErrorString(nil, @"items", @"it is not JSON-serializable");
            return;
        }

        // FIXME: <https://webkit.org/b/264892> Get data from storage.
        callback->call();
    };

    if ([items isKindOfClass:NSDictionary.class]) {
        auto *keysWithDefaultValues = (NSDictionary *)items;
        if (!keysWithDefaultValues.count) {
            returnEmptyResult();
            return;
        }

        validateJSONAndReturnData(keysWithDefaultValues, { }, outExceptionString);
        return;
    }

    if ([items isKindOfClass:NSArray.class]) {
        auto *keys = (NSArray *)items;
        if (!keys.count) {
            returnEmptyResult();
            return;
        }

        validateJSONAndReturnData(keys, { JSONOptions::FragmentsAllowed }, outExceptionString);
        return;
    }

    if ([items isKindOfClass:NSString.class]) {
        auto *key = (NSString *)items;
        if (!key.length) {
            returnEmptyResult();
            return;
        }

        validateJSONAndReturnData(key, { JSONOptions::FragmentsAllowed }, outExceptionString);
        return;
    }

    if (items && ![items isKindOfClass:NSNull.class]) {
        *outExceptionString = *outExceptionString = toErrorString(nil, @"items", @"an invalid parameter was passed");
        return;
    }
}

void WebExtensionAPIStorageArea::getBytesInUse(id keys, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/storage/StorageArea/getBytesInUse

    if (keys && !validateObject(keys, @"keys", [NSOrderedSet orderedSetWithObjects:NSString.class, @[ NSString.class ], NSNull.class, nil], outExceptionString))
        return;

    // If empty, return the total storage size.
    Vector<String> keysVector = [keys isKindOfClass:NSArray.class] ? makeVector<String>((NSArray *)keys) : Vector<String> { (NSString *)keys };

    // FIXME: <https://webkit.org/b/264892> Get bytes used in storage.
    callback->call();
}

void WebExtensionAPIStorageArea::set(NSDictionary *items, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/storage/StorageArea/set

    if (!items.count) {
        callback->call();
        return;
    }

    if (!isValidJSONObject(items)) {
        *outExceptionString = toErrorString(nil, @"items", @"it is not JSON-serializable");
        return;
    }

    auto *data = encodeJSONData(items);
    if (!data) {
        *outExceptionString = toErrorString(nil, @"items", @"it is not JSON-serializable");
        return;
    }

    // FIXME: <https://webkit.org/b/264892> Save data to storage.
    callback->call();
}

void WebExtensionAPIStorageArea::remove(id keys, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/storage/StorageArea/remove

    if (!validateObject(keys, @"keys", [NSOrderedSet orderedSetWithObjects:NSString.class, @[ NSString.class ], nil], outExceptionString))
        return;

    Vector<String> keysVector = [keys isKindOfClass:NSArray.class] ? makeVector<String>((NSArray *)keys) : Vector<String> { keys };

    // FIXME: <https://webkit.org/b/264892> Remove keys from storage.
    callback->call();
}

WebExtensionAPIEvent& WebExtensionAPIStorageArea::onChanged()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/storage/StorageArea/onChanged

    if (!m_onChanged)
        m_onChanged = WebExtensionAPIEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::StorageOnChanged);
    return *m_onChanged;
}

void WebExtensionAPIStorageArea::clear(Ref<WebExtensionCallbackHandler>&& callback)
{
    // FIXME: <https://webkit.org/b/264892> Clear storage.
    callback->call();
}

void WebExtensionAPIStorageArea::setAccessLevel(NSDictionary *accessOptions, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
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

    // FIXME: <https://webkit.org/b/264892> Set access level.
}

double WebExtensionAPIStorageArea::quotaBytes()
{
    switch (m_type) {
    case StorageType::Local:
        return webExtensionStorageAreaLocalQuotaBytes;
    case StorageType::Sync:
        return webExtensionStorageAreaSyncQuotaBytes;
    case StorageType::Session:
        return webExtensionStorageAreaSessionQuotaBytes;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

double WebExtensionAPIStorageArea::quotaBytesPerItem()
{
    ASSERT(m_type == StorageType::Sync);
    return webExtensionStorageAreaSyncQuotaBytesPerItem;
}

NSUInteger WebExtensionAPIStorageArea::maxItems()
{
    ASSERT(m_type == StorageType::Sync);
    return webExtensionStorageAreaSyncMaximumItems;
}

NSUInteger WebExtensionAPIStorageArea::maxWriteOperationsPerHour()
{
    ASSERT(m_type == StorageType::Sync);
    return webExtensionStorageAreaSyncMaximumWriteOperationsPerHour;
}

NSUInteger WebExtensionAPIStorageArea::maxWriteOperationsPerMinute()
{
    ASSERT(m_type == StorageType::Sync);
    return webExtensionStorageAreaSyncMaximumWriteOperationsPerMinute;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
