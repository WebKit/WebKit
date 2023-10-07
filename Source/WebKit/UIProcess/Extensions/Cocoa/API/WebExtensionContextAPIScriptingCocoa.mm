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
#import "WKFrameInfoPrivate.h"
#import "WKWebViewInternal.h"
#import "WKWebViewPrivate.h"
#import "WebExtensionContextProxy.h"
#import "WebExtensionContextProxyMessages.h"
#import "WebExtensionScriptInjectionParameters.h"
#import "WebExtensionScriptInjectionResultParameters.h"
#import "WebExtensionTab.h"
#import "WebExtensionTabIdentifier.h"
#import "WebExtensionUtilities.h"
#import "_WKFrameHandle.h"
#import "_WKFrameTreeNode.h"
#import <WebCore/UserStyleSheet.h>
#import <wtf/BlockPtr.h>
#import <wtf/cocoa/VectorCocoa.h>

namespace WebKit {

using namespace WebExtensionDynamicScripts;

void WebExtensionContext::scriptingExecuteScript(const WebExtensionScriptInjectionParameters& parameters, CompletionHandler<void(std::optional<InjectionResults>, WebExtensionDynamicScripts::Error)>&& completionHandler)
{
    NSString *apiName = @"scripting.executeScript()";
    auto tab = getTab(parameters.tabIdentifier.value());
    if (!tab) {
        completionHandler(std::nullopt, toErrorString(apiName, nil, @"tab not found"));
        return;
    }

    auto *webView = tab->mainWebView();
    if (!webView) {
        completionHandler(std::nullopt, toErrorString(apiName, nil, @"could not execute script on this tab"));
        return;
    }

    if (!hasPermission(webView.URL, tab.get())) {
        completionHandler(std::nullopt, toErrorString(apiName, nil, @"this extension does not have access to this tab"));
        return;
    }

    // FIXME: <https://webkit.org/b/259954> Implement scripting.executeScript().
    completionHandler({ }, std::nullopt);
}

void WebExtensionContext::scriptingInsertCSS(const WebExtensionScriptInjectionParameters& parameters, CompletionHandler<void(WebExtensionDynamicScripts::Error)>&& completionHandler)
{
    NSString *apiName = @"scripting.insertCSS()";
    auto tab = getTab(parameters.tabIdentifier.value());
    if (!tab) {
        completionHandler(toErrorString(apiName, nil, @"tab not found"));
        return;
    }

    auto *webView = tab->mainWebView();
    if (!webView) {
        completionHandler(toErrorString(apiName, nil, @"could not inject stylesheet on this tab"));
        return;
    }

    if (!hasPermission(webView.URL, tab.get())) {
        completionHandler(toErrorString(apiName, nil, @"this extension does not have access to this tab"));
        return;
    }

    // FIXME: <https://webkit.org/b/262491> There is currently no way to inject CSS in specific frames based on ID's. If 'frameIds' is passed, default to the main frame.
    auto injectedFrames = parameters.frameIDs ? WebCore::UserContentInjectedFrames::InjectInTopFrameOnly : WebCore::UserContentInjectedFrames::InjectInAllFrames;

    SourcePairs styleSheetPairs = getSourcePairsForResource(parameters.files, parameters.css, m_extension);
    injectStyleSheets(styleSheetPairs, webView, *m_contentScriptWorld, injectedFrames, *this);

    completionHandler(std::nullopt);
}

void WebExtensionContext::scriptingRemoveCSS(const WebExtensionScriptInjectionParameters& parameters, CompletionHandler<void(WebExtensionDynamicScripts::Error)>&& completionHandler)
{
    if (m_dynamicallyInjectedUserStyleSheets.isEmpty()) {
        completionHandler(std::nullopt);
        return;
    }

    NSString *apiName = @"scripting.removeCSS()";
    auto tab = getTab(parameters.tabIdentifier.value());
    if (!tab) {
        completionHandler(toErrorString(apiName, nil, @"tab not found"));
        return;
    }

    auto *webView = tab->mainWebView();
    if (!webView) {
        completionHandler(toErrorString(apiName, nil, @"could not remove stylesheet from this tab"));
        return;
    }

    if (!hasPermission(webView.URL, tab.get())) {
        completionHandler(toErrorString(apiName, nil, @"this extension does not have access to this tab"));
        return;
    }

    // FIXME: <https://webkit.org/b/262491> There is currently no way to inject CSS in specific frames based on ID's. If 'frameIds' is passed, default to the main frame.
    auto injectedFrames = parameters.frameIDs ? WebCore::UserContentInjectedFrames::InjectInTopFrameOnly : WebCore::UserContentInjectedFrames::InjectInAllFrames;

    Vector<std::optional<SourcePair>> styleSheetPairs = getSourcePairsForResource(parameters.files, parameters.css, m_extension);
    removeStyleSheets(styleSheetPairs, injectedFrames, *this);

    completionHandler(std::nullopt);
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)

