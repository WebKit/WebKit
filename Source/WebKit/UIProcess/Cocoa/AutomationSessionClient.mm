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

#import "WKSharedAPICast.h"
#import "WKWebViewInternal.h"
#import "WebAutomationSession.h"
#import "WebPageProxy.h"
#import "_WKAutomationSessionDelegate.h"
#import "_WKAutomationSessionInternal.h"
#import <wtf/BlockPtr.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(AutomationSessionClient);

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
    m_delegateMethods.defaultTextOfCurrentJavaScriptDialogForWebView = [delegate respondsToSelector:@selector(_automationSession:defaultTextOfCurrentJavaScriptDialogForWebView:)];
    m_delegateMethods.userInputOfCurrentJavaScriptDialogForWebView = [delegate respondsToSelector:@selector(_automationSession:userInputOfCurrentJavaScriptDialogForWebView:)];
    m_delegateMethods.setUserInputForCurrentJavaScriptPromptForWebView = [delegate respondsToSelector:@selector(_automationSession:setUserInput:forCurrentJavaScriptDialogForWebView:)];
    m_delegateMethods.typeOfCurrentJavaScriptDialogForWebView = [delegate respondsToSelector:@selector(_automationSession:typeOfCurrentJavaScriptDialogForWebView:)];
    m_delegateMethods.currentPresentationForWebView = [delegate respondsToSelector:@selector(_automationSession:currentPresentationForWebView:)];
#if ENABLE(WK_WEB_EXTENSIONS_IN_WEBDRIVER)
    m_delegateMethods.loadWebExtensionWithOptions = [delegate respondsToSelector:@selector(_automationSession:loadWebExtensionWithOptions:resource:completionHandler:)];
    m_delegateMethods.unloadWebExtension = [delegate respondsToSelector:@selector(_automationSession:unloadWebExtensionWithIdentifier:completionHandler:)];
#endif
}

void AutomationSessionClient::didDisconnectFromRemote(WebAutomationSession& session)
{
    if (m_delegateMethods.didDisconnectFromRemote)
        [m_delegate.get() _automationSessionDidDisconnectFromRemote:RetainPtr { wrapper(session) }.get()];
}

static inline _WKAutomationSessionBrowsingContextOptions NODELETE toAPI(API::AutomationSessionBrowsingContextOptions options)
{
    uint16_t wkOptions = 0;

    if (options & API::AutomationSessionBrowsingContextOptionsPreferNewTab)
        wkOptions |= _WKAutomationSessionBrowsingContextOptionsPreferNewTab;

    return static_cast<_WKAutomationSessionBrowsingContextOptions>(wkOptions);
}

#if ENABLE(WK_WEB_EXTENSIONS_IN_WEBDRIVER)
static inline _WKAutomationSessionWebExtensionResourceOptions NODELETE toAPI(API::AutomationSessionWebExtensionResourceOptions options)
{
    uint16_t wkOptions = 0;

    if (options & API::AutomationSessionWebExtensionResourceOptionsPath)
        wkOptions |= _WKAutomationSessionWebExtensionResourceOptionsPath;
    else if (options & API::AutomationSessionWebExtensionResourceOptionsArchivePath)
        wkOptions |= _WKAutomationSessionWebExtensionResourceOptionsArchivePath;
    else
        wkOptions |= _WKAutomationSessionWebExtensionResourceOptionsBase64;

    return static_cast<_WKAutomationSessionWebExtensionResourceOptions>(wkOptions);
}
#endif

void AutomationSessionClient::requestNewPageWithOptions(WebAutomationSession& session, API::AutomationSessionBrowsingContextOptions options, CompletionHandler<void(WebKit::WebPageProxy*)>&& completionHandler)
{
    if (m_delegateMethods.requestNewWebViewWithOptions) {
        [m_delegate.get() _automationSession:RetainPtr { wrapper(session) }.get() requestNewWebViewWithOptions:toAPI(options) completionHandler:makeBlockPtr([completionHandler = WTF::move(completionHandler)](WKWebView *webView) mutable {
            completionHandler(webView->_page.get());
        }).get()];
    } else
        completionHandler(nullptr);
}

