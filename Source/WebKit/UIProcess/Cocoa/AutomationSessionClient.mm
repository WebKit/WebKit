/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#import "config.h"
#import "AutomationSessionClient.h"

#if WK_API_ENABLED

#import "WKSharedAPICast.h"
#import "WKWebViewInternal.h"
#import "WebAutomationSession.h"
#import "WebPageProxy.h"
#import "_WKAutomationSessionDelegate.h"
#import "_WKAutomationSessionInternal.h"
#import <wtf/BlockPtr.h>

namespace WebKit {

AutomationSessionClient::AutomationSessionClient(id <_WKAutomationSessionDelegate> delegate)
    : m_delegate(delegate)
{
    m_delegateMethods.didDisconnectFromRemote = [delegate respondsToSelector:@selector(_automationSessionDidDisconnectFromRemote:)];

    m_delegateMethods.requestNewWebViewWithOptions = [delegate respondsToSelector:@selector(_automationSession:requestNewWebViewWithOptions:completionHandler:)];
    m_delegateMethods.requestSwitchToWebView = [delegate respondsToSelector:@selector(_automationSession:requestSwitchToWebView:completionHandler:)];
    m_delegateMethods.requestHideWindowOfWebView = [delegate respondsToSelector:@selector(_automationSession:requestHideWindowOfWebView:completionHandler:)];
    m_delegateMethods.requestRestoreWindowOfWebView = [delegate respondsToSelector:@selector(_automationSession:requestRestoreWindowOfWebView:completionHandler:)];
    m_delegateMethods.requestMaximizeWindowOfWebView = [delegate respondsToSelector:@selector(_automationSession:requestMaximizeWindowOfWebView:completionHandler:)];
    m_delegateMethods.isShowingJavaScriptDialogForWebView = [delegate respondsToSelector:@selector(_automationSession:isShowingJavaScriptDialogForWebView:)];
    m_delegateMethods.dismissCurrentJavaScriptDialogForWebView = [delegate respondsToSelector:@selector(_automationSession:dismissCurrentJavaScriptDialogForWebView:)];
    m_delegateMethods.acceptCurrentJavaScriptDialogForWebView = [delegate respondsToSelector:@selector(_automationSession:acceptCurrentJavaScriptDialogForWebView:)];
    m_delegateMethods.messageOfCurrentJavaScriptDialogForWebView = [delegate respondsToSelector:@selector(_automationSession:messageOfCurrentJavaScriptDialogForWebView:)];
    m_delegateMethods.setUserInputForCurrentJavaScriptPromptForWebView = [delegate respondsToSelector:@selector(_automationSession:setUserInput:forCurrentJavaScriptDialogForWebView:)];
    m_delegateMethods.typeOfCurrentJavaScriptDialogForWebView = [delegate respondsToSelector:@selector(_automationSession:typeOfCurrentJavaScriptDialogForWebView:)];

    // FIXME 37408718: these delegate methods should be removed.
    m_delegateMethods.requestNewPageWithOptions = [delegate respondsToSelector:@selector(_automationSession:requestNewPageWithOptions:completionHandler:)];
    m_delegateMethods.requestSwitchToPage = [delegate respondsToSelector:@selector(_automationSession:requestSwitchToPage:completionHandler:)];
    m_delegateMethods.requestHideWindowOfPage = [delegate respondsToSelector:@selector(_automationSession:requestHideWindowOfPage:completionHandler:)];
    m_delegateMethods.requestRestoreWindowOfPage = [delegate respondsToSelector:@selector(_automationSession:requestRestoreWindowOfPage:completionHandler:)];
    m_delegateMethods.requestMaximizeWindowOfPage = [delegate respondsToSelector:@selector(_automationSession:requestMaximizeWindowOfPage:completionHandler:)];
    m_delegateMethods.isShowingJavaScriptDialogOnPage = [delegate respondsToSelector:@selector(_automationSession:isShowingJavaScriptDialogOnPage:)];
    m_delegateMethods.dismissCurrentJavaScriptDialogOnPage = [delegate respondsToSelector:@selector(_automationSession:dismissCurrentJavaScriptDialogOnPage:)];
    m_delegateMethods.acceptCurrentJavaScriptDialogOnPage = [delegate respondsToSelector:@selector(_automationSession:acceptCurrentJavaScriptDialogOnPage:)];
    m_delegateMethods.messageOfCurrentJavaScriptDialogOnPage = [delegate respondsToSelector:@selector(_automationSession:messageOfCurrentJavaScriptDialogOnPage:)];
    m_delegateMethods.setUserInputForCurrentJavaScriptPromptOnPage = [delegate respondsToSelector:@selector(_automationSession:setUserInput:forCurrentJavaScriptDialogOnPage:)];
    m_delegateMethods.typeOfCurrentJavaScriptDialogOnPage = [delegate respondsToSelector:@selector(_automationSession:typeOfCurrentJavaScriptDialogOnPage:)];
}

void AutomationSessionClient::didDisconnectFromRemote(WebAutomationSession& session)
{
    if (m_delegateMethods.didDisconnectFromRemote)
        [m_delegate.get() _automationSessionDidDisconnectFromRemote:wrapper(session)];
}

static inline _WKAutomationSessionBrowsingContextOptions toAPI(API::AutomationSessionBrowsingContextOptions options)
{
    uint16_t wkOptions = 0;

    if (options & API::AutomationSessionBrowsingContextOptionsPreferNewTab)
        wkOptions |= _WKAutomationSessionBrowsingContextOptionsPreferNewTab;

    return static_cast<_WKAutomationSessionBrowsingContextOptions>(wkOptions);
}

// FIXME 37408718: support for WKPageRef-based delegate methods should be removed.
// Until these are removed, prefer to use the WKWebView delegate methods if implemented.
void AutomationSessionClient::requestNewPageWithOptions(WebAutomationSession& session, API::AutomationSessionBrowsingContextOptions options, CompletionHandler<void(WebKit::WebPageProxy*)>&& completionHandler)
{
    if (m_delegateMethods.requestNewWebViewWithOptions) {
        [m_delegate.get() _automationSession:wrapper(session) requestNewWebViewWithOptions:toAPI(options) completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler)](WKWebView *webView) mutable {
            completionHandler(webView->_page.get());
        }).get()];
    } else if (m_delegateMethods.requestNewPageWithOptions) {
        [m_delegate.get() _automationSession:wrapper(session) requestNewPageWithOptions:toAPI(options) completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler)](WKPageRef page) mutable {
            completionHandler(toImpl(page));
        }).get()];
    }
}

