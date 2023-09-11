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

void WebExtensionContext::tabsReload(WebPageProxyIdentifier webPageProxyIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, ReloadFromOrigin reloadFromOrigin, CompletionHandler<void(WebExtensionTab::Error)>&& completionHandler)
{
    auto tab = getTab(webPageProxyIdentifier, tabIdentifier);
    if (!tab) {
        completionHandler(toErrorString(@"tabs.reload()", nil, @"tab not found"));
        return;
    }

    if (reloadFromOrigin == ReloadFromOrigin::Yes)
        tab->reloadFromOrigin(WTFMove(completionHandler));
    else
        tab->reload(WTFMove(completionHandler));
}

void WebExtensionContext::tabsGoBack(WebPageProxyIdentifier webPageProxyIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, CompletionHandler<void(WebExtensionTab::Error)>&& completionHandler)
{
    auto tab = getTab(webPageProxyIdentifier, tabIdentifier);
    if (!tab) {
        completionHandler(toErrorString(@"tabs.goBack()", nil, @"tab not found"));
        return;
    }

    tab->goBack(WTFMove(completionHandler));
}

void WebExtensionContext::tabsGoForward(WebPageProxyIdentifier webPageProxyIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, CompletionHandler<void(WebExtensionTab::Error)>&& completionHandler)
{
    auto tab = getTab(webPageProxyIdentifier, tabIdentifier);
    if (!tab) {
        completionHandler(toErrorString(@"tabs.goForward()", nil, @"tab not found"));
        return;
    }

    tab->goForward(WTFMove(completionHandler));
}

void WebExtensionContext::tabsDetectLanguage(WebPageProxyIdentifier webPageProxyIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, CompletionHandler<void(std::optional<String>, WebExtensionTab::Error)>&& completionHandler)
{
    auto tab = getTab(webPageProxyIdentifier, tabIdentifier);
    if (!tab) {
        completionHandler(std::nullopt, toErrorString(@"tabs.detectLanguage()", nil, @"tab not found"));
        return;
    }

    tab->detectWebpageLocale([completionHandler = WTFMove(completionHandler)](NSLocale *locale, WebExtensionTab::Error error) mutable {
        if (error) {
            completionHandler(std::nullopt, error);
            return;
        }

        if (!locale) {
            completionHandler(std::nullopt, std::nullopt);
            return;
        }

        completionHandler(toWebAPI(locale), std::nullopt);
    });
}

void WebExtensionContext::tabsToggleReaderMode(WebPageProxyIdentifier webPageProxyIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, CompletionHandler<void(WebExtensionTab::Error)>&& completionHandler)
{
    auto tab = getTab(webPageProxyIdentifier, tabIdentifier);
    if (!tab) {
        completionHandler(toErrorString(@"tabs.toggleReaderMode()", nil, @"tab not found"));
        return;
    }

    tab->toggleReaderMode(WTFMove(completionHandler));
}

void WebExtensionContext::tabsGetZoom(WebPageProxyIdentifier webPageProxyIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, CompletionHandler<void(std::optional<double>, WebExtensionTab::Error)>&& completionHandler)
{
    auto tab = getTab(webPageProxyIdentifier, tabIdentifier);
    if (!tab) {
        completionHandler(std::nullopt, toErrorString(@"tabs.getZoom()", nil, @"tab not found"));
        return;
    }

    completionHandler(tab->zoomFactor(), std::nullopt);
}

void WebExtensionContext::tabsSetZoom(WebPageProxyIdentifier webPageProxyIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, double zoomFactor, CompletionHandler<void(WebExtensionTab::Error)>&& completionHandler)
{
    auto tab = getTab(webPageProxyIdentifier, tabIdentifier);
    if (!tab) {
        completionHandler(toErrorString(@"tabs.setZoom()", nil, @"tab not found"));
        return;
    }

    tab->setZoomFactor(zoomFactor, WTFMove(completionHandler));
}

void WebExtensionContext::tabsRemove(Vector<WebExtensionTabIdentifier> tabIdentifiers, CompletionHandler<void(WebExtensionTab::Error)>&& completionHandler)
{
    auto tabs = tabIdentifiers.map([&](auto& tabIdentifier) -> RefPtr<WebExtensionTab> {
        auto tab = getTab(tabIdentifier);
        if (!tab) {
            completionHandler(toErrorString(@"tabs.remove()", nil, @"tab '%llu' was not found", tabIdentifier.toUInt64()));
            return nullptr;
        }

        return tab;
    });

    if (tabs.contains(nullptr)) {
        // The completionHandler was called with an error in map() when returning nullptr.
        return;
    }

    size_t completed = 0;
    bool errorOccured = false;
    auto internalCompletionHandler = WTFMove(completionHandler);

    for (auto& tab : tabs) {
        if (errorOccured)
            break;

        tab->close([&](WebExtensionTab::Error error) mutable {
            if (errorOccured)
                return;

            if (error) {
                errorOccured = true;
                internalCompletionHandler(error);
                return;
            }

            if (++completed < tabs.size())
                return;

            internalCompletionHandler(std::nullopt);
        });
    }
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
