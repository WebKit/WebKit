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

#if ENABLE(WK_WEB_EXTENSIONS) && ENABLE(INSPECTOR_EXTENSIONS)

#import "APIInspectorExtension.h"
#import "JavaScriptEvaluationResult.h"
#import "WebExtensionContextProxyMessages.h"
#import "WebExtensionUtilities.h"
#import <WebCore/ExceptionDetails.h>

namespace WebKit {

void WebExtensionContext::devToolsInspectedWindowEval(WebPageProxyIdentifier webPageProxyIdentifier, const String& scriptSource, const std::optional<URL>& frameURL, CompletionHandler<void(Expected<Expected<JavaScriptEvaluationResult, std::optional<WebCore::ExceptionDetails>>, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"devtools.inspectedWindow.eval()";

    RefPtr extension = inspectorExtension(webPageProxyIdentifier);
    if (!extension) {
        RELEASE_LOG_ERROR(Extensions, "Inspector extension not found for page %llu", webPageProxyIdentifier.toUInt64());
        completionHandler(toWebExtensionError(apiName, nullString(), @"Web Inspector not found"));
        return;
    }

    // FIXME: <https://webkit.org/b/269349> Implement `contextSecurityOrigin` and `useContentScriptContext` options for `devtools.inspectedWindow.eval` command

    RefPtr tab = getTab(webPageProxyIdentifier, std::nullopt, IncludeExtensionViews::Yes);
    if (!tab) {
        completionHandler(toWebExtensionError(apiName, nullString(), @"tab not found"));
        return;
    }

    requestPermissionToAccessURLs({ tab->url() }, tab, [extension, tab, scriptSource, frameURL, completionHandler = WTFMove(completionHandler)](auto&& requestedURLs, auto&& allowedURLs, auto expirationDate) mutable {
        if (!tab->extensionHasPermission()) {
            completionHandler(toWebExtensionError(apiName, nullString(), @"this extension does not have access to this tab"));
            return;
        }

        extension->evaluateScript(scriptSource, frameURL, std::nullopt, std::nullopt, [completionHandler = WTFMove(completionHandler)](Inspector::ExtensionEvaluationResult&& result) mutable {
            if (!result) {
                RELEASE_LOG_ERROR(Extensions, "Inspector could not evaluate script (%{public}@)", extensionErrorToString(result.error()).createNSString().get());
                completionHandler(toWebExtensionError(apiName, nullString(), @"Web Inspector could not evaluate script"));
                return;
            }

            completionHandler({ WTFMove(*result) });
        });
    });
}

void WebExtensionContext::devToolsInspectedWindowReload(WebPageProxyIdentifier webPageProxyIdentifier, const std::optional<bool>& ignoreCache)
{
    RefPtr extension = inspectorExtension(webPageProxyIdentifier);
    if (!extension) {
        RELEASE_LOG_ERROR(Extensions, "Inspector extension not found for page %llu", webPageProxyIdentifier.toUInt64());
        return;
    }

    // FIXME: <https://webkit.org/b/222328> Implement `userAgent` and `injectedScript` options for `devtools.inspectedWindow.reload` command

    extension->reloadIgnoringCache(ignoreCache, std::nullopt, std::nullopt, [](Inspector::ExtensionVoidResult&& result) {
        if (!result)
            RELEASE_LOG_ERROR(Extensions, "Inspector could not reload page (%{public}@)", extensionErrorToString(result.error()).createNSString().get());
    });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS) && ENABLE(INSPECTOR_EXTENSIONS)