void AutomationSessionClient::requestSwitchToPage(WebAutomationSession& session, WebPageProxy& page, CompletionHandler<void()>&& completionHandler)
{
    if (!m_delegateMethods.requestSwitchToWebView && !m_delegateMethods.requestSwitchToPage) {
        completionHandler();
        return;
    }

    auto completionBlock = makeBlockPtr(WTFMove(completionHandler));
    if (m_delegateMethods.requestSwitchToWebView)
        [m_delegate.get() _automationSession:wrapper(session) requestSwitchToWebView:fromWebPageProxy(page) completionHandler:completionBlock.get()];
    else if (m_delegateMethods.requestSwitchToPage)
        [m_delegate.get() _automationSession:wrapper(session) requestSwitchToPage:toAPI(&page) completionHandler:completionBlock.get()];
}

void AutomationSessionClient::requestHideWindowOfPage(WebAutomationSession& session, WebPageProxy& page, CompletionHandler<void()>&& completionHandler)
{
    if (!m_delegateMethods.requestHideWindowOfWebView && !m_delegateMethods.requestHideWindowOfPage) {
        completionHandler();
        return;
    }

    auto completionBlock = makeBlockPtr(WTFMove(completionHandler));
    if (m_delegateMethods.requestHideWindowOfWebView)
        [m_delegate.get() _automationSession:wrapper(session) requestHideWindowOfWebView:fromWebPageProxy(page) completionHandler:completionBlock.get()];
    else if (m_delegateMethods.requestHideWindowOfPage)
        [m_delegate.get() _automationSession:wrapper(session) requestHideWindowOfPage:toAPI(&page) completionHandler:completionBlock.get()];
}

void AutomationSessionClient::requestRestoreWindowOfPage(WebAutomationSession& session, WebPageProxy& page, CompletionHandler<void()>&& completionHandler)
{
    if (!m_delegateMethods.requestRestoreWindowOfWebView && !m_delegateMethods.requestRestoreWindowOfPage) {
        completionHandler();
        return;
    }

    auto completionBlock = makeBlockPtr(WTFMove(completionHandler));
    if (m_delegateMethods.requestRestoreWindowOfWebView)
        [m_delegate.get() _automationSession:wrapper(session) requestRestoreWindowOfWebView:fromWebPageProxy(page) completionHandler:completionBlock.get()];
    else if (m_delegateMethods.requestRestoreWindowOfPage)
        [m_delegate.get() _automationSession:wrapper(session) requestRestoreWindowOfPage:toAPI(&page) completionHandler:completionBlock.get()];
}

void AutomationSessionClient::requestMaximizeWindowOfPage(WebAutomationSession& session, WebPageProxy& page, CompletionHandler<void()>&& completionHandler)
{
    if (!m_delegateMethods.requestMaximizeWindowOfWebView && !m_delegateMethods.requestMaximizeWindowOfPage) {
        completionHandler();
        return;
    }

    auto completionBlock = makeBlockPtr(WTFMove(completionHandler));
    if (m_delegateMethods.requestMaximizeWindowOfWebView)
        [m_delegate.get() _automationSession:wrapper(session) requestMaximizeWindowOfWebView:fromWebPageProxy(page) completionHandler:completionBlock.get()];
    else if (m_delegateMethods.requestMaximizeWindowOfPage)
        [m_delegate.get() _automationSession:wrapper(session) requestMaximizeWindowOfPage:toAPI(&page) completionHandler:completionBlock.get()];
}

