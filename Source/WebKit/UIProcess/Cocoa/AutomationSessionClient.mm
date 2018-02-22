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

namespace WebKit {

AutomationSessionClient::AutomationSessionClient(id <_WKAutomationSessionDelegate> delegate)
    : m_delegate(delegate)
{
    m_delegateMethods.didDisconnectFromRemote = [delegate respondsToSelector:@selector(_automationSessionDidDisconnectFromRemote:)];

    m_delegateMethods.didRequestNewWebView = [delegate respondsToSelector:@selector(_automationSessionDidRequestNewWebView:)];
    m_delegateMethods.isShowingJavaScriptDialogForWebView = [delegate respondsToSelector:@selector(_automationSession:isShowingJavaScriptDialogForWebView:)];
    m_delegateMethods.dismissCurrentJavaScriptDialogForWebView = [delegate respondsToSelector:@selector(_automationSession:dismissCurrentJavaScriptDialogForWebView:)];
    m_delegateMethods.acceptCurrentJavaScriptDialogForWebView = [delegate respondsToSelector:@selector(_automationSession:acceptCurrentJavaScriptDialogForWebView:)];
    m_delegateMethods.messageOfCurrentJavaScriptDialogForWebView = [delegate respondsToSelector:@selector(_automationSession:messageOfCurrentJavaScriptDialogForWebView:)];
    m_delegateMethods.setUserInputForCurrentJavaScriptPromptForWebView = [delegate respondsToSelector:@selector(_automationSession:setUserInput:forCurrentJavaScriptDialogForWebView:)];

    // FIXME 28524687: these delegate methods should be removed.
    m_delegateMethods.didRequestNewWindow = [delegate respondsToSelector:@selector(_automationSessionDidRequestNewWindow:)];
    m_delegateMethods.isShowingJavaScriptDialogOnPage = [delegate respondsToSelector:@selector(_automationSession:isShowingJavaScriptDialogOnPage:)];
    m_delegateMethods.dismissCurrentJavaScriptDialogOnPage = [delegate respondsToSelector:@selector(_automationSession:dismissCurrentJavaScriptDialogOnPage:)];
    m_delegateMethods.acceptCurrentJavaScriptDialogOnPage = [delegate respondsToSelector:@selector(_automationSession:acceptCurrentJavaScriptDialogOnPage:)];
    m_delegateMethods.messageOfCurrentJavaScriptDialogOnPage = [delegate respondsToSelector:@selector(_automationSession:messageOfCurrentJavaScriptDialogOnPage:)];
    m_delegateMethods.setUserInputForCurrentJavaScriptPromptOnPage = [delegate respondsToSelector:@selector(_automationSession:setUserInput:forCurrentJavaScriptDialogOnPage:)];
}

void AutomationSessionClient::didDisconnectFromRemote(WebAutomationSession& session)
{
    if (m_delegateMethods.didDisconnectFromRemote)
        [m_delegate.get() _automationSessionDidDisconnectFromRemote:wrapper(session)];
}

// FIXME 28524687: support for WKPageRef-based delegate methods should be removed.
// Until these are removed, prefer to use the WKWebView delegate methods if implemented.
WebPageProxy* AutomationSessionClient::didRequestNewWindow(WebAutomationSession& session)
{
    if (m_delegateMethods.didRequestNewWebView)
        return [m_delegate.get() _automationSessionDidRequestNewWebView:wrapper(session)]->_page.get();

    if (m_delegateMethods.didRequestNewWindow)
        return toImpl([m_delegate.get() _automationSessionDidRequestNewWindow:wrapper(session)]);

    return nullptr;
}

bool AutomationSessionClient::isShowingJavaScriptDialogOnPage(WebAutomationSession& session, WebPageProxy& page)
{
    if (m_delegateMethods.isShowingJavaScriptDialogForWebView)
        return [m_delegate.get() _automationSession:wrapper(session) isShowingJavaScriptDialogForWebView:fromWebPageProxy(page)];

    if (m_delegateMethods.isShowingJavaScriptDialogOnPage)
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

std::optional<API::AutomationSessionClient::JavaScriptDialogType> AutomationSessionClient::typeOfCurrentJavaScriptDialogOnPage(WebAutomationSession&, WebPageProxy&)
{
    // FIXME: Implement it. This is only used in WebAutomationSession::setUserInputForCurrentJavaScriptPrompt() so for now we return
    // always Prompt type for compatibility.
    return API::AutomationSessionClient::JavaScriptDialogType::Prompt;
}

} // namespace WebKit

#endif // WK_API_ENABLED
