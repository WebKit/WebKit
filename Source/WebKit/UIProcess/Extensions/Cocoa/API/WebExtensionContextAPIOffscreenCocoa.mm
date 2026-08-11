/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if ENABLE(WK_WEB_EXTENSIONS_OFFSCREEN)

#import "WKNavigationDelegate.h"
#import "WKUIDelegate.h"
#import "WKWebViewInternal.h"
#import "WebExtensionOffscreenDocumentParameters.h"

namespace WebKit {

bool WebExtensionContext::isOffscreenMessageAllowed(IPC::Decoder& message)
{
    return isLoadedAndPrivilegedMessage(message) && hasPermission(WebExtensionPermission::offscreen());
}

void WebExtensionContext::offscreenCreateDocument(const WebExtensionOffscreenDocumentParameters& parameters, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static constexpr auto apiName = "offscreen.createDocument()"_s;

    if (m_offscreenWebView) {
        completionHandler(toWebExtensionError(apiName, nullString(), @"only a single offscreen document may be created"));
        return;
    }

    URL documentURL { m_baseURL, parameters.url };
    if (!isURLForThisExtension(documentURL)) {
        completionHandler(toWebExtensionError(apiName, nullString(), @"url must be a resource within the extension"));
        return;
    }

    RefPtr extensionController = this->extensionController();
    if (!extensionController) {
        completionHandler(toWebExtensionError(apiName, nullString(), @"the extension is not loaded"));
        return;
    }

    m_offscreenWebView = [[WKWebView alloc] initWithFrame:CGRectZero configuration:webViewConfiguration(WebViewPurpose::Offscreen)];

    // The _WKWebExtensionContextDelegate class interface, which declares conformance to WKUIDelegate and WKNavigationDelegate,
    // is private to WebExtensionContextCocoa.mm, so the protocol conformances aren't visible here and require explicit casts.
    m_offscreenWebView.get().UIDelegate = static_cast<id<WKUIDelegate>>(m_delegate.get());
    m_offscreenWebView.get().navigationDelegate = static_cast<id<WKNavigationDelegate>>(m_delegate.get());

    m_offscreenWebView.get().inspectable = m_inspectable;

    Ref offscreenPage = *m_offscreenWebView.get()._page;
    Ref offscreenProcess = offscreenPage->siteIsolatedProcess();

    constexpr ASCIILiteral activityName = "Web Extension offscreen document"_s;
    if (protect(offscreenPage->preferences())->siteIsolationEnabled())
        m_offscreenWebViewActivity = protect(offscreenPage->activityGroupContext())->foregroundProcessActivityGroup(activityName);
    else
        m_offscreenWebViewActivity = protect(offscreenProcess->throttler())->foregroundActivity(activityName);

    [m_offscreenWebView loadRequest:[NSURLRequest requestWithURL:documentURL.createNSURL().get()]];

    m_offscreenDocumentLoadCompletionHandlers.append(WTF::move(completionHandler));
}

void WebExtensionContext::offscreenCloseDocument(CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static constexpr auto apiName = "offscreen.closeDocument()"_s;

    if (!m_offscreenWebView) {
        completionHandler(toWebExtensionError(apiName, nullString(), @"no offscreen document is open"));
        return;
    }

    unloadOffscreenWebView();

    completionHandler({ });
}

void WebExtensionContext::offscreenHasDocument(CompletionHandler<void(Expected<bool, WebExtensionError>&&)>&& completionHandler)
{
    completionHandler(!!m_offscreenWebView);
}

void WebExtensionContext::unloadOffscreenWebView()
{
    static constexpr auto completionHandlerAPIName = "offscreen.createDocument()"_s;
    for (auto& completionHandler : std::exchange(m_offscreenDocumentLoadCompletionHandlers, { }))
        completionHandler(toWebExtensionError(completionHandlerAPIName, nullString(), @"offscreen document was closed"));

    m_offscreenWebViewActivity = { };

    [m_offscreenWebView _close];
    m_offscreenWebView = nil;
}

void WebExtensionContext::performTasksAfterOffscreenContentLoads()
{
    for (auto& completionHandler : std::exchange(m_offscreenDocumentLoadCompletionHandlers, { }))
        completionHandler({ });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS_OFFSCREEN)