void AutomationSessionClient::requestSwitchToPage(WebAutomationSession& session, WebPageProxy& page, CompletionHandler<void()>&& completionHandler)
{
    if (auto webView = page.cocoaView(); webView && m_delegateMethods.requestSwitchToWebView)
        [m_delegate.get() _automationSession:RetainPtr { wrapper(session) }.get() requestSwitchToWebView:webView.get() completionHandler:makeBlockPtr(WTF::move(completionHandler)).get()];
    else
        completionHandler();
}

void AutomationSessionClient::requestHideWindowOfPage(WebAutomationSession& session, WebPageProxy& page, CompletionHandler<void()>&& completionHandler)
{
    if (auto webView = page.cocoaView(); webView && m_delegateMethods.requestHideWindowOfWebView)
        [m_delegate.get() _automationSession:RetainPtr { wrapper(session) }.get() requestHideWindowOfWebView:webView.get() completionHandler:makeBlockPtr(WTF::move(completionHandler)).get()];
    else
        completionHandler();
}

void AutomationSessionClient::requestRestoreWindowOfPage(WebAutomationSession& session, WebPageProxy& page, CompletionHandler<void()>&& completionHandler)
{
    if (auto webView = page.cocoaView(); webView && m_delegateMethods.requestRestoreWindowOfWebView)
        [m_delegate.get() _automationSession:RetainPtr { wrapper(session) }.get() requestRestoreWindowOfWebView:webView.get() completionHandler:makeBlockPtr(WTF::move(completionHandler)).get()];
    else
        completionHandler();
}

void AutomationSessionClient::requestMaximizeWindowOfPage(WebAutomationSession& session, WebPageProxy& page, CompletionHandler<void()>&& completionHandler)
{
    if (auto webView = page.cocoaView(); webView && m_delegateMethods.requestMaximizeWindowOfWebView)
        [m_delegate.get() _automationSession:RetainPtr { wrapper(session) }.get() requestMaximizeWindowOfWebView:webView.get() completionHandler:makeBlockPtr(WTF::move(completionHandler)).get()];
    else
        completionHandler();
}

#if ENABLE(WK_WEB_EXTENSIONS_IN_WEBDRIVER)
void AutomationSessionClient::loadWebExtensionWithOptions(WebKit::WebAutomationSession& session, API::AutomationSessionWebExtensionResourceOptions options, const String& resource, CompletionHandler<void(const String&)>&& completionHandler)
{
    if (!m_delegateMethods.loadWebExtensionWithOptions) {
        completionHandler(nullString());
        return;
    }

    [m_delegate.get() _automationSession:RetainPtr { wrapper(session) }.get() loadWebExtensionWithOptions:toAPI(options) resource:resource.createNSString().get() completionHandler:makeBlockPtr([completionHandler = WTF::move(completionHandler)](NSString *extensionId) mutable {
        completionHandler(extensionId);
    }).get()];
}

void AutomationSessionClient::unloadWebExtension(WebKit::WebAutomationSession& session, const String& identifier, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!m_delegateMethods.unloadWebExtension) {
        completionHandler(false);
        return;
    }

    [m_delegate.get() _automationSession:RetainPtr { wrapper(session) }.get() unloadWebExtensionWithIdentifier:identifier.createNSString().get() completionHandler:makeBlockPtr([completionHandler = WTF::move(completionHandler)](BOOL success) mutable {
        completionHandler(success);
    }).get()];
}
#endif

bool AutomationSessionClient::isShowingJavaScriptDialogOnPage(WebAutomationSession& session, WebPageProxy& page)
{
    if (auto webView = page.cocoaView(); webView && m_delegateMethods.isShowingJavaScriptDialogForWebView)
        return [m_delegate.get() _automationSession:RetainPtr { wrapper(session) }.get() isShowingJavaScriptDialogForWebView:webView.get()];
    
    return false;
}

void AutomationSessionClient::dismissCurrentJavaScriptDialogOnPage(WebAutomationSession& session, WebPageProxy& page)
{
    if (auto webView = page.cocoaView(); webView && m_delegateMethods.dismissCurrentJavaScriptDialogForWebView)
        [m_delegate.get() _automationSession:RetainPtr { wrapper(session) }.get() dismissCurrentJavaScriptDialogForWebView:webView.get()];
}

