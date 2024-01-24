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
#import "WebExtensionContext.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "WebExtensionConstants.h"
#import "WebExtensionStorageAccessLevel.h"
#import "WebExtensionStorageType.h"
#import "WebExtensionUtilities.h"

namespace WebKit {

void WebExtensionContext::storageGet(WebPageProxyIdentifier webPageProxyIdentifier, WebExtensionStorageType storageType, const IPC::DataReference& dataReference, CompletionHandler<void(std::optional<IPC::DataReference>, ErrorString)>&& completionHandler)
{
    static NSString * const callingAPIName = [NSString stringWithFormat:@"%@.get()", (NSString *)toAPIPrefixString(storageType)];

    if (!extensionCanAccessWebPage(webPageProxyIdentifier)) {
        completionHandler(std::nullopt, toErrorString(callingAPIName, nil, @"access not allowed"));
        return;
    }

    // FIXME: <https://webkit.org/b/267649> Get values from StorageAPISQLiteStore.
    completionHandler(std::nullopt, std::nullopt);
}

void WebExtensionContext::storageGetBytesInUse(WebPageProxyIdentifier webPageProxyIdentifier, WebExtensionStorageType storageType, const Vector<String>& keys, CompletionHandler<void(std::optional<size_t> size, ErrorString)>&& completionHandler)
{
    static NSString * const callingAPIName = [NSString stringWithFormat:@"%@.getBytesInUse()", (NSString *)toAPIPrefixString(storageType)];

    if (!extensionCanAccessWebPage(webPageProxyIdentifier)) {
        completionHandler(std::nullopt, toErrorString(callingAPIName, nil, @"access not allowed"));
        return;
    }

    // FIXME: <https://webkit.org/b/267649> Get storage size from StorageAPISQLiteStore.
    completionHandler(std::nullopt, std::nullopt);
}

void WebExtensionContext::storageSet(WebPageProxyIdentifier webPageProxyIdentifier, WebExtensionStorageType storageType, const IPC::DataReference& dataReference, CompletionHandler<void(ErrorString)>&& completionHandler)
{
    static NSString * const callingAPIName = [NSString stringWithFormat:@"%@.set()", (NSString *)toAPIPrefixString(storageType)];

    if (!extensionCanAccessWebPage(webPageProxyIdentifier)) {
        completionHandler(toErrorString(callingAPIName, nil, @"access not allowed"));
        return;
    }

    // FIXME: <https://webkit.org/b/267649> Set values for StorageAPISQLiteStore.
    completionHandler(std::nullopt);
}

void WebExtensionContext::storageRemove(WebPageProxyIdentifier webPageProxyIdentifier, WebExtensionStorageType storageType, const Vector<String>& keys, CompletionHandler<void(ErrorString)>&& completionHandler)
{
    static NSString * const callingAPIName = [NSString stringWithFormat:@"%@.remove()", (NSString *)toAPIPrefixString(storageType)];

    if (!extensionCanAccessWebPage(webPageProxyIdentifier)) {
        completionHandler(toErrorString(callingAPIName, nil, @"access not allowed"));
        return;
    }

    // FIXME: <https://webkit.org/b/267649> Remove values from StorageAPISQLiteStore.
    completionHandler(std::nullopt);
}

void WebExtensionContext::storageClear(WebPageProxyIdentifier webPageProxyIdentifier, WebExtensionStorageType storageType, CompletionHandler<void(ErrorString)>&& completionHandler)
{
    static NSString * const callingAPIName = [NSString stringWithFormat:@"%@.clear()", (NSString *)toAPIPrefixString(storageType)];

    if (!extensionCanAccessWebPage(webPageProxyIdentifier)) {
        completionHandler(toErrorString(callingAPIName, nil, @"access not allowed"));
        return;
    }

    // FIXME: <https://webkit.org/b/267649> Clear values in StorageAPISQLiteStore.
    completionHandler(std::nullopt);
}

void WebExtensionContext::storageSetAccessLevel(WebPageProxyIdentifier webPageProxyIdentifier, WebExtensionStorageType storageType, const WebExtensionStorageAccessLevel accessLevel, CompletionHandler<void(ErrorString)>&& completionHandler)
{
    static NSString * const callingAPIName = @"browser.session.setAccessLevel()";

    if (!extensionCanAccessWebPage(webPageProxyIdentifier)) {
        completionHandler(toErrorString(callingAPIName, nil, @"access not allowed"));
        return;
    }

    setSessionStorageAllowedInContentScripts(accessLevel == WebExtensionStorageAccessLevel::TrustedAndUntrustedContexts);

    completionHandler(std::nullopt);
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)

