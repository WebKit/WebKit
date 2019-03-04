/*
 * Copyright (C) 2014-2018 Apple Inc. All rights reserved.
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
#import "UIDelegate.h"

#import "APIFrameInfo.h"
#import "APIHitTestResult.h"
#import "CompletionHandlerCallChecker.h"
#import "MediaCaptureUtilities.h"
#import "NativeWebWheelEvent.h"
#import "NavigationActionData.h"
#import "UserMediaPermissionCheckProxy.h"
#import "UserMediaPermissionRequestProxy.h"
#import "WKFrameInfoInternal.h"
#import "WKNSData.h"
#import "WKNSDictionary.h"
#import "WKNavigationActionInternal.h"
#import "WKOpenPanelParametersInternal.h"
#import "WKSecurityOriginInternal.h"
#import "WKUIDelegatePrivate.h"
#import "WKWebViewConfigurationInternal.h"
#import "WKWebViewInternal.h"
#import "WKWindowFeaturesInternal.h"
#import "WebEventFactory.h"
#import "WebOpenPanelResultListenerProxy.h"
#import "WebProcessProxy.h"
#import "_WKContextMenuElementInfo.h"
#import "_WKFrameHandleInternal.h"
#import "_WKHitTestResultInternal.h"
#import <WebCore/FontAttributes.h>
#import <WebCore/SecurityOriginData.h>
#import <wtf/BlockPtr.h>
#import <wtf/URL.h>

#if HAVE(AUTHORIZATION_STATUS_FOR_MEDIA_TYPE)
#import <AVFoundation/AVCaptureDevice.h>
#import <AVFoundation/AVMediaFormat.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK(AVFoundation);
SOFT_LINK_CLASS(AVFoundation, AVCaptureDevice);
SOFT_LINK_CONSTANT(AVFoundation, AVMediaTypeAudio, NSString *);
SOFT_LINK_CONSTANT(AVFoundation, AVMediaTypeVideo, NSString *);
#endif

namespace WebKit {

UIDelegate::UIDelegate(WKWebView *webView)
    : m_webView(webView)
{
}

UIDelegate::~UIDelegate()
{
}

#if ENABLE(CONTEXT_MENUS)
std::unique_ptr<API::ContextMenuClient> UIDelegate::createContextMenuClient()
{
    return std::make_unique<ContextMenuClient>(*this);
}
#endif

std::unique_ptr<API::UIClient> UIDelegate::createUIClient()
{
    return std::make_unique<UIClient>(*this);
}

RetainPtr<id <WKUIDelegate> > UIDelegate::delegate()
{
    return m_delegate.get();
}

void UIDelegate::setDelegate(id <WKUIDelegate> delegate)
{
    m_delegate = delegate;

    m_delegateMethods.webViewCreateWebViewWithConfigurationForNavigationActionWindowFeatures = [delegate respondsToSelector:@selector(webView:createWebViewWithConfiguration:forNavigationAction:windowFeatures:)];
    m_delegateMethods.webViewCreateWebViewWithConfigurationForNavigationActionWindowFeaturesAsync = [delegate respondsToSelector:@selector(_webView:createWebViewWithConfiguration:forNavigationAction:windowFeatures:completionHandler:)];
    m_delegateMethods.webViewRunJavaScriptAlertPanelWithMessageInitiatedByFrameCompletionHandler = [delegate respondsToSelector:@selector(webView:runJavaScriptAlertPanelWithMessage:initiatedByFrame:completionHandler:)];
    m_delegateMethods.webViewRunJavaScriptConfirmPanelWithMessageInitiatedByFrameCompletionHandler = [delegate respondsToSelector:@selector(webView:runJavaScriptConfirmPanelWithMessage:initiatedByFrame:completionHandler:)];
    m_delegateMethods.webViewRunJavaScriptTextInputPanelWithPromptDefaultTextInitiatedByFrameCompletionHandler = [delegate respondsToSelector:@selector(webView:runJavaScriptTextInputPanelWithPrompt:defaultText:initiatedByFrame:completionHandler:)];
    m_delegateMethods.webViewRequestStorageAccessPanelForTopPrivatelyControlledDomainUnderFirstPartyTopPrivatelyControlledDomainCompletionHandler = [delegate respondsToSelector:@selector(_webView:requestStorageAccessPanelForDomain:underCurrentDomain:completionHandler:)];
    m_delegateMethods.webViewRunBeforeUnloadConfirmPanelWithMessageInitiatedByFrameCompletionHandler = [delegate respondsToSelector:@selector(_webView:runBeforeUnloadConfirmPanelWithMessage:initiatedByFrame:completionHandler:)];
    m_delegateMethods.webViewRequestGeolocationPermissionForFrameDecisionHandler = [delegate respondsToSelector:@selector(_webView:requestGeolocationPermissionForFrame:decisionHandler:)];
    m_delegateMethods.webViewDidResignInputElementStrongPasswordAppearanceWithUserInfo = [delegate respondsToSelector:@selector(_webView:didResignInputElementStrongPasswordAppearanceWithUserInfo:)];
    m_delegateMethods.webViewTakeFocus = [delegate respondsToSelector:@selector(_webView:takeFocus:)];

#if PLATFORM(MAC)
    m_delegateMethods.showWebView = [delegate respondsToSelector:@selector(_showWebView:)];
    m_delegateMethods.focusWebView = [delegate respondsToSelector:@selector(_focusWebView:)];
    m_delegateMethods.unfocusWebView = [delegate respondsToSelector:@selector(_unfocusWebView:)];
    m_delegateMethods.webViewRunModal = [delegate respondsToSelector:@selector(_webViewRunModal:)];
    m_delegateMethods.webViewDidScroll = [delegate respondsToSelector:@selector(_webViewDidScroll:)];
    m_delegateMethods.webViewGetToolbarsAreVisibleWithCompletionHandler = [delegate respondsToSelector:@selector(_webView:getToolbarsAreVisibleWithCompletionHandler:)];
    m_delegateMethods.webViewDidNotHandleWheelEvent = [delegate respondsToSelector:@selector(_webView:didNotHandleWheelEvent:)];
    m_delegateMethods.webViewSetResizable = [delegate respondsToSelector:@selector(_webView:setResizable:)];
    m_delegateMethods.webViewGetWindowFrameWithCompletionHandler = [delegate respondsToSelector:@selector(_webView:getWindowFrameWithCompletionHandler:)];
    m_delegateMethods.webViewSetWindowFrame = [delegate respondsToSelector:@selector(_webView:setWindowFrame:)];
    m_delegateMethods.webViewUnavailablePlugInButtonClicked = [delegate respondsToSelector:@selector(_webView:unavailablePlugInButtonClickedWithReason:plugInInfo:)];
    m_delegateMethods.webViewHandleAutoplayEventWithFlags = [delegate respondsToSelector:@selector(_webView:handleAutoplayEvent:withFlags:)];
    m_delegateMethods.webViewDidClickAutoFillButtonWithUserInfo = [delegate respondsToSelector:@selector(_webView:didClickAutoFillButtonWithUserInfo:)];
    m_delegateMethods.webViewDrawHeaderInRectForPageWithTitleURL = [delegate respondsToSelector:@selector(_webView:drawHeaderInRect:forPageWithTitle:URL:)];
    m_delegateMethods.webViewDrawFooterInRectForPageWithTitleURL = [delegate respondsToSelector:@selector(_webView:drawFooterInRect:forPageWithTitle:URL:)];
    m_delegateMethods.webViewHeaderHeight = [delegate respondsToSelector:@selector(_webViewHeaderHeight:)];
    m_delegateMethods.webViewFooterHeight = [delegate respondsToSelector:@selector(_webViewFooterHeight:)];
    m_delegateMethods.webViewMouseDidMoveOverElementWithFlagsUserInfo = [delegate respondsToSelector:@selector(_webView:mouseDidMoveOverElement:withFlags:userInfo:)];
    m_delegateMethods.webViewDidExceedBackgroundResourceLimitWhileInForeground = [delegate respondsToSelector:@selector(_webView:didExceedBackgroundResourceLimitWhileInForeground:)];
    m_delegateMethods.webViewSaveDataToFileSuggestedFilenameMimeTypeOriginatingURL = [delegate respondsToSelector:@selector(_webView:saveDataToFile:suggestedFilename:mimeType:originatingURL:)];
    m_delegateMethods.webViewRunOpenPanelWithParametersInitiatedByFrameCompletionHandler = [delegate respondsToSelector:@selector(webView:runOpenPanelWithParameters:initiatedByFrame:completionHandler:)];
    m_delegateMethods.webViewRequestNotificationPermissionForSecurityOriginDecisionHandler = [delegate respondsToSelector:@selector(_webView:requestNotificationPermissionForSecurityOrigin:decisionHandler:)];
#endif
    m_delegateMethods.webViewDecideDatabaseQuotaForSecurityOriginCurrentQuotaCurrentOriginUsageCurrentDatabaseUsageExpectedUsageDecisionHandler = [delegate respondsToSelector:@selector(_webView:decideDatabaseQuotaForSecurityOrigin:currentQuota:currentOriginUsage:currentDatabaseUsage:expectedUsage:decisionHandler:)];
    m_delegateMethods.webViewDecideDatabaseQuotaForSecurityOriginDatabaseNameDisplayNameCurrentQuotaCurrentOriginUsageCurrentDatabaseUsageExpectedUsageDecisionHandler = [delegate respondsToSelector:@selector(_webView:decideDatabaseQuotaForSecurityOrigin:databaseName:displayName:currentQuota:currentOriginUsage:currentDatabaseUsage:expectedUsage:decisionHandler:)];
    m_delegateMethods.webViewDecideWebApplicationCacheQuotaForSecurityOriginCurrentQuotaTotalBytesNeeded = [delegate respondsToSelector:@selector(_webView:decideWebApplicationCacheQuotaForSecurityOrigin:currentQuota:totalBytesNeeded:decisionHandler:)];
    m_delegateMethods.webViewPrintFrame = [delegate respondsToSelector:@selector(_webView:printFrame:)];
    m_delegateMethods.webViewDidClose = [delegate respondsToSelector:@selector(webViewDidClose:)];
    m_delegateMethods.webViewClose = [delegate respondsToSelector:@selector(_webViewClose:)];
    m_delegateMethods.webViewFullscreenMayReturnToInline = [delegate respondsToSelector:@selector(_webViewFullscreenMayReturnToInline:)];
    m_delegateMethods.webViewDidEnterFullscreen = [delegate respondsToSelector:@selector(_webViewDidEnterFullscreen:)];
    m_delegateMethods.webViewDidExitFullscreen = [delegate respondsToSelector:@selector(_webViewDidExitFullscreen:)];
#if PLATFORM(IOS_FAMILY)
#if HAVE(APP_LINKS)
    m_delegateMethods.webViewShouldIncludeAppLinkActionsForElement = [delegate respondsToSelector:@selector(_webView:shouldIncludeAppLinkActionsForElement:)];
#endif
    m_delegateMethods.webViewActionsForElementDefaultActions = [delegate respondsToSelector:@selector(_webView:actionsForElement:defaultActions:)];
    m_delegateMethods.webViewDidNotHandleTapAsClickAtPoint = [delegate respondsToSelector:@selector(_webView:didNotHandleTapAsClickAtPoint:)];
    m_delegateMethods.presentingViewControllerForWebView = [delegate respondsToSelector:@selector(_presentingViewControllerForWebView:)];
#endif
    m_delegateMethods.webViewRequestMediaCaptureAuthorizationForFrameDecisionHandler = [delegate respondsToSelector:@selector(_webView:requestUserMediaAuthorizationForDevices:url:mainFrameURL:decisionHandler:)] || [delegate respondsToSelector:@selector(_webView:requestMediaCaptureAuthorization:decisionHandler:)];
    m_delegateMethods.webViewIsMediaCaptureAuthorizedForFrameDecisionHandler = [delegate respondsToSelector:@selector(_webView:checkUserMediaPermissionForURL:mainFrameURL:frameIdentifier:decisionHandler:)] || [delegate respondsToSelector:@selector(_webView:includeSensitiveMediaDeviceDetails:)];

    m_delegateMethods.webViewMediaCaptureStateDidChange = [delegate respondsToSelector:@selector(_webView:mediaCaptureStateDidChange:)];
    m_delegateMethods.webViewDidChangeFontAttributes = [delegate respondsToSelector:@selector(_webView:didChangeFontAttributes:)];
    m_delegateMethods.dataDetectionContextForWebView = [delegate respondsToSelector:@selector(_dataDetectionContextForWebView:)];
    m_delegateMethods.webViewImageOrMediaDocumentSizeChanged = [delegate respondsToSelector:@selector(_webView:imageOrMediaDocumentSizeChanged:)];

#if ENABLE(POINTER_LOCK)
    m_delegateMethods.webViewRequestPointerLock = [delegate respondsToSelector:@selector(_webViewRequestPointerLock:)];
    m_delegateMethods.webViewDidRequestPointerLockCompletionHandler = [delegate respondsToSelector:@selector(_webViewDidRequestPointerLock:completionHandler:)];
    m_delegateMethods.webViewDidLosePointerLock = [delegate respondsToSelector:@selector(_webViewDidLosePointerLock:)];
#endif
#if ENABLE(CONTEXT_MENUS)
    m_delegateMethods.webViewContextMenuForElement = [delegate respondsToSelector:@selector(_webView:contextMenu:forElement:)];
    m_delegateMethods.webViewContextMenuForElementUserInfo = [delegate respondsToSelector:@selector(_webView:contextMenu:forElement:userInfo:)];
    m_delegateMethods.webViewGetContextMenuFromProposedMenuForElementUserInfoCompletionHandler = [delegate respondsToSelector:@selector(_webView:getContextMenuFromProposedMenu:forElement:userInfo:completionHandler:)];
#endif
    
    m_delegateMethods.webViewHasVideoInPictureInPictureDidChange = [delegate respondsToSelector:@selector(_webView:hasVideoInPictureInPictureDidChange:)];
    m_delegateMethods.webViewDidShowSafeBrowsingWarning = [delegate respondsToSelector:@selector(_webViewDidShowSafeBrowsingWarning:)];
}

#if ENABLE(CONTEXT_MENUS)
UIDelegate::ContextMenuClient::ContextMenuClient(UIDelegate& uiDelegate)
    : m_uiDelegate(uiDelegate)
{
}

UIDelegate::ContextMenuClient::~ContextMenuClient()
{
}

void UIDelegate::ContextMenuClient::menuFromProposedMenu(WebPageProxy&, NSMenu *menu, const WebHitTestResultData&, API::Object* userInfo, CompletionHandler<void(RetainPtr<NSMenu>&&)>&& completionHandler)
{
    if (!m_uiDelegate.m_delegateMethods.webViewContextMenuForElement
        && !m_uiDelegate.m_delegateMethods.webViewContextMenuForElementUserInfo
        && !m_uiDelegate.m_delegateMethods.webViewGetContextMenuFromProposedMenuForElementUserInfoCompletionHandler)
        return completionHandler(menu);

    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return completionHandler(menu);

    auto contextMenuElementInfo = adoptNS([[_WKContextMenuElementInfo alloc] init]);

    if (m_uiDelegate.m_delegateMethods.webViewGetContextMenuFromProposedMenuForElementUserInfoCompletionHandler) {
        auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:getContextMenuFromProposedMenu:forElement:userInfo:completionHandler:));
        [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate.m_webView getContextMenuFromProposedMenu:menu forElement:contextMenuElementInfo.get() userInfo:userInfo ? static_cast<id <NSSecureCoding>>(userInfo->wrapper()) : nil completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] (NSMenu *menu) mutable {
            if (checker->completionHandlerHasBeenCalled())
                return;
            checker->didCallCompletionHandler();
            completionHandler(menu);
        }).get()];
        return;
    }
    
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (m_uiDelegate.m_delegateMethods.webViewContextMenuForElement)
        return completionHandler([(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate.m_webView contextMenu:menu forElement:contextMenuElementInfo.get()]);

    completionHandler([(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate.m_webView contextMenu:menu forElement:contextMenuElementInfo.get() userInfo:userInfo ? static_cast<id <NSSecureCoding>>(userInfo->wrapper()) : nil]);
    ALLOW_DEPRECATED_DECLARATIONS_END
}
#endif

UIDelegate::UIClient::UIClient(UIDelegate& uiDelegate)
    : m_uiDelegate(uiDelegate)
{
}

UIDelegate::UIClient::~UIClient()
{
}

void UIDelegate::UIClient::createNewPage(WebPageProxy& page, Ref<API::FrameInfo>&& sourceFrameInfo, WebCore::ResourceRequest&& request, WebCore::WindowFeatures&& windowFeatures, NavigationActionData&& navigationActionData, CompletionHandler<void(RefPtr<WebPageProxy>&&)>&& completionHandler)
{
    auto delegate = m_uiDelegate.m_delegate.get();
    ASSERT(delegate);

    auto configuration = adoptNS([m_uiDelegate.m_webView->_configuration copy]);
    [configuration _setRelatedWebView:m_uiDelegate.m_webView];

    auto userInitiatedActivity = page.process().userInitiatedActivity(navigationActionData.userGestureTokenIdentifier);
    bool shouldOpenAppLinks = !hostsAreEqual(sourceFrameInfo->request().url(), request.url());
    auto apiNavigationAction = API::NavigationAction::create(WTFMove(navigationActionData), sourceFrameInfo.ptr(), nullptr, WTF::nullopt, WTFMove(request), URL(), shouldOpenAppLinks, WTFMove(userInitiatedActivity));

    auto apiWindowFeatures = API::WindowFeatures::create(windowFeatures);

    if (m_uiDelegate.m_delegateMethods.webViewCreateWebViewWithConfigurationForNavigationActionWindowFeaturesAsync) {
        auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:createWebViewWithConfiguration:forNavigationAction:windowFeatures:completionHandler:));

        [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate.m_webView createWebViewWithConfiguration:configuration.get() forNavigationAction:wrapper(apiNavigationAction) windowFeatures:wrapper(apiWindowFeatures) completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker), relatedWebView = RetainPtr<WKWebView>(m_uiDelegate.m_webView)] (WKWebView *webView) mutable {
            if (checker->completionHandlerHasBeenCalled())
                return;
            checker->didCallCompletionHandler();

            if (!webView) {
                completionHandler(nullptr);
                return;
            }

            if ([webView->_configuration _relatedWebView] != relatedWebView.get())
                [NSException raise:NSInternalInconsistencyException format:@"Returned WKWebView was not created with the given configuration."];

            completionHandler(webView->_page.get());
        }).get()];
        return;
    }
    if (!m_uiDelegate.m_delegateMethods.webViewCreateWebViewWithConfigurationForNavigationActionWindowFeatures)
        return completionHandler(nullptr);

    RetainPtr<WKWebView> webView = [delegate webView:m_uiDelegate.m_webView createWebViewWithConfiguration:configuration.get() forNavigationAction:wrapper(apiNavigationAction) windowFeatures:wrapper(apiWindowFeatures)];
    if (!webView)
        return completionHandler(nullptr);

    if ([webView->_configuration _relatedWebView] != m_uiDelegate.m_webView)
        [NSException raise:NSInternalInconsistencyException format:@"Returned WKWebView was not created with the given configuration."];
    completionHandler(webView->_page.get());
}

void UIDelegate::UIClient::runJavaScriptAlert(WebPageProxy*, const WTF::String& message, WebFrameProxy* webFrameProxy, const WebCore::SecurityOriginData& securityOriginData, Function<void()>&& completionHandler)
{
    if (!m_uiDelegate.m_delegateMethods.webViewRunJavaScriptAlertPanelWithMessageInitiatedByFrameCompletionHandler) {
        completionHandler();
        return;
    }

    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate) {
        completionHandler();
        return;
    }

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(webView:runJavaScriptAlertPanelWithMessage:initiatedByFrame:completionHandler:));
    [delegate webView:m_uiDelegate.m_webView runJavaScriptAlertPanelWithMessage:message initiatedByFrame:wrapper(API::FrameInfo::create(*webFrameProxy, securityOriginData.securityOrigin())) completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] {
        if (checker->completionHandlerHasBeenCalled())
            return;
        completionHandler();
        checker->didCallCompletionHandler();
    }).get()];
}

void UIDelegate::UIClient::runJavaScriptConfirm(WebPageProxy*, const WTF::String& message, WebFrameProxy* webFrameProxy, const WebCore::SecurityOriginData& securityOriginData, Function<void(bool)>&& completionHandler)
{
    if (!m_uiDelegate.m_delegateMethods.webViewRunJavaScriptConfirmPanelWithMessageInitiatedByFrameCompletionHandler) {
        completionHandler(false);
        return;
    }

    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate) {
        completionHandler(false);
        return;
    }

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(webView:runJavaScriptConfirmPanelWithMessage:initiatedByFrame:completionHandler:));
    [delegate webView:m_uiDelegate.m_webView runJavaScriptConfirmPanelWithMessage:message initiatedByFrame:wrapper(API::FrameInfo::create(*webFrameProxy, securityOriginData.securityOrigin())) completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] (BOOL result) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        completionHandler(result);
        checker->didCallCompletionHandler();
    }).get()];
}

void UIDelegate::UIClient::runJavaScriptPrompt(WebPageProxy*, const WTF::String& message, const WTF::String& defaultValue, WebFrameProxy* webFrameProxy, const WebCore::SecurityOriginData& securityOriginData, Function<void(const WTF::String&)>&& completionHandler)
{
    if (!m_uiDelegate.m_delegateMethods.webViewRunJavaScriptTextInputPanelWithPromptDefaultTextInitiatedByFrameCompletionHandler) {
        completionHandler(String());
        return;
    }

    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate) {
        completionHandler(String());
        return;
    }

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(webView:runJavaScriptTextInputPanelWithPrompt:defaultText:initiatedByFrame:completionHandler:));
    [delegate webView:m_uiDelegate.m_webView runJavaScriptTextInputPanelWithPrompt:message defaultText:defaultValue initiatedByFrame:wrapper(API::FrameInfo::create(*webFrameProxy, securityOriginData.securityOrigin())) completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] (NSString *result) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        completionHandler(result);
        checker->didCallCompletionHandler();
    }).get()];
}

void UIDelegate::UIClient::requestStorageAccessConfirm(WebPageProxy&, WebFrameProxy*, const WTF::String& requestingDomain, const WTF::String& currentDomain, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!m_uiDelegate.m_delegateMethods.webViewRequestStorageAccessPanelForTopPrivatelyControlledDomainUnderFirstPartyTopPrivatelyControlledDomainCompletionHandler) {
        completionHandler(true);
        return;
    }

    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate) {
        completionHandler(true);
        return;
    }
    
    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:requestStorageAccessPanelForDomain:underCurrentDomain:completionHandler:));
    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate.m_webView requestStorageAccessPanelForDomain:requestingDomain underCurrentDomain:currentDomain completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] (BOOL result) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        completionHandler(result);
        checker->didCallCompletionHandler();
    }).get()];
}

void UIDelegate::UIClient::decidePolicyForGeolocationPermissionRequest(WebKit::WebPageProxy&, WebKit::WebFrameProxy& frame, API::SecurityOrigin& securityOrigin, Function<void(bool)>& completionHandler)
{
    if (!m_uiDelegate.m_delegateMethods.webViewRequestGeolocationPermissionForFrameDecisionHandler)
        return;
    
    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return;

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:requestGeolocationPermissionForFrame:decisionHandler:));
    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate.m_webView requestGeolocationPermissionForFrame:wrapper(API::FrameInfo::create(frame, securityOrigin.securityOrigin())) decisionHandler:makeBlockPtr([completionHandler = std::exchange(completionHandler, nullptr), checker = WTFMove(checker)] (BOOL result) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();
        completionHandler(result);
    }).get()];
}

void UIDelegate::UIClient::didResignInputElementStrongPasswordAppearance(WebPageProxy&, API::Object* userInfo)
{
    if (!m_uiDelegate.m_delegateMethods.webViewDidResignInputElementStrongPasswordAppearanceWithUserInfo)
        return;

    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return;

    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate.m_webView didResignInputElementStrongPasswordAppearanceWithUserInfo:userInfo ? static_cast<id <NSSecureCoding>>(userInfo->wrapper()) : nil];
}

bool UIDelegate::UIClient::canRunBeforeUnloadConfirmPanel() const
{
    return m_uiDelegate.m_delegateMethods.webViewRunBeforeUnloadConfirmPanelWithMessageInitiatedByFrameCompletionHandler;
}

void UIDelegate::UIClient::runBeforeUnloadConfirmPanel(WebPageProxy*, const WTF::String& message, WebFrameProxy* webFrameProxy, const WebCore::SecurityOriginData& securityOriginData, Function<void(bool)>&& completionHandler)
{
    if (!m_uiDelegate.m_delegateMethods.webViewRunBeforeUnloadConfirmPanelWithMessageInitiatedByFrameCompletionHandler) {
        completionHandler(false);
        return;
    }

    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate) {
        completionHandler(false);
        return;
    }

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:runBeforeUnloadConfirmPanelWithMessage:initiatedByFrame:completionHandler:));
    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate.m_webView runBeforeUnloadConfirmPanelWithMessage:message initiatedByFrame:wrapper(API::FrameInfo::create(*webFrameProxy, securityOriginData.securityOrigin())) completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] (BOOL result) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        completionHandler(result);
        checker->didCallCompletionHandler();
    }).get()];
}

void UIDelegate::UIClient::exceededDatabaseQuota(WebPageProxy*, WebFrameProxy*, API::SecurityOrigin* securityOrigin, const WTF::String& databaseName, const WTF::String& displayName, unsigned long long currentQuota, unsigned long long currentOriginUsage, unsigned long long currentUsage, unsigned long long expectedUsage, Function<void (unsigned long long)>&& completionHandler)
{
    if (!m_uiDelegate.m_delegateMethods.webViewDecideDatabaseQuotaForSecurityOriginCurrentQuotaCurrentOriginUsageCurrentDatabaseUsageExpectedUsageDecisionHandler && !m_uiDelegate.m_delegateMethods.webViewDecideDatabaseQuotaForSecurityOriginDatabaseNameDisplayNameCurrentQuotaCurrentOriginUsageCurrentDatabaseUsageExpectedUsageDecisionHandler) {

        // Use 50 MB as the default database quota.
        unsigned long long defaultPerOriginDatabaseQuota = 50 * 1024 * 1024;

        completionHandler(defaultPerOriginDatabaseQuota);
        return;
    }

    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate) {
        completionHandler(currentQuota);
        return;
    }

    ASSERT(securityOrigin);

    if (m_uiDelegate.m_delegateMethods.webViewDecideDatabaseQuotaForSecurityOriginDatabaseNameDisplayNameCurrentQuotaCurrentOriginUsageCurrentDatabaseUsageExpectedUsageDecisionHandler) {
        auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:decideDatabaseQuotaForSecurityOrigin:databaseName:displayName:currentQuota:currentOriginUsage:currentDatabaseUsage:expectedUsage:decisionHandler:));
        [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate.m_webView decideDatabaseQuotaForSecurityOrigin:wrapper(*securityOrigin) databaseName:databaseName displayName:displayName currentQuota:currentQuota currentOriginUsage:currentOriginUsage currentDatabaseUsage:currentUsage expectedUsage:expectedUsage decisionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)](unsigned long long newQuota) {
            if (checker->completionHandlerHasBeenCalled())
                return;
            checker->didCallCompletionHandler();
            completionHandler(newQuota);
        }).get()];
        return;
    }

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:decideDatabaseQuotaForSecurityOrigin:currentQuota:currentOriginUsage:currentDatabaseUsage:expectedUsage:decisionHandler:));
    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate.m_webView decideDatabaseQuotaForSecurityOrigin:wrapper(*securityOrigin) currentQuota:currentQuota currentOriginUsage:currentOriginUsage currentDatabaseUsage:currentUsage expectedUsage:expectedUsage decisionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)](unsigned long long newQuota) {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();
        completionHandler(newQuota);
    }).get()];
}

static inline _WKFocusDirection toWKFocusDirection(WKFocusDirection direction)
{
    switch (direction) {
    case kWKFocusDirectionBackward:
        return _WKFocusDirectionBackward;
    case kWKFocusDirectionForward:
        return _WKFocusDirectionForward;
    }
    ASSERT_NOT_REACHED();
    return _WKFocusDirectionForward;
}

bool UIDelegate::UIClient::takeFocus(WebPageProxy*, WKFocusDirection direction)
{
    if (!m_uiDelegate.m_delegateMethods.webViewTakeFocus)
        return false;
    
    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return false;
    
    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate.m_webView takeFocus:toWKFocusDirection(direction)];
    return true;
}

#if PLATFORM(MAC)
bool UIDelegate::UIClient::canRunModal() const
{
    return m_uiDelegate.m_delegateMethods.webViewRunModal;
}

void UIDelegate::UIClient::runModal(WebPageProxy&)
{
    if (!m_uiDelegate.m_delegateMethods.webViewRunModal)
        return;
    
    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return;

    [(id <WKUIDelegatePrivate>)delegate _webViewRunModal:m_uiDelegate.m_webView];
}

float UIDelegate::UIClient::headerHeight(WebPageProxy&, WebFrameProxy& webFrameProxy)
{
    if (!m_uiDelegate.m_delegateMethods.webViewHeaderHeight)
        return 0;
    
    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return 0;
    
    return [(id <WKUIDelegatePrivate>)delegate _webViewHeaderHeight:m_uiDelegate.m_webView];
}

float UIDelegate::UIClient::footerHeight(WebPageProxy&, WebFrameProxy&)
{
    if (!m_uiDelegate.m_delegateMethods.webViewFooterHeight)
        return 0;
    
    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return 0;
    
    return [(id <WKUIDelegatePrivate>)delegate _webViewFooterHeight:m_uiDelegate.m_webView];
}

void UIDelegate::UIClient::drawHeader(WebPageProxy&, WebFrameProxy& frame, WebCore::FloatRect&& rect)
{
    if (!m_uiDelegate.m_delegateMethods.webViewDrawHeaderInRectForPageWithTitleURL)
        return;
    
    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return;
    
    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate.m_webView drawHeaderInRect:rect forPageWithTitle:frame.title() URL:frame.url()];
}

void UIDelegate::UIClient::drawFooter(WebPageProxy&, WebFrameProxy& frame, WebCore::FloatRect&& rect)
{
    if (!m_uiDelegate.m_delegateMethods.webViewDrawFooterInRectForPageWithTitleURL)
        return;
    
    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return;
    
    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate.m_webView drawFooterInRect:rect forPageWithTitle:frame.title() URL:frame.url()];
}

void UIDelegate::UIClient::pageDidScroll(WebPageProxy*)
{
    if (!m_uiDelegate.m_delegateMethods.webViewDidScroll)
        return;
    
    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return;
    
    [(id <WKUIDelegatePrivate>)delegate _webViewDidScroll:m_uiDelegate.m_webView];
}

void UIDelegate::UIClient::focus(WebPageProxy*)
{
    if (!m_uiDelegate.m_delegateMethods.focusWebView)
        return;
    
    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return;
    
    [(id <WKUIDelegatePrivate>)delegate _focusWebView:m_uiDelegate.m_webView];
}

void UIDelegate::UIClient::unfocus(WebPageProxy*)
{
    if (!m_uiDelegate.m_delegateMethods.unfocusWebView)
        return;
    
    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return;
    
    [(id <WKUIDelegatePrivate>)delegate _unfocusWebView:m_uiDelegate.m_webView];
}

static _WKPlugInUnavailabilityReason toWKPlugInUnavailabilityReason(WKPluginUnavailabilityReason reason)
{
    switch (reason) {
    case kWKPluginUnavailabilityReasonPluginMissing:
        return _WKPlugInUnavailabilityReasonPluginMissing;
    case kWKPluginUnavailabilityReasonPluginCrashed:
        return _WKPlugInUnavailabilityReasonPluginCrashed;
    case kWKPluginUnavailabilityReasonInsecurePluginVersion:
        return _WKPlugInUnavailabilityReasonInsecurePluginVersion;
    }
    ASSERT_NOT_REACHED();
    return _WKPlugInUnavailabilityReasonPluginMissing;
}
    
void UIDelegate::UIClient::unavailablePluginButtonClicked(WebPageProxy&, WKPluginUnavailabilityReason reason, API::Dictionary& plugInInfo)
{
    if (!m_uiDelegate.m_delegateMethods.webViewUnavailablePlugInButtonClicked)
        return;
    
    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return;

    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate.m_webView unavailablePlugInButtonClickedWithReason:toWKPlugInUnavailabilityReason(reason) plugInInfo:wrapper(plugInInfo)];
}
    
static _WKResourceLimit toWKResourceLimit(WKResourceLimit limit)
{
    switch (limit) {
    case kWKResourceLimitMemory:
        return _WKResourceLimitMemory;
    case kWKResourceLimitCPU:
        return _WKResourceLimitCPU;
    }
    ASSERT_NOT_REACHED();
    return _WKResourceLimitMemory;
}

void UIDelegate::UIClient::didExceedBackgroundResourceLimitWhileInForeground(WebPageProxy&, WKResourceLimit limit)
{
    if (!m_uiDelegate.m_delegateMethods.webViewDidExceedBackgroundResourceLimitWhileInForeground)
        return;
    
    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return;
    
    [static_cast<id <WKUIDelegatePrivate>>(delegate) _webView:m_uiDelegate.m_webView didExceedBackgroundResourceLimitWhileInForeground:toWKResourceLimit(limit)];
}

void UIDelegate::UIClient::didNotHandleWheelEvent(WebPageProxy*, const NativeWebWheelEvent& event)
{
    if (!m_uiDelegate.m_delegateMethods.webViewDidNotHandleWheelEvent)
        return;
    
    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return;
    
    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate.m_webView didNotHandleWheelEvent:event.nativeEvent()];
}

void UIDelegate::UIClient::setIsResizable(WebKit::WebPageProxy&, bool resizable)
{
    if (!m_uiDelegate.m_delegateMethods.webViewSetResizable)
        return;
    
    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return;
    
    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate.m_webView setResizable:resizable];
}

void UIDelegate::UIClient::setWindowFrame(WebKit::WebPageProxy&, const WebCore::FloatRect& frame)
{
    if (!m_uiDelegate.m_delegateMethods.webViewSetWindowFrame)
        return;
    
    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return;
    
    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate.m_webView setWindowFrame:frame];
}

void UIDelegate::UIClient::windowFrame(WebKit::WebPageProxy&, Function<void(WebCore::FloatRect)>&& completionHandler)
{
    if (!m_uiDelegate.m_delegateMethods.webViewGetWindowFrameWithCompletionHandler)
        return completionHandler({ });
    
    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return completionHandler({ });
    
    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate.m_webView getWindowFrameWithCompletionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:getWindowFrameWithCompletionHandler:))](CGRect frame) {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();
        completionHandler(frame);
    }).get()];
}

void UIDelegate::UIClient::mouseDidMoveOverElement(WebPageProxy&, const WebHitTestResultData& data, OptionSet<WebEvent::Modifier> modifiers, API::Object* userInfo)
{
    if (!m_uiDelegate.m_delegateMethods.webViewMouseDidMoveOverElementWithFlagsUserInfo)
        return;

    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return;

    auto apiHitTestResult = API::HitTestResult::create(data);
    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate.m_webView mouseDidMoveOverElement:wrapper(apiHitTestResult.get()) withFlags:WebEventFactory::toNSEventModifierFlags(modifiers) userInfo:userInfo ? static_cast<id <NSSecureCoding>>(userInfo->wrapper()) : nil];
}

static _WKAutoplayEventFlags toWKAutoplayEventFlags(OptionSet<WebCore::AutoplayEventFlags> flags)
{
    _WKAutoplayEventFlags wkFlags = _WKAutoplayEventFlagsNone;
    if (flags.contains(WebCore::AutoplayEventFlags::HasAudio))
        wkFlags |= _WKAutoplayEventFlagsHasAudio;
    if (flags.contains(WebCore::AutoplayEventFlags::PlaybackWasPrevented))
        wkFlags |= _WKAutoplayEventFlagsPlaybackWasPrevented;
    if (flags.contains(WebCore::AutoplayEventFlags::MediaIsMainContent))
        wkFlags |= _WKAutoplayEventFlagsMediaIsMainContent;
    
    return wkFlags;
}

static _WKAutoplayEvent toWKAutoplayEvent(WebCore::AutoplayEvent event)
{
    switch (event) {
    case WebCore::AutoplayEvent::DidPreventMediaFromPlaying:
        return _WKAutoplayEventDidPreventFromAutoplaying;
    case WebCore::AutoplayEvent::DidPlayMediaWithUserGesture:
        return _WKAutoplayEventDidPlayMediaWithUserGesture;
    case WebCore::AutoplayEvent::DidAutoplayMediaPastThresholdWithoutUserInterference:
        return _WKAutoplayEventDidAutoplayMediaPastThresholdWithoutUserInterference;
    case WebCore::AutoplayEvent::UserDidInterfereWithPlayback:
        return _WKAutoplayEventUserDidInterfereWithPlayback;
    }
    ASSERT_NOT_REACHED();
    return _WKAutoplayEventDidPlayMediaWithUserGesture;
}

void UIDelegate::UIClient::toolbarsAreVisible(WebPageProxy&, Function<void(bool)>&& completionHandler)
{
    if (!m_uiDelegate.m_delegateMethods.webViewGetToolbarsAreVisibleWithCompletionHandler)
        return completionHandler(true);
    
    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return completionHandler(true);
    
    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate.m_webView getToolbarsAreVisibleWithCompletionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:getToolbarsAreVisibleWithCompletionHandler:))](BOOL visible) {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();
        completionHandler(visible);
    }).get()];
}

void UIDelegate::UIClient::didClickAutoFillButton(WebPageProxy&, API::Object* userInfo)
{
    if (!m_uiDelegate.m_delegateMethods.webViewDidClickAutoFillButtonWithUserInfo)
        return;
    
    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return;
    
    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate.m_webView didClickAutoFillButtonWithUserInfo:userInfo ? static_cast<id <NSSecureCoding>>(userInfo->wrapper()) : nil];
}

void UIDelegate::UIClient::handleAutoplayEvent(WebPageProxy&, WebCore::AutoplayEvent event, OptionSet<WebCore::AutoplayEventFlags> flags)
{
    if (!m_uiDelegate.m_delegateMethods.webViewHandleAutoplayEventWithFlags)
        return;
    
    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return;
    
    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate.m_webView handleAutoplayEvent:toWKAutoplayEvent(event) withFlags:toWKAutoplayEventFlags(flags)];
}

void UIDelegate::UIClient::showPage(WebPageProxy*)
{
    if (!m_uiDelegate.m_delegateMethods.showWebView)
        return;

    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return;
    
    [(id <WKUIDelegatePrivate>)delegate _showWebView:m_uiDelegate.m_webView];
}
    
void UIDelegate::UIClient::saveDataToFileInDownloadsFolder(WebPageProxy*, const WTF::String& suggestedFilename, const WTF::String& mimeType, const URL& originatingURL, API::Data& data)
{
    if (!m_uiDelegate.m_delegateMethods.webViewSaveDataToFileSuggestedFilenameMimeTypeOriginatingURL)
        return;
    
    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return;

    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate.m_webView saveDataToFile:wrapper(data) suggestedFilename:suggestedFilename mimeType:mimeType originatingURL:originatingURL];
}

void UIDelegate::UIClient::decidePolicyForNotificationPermissionRequest(WebKit::WebPageProxy&, API::SecurityOrigin& securityOrigin, WTF::Function<void(bool)>&& completionHandler)
{
    if (!m_uiDelegate.m_delegateMethods.webViewRequestNotificationPermissionForSecurityOriginDecisionHandler)
        return completionHandler(false);
    
    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return completionHandler(false);

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:requestNotificationPermissionForSecurityOrigin:decisionHandler:));
    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate.m_webView requestNotificationPermissionForSecurityOrigin:wrapper(securityOrigin) decisionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] (BOOL result) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();
        completionHandler(result);
    }).get()];
}

bool UIDelegate::UIClient::runOpenPanel(WebPageProxy*, WebFrameProxy* webFrameProxy, const WebCore::SecurityOriginData& securityOriginData, API::OpenPanelParameters* openPanelParameters, WebOpenPanelResultListenerProxy* listener)
{
    if (!m_uiDelegate.m_delegateMethods.webViewRunOpenPanelWithParametersInitiatedByFrameCompletionHandler)
        return false;

    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return false;

    auto frame = API::FrameInfo::create(*webFrameProxy, securityOriginData.securityOrigin());

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(webView:runOpenPanelWithParameters:initiatedByFrame:completionHandler:));

    [delegate webView:m_uiDelegate.m_webView runOpenPanelWithParameters:wrapper(*openPanelParameters) initiatedByFrame:wrapper(frame) completionHandler:makeBlockPtr([checker = WTFMove(checker), listener = WTFMove(listener)] (NSArray *URLs) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();

        if (!URLs) {
            listener->cancel();
            return;
        }

        Vector<String> filenames;
        for (NSURL *url in URLs)
            filenames.append(url.path);

        listener->chooseFiles(filenames);
    }).get()];

    return true;
}
#endif

void UIDelegate::UIClient::didChangeFontAttributes(const WebCore::FontAttributes& fontAttributes)
{
    if (!needsFontAttributes())
        return;

    auto privateUIDelegate = (id <WKUIDelegatePrivate>)m_uiDelegate.m_delegate.get();
    [privateUIDelegate _webView:m_uiDelegate.m_webView didChangeFontAttributes:fontAttributes.createDictionary().get()];
}

#if ENABLE(MEDIA_STREAM)
static void requestUserMediaAuthorizationForFrame(const WebFrameProxy& frame, API::SecurityOrigin& topLevelOrigin, UserMediaPermissionRequestProxy& request, id <WKUIDelegatePrivate> delegate, WKWebView& webView)
{
    auto checker = CompletionHandlerCallChecker::create(delegate, @selector(_webView:requestMediaCaptureAuthorization:decisionHandler:));
    auto decisionHandler = makeBlockPtr([protectedRequest = makeRef(request), checker = WTFMove(checker)](BOOL authorized) {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();

        if (!authorized) {
            protectedRequest->deny(UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::PermissionDenied);
            return;
        }
        const String& videoDeviceUID = (protectedRequest->requiresVideoCapture() || protectedRequest->requiresDisplayCapture()) ? protectedRequest->videoDeviceUIDs().first() : String();
        const String& audioDeviceUID = protectedRequest->requiresAudioCapture() ? protectedRequest->audioDeviceUIDs().first() : String();
        protectedRequest->allow(audioDeviceUID, videoDeviceUID);
    });

    _WKCaptureDevices devices = 0;
    if (request.requiresAudioCapture())
        devices |= _WKCaptureDeviceMicrophone;
    if (request.requiresVideoCapture())
        devices |= _WKCaptureDeviceCamera;
    if (request.requiresDisplayCapture()) {
        devices |= _WKCaptureDeviceDisplay;
        ASSERT(!(devices & _WKCaptureDeviceCamera));
    }

    const auto* mainFrame = frame.page()->mainFrame();

    // FIXME: Provide a specific delegate for display capture.
    if (!request.requiresDisplayCapture() && [delegate respondsToSelector:@selector(_webView:requestMediaCaptureAuthorization:decisionHandler:)]) {

        [delegate _webView:&webView requestMediaCaptureAuthorization:devices decisionHandler:decisionHandler.get()];
        return;
    }

    URL requestFrameURL { URL(), frame.url() };
    URL mainFrameURL { URL(), mainFrame->url() };

    [delegate _webView:&webView requestUserMediaAuthorizationForDevices:devices url:requestFrameURL mainFrameURL:mainFrameURL decisionHandler:decisionHandler.get()];
}
#endif

void UIDelegate::UIClient::decidePolicyForUserMediaPermissionRequest(WebPageProxy& page, WebFrameProxy& frame, API::SecurityOrigin& userMediaOrigin, API::SecurityOrigin& topLevelOrigin, UserMediaPermissionRequestProxy& request)
{
#if ENABLE(MEDIA_STREAM)
    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate || !m_uiDelegate.m_delegateMethods.webViewRequestMediaCaptureAuthorizationForFrameDecisionHandler) {
        request.deny(UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::UserMediaDisabled);
        return;
    }

    bool requiresAudioCapture = request.requiresAudioCapture();
    bool requiresVideoCapture = request.requiresVideoCapture();
    bool requiresDisplayCapture = request.requiresDisplayCapture();
    if (!requiresAudioCapture && !requiresVideoCapture && !requiresDisplayCapture) {
        request.deny(UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::NoConstraints);
        return;
    }

#if HAVE(AUTHORIZATION_STATUS_FOR_MEDIA_TYPE)
    bool usingMockCaptureDevices = page.preferences().mockCaptureDevicesEnabled();
    auto requestCameraAuthorization = makeBlockPtr([this, &frame, protectedRequest = makeRef(request), webView = RetainPtr<WKWebView>(m_uiDelegate.m_webView), topLevelOrigin = makeRef(topLevelOrigin), usingMockCaptureDevices]() mutable {

        if (!protectedRequest->requiresVideoCapture()) {
            requestUserMediaAuthorizationForFrame(frame, topLevelOrigin, protectedRequest, (id <WKUIDelegatePrivate>)m_uiDelegate.m_delegate.get(), *webView.get());
            return;
        }
        AVAuthorizationStatus cameraAuthorizationStatus = usingMockCaptureDevices ? AVAuthorizationStatusAuthorized : [getAVCaptureDeviceClass() authorizationStatusForMediaType:getAVMediaTypeVideo()];
        switch (cameraAuthorizationStatus) {
        case AVAuthorizationStatusAuthorized:
            requestUserMediaAuthorizationForFrame(frame, topLevelOrigin, protectedRequest, (id <WKUIDelegatePrivate>)m_uiDelegate.m_delegate.get(), *webView.get());
            break;
        case AVAuthorizationStatusDenied:
        case AVAuthorizationStatusRestricted:
            protectedRequest->deny(UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::PermissionDenied);
            return;
        case AVAuthorizationStatusNotDetermined:
            auto decisionHandler = makeBlockPtr([this, &frame, protectedRequest = makeRef(protectedRequest.get()), webView = RetainPtr<WKWebView>(m_uiDelegate.m_webView), topLevelOrigin = WTFMove(topLevelOrigin)](BOOL authorized) {
                if (!authorized) {
                    protectedRequest->deny(UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::PermissionDenied);
                    return;
                }
                requestUserMediaAuthorizationForFrame(frame, topLevelOrigin, protectedRequest, (id <WKUIDelegatePrivate>)m_uiDelegate.m_delegate.get(), *webView.get());
            });

            [getAVCaptureDeviceClass() requestAccessForMediaType:getAVMediaTypeVideo() completionHandler:decisionHandler.get()];
            break;
        }
    });

    if (requiresAudioCapture) {
        AVAuthorizationStatus microphoneAuthorizationStatus = usingMockCaptureDevices ? AVAuthorizationStatusAuthorized : [getAVCaptureDeviceClass() authorizationStatusForMediaType:getAVMediaTypeAudio()];
        switch (microphoneAuthorizationStatus) {
        case AVAuthorizationStatusAuthorized:
            requestCameraAuthorization();
            break;
        case AVAuthorizationStatusDenied:
        case AVAuthorizationStatusRestricted:
            request.deny(UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::PermissionDenied);
            return;
        case AVAuthorizationStatusNotDetermined:
            auto decisionHandler = makeBlockPtr([protectedRequest = makeRef(request), requestCameraAuthorization](BOOL authorized) {
                if (!authorized) {
                    protectedRequest->deny(UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::PermissionDenied);
                    return;
                }
                requestCameraAuthorization();
            });

            [getAVCaptureDeviceClass() requestAccessForMediaType:getAVMediaTypeAudio() completionHandler:decisionHandler.get()];
            break;
        }
    } else
        requestCameraAuthorization();
#else
    requestUserMediaAuthorizationForFrame(frame, topLevelOrigin, request, (id <WKUIDelegatePrivate>)m_uiDelegate.m_delegate.get(), *m_uiDelegate.m_webView);
#endif
#endif
}

void UIDelegate::UIClient::checkUserMediaPermissionForOrigin(WebPageProxy& page, WebFrameProxy& frame, API::SecurityOrigin& userMediaOrigin, API::SecurityOrigin& topLevelOrigin, UserMediaPermissionCheckProxy& request)
{
    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate || !m_uiDelegate.m_delegateMethods.webViewIsMediaCaptureAuthorizedForFrameDecisionHandler) {
        request.setUserMediaAccessInfo(false);
        return;
    }

    const auto* mainFrame = frame.page()->mainFrame();

    if ([delegate respondsToSelector:@selector(_webView:includeSensitiveMediaDeviceDetails:)]) {
        auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:includeSensitiveMediaDeviceDetails:));
        auto decisionHandler = makeBlockPtr([protectedRequest = makeRef(request), checker = WTFMove(checker)](BOOL includeSensitiveDetails) {
            if (checker->completionHandlerHasBeenCalled())
                return;
            checker->didCallCompletionHandler();

            protectedRequest->setUserMediaAccessInfo(includeSensitiveDetails);
        });

        [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate.m_webView includeSensitiveMediaDeviceDetails:decisionHandler.get()];
        return;
    }

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:checkUserMediaPermissionForURL:mainFrameURL:frameIdentifier:decisionHandler:));
    auto decisionHandler = makeBlockPtr([protectedRequest = makeRef(request), checker = WTFMove(checker)](NSString*, BOOL authorized) {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();

        protectedRequest->setUserMediaAccessInfo(authorized);
    });

    URL requestFrameURL { URL(), frame.url() };
    URL mainFrameURL { URL(), mainFrame->url() };

    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate.m_webView checkUserMediaPermissionForURL:requestFrameURL mainFrameURL:mainFrameURL frameIdentifier:frame.frameID() decisionHandler:decisionHandler.get()];
}

void UIDelegate::UIClient::mediaCaptureStateDidChange(WebCore::MediaProducer::MediaStateFlags state)
{
    WKWebView *webView = m_uiDelegate.m_webView;

    [webView didChangeValueForKey:@"mediaCaptureState"];

    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate || !m_uiDelegate.m_delegateMethods.webViewMediaCaptureStateDidChange)
        return;

    [(id <WKUIDelegatePrivate>)delegate _webView:webView mediaCaptureStateDidChange:toWKMediaCaptureState(state)];
}

void UIDelegate::UIClient::reachedApplicationCacheOriginQuota(WebPageProxy*, const WebCore::SecurityOrigin& securityOrigin, uint64_t currentQuota, uint64_t totalBytesNeeded, Function<void (unsigned long long)>&& completionHandler)
{
    if (!m_uiDelegate.m_delegateMethods.webViewDecideWebApplicationCacheQuotaForSecurityOriginCurrentQuotaTotalBytesNeeded) {
        completionHandler(currentQuota);
        return;
    }

    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate) {
        completionHandler(currentQuota);
        return;
    }

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:decideWebApplicationCacheQuotaForSecurityOrigin:currentQuota:totalBytesNeeded:decisionHandler:));
    auto apiOrigin = API::SecurityOrigin::create(securityOrigin);
    
    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate.m_webView decideWebApplicationCacheQuotaForSecurityOrigin:wrapper(apiOrigin.get()) currentQuota:currentQuota totalBytesNeeded:totalBytesNeeded decisionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)](unsigned long long newQuota) {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();
        completionHandler(newQuota);
    }).get()];
}

void UIDelegate::UIClient::printFrame(WebPageProxy&, WebFrameProxy& webFrameProxy)
{
    if (!m_uiDelegate.m_delegateMethods.webViewPrintFrame)
        return;

    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return;

    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate.m_webView printFrame:wrapper(API::FrameHandle::create(webFrameProxy.frameID()))];
}

void UIDelegate::UIClient::close(WebPageProxy*)
{
    if (m_uiDelegate.m_delegateMethods.webViewClose) {
        auto delegate = m_uiDelegate.m_delegate.get();
        if (!delegate)
            return;

        [(id <WKUIDelegatePrivate>)delegate _webViewClose:m_uiDelegate.m_webView];
        return;
    }

    if (!m_uiDelegate.m_delegateMethods.webViewDidClose)
        return;

    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return;

    [delegate webViewDidClose:m_uiDelegate.m_webView];
}

void UIDelegate::UIClient::fullscreenMayReturnToInline(WebPageProxy*)
{
    if (!m_uiDelegate.m_delegateMethods.webViewFullscreenMayReturnToInline)
        return;
    
    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return;
    
    [(id <WKUIDelegatePrivate>)delegate _webViewFullscreenMayReturnToInline:m_uiDelegate.m_webView];
}

void UIDelegate::UIClient::didEnterFullscreen(WebPageProxy*)
{
    if (!m_uiDelegate.m_delegateMethods.webViewDidEnterFullscreen)
        return;

    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return;

    [(id <WKUIDelegatePrivate>)delegate _webViewDidEnterFullscreen:m_uiDelegate.m_webView];
}

void UIDelegate::UIClient::didExitFullscreen(WebPageProxy*)
{
    if (!m_uiDelegate.m_delegateMethods.webViewDidExitFullscreen)
        return;

    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return;

    [(id <WKUIDelegatePrivate>)delegate _webViewDidExitFullscreen:m_uiDelegate.m_webView];
}
    
#if PLATFORM(IOS_FAMILY)
#if HAVE(APP_LINKS)
bool UIDelegate::UIClient::shouldIncludeAppLinkActionsForElement(_WKActivatedElementInfo *elementInfo)
{
    if (!m_uiDelegate.m_delegateMethods.webViewShouldIncludeAppLinkActionsForElement)
        return true;

    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return true;

    return [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate.m_webView shouldIncludeAppLinkActionsForElement:elementInfo];
}
#endif

RetainPtr<NSArray> UIDelegate::UIClient::actionsForElement(_WKActivatedElementInfo *elementInfo, RetainPtr<NSArray> defaultActions)
{
    if (!m_uiDelegate.m_delegateMethods.webViewActionsForElementDefaultActions)
        return defaultActions;

    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return defaultActions;

    return [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate.m_webView actionsForElement:elementInfo defaultActions:defaultActions.get()];
}

void UIDelegate::UIClient::didNotHandleTapAsClick(const WebCore::IntPoint& point)
{
    if (!m_uiDelegate.m_delegateMethods.webViewDidNotHandleTapAsClickAtPoint)
        return;

    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return;

    [static_cast<id <WKUIDelegatePrivate>>(delegate) _webView:m_uiDelegate.m_webView didNotHandleTapAsClickAtPoint:point];
}

UIViewController *UIDelegate::UIClient::presentingViewController()
{
    if (!m_uiDelegate.m_delegateMethods.presentingViewControllerForWebView)
        return nullptr;

    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return nullptr;

    return [static_cast<id <WKUIDelegatePrivate>>(delegate) _presentingViewControllerForWebView:m_uiDelegate.m_webView];
}

#endif

NSDictionary *UIDelegate::UIClient::dataDetectionContext()
{
    if (!m_uiDelegate.m_delegateMethods.dataDetectionContextForWebView)
        return nullptr;

    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return nullptr;

    return [static_cast<id <WKUIDelegatePrivate>>(delegate) _dataDetectionContextForWebView:m_uiDelegate.m_webView];
}

#if ENABLE(POINTER_LOCK)

void UIDelegate::UIClient::requestPointerLock(WebPageProxy* page)
{
    if (!m_uiDelegate.m_delegateMethods.webViewRequestPointerLock && !m_uiDelegate.m_delegateMethods.webViewDidRequestPointerLockCompletionHandler)
        return;

    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return;

    if (m_uiDelegate.m_delegateMethods.webViewRequestPointerLock) {
        [static_cast<id <WKUIDelegatePrivate>>(delegate) _webViewRequestPointerLock:m_uiDelegate.m_webView];
        return;
    }

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webViewDidRequestPointerLock:completionHandler:));
    [static_cast<id <WKUIDelegatePrivate>>(delegate) _webViewDidRequestPointerLock:m_uiDelegate.m_webView completionHandler:makeBlockPtr([checker = WTFMove(checker), page = makeRefPtr(page)] (BOOL allow) {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();

        if (allow)
            page->didAllowPointerLock();
        else
            page->didDenyPointerLock();
    }).get()];
}

void UIDelegate::UIClient::didLosePointerLock(WebPageProxy*)
{
    if (!m_uiDelegate.m_delegateMethods.webViewDidLosePointerLock)
        return;

    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return;

    [static_cast<id <WKUIDelegatePrivate>>(delegate) _webViewDidLosePointerLock:m_uiDelegate.m_webView];
}

#endif
    
void UIDelegate::UIClient::didShowSafeBrowsingWarning()
{
    if (!m_uiDelegate.m_delegateMethods.webViewDidShowSafeBrowsingWarning)
        return;

    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return;

    [static_cast<id <WKUIDelegatePrivate>>(delegate) _webViewDidShowSafeBrowsingWarning:m_uiDelegate.m_webView];
}

void UIDelegate::UIClient::hasVideoInPictureInPictureDidChange(WebPageProxy*, bool hasVideoInPictureInPicture)
{
    if (!m_uiDelegate.m_delegateMethods.webViewHasVideoInPictureInPictureDidChange)
        return;
    
    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return;
    
    [static_cast<id <WKUIDelegatePrivate>>(delegate) _webView:m_uiDelegate.m_webView hasVideoInPictureInPictureDidChange:hasVideoInPictureInPicture];
}

void UIDelegate::UIClient::imageOrMediaDocumentSizeChanged(const WebCore::IntSize& newSize)
{
    if (!m_uiDelegate.m_delegateMethods.webViewImageOrMediaDocumentSizeChanged)
        return;

    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return;

    [static_cast<id <WKUIDelegatePrivate>>(delegate) _webView:m_uiDelegate.m_webView imageOrMediaDocumentSizeChanged:newSize];
}

} // namespace WebKit
