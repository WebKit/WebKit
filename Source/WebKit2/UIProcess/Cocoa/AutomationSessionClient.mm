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
#import "WebAutomationSession.h"
#import "WebPageProxy.h"
#import "_WKAutomationSessionDelegate.h"
#import "_WKAutomationSessionInternal.h"

namespace WebKit {

AutomationSessionClient::AutomationSessionClient(id <_WKAutomationSessionDelegate> delegate)
    : m_delegate(delegate)
{
    m_delegateMethods.didRequestNewWindow = [delegate respondsToSelector:@selector(_automationSessionDidRequestNewWindow:)];
    m_delegateMethods.didDisconnectFromRemote = [delegate respondsToSelector:@selector(_automationSessionDidDisconnectFromRemote:)];
    m_delegateMethods.isShowingJavaScriptDialogOnPage = [delegate respondsToSelector:@selector(_automationSession:isShowingJavaScriptDialogOnPage:)];
    m_delegateMethods.dismissCurrentJavaScriptDialogOnPage = [delegate respondsToSelector:@selector(_automationSession:dismissCurrentJavaScriptDialogOnPage:)];
    m_delegateMethods.acceptCurrentJavaScriptDialogOnPage = [delegate respondsToSelector:@selector(_automationSession:acceptCurrentJavaScriptDialogOnPage:)];
    m_delegateMethods.messageOfCurrentJavaScriptDialogOnPage = [delegate respondsToSelector:@selector(_automationSession:messageOfCurrentJavaScriptDialogOnPage:)];
    m_delegateMethods.setUserInputForCurrentJavaScriptPromptOnPage = [delegate respondsToSelector:@selector(_automationSession:setUserInput:forCurrentJavaScriptDialogOnPage:)];
}

WebPageProxy* AutomationSessionClient::didRequestNewWindow(WebAutomationSession* session)
{
    if (m_delegateMethods.didRequestNewWindow)
        return toImpl([m_delegate.get() _automationSessionDidRequestNewWindow:wrapper(*session)]);
    return nullptr;
}

void AutomationSessionClient::didDisconnectFromRemote(WebAutomationSession* session)
{
    if (m_delegateMethods.didDisconnectFromRemote)
        [m_delegate.get() _automationSessionDidDisconnectFromRemote:wrapper(*session)];
}

bool AutomationSessionClient::isShowingJavaScriptDialogOnPage(WebAutomationSession* session, WebPageProxy* page)
{
    if (m_delegateMethods.isShowingJavaScriptDialogOnPage)
        return [m_delegate.get() _automationSession:wrapper(*session) isShowingJavaScriptDialogOnPage:toAPI(page)];
    return false;
}

void AutomationSessionClient::dismissCurrentJavaScriptDialogOnPage(WebAutomationSession* session, WebPageProxy* page)
{
    if (m_delegateMethods.dismissCurrentJavaScriptDialogOnPage)
        [m_delegate.get() _automationSession:wrapper(*session) dismissCurrentJavaScriptDialogOnPage:toAPI(page)];
}

void AutomationSessionClient::acceptCurrentJavaScriptDialogOnPage(WebAutomationSession* session, WebPageProxy* page)
{
    if (m_delegateMethods.acceptCurrentJavaScriptDialogOnPage)
        [m_delegate.get() _automationSession:wrapper(*session) acceptCurrentJavaScriptDialogOnPage:toAPI(page)];
}

String AutomationSessionClient::messageOfCurrentJavaScriptDialogOnPage(WebAutomationSession* session, WebPageProxy* page)
{
    if (m_delegateMethods.messageOfCurrentJavaScriptDialogOnPage)
        return [m_delegate.get() _automationSession:wrapper(*session) messageOfCurrentJavaScriptDialogOnPage:toAPI(page)];
    return String();
}

void AutomationSessionClient::setUserInputForCurrentJavaScriptPromptOnPage(WebAutomationSession* session, WebPageProxy* page, const String& value)
{
    if (m_delegateMethods.setUserInputForCurrentJavaScriptPromptOnPage)
        [m_delegate.get() _automationSession:wrapper(*session) setUserInput:value forCurrentJavaScriptDialogOnPage:toAPI(page)];
}

} // namespace WebKit

#endif // WK_API_ENABLED