void AutomationSessionClient::acceptCurrentJavaScriptDialogOnPage(WebAutomationSession& session, WebPageProxy& page)
{
    if (auto webView = page.cocoaView(); webView && m_delegateMethods.acceptCurrentJavaScriptDialogForWebView)
        [m_delegate.get() _automationSession:RetainPtr { wrapper(session) }.get() acceptCurrentJavaScriptDialogForWebView:webView.get()];
}

std::optional<String> AutomationSessionClient::messageOfCurrentJavaScriptDialogOnPage(WebAutomationSession& session, WebPageProxy& page)
{
    if (auto webView = page.cocoaView(); webView && m_delegateMethods.messageOfCurrentJavaScriptDialogForWebView)
        return [m_delegate.get() _automationSession:RetainPtr { wrapper(session) }.get() messageOfCurrentJavaScriptDialogForWebView:webView.get()];

    return std::nullopt;
}

std::optional<String> AutomationSessionClient::defaultTextOfCurrentJavaScriptDialogOnPage(WebAutomationSession& session, WebPageProxy& page)
{
    if (auto webView = page.cocoaView(); webView && m_delegateMethods.defaultTextOfCurrentJavaScriptDialogForWebView)
        return [m_delegate.get() _automationSession:RetainPtr { wrapper(session) }.get() defaultTextOfCurrentJavaScriptDialogForWebView:webView.get()];

    return std::nullopt;
}

std::optional<String> AutomationSessionClient::userInputOfCurrentJavaScriptDialogOnPage(WebAutomationSession& session, WebPageProxy& page)
{
    if (auto webView = page.cocoaView(); webView && m_delegateMethods.userInputOfCurrentJavaScriptDialogForWebView)
        return [m_delegate.get() _automationSession:RetainPtr { wrapper(session) }.get() userInputOfCurrentJavaScriptDialogForWebView:webView.get()];

    return std::nullopt;
}

void AutomationSessionClient::setUserInputForCurrentJavaScriptPromptOnPage(WebAutomationSession& session, WebPageProxy& page, const String& value)
{
    if (auto webView = page.cocoaView(); webView && m_delegateMethods.setUserInputForCurrentJavaScriptPromptForWebView)
        [m_delegate.get() _automationSession:RetainPtr { wrapper(session) }.get() setUserInput:value.createNSString().get() forCurrentJavaScriptDialogForWebView:webView.get()];
}

static std::optional<API::AutomationSessionClient::JavaScriptDialogType> NODELETE toImpl(_WKAutomationSessionJavaScriptDialogType type)
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

static API::AutomationSessionClient::BrowsingContextPresentation NODELETE toImpl(_WKAutomationSessionBrowsingContextPresentation presentation)
{
    switch (presentation) {
    case _WKAutomationSessionBrowsingContextPresentationTab:
        return API::AutomationSessionClient::BrowsingContextPresentation::Tab;
    case _WKAutomationSessionBrowsingContextPresentationWindow:
        return API::AutomationSessionClient::BrowsingContextPresentation::Window;
    }
}

std::optional<API::AutomationSessionClient::JavaScriptDialogType> AutomationSessionClient::typeOfCurrentJavaScriptDialogOnPage(WebAutomationSession& session, WebPageProxy& page)
{
    if (auto webView = page.cocoaView(); webView && m_delegateMethods.typeOfCurrentJavaScriptDialogForWebView)
        return toImpl([m_delegate.get() _automationSession:RetainPtr { wrapper(session) }.get() typeOfCurrentJavaScriptDialogForWebView:webView.get()]);

    return API::AutomationSessionClient::JavaScriptDialogType::Prompt;
}

API::AutomationSessionClient::BrowsingContextPresentation AutomationSessionClient::currentPresentationOfPage(WebAutomationSession& session, WebPageProxy& page)
{
    if (auto webView = page.cocoaView(); webView && m_delegateMethods.currentPresentationForWebView)
        return toImpl([m_delegate.get() _automationSession:RetainPtr { wrapper(session) }.get() currentPresentationForWebView:webView.get()]);

    return API::AutomationSessionClient::BrowsingContextPresentation::Window;
}

} // namespace WebKit
