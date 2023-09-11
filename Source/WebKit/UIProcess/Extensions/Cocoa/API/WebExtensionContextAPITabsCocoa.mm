/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#import "CocoaHelpers.h"
#import "WKWebViewInternal.h"
#import "WebExtensionContextProxy.h"
#import "WebExtensionContextProxyMessages.h"
#import "WebExtensionTabIdentifier.h"
#import "WebExtensionUtilities.h"
#import "WebExtensionWindowIdentifier.h"
#import "WebPageProxy.h"
#import "_WKWebExtensionTabCreationOptionsInternal.h"

namespace WebKit {

void WebExtensionContext::tabsGet(WebExtensionTabIdentifier tabIdentifier, CompletionHandler<void(std::optional<WebExtensionTabParameters>, WebExtensionTab::Error)>&& completionHandler)
{
    auto tab = getTab(tabIdentifier);
    if (!tab) {
        completionHandler(std::nullopt, toErrorString(@"tabs.get()", nil, @"tab not found"));
        return;
    }

    completionHandler(tab->parameters(), std::nullopt);
}

void WebExtensionContext::tabsGetCurrent(WebPageProxyIdentifier webPageProxyIdentifier, CompletionHandler<void(std::optional<WebExtensionTabParameters>, WebExtensionTab::Error)>&& completionHandler)
{
    for (auto& tab : openTabs()) {
        for (WKWebView *webView in tab->webViews()) {
            if (webView._page->identifier() == webPageProxyIdentifier) {
                completionHandler(tab->parameters(), std::nullopt);
                return;
            }
        }
    }

    // No error is reported when the page isn't a tab (e.g. the background page).
    completionHandler(std::nullopt, std::nullopt);
}

void WebExtensionContext::tabsQuery(WebPageProxyIdentifier webPageProxyIdentifier, const WebExtensionTabQueryParameters& queryParameters, CompletionHandler<void(Vector<WebExtensionTabParameters>, WebExtensionTab::Error)>&& completionHandler)
{
    Vector<WebExtensionTabParameters> result;

    for (auto& window : openWindows()) {
        if (!window->matches(queryParameters, webPageProxyIdentifier))
            continue;

        for (auto& tab : window->tabs()) {
            if (tab->matches(queryParameters, WebExtensionTab::AssumeWindowMatches::Yes, webPageProxyIdentifier))
                result.append(tab->parameters());
        }
    }

    completionHandler(WTFMove(result), std::nullopt);
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