bool AutomationSessionClient::isShowingJavaScriptDialogOnPage(WebAutomationSession& session, WebPageProxy& page)
{
    if (m_delegateMethods.isShowingJavaScriptDialogForWebView)
        return [m_delegate.get() _automationSession:wrapper(session) isShowingJavaScriptDialogForWebView:fromWebPageProxy(page)];
    else if (m_delegateMethods.isShowingJavaScriptDialogOnPage)
        return [m_delegate.get() _automationSession:wrapper(session) isShowingJavaScriptDialogOnPage:toAPI(&page)];
    
    return false;
}

void AutomationSessionClient::dismissCurrentJavaScriptDialogOnPage(WebAutomationSession& session, WebPageProxy& page)
{
    if (m_delegateMethods.dismissCurrentJavaScriptDialogForWebView)
        [m_delegate.get() _automationSession:wrapper(session) dismissCurrentJavaScriptDialogForWebView:fromWebPageProxy(page)];
    else if (m_delegateMethods.dismissCurrentJavaScriptDialogOnPage)
        [m_delegate.get() _automationSession:wrapper(session) dismissCurrentJavaScriptDialogOnPage:toAPI(&page)];
}

void AutomationSessionClient::acceptCurrentJavaScriptDialogOnPage(WebAutomationSession& session, WebPageProxy& page)
{
    if (m_delegateMethods.acceptCurrentJavaScriptDialogForWebView)
        [m_delegate.get() _automationSession:wrapper(session) acceptCurrentJavaScriptDialogForWebView:fromWebPageProxy(page)];
    else if (m_delegateMethods.acceptCurrentJavaScriptDialogOnPage)
        [m_delegate.get() _automationSession:wrapper(session) acceptCurrentJavaScriptDialogOnPage:toAPI(&page)];
}

String AutomationSessionClient::messageOfCurrentJavaScriptDialogOnPage(WebAutomationSession& session, WebPageProxy& page)
{
    if (m_delegateMethods.messageOfCurrentJavaScriptDialogForWebView)
        return [m_delegate.get() _automationSession:wrapper(session) messageOfCurrentJavaScriptDialogForWebView:fromWebPageProxy(page)];

    if (m_delegateMethods.messageOfCurrentJavaScriptDialogOnPage)
        return [m_delegate.get() _automationSession:wrapper(session) messageOfCurrentJavaScriptDialogOnPage:toAPI(&page)];

    return String();
}

void AutomationSessionClient::setUserInputForCurrentJavaScriptPromptOnPage(WebAutomationSession& session, WebPageProxy& page, const String& value)
{
    if (m_delegateMethods.setUserInputForCurrentJavaScriptPromptForWebView)
        [m_delegate.get() _automationSession:wrapper(session) setUserInput:value forCurrentJavaScriptDialogForWebView:fromWebPageProxy(page)];
    else if (m_delegateMethods.setUserInputForCurrentJavaScriptPromptOnPage)
        [m_delegate.get() _automationSession:wrapper(session) setUserInput:value forCurrentJavaScriptDialogOnPage:toAPI(&page)];
}

static std::optional<API::AutomationSessionClient::JavaScriptDialogType> toImpl(_WKAutomationSessionJavaScriptDialogType type)
{
    switch (type) {
    case _WKAutomationSessionJavaScriptDialogTypeNone:
        return std::nullopt;
    case _WKAutomationSessionJavaScriptDialogTypePrompt:
        return API::AutomationSessionClient::JavaScriptDialogType::Prompt;
    case _WKAutomationSessionJavaScriptDialogTypeConfirm:
        return API::AutomationSessionClient::JavaScriptDialogType::Confirm;
    case _WKAutomationSessionJavaScriptDialogTypeAlert:
        return API::AutomationSessionClient::JavaScriptDialogType::Alert;
    }
}

std::optional<API::AutomationSessionClient::JavaScriptDialogType> AutomationSessionClient::typeOfCurrentJavaScriptDialogOnPage(WebAutomationSession& session, WebPageProxy& page)
{
    if (m_delegateMethods.typeOfCurrentJavaScriptDialogForWebView)
        return toImpl([m_delegate.get() _automationSession:wrapper(session) typeOfCurrentJavaScriptDialogForWebView:fromWebPageProxy(page)]);
    if (m_delegateMethods.typeOfCurrentJavaScriptDialogOnPage)
        return toImpl([m_delegate.get() _automationSession:wrapper(session) typeOfCurrentJavaScriptDialogOnPage:toAPI(&page)]);

    return API::AutomationSessionClient::JavaScriptDialogType::Prompt;
}

} // namespace WebKit

#endif // WK_API_ENABLED
