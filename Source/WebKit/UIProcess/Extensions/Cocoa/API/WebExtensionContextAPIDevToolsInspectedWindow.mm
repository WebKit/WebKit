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
#import "APISerializedScriptValue.h"
#import "WebExtensionContextProxyMessages.h"
#import "WebExtensionUtilities.h"
#import <WebCore/ExceptionDetails.h>

namespace WebKit {

void WebExtensionContext::devToolsInspectedWindowEval(WebPageProxyIdentifier webPageProxyIdentifier, const String& scriptSource, const std::optional<URL>& frameURL, CompletionHandler<void(Expected<Expected<std::span<const uint8_t>, WebCore::ExceptionDetails>, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"devtools.inspectedWindow.eval()";

    RefPtr extension = inspectorExtension(webPageProxyIdentifier);
    if (!extension) {
        RELEASE_LOG_ERROR(Extensions, "Inspector extension not found for page %llu", webPageProxyIdentifier.toUInt64());
        completionHandler(toWebExtensionError(apiName, nil, @"Web Inspector not found"));
        return;
    }

    // FIXME: <https://webkit.org/b/269349> Implement `contextSecurityOrigin` and `useContentScriptContext` options for `devtools.inspectedWindow.eval` command

    extension->evaluateScript(scriptSource, frameURL, std::nullopt, std::nullopt, [completionHandler = WTFMove(completionHandler)](Inspector::ExtensionEvaluationResult&& result) mutable {
        if (!result) {
            RELEASE_LOG_ERROR(Extensions, "Inspector could not evaluate script (%{public}@)", (NSString *)extensionErrorToString(result.error()));
            completionHandler(toWebExtensionError(apiName, nil, @"Web Inspector could not evaluate script"));
            return;
        }

        if (!result.value()) {
            Expected<std::span<const uint8_t>, WebCore::ExceptionDetails> returnedValue = makeUnexpected(result.value().error());
            completionHandler({ WTFMove(returnedValue) });
            return;
        }

        completionHandler({ result.value()->get().dataReference() });
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
            RELEASE_LOG_ERROR(Extensions, "Inspector could not reload page (%{public}@)", (NSString *)extensionErrorToString(result.error()));
    });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS) && ENABLE(INSPECTOR_EXTENSIONS)
