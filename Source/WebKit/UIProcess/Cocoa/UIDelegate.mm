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

#import "APIArray.h"
#import "APIFrameInfo.h"
#import "APIHitTestResult.h"
#import "APIInspectorConfiguration.h"
#import "CompletionHandlerCallChecker.h"
#import "MediaPermissionUtilities.h"
#import "MediaUtilities.h"
#import "NativeWebWheelEvent.h"
#import "NavigationActionData.h"
#import "UserMediaPermissionCheckProxy.h"
#import "UserMediaPermissionRequestManagerProxy.h"
#import "UserMediaPermissionRequestProxy.h"
#import "WKFrameInfoInternal.h"
#import "WKNSData.h"
#import "WKNSDictionary.h"
#import "WKNavigationActionInternal.h"
#import "WKOpenPanelParametersInternal.h"
#import "WKSecurityOriginInternal.h"
#import "WKStorageAccessAlert.h"
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
#import "_WKInspectorConfigurationInternal.h"
#import "_WKInspectorInternal.h"
#import "_WKModalContainerInfoInternal.h"
#import "_WKWebAuthenticationPanelInternal.h"
#import <AVFoundation/AVCaptureDevice.h>
#import <AVFoundation/AVMediaFormat.h>
#import <WebCore/FontAttributes.h>
#import <WebCore/SecurityOrigin.h>
#import <wtf/BlockPtr.h>
#import <wtf/URL.h>

#if PLATFORM(IOS_FAMILY)
#import "TapHandlingResult.h"
#import "WKWebViewIOS.h"
#import "WebIOSEventFactory.h"
#endif

#import <pal/cocoa/AVFoundationSoftLink.h>

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
    return makeUnique<ContextMenuClient>(*this);
}
#endif

std::unique_ptr<API::UIClient> UIDelegate::createUIClient()
{
    return makeUnique<UIClient>(*this);
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
    m_delegateMethods.webViewRequestStorageAccessPanelUnderFirstPartyCompletionHandler = [delegate respondsToSelector:@selector(_webView:requestStorageAccessPanelForDomain:underCurrentDomain:completionHandler:)];
    m_delegateMethods.webViewRunBeforeUnloadConfirmPanelWithMessageInitiatedByFrameCompletionHandler = [delegate respondsToSelector:@selector(_webView:runBeforeUnloadConfirmPanelWithMessage:initiatedByFrame:completionHandler:)];
    m_delegateMethods.webViewRequestGeolocationPermissionForOriginDecisionHandler = [delegate respondsToSelector:@selector(_webView:requestGeolocationPermissionForOrigin:initiatedByFrame:decisionHandler:)];
    m_delegateMethods.webViewRequestGeolocationPermissionForFrameDecisionHandler = [delegate respondsToSelector:@selector(_webView:requestGeolocationPermissionForFrame:decisionHandler:)];
    m_delegateMethods.webViewDidResignInputElementStrongPasswordAppearanceWithUserInfo = [delegate respondsToSelector:@selector(_webView:didResignInputElementStrongPasswordAppearanceWithUserInfo:)];
    m_delegateMethods.webViewTakeFocus = [delegate respondsToSelector:@selector(_webView:takeFocus:)];
    m_delegateMethods.webViewHandleAutoplayEventWithFlags = [delegate respondsToSelector:@selector(_webView:handleAutoplayEvent:withFlags:)];
#if PLATFORM(MAC) || HAVE(UIKIT_WITH_MOUSE_SUPPORT)
    m_delegateMethods.webViewMouseDidMoveOverElementWithFlagsUserInfo = [delegate respondsToSelector:@selector(_webView:mouseDidMoveOverElement:withFlags:userInfo:)];
#endif

#if PLATFORM(MAC)
    m_delegateMethods.showWebView = [delegate respondsToSelector:@selector(_showWebView:)];
    m_delegateMethods.focusWebView = [delegate respondsToSelector:@selector(_focusWebView:)];
    m_delegateMethods.focusWebViewFromServiceWorker = [delegate respondsToSelector:@selector(_focusWebViewFromServiceWorker:)];
    m_delegateMethods.unfocusWebView = [delegate respondsToSelector:@selector(_unfocusWebView:)];
    m_delegateMethods.webViewRunModal = [delegate respondsToSelector:@selector(_webViewRunModal:)];
    m_delegateMethods.webViewDidScroll = [delegate respondsToSelector:@selector(_webViewDidScroll:)];
    m_delegateMethods.webViewGetToolbarsAreVisibleWithCompletionHandler = [delegate respondsToSelector:@selector(_webView:getToolbarsAreVisibleWithCompletionHandler:)];
    m_delegateMethods.webViewDidNotHandleWheelEvent = [delegate respondsToSelector:@selector(_webView:didNotHandleWheelEvent:)];
    m_delegateMethods.webViewSetResizable = [delegate respondsToSelector:@selector(_webView:setResizable:)];
    m_delegateMethods.webViewGetWindowFrameWithCompletionHandler = [delegate respondsToSelector:@selector(_webView:getWindowFrameWithCompletionHandler:)];
    m_delegateMethods.webViewSetWindowFrame = [delegate respondsToSelector:@selector(_webView:setWindowFrame:)];
    m_delegateMethods.webViewUnavailablePlugInButtonClicked = [delegate respondsToSelector:@selector(_webView:unavailablePlugInButtonClickedWithReason:plugInInfo:)];
    m_delegateMethods.webViewDidClickAutoFillButtonWithUserInfo = [delegate respondsToSelector:@selector(_webView:didClickAutoFillButtonWithUserInfo:)];
    m_delegateMethods.webViewDrawHeaderInRectForPageWithTitleURL = [delegate respondsToSelector:@selector(_webView:drawHeaderInRect:forPageWithTitle:URL:)];
    m_delegateMethods.webViewDrawFooterInRectForPageWithTitleURL = [delegate respondsToSelector:@selector(_webView:drawFooterInRect:forPageWithTitle:URL:)];
    m_delegateMethods.webViewHeaderHeight = [delegate respondsToSelector:@selector(_webViewHeaderHeight:)];
    m_delegateMethods.webViewFooterHeight = [delegate respondsToSelector:@selector(_webViewFooterHeight:)];
    m_delegateMethods.webViewSaveDataToFileSuggestedFilenameMimeTypeOriginatingURL = [delegate respondsToSelector:@selector(_webView:saveDataToFile:suggestedFilename:mimeType:originatingURL:)];
    m_delegateMethods.webViewRunOpenPanelWithParametersInitiatedByFrameCompletionHandler = [delegate respondsToSelector:@selector(webView:runOpenPanelWithParameters:initiatedByFrame:completionHandler:)];
    m_delegateMethods.webViewConfigurationForLocalInspector = [delegate respondsToSelector:@selector(_webView:configurationForLocalInspector:)];
    m_delegateMethods.webViewDidAttachLocalInspector = [delegate respondsToSelector:@selector(_webView:didAttachLocalInspector:)];
    m_delegateMethods.webViewWillCloseLocalInspector = [delegate respondsToSelector:@selector(_webView:willCloseLocalInspector:)];
#endif
#if ENABLE(DEVICE_ORIENTATION)
    m_delegateMethods.webViewRequestDeviceOrientationAndMotionPermissionForOriginDecisionHandler = [delegate respondsToSelector:@selector(webView:requestDeviceOrientationAndMotionPermissionForOrigin:initiatedByFrame:decisionHandler:)];
#endif
    m_delegateMethods.webViewDecideDatabaseQuotaForSecurityOriginCurrentQuotaCurrentOriginUsageCurrentDatabaseUsageExpectedUsageDecisionHandler = [delegate respondsToSelector:@selector(_webView:decideDatabaseQuotaForSecurityOrigin:currentQuota:currentOriginUsage:currentDatabaseUsage:expectedUsage:decisionHandler:)];
    m_delegateMethods.webViewDecideDatabaseQuotaForSecurityOriginDatabaseNameDisplayNameCurrentQuotaCurrentOriginUsageCurrentDatabaseUsageExpectedUsageDecisionHandler = [delegate respondsToSelector:@selector(_webView:decideDatabaseQuotaForSecurityOrigin:databaseName:displayName:currentQuota:currentOriginUsage:currentDatabaseUsage:expectedUsage:decisionHandler:)];
    m_delegateMethods.webViewDecideWebApplicationCacheQuotaForSecurityOriginCurrentQuotaTotalBytesNeeded = [delegate respondsToSelector:@selector(_webView:decideWebApplicationCacheQuotaForSecurityOrigin:currentQuota:totalBytesNeeded:decisionHandler:)];
    m_delegateMethods.webViewPrintFrame = [delegate respondsToSelector:@selector(_webView:printFrame:)];
    m_delegateMethods.webViewPrintFramePDFFirstPageSizeCompletionHandler = [delegate respondsToSelector:@selector(_webView:printFrame:pdfFirstPageSize:completionHandler:)];
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
    m_delegateMethods.webViewStatusBarWasTapped = [delegate respondsToSelector:@selector(_webViewStatusBarWasTapped:)];
#endif
#if PLATFORM(IOS)
    m_delegateMethods.webViewLockScreenOrientation = [delegate respondsToSelector:@selector(_webViewLockScreenOrientation:lockType:)];
    m_delegateMethods.webViewUnlockScreenOrientation = [delegate respondsToSelector:@selector(_webViewUnlockScreenOrientation:)];
#endif
    m_delegateMethods.presentingViewControllerForWebView = [delegate respondsToSelector:@selector(_presentingViewControllerForWebView:)];
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
    m_delegateMethods.webViewShouldAllowPDFAtURLToOpenFromFrameCompletionHandler = [delegate respondsToSelector:@selector(_webView:shouldAllowPDFAtURL:toOpenFromFrame:completionHandler:)];

#if ENABLE(WEB_AUTHN)
    m_delegateMethods.webViewRunWebAuthenticationPanelInitiatedByFrameCompletionHandler = [delegate respondsToSelector:@selector(_webView:runWebAuthenticationPanel:initiatedByFrame:completionHandler:)];
    m_delegateMethods.webViewRequestWebAuthenticationNoGestureForOriginCompletionHandler = [delegate respondsToSelector:@selector(_webView:requestWebAuthenticationNoGestureForOrigin:completionHandler:)];
#endif
    
    m_delegateMethods.webViewDidEnableInspectorBrowserDomain = [delegate respondsToSelector:@selector(_webViewDidEnableInspectorBrowserDomain:)];
    m_delegateMethods.webViewDidDisableInspectorBrowserDomain = [delegate respondsToSelector:@selector(_webViewDidDisableInspectorBrowserDomain:)];

#if ENABLE(WEBXR)
    m_delegateMethods.webViewRequestPermissionForXRSessionOriginModeAndFeaturesWithCompletionHandler = [delegate respondsToSelector:@selector(_webView:requestPermissionForXRSessionOrigin:mode:grantedFeatures:consentRequiredFeatures:consentOptionalFeatures:completionHandler:)];
    m_delegateMethods.webViewStartXRSessionWithCompletionHandler = [delegate respondsToSelector:@selector(_webView:startXRSessionWithCompletionHandler:)];
    m_delegateMethods.webViewEndXRSession = [delegate respondsToSelector:@selector(_webViewEndXRSession:)];
#endif
    m_delegateMethods.webViewRequestNotificationPermissionForSecurityOriginDecisionHandler = [delegate respondsToSelector:@selector(_webView:requestNotificationPermissionForSecurityOrigin:decisionHandler:)];
    m_delegateMethods.webViewRequestCookieConsentWithMoreInfoHandlerDecisionHandler = [delegate respondsToSelector:@selector(_webView:requestCookieConsentWithMoreInfoHandler:decisionHandler:)];
    m_delegateMethods.webViewDecidePolicyForModalContainerDecisionHandler = [delegate respondsToSelector:@selector(_webView:decidePolicyForModalContainer:decisionHandler:)];

    m_delegateMethods.webViewUpdatedAppBadge = [delegate respondsToSelector:@selector(_webView:updatedAppBadge:)];
    m_delegateMethods.webViewUpdatedClientBadge = [delegate respondsToSelector:@selector(_webView:updatedClientBadge:)];
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
    if (!m_uiDelegate)
        return completionHandler(menu);

    if (!m_uiDelegate->m_delegateMethods.webViewContextMenuForElement
        && !m_uiDelegate->m_delegateMethods.webViewContextMenuForElementUserInfo
        && !m_uiDelegate->m_delegateMethods.webViewGetContextMenuFromProposedMenuForElementUserInfoCompletionHandler)
        return completionHandler(menu);

    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return completionHandler(menu);

    auto contextMenuElementInfo = adoptNS([[_WKContextMenuElementInfo alloc] init]);

    if (m_uiDelegate->m_delegateMethods.webViewGetContextMenuFromProposedMenuForElementUserInfoCompletionHandler) {
        auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:getContextMenuFromProposedMenu:forElement:userInfo:completionHandler:));
        [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate->m_webView.get().get() getContextMenuFromProposedMenu:menu forElement:contextMenuElementInfo.get() userInfo:userInfo ? static_cast<id <NSSecureCoding>>(userInfo->wrapper()) : nil completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] (NSMenu *menu) mutable {
            if (checker->completionHandlerHasBeenCalled())
                return;
            checker->didCallCompletionHandler();
            completionHandler(menu);
        }).get()];
        return;
    }
    
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (m_uiDelegate->m_delegateMethods.webViewContextMenuForElement)
        return completionHandler([(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate->m_webView.get().get() contextMenu:menu forElement:contextMenuElementInfo.get()]);

    completionHandler([(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate->m_webView.get().get() contextMenu:menu forElement:contextMenuElementInfo.get() userInfo:userInfo ? static_cast<id <NSSecureCoding>>(userInfo->wrapper()) : nil]);
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

#if PLATFORM(MAC) || HAVE(UIKIT_WITH_MOUSE_SUPPORT)
void UIDelegate::UIClient::mouseDidMoveOverElement(WebPageProxy&, const WebHitTestResultData& data, OptionSet<WebEventModifier> modifiers, API::Object* userInfo)
{
    if (!m_uiDelegate)
        return;

    if (!m_uiDelegate->m_delegateMethods.webViewMouseDidMoveOverElementWithFlagsUserInfo)
        return;

    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    auto apiHitTestResult = API::HitTestResult::create(data);
#if PLATFORM(MAC)
    auto modifierFlags = WebEventFactory::toNSEventModifierFlags(modifiers);
#else
    auto modifierFlags = WebIOSEventFactory::toUIKeyModifierFlags(modifiers);
#endif
    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate->m_webView.get().get() mouseDidMoveOverElement:wrapper(apiHitTestResult.get()) withFlags:modifierFlags userInfo:userInfo ? static_cast<id <NSSecureCoding>>(userInfo->wrapper()) : nil];
}
#endif

void UIDelegate::UIClient::createNewPage(WebKit::WebPageProxy&, WebCore::WindowFeatures&& windowFeatures, Ref<API::NavigationAction>&& navigationAction, CompletionHandler<void(RefPtr<WebPageProxy>&&)>&& completionHandler)
{
    if (!m_uiDelegate)
        return completionHandler(nullptr);

    auto delegate = m_uiDelegate->m_delegate.get();
    ASSERT(delegate);

    auto configuration = adoptNS([m_uiDelegate->m_webView.get()->_configuration copy]);
    [configuration _setRelatedWebView:m_uiDelegate->m_webView.get().get()];

    auto apiWindowFeatures = API::WindowFeatures::create(windowFeatures);

    if (m_uiDelegate->m_delegateMethods.webViewCreateWebViewWithConfigurationForNavigationActionWindowFeaturesAsync) {
        auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:createWebViewWithConfiguration:forNavigationAction:windowFeatures:completionHandler:));

        [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate->m_webView.get().get() createWebViewWithConfiguration:configuration.get() forNavigationAction:wrapper(navigationAction) windowFeatures:wrapper(apiWindowFeatures) completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker), relatedWebView = m_uiDelegate->m_webView.get()] (WKWebView *webView) mutable {
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
    if (!m_uiDelegate->m_delegateMethods.webViewCreateWebViewWithConfigurationForNavigationActionWindowFeatures)
        return completionHandler(nullptr);

    RetainPtr<WKWebView> webView = [delegate webView:m_uiDelegate->m_webView.get().get() createWebViewWithConfiguration:configuration.get() forNavigationAction:wrapper(navigationAction) windowFeatures:wrapper(apiWindowFeatures)];
    if (!webView)
        return completionHandler(nullptr);

    if ([webView.get()->_configuration _relatedWebView] != m_uiDelegate->m_webView.get().get())
        [NSException raise:NSInternalInconsistencyException format:@"Returned WKWebView was not created with the given configuration."];
    completionHandler(webView->_page.get());
}

void UIDelegate::UIClient::runJavaScriptAlert(WebPageProxy& page, const WTF::String& message, WebFrameProxy*, FrameInfoData&& frameInfo, Function<void()>&& completionHandler)
{
    if (!m_uiDelegate)
        return completionHandler();

    if (!m_uiDelegate->m_delegateMethods.webViewRunJavaScriptAlertPanelWithMessageInitiatedByFrameCompletionHandler) {
        completionHandler();
        return;
    }

    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate) {
        completionHandler();
        return;
    }

    page.makeViewBlankIfUnpaintedSinceLastLoadCommit();

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(webView:runJavaScriptAlertPanelWithMessage:initiatedByFrame:completionHandler:));
    [delegate webView:m_uiDelegate->m_webView.get().get() runJavaScriptAlertPanelWithMessage:message initiatedByFrame:wrapper(API::FrameInfo::create(WTFMove(frameInfo), &page)) completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] {
        if (checker->completionHandlerHasBeenCalled())
            return;
        completionHandler();
        checker->didCallCompletionHandler();
    }).get()];
}

void UIDelegate::UIClient::runJavaScriptConfirm(WebPageProxy& page, const WTF::String& message, WebFrameProxy* webFrameProxy, FrameInfoData&& frameInfo, Function<void(bool)>&& completionHandler)
{
    if (!m_uiDelegate)
        return completionHandler(false);

    if (!m_uiDelegate->m_delegateMethods.webViewRunJavaScriptConfirmPanelWithMessageInitiatedByFrameCompletionHandler) {
        completionHandler(false);
        return;
    }

    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate) {
        completionHandler(false);
        return;
    }

    page.makeViewBlankIfUnpaintedSinceLastLoadCommit();

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(webView:runJavaScriptConfirmPanelWithMessage:initiatedByFrame:completionHandler:));
    [delegate webView:m_uiDelegate->m_webView.get().get() runJavaScriptConfirmPanelWithMessage:message initiatedByFrame:wrapper(API::FrameInfo::create(WTFMove(frameInfo), &page)) completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] (BOOL result) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        completionHandler(result);
        checker->didCallCompletionHandler();
    }).get()];
}

void UIDelegate::UIClient::runJavaScriptPrompt(WebPageProxy& page, const WTF::String& message, const WTF::String& defaultValue, WebFrameProxy*, FrameInfoData&& frameInfo, Function<void(const WTF::String&)>&& completionHandler)
{
    if (!m_uiDelegate)
        return completionHandler({ });

    if (!m_uiDelegate->m_delegateMethods.webViewRunJavaScriptTextInputPanelWithPromptDefaultTextInitiatedByFrameCompletionHandler) {
        completionHandler(String());
        return;
    }

    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate) {
        completionHandler(String());
        return;
    }

    page.makeViewBlankIfUnpaintedSinceLastLoadCommit();

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(webView:runJavaScriptTextInputPanelWithPrompt:defaultText:initiatedByFrame:completionHandler:));
    [delegate webView:m_uiDelegate->m_webView.get().get() runJavaScriptTextInputPanelWithPrompt:message defaultText:defaultValue initiatedByFrame:wrapper(API::FrameInfo::create(WTFMove(frameInfo), &page)) completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] (NSString *result) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        completionHandler(result);
        checker->didCallCompletionHandler();
    }).get()];
}

void UIDelegate::UIClient::requestStorageAccessConfirm(WebPageProxy& webPageProxy, WebFrameProxy*, const WebCore::RegistrableDomain& requestingDomain, const WebCore::RegistrableDomain& currentDomain, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!m_uiDelegate)
        return completionHandler(false);

    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate) {
        completionHandler(false);
        return;
    }

    // Some sites have quirks where multiple login domains require storage access.
    if (auto additionalLoginDomain = WebCore::NetworkStorageSession::findAdditionalLoginDomain(currentDomain, requestingDomain)) {
#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
        presentStorageAccessAlertQuirk(m_uiDelegate->m_webView.get().get(), requestingDomain, *additionalLoginDomain, currentDomain, WTFMove(completionHandler));
#endif
        return;
    }

    if (!m_uiDelegate->m_delegateMethods.webViewRequestStorageAccessPanelUnderFirstPartyCompletionHandler) {
#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
        presentStorageAccessAlert(m_uiDelegate->m_webView.get().get(), requestingDomain, currentDomain, WTFMove(completionHandler));
#endif
        return;
    }

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:requestStorageAccessPanelForDomain:underCurrentDomain:completionHandler:));
    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate->m_webView.get().get() requestStorageAccessPanelForDomain:requestingDomain.string() underCurrentDomain:currentDomain.string() completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] (BOOL result) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        completionHandler(result);
        checker->didCallCompletionHandler();
    }).get()];
}

void UIDelegate::UIClient::decidePolicyForGeolocationPermissionRequest(WebKit::WebPageProxy& page, WebKit::WebFrameProxy& frame, const FrameInfoData& frameInfo, Function<void(bool)>& completionHandler)
{
    if (!m_uiDelegate || (!m_uiDelegate->m_delegateMethods.webViewRequestGeolocationPermissionForFrameDecisionHandler && !m_uiDelegate->m_delegateMethods.webViewRequestGeolocationPermissionForOriginDecisionHandler))
        return;

    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    if (m_uiDelegate->m_delegateMethods.webViewRequestGeolocationPermissionForOriginDecisionHandler) {
        auto securityOrigin = WebCore::SecurityOrigin::createFromString(page.pageLoadState().activeURL());
        auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:requestGeolocationPermissionForOrigin:initiatedByFrame:decisionHandler:));
        auto decisionHandler = makeBlockPtr([completionHandler = std::exchange(completionHandler, nullptr), securityOrigin = securityOrigin->data(), checker = WTFMove(checker), page = WeakPtr { page }] (WKPermissionDecision decision) mutable {
            if (checker->completionHandlerHasBeenCalled())
                return;
            checker->didCallCompletionHandler();
            if (!page) {
                completionHandler(false);
                return;
            }
            switch (decision) {
            case WKPermissionDecisionPrompt:
                alertForPermission(*page, MediaPermissionReason::Geolocation, securityOrigin, WTFMove(completionHandler));
                break;
            case WKPermissionDecisionGrant:
                completionHandler(true);
                break;
            case WKPermissionDecisionDeny:
                completionHandler(false);
                break;
            }
        });
        [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate->m_webView.get().get() requestGeolocationPermissionForOrigin:wrapper(API::SecurityOrigin::create(securityOrigin.get())) initiatedByFrame:wrapper(API::FrameInfo::create(FrameInfoData { frameInfo }, &page)) decisionHandler:decisionHandler.get()];
        return;
    }

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:requestGeolocationPermissionForFrame:decisionHandler:));
    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate->m_webView.get().get() requestGeolocationPermissionForFrame:wrapper(API::FrameInfo::create(FrameInfoData { frameInfo }, &page)) decisionHandler:makeBlockPtr([completionHandler = std::exchange(completionHandler, nullptr), checker = WTFMove(checker)] (BOOL result) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();
        completionHandler(result);
    }).get()];
}

void UIDelegate::UIClient::didResignInputElementStrongPasswordAppearance(WebPageProxy&, API::Object* userInfo)
{
    if (!m_uiDelegate)
        return;
    if (!m_uiDelegate->m_delegateMethods.webViewDidResignInputElementStrongPasswordAppearanceWithUserInfo)
        return;

    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate->m_webView.get().get() didResignInputElementStrongPasswordAppearanceWithUserInfo:userInfo ? static_cast<id <NSSecureCoding>>(userInfo->wrapper()) : nil];
}

bool UIDelegate::UIClient::canRunBeforeUnloadConfirmPanel() const
{
    if (!m_uiDelegate)
        return false;

    return m_uiDelegate->m_delegateMethods.webViewRunBeforeUnloadConfirmPanelWithMessageInitiatedByFrameCompletionHandler;
}

void UIDelegate::UIClient::runBeforeUnloadConfirmPanel(WebPageProxy& page, const WTF::String& message, WebFrameProxy*, FrameInfoData&& frameInfo, Function<void(bool)>&& completionHandler)
{
    if (!m_uiDelegate)
        return completionHandler(false);

    if (!m_uiDelegate->m_delegateMethods.webViewRunBeforeUnloadConfirmPanelWithMessageInitiatedByFrameCompletionHandler) {
        completionHandler(false);
        return;
    }

    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate) {
        completionHandler(false);
        return;
    }

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:runBeforeUnloadConfirmPanelWithMessage:initiatedByFrame:completionHandler:));
    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate->m_webView.get().get() runBeforeUnloadConfirmPanelWithMessage:message initiatedByFrame:wrapper(API::FrameInfo::create(WTFMove(frameInfo), &page)) completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] (BOOL result) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        completionHandler(result);
        checker->didCallCompletionHandler();
    }).get()];
}

void UIDelegate::UIClient::exceededDatabaseQuota(WebPageProxy*, WebFrameProxy*, API::SecurityOrigin* securityOrigin, const WTF::String& databaseName, const WTF::String& displayName, unsigned long long currentQuota, unsigned long long currentOriginUsage, unsigned long long currentUsage, unsigned long long expectedUsage, Function<void (unsigned long long)>&& completionHandler)
{
    // Use 50 MB as the default database quota.
    unsigned long long defaultPerOriginDatabaseQuota = 50 * 1024 * 1024;

    if (!m_uiDelegate)
        return completionHandler(defaultPerOriginDatabaseQuota);
    if (!m_uiDelegate->m_delegateMethods.webViewDecideDatabaseQuotaForSecurityOriginCurrentQuotaCurrentOriginUsageCurrentDatabaseUsageExpectedUsageDecisionHandler && !m_uiDelegate->m_delegateMethods.webViewDecideDatabaseQuotaForSecurityOriginDatabaseNameDisplayNameCurrentQuotaCurrentOriginUsageCurrentDatabaseUsageExpectedUsageDecisionHandler) {

        completionHandler(defaultPerOriginDatabaseQuota);
        return;
    }

    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate) {
        completionHandler(defaultPerOriginDatabaseQuota);
        return;
    }

    ASSERT(securityOrigin);

    if (m_uiDelegate->m_delegateMethods.webViewDecideDatabaseQuotaForSecurityOriginDatabaseNameDisplayNameCurrentQuotaCurrentOriginUsageCurrentDatabaseUsageExpectedUsageDecisionHandler) {
        auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:decideDatabaseQuotaForSecurityOrigin:databaseName:displayName:currentQuota:currentOriginUsage:currentDatabaseUsage:expectedUsage:decisionHandler:));
        [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate->m_webView.get().get() decideDatabaseQuotaForSecurityOrigin:wrapper(*securityOrigin) databaseName:databaseName displayName:displayName currentQuota:currentQuota currentOriginUsage:currentOriginUsage currentDatabaseUsage:currentUsage expectedUsage:expectedUsage decisionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)](unsigned long long newQuota) {
            if (checker->completionHandlerHasBeenCalled())
                return;
            checker->didCallCompletionHandler();
            completionHandler(newQuota);
        }).get()];
        return;
    }

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:decideDatabaseQuotaForSecurityOrigin:currentQuota:currentOriginUsage:currentDatabaseUsage:expectedUsage:decisionHandler:));
    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate->m_webView.get().get() decideDatabaseQuotaForSecurityOrigin:wrapper(*securityOrigin) currentQuota:currentQuota currentOriginUsage:currentOriginUsage currentDatabaseUsage:currentUsage expectedUsage:expectedUsage decisionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)](unsigned long long newQuota) {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();
        completionHandler(newQuota);
    }).get()];
}

#if PLATFORM(IOS)
static _WKScreenOrientationType toWKScreenOrientationType(WebCore::ScreenOrientationType lockType)
{
    switch (lockType) {
    case WebCore::ScreenOrientationType::PortraitSecondary:
        return _WKScreenOrientationTypePortraitSecondary;
    case WebCore::ScreenOrientationType::LandscapePrimary:
        return _WKScreenOrientationTypeLandscapePrimary;
    case WebCore::ScreenOrientationType::LandscapeSecondary:
        return _WKScreenOrientationTypeLandscapeSecondary;
    case WebCore::ScreenOrientationType::PortraitPrimary:
        break;
    }
    return _WKScreenOrientationTypePortraitPrimary;
}
#endif

bool UIDelegate::UIClient::lockScreenOrientation(WebPageProxy&, WebCore::ScreenOrientationType orientation)
{
#if PLATFORM(IOS)
    if (!m_uiDelegate)
        return false;
    if (!m_uiDelegate->m_delegateMethods.webViewLockScreenOrientation)
        return false;
    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return false;

    return [(id<WKUIDelegatePrivate>)delegate _webViewLockScreenOrientation:m_uiDelegate->m_webView.get().get() lockType:toWKScreenOrientationType(orientation)];
#else
    UNUSED_PARAM(orientation);
    return false;
#endif
}

void UIDelegate::UIClient::unlockScreenOrientation(WebPageProxy&)
{
#if PLATFORM(IOS)
    if (!m_uiDelegate)
        return;
    if (!m_uiDelegate->m_delegateMethods.webViewUnlockScreenOrientation)
        return;
    if (auto delegate = m_uiDelegate->m_delegate.get())
        [(id<WKUIDelegatePrivate>)delegate _webViewUnlockScreenOrientation:m_uiDelegate->m_webView.get().get()];
#endif
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
    if (!m_uiDelegate)
        return false;

    if (!m_uiDelegate->m_delegateMethods.webViewTakeFocus)
        return false;
    
    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return false;
    
    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate->m_webView.get().get() takeFocus:toWKFocusDirection(direction)];
    return true;
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

void UIDelegate::UIClient::handleAutoplayEvent(WebPageProxy&, WebCore::AutoplayEvent event, OptionSet<WebCore::AutoplayEventFlags> flags)
{
    if (!m_uiDelegate)
        return;

    if (!m_uiDelegate->m_delegateMethods.webViewHandleAutoplayEventWithFlags)
        return;
    
    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return;
    
    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate->m_webView.get().get() handleAutoplayEvent:toWKAutoplayEvent(event) withFlags:toWKAutoplayEventFlags(flags)];
}

void UIDelegate::UIClient::decidePolicyForNotificationPermissionRequest(WebKit::WebPageProxy&, API::SecurityOrigin& securityOrigin, CompletionHandler<void(bool allowed)>&& completionHandler)
{
    if (!m_uiDelegate)
        return completionHandler(false);

    if (!m_uiDelegate->m_delegateMethods.webViewRequestNotificationPermissionForSecurityOriginDecisionHandler)
        return completionHandler(false);

    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return completionHandler(false);

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:requestNotificationPermissionForSecurityOrigin:decisionHandler:));
    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate->m_webView.get().get() requestNotificationPermissionForSecurityOrigin:wrapper(securityOrigin) decisionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] (BOOL result) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();

        completionHandler(result);
    }).get()];
}

void UIDelegate::UIClient::requestCookieConsent(CompletionHandler<void(WebCore::CookieConsentDecisionResult)>&& completion)
{
    if (!m_uiDelegate)
        return completion(WebCore::CookieConsentDecisionResult::NotSupported);

    if (!m_uiDelegate->m_delegateMethods.webViewRequestCookieConsentWithMoreInfoHandlerDecisionHandler)
        return completion(WebCore::CookieConsentDecisionResult::NotSupported);

    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return completion(WebCore::CookieConsentDecisionResult::NotSupported);

    // FIXME: Add support for the 'more info' handler.
    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:requestCookieConsentWithMoreInfoHandler:decisionHandler:));
    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate->m_webView.get().get() requestCookieConsentWithMoreInfoHandler:nil decisionHandler:makeBlockPtr([completion = WTFMove(completion), checker = WTFMove(checker)] (BOOL decision) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();
        completion(decision ? WebCore::CookieConsentDecisionResult::Consent : WebCore::CookieConsentDecisionResult::Dissent);
    }).get()];
}

static WebCore::ModalContainerDecision coreModalContainerDecision(_WKModalContainerDecision decision)
{
    switch (decision) {
    case _WKModalContainerDecisionShow:
        return WebCore::ModalContainerDecision::Show;
    case _WKModalContainerDecisionHideAndIgnore:
        return WebCore::ModalContainerDecision::HideAndIgnore;
    case _WKModalContainerDecisionHideAndAllow:
        return WebCore::ModalContainerDecision::HideAndAllow;
    case _WKModalContainerDecisionHideAndDisallow:
        return WebCore::ModalContainerDecision::HideAndDisallow;
    }
    return WebCore::ModalContainerDecision::Show;
}

void UIDelegate::UIClient::decidePolicyForModalContainer(OptionSet<WebCore::ModalContainerControlType> controlTypes, CompletionHandler<void(WebCore::ModalContainerDecision)>&& completion)
{
    if (!m_uiDelegate)
        return completion(WebCore::ModalContainerDecision::Show);

    if (!m_uiDelegate->m_delegateMethods.webViewDecidePolicyForModalContainerDecisionHandler)
        return completion(WebCore::ModalContainerDecision::Show);

    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return completion(WebCore::ModalContainerDecision::Show);

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:decidePolicyForModalContainer:decisionHandler:));
    auto info = adoptNS([[_WKModalContainerInfo alloc] initWithTypes:controlTypes]);
    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate->m_webView.get().get() decidePolicyForModalContainer:info.get() decisionHandler:makeBlockPtr([completion = WTFMove(completion), checker = WTFMove(checker)] (_WKModalContainerDecision decision) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;

        checker->didCallCompletionHandler();
        completion(coreModalContainerDecision(decision));
    }).get()];
}

#if PLATFORM(MAC)
bool UIDelegate::UIClient::canRunModal() const
{
    if (!m_uiDelegate)
        return false;

    return m_uiDelegate->m_delegateMethods.webViewRunModal;
}

void UIDelegate::UIClient::runModal(WebPageProxy&)
{
    if (!m_uiDelegate)
        return;

    if (!m_uiDelegate->m_delegateMethods.webViewRunModal)
        return;
    
    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    [(id <WKUIDelegatePrivate>)delegate _webViewRunModal:m_uiDelegate->m_webView.get().get()];
}

float UIDelegate::UIClient::headerHeight(WebPageProxy&, WebFrameProxy& webFrameProxy)
{
    if (!m_uiDelegate)
        return 0;

    if (!m_uiDelegate->m_delegateMethods.webViewHeaderHeight)
        return 0;
    
    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return 0;
    
    return [(id <WKUIDelegatePrivate>)delegate _webViewHeaderHeight:m_uiDelegate->m_webView.get().get()];
}

float UIDelegate::UIClient::footerHeight(WebPageProxy&, WebFrameProxy&)
{
    if (!m_uiDelegate)
        return 0;

    if (!m_uiDelegate->m_delegateMethods.webViewFooterHeight)
        return 0;
    
    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return 0;
    
    return [(id <WKUIDelegatePrivate>)delegate _webViewFooterHeight:m_uiDelegate->m_webView.get().get()];
}

void UIDelegate::UIClient::drawHeader(WebPageProxy&, WebFrameProxy& frame, WebCore::FloatRect&& rect)
{
    if (!m_uiDelegate)
        return;

    if (!m_uiDelegate->m_delegateMethods.webViewDrawHeaderInRectForPageWithTitleURL)
        return;
    
    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return;
    
    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate->m_webView.get().get() drawHeaderInRect:rect forPageWithTitle:frame.title() URL:frame.url()];
}

void UIDelegate::UIClient::drawFooter(WebPageProxy&, WebFrameProxy& frame, WebCore::FloatRect&& rect)
{
    if (!m_uiDelegate)
        return;

    if (!m_uiDelegate->m_delegateMethods.webViewDrawFooterInRectForPageWithTitleURL)
        return;
    
    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return;
    
    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate->m_webView.get().get() drawFooterInRect:rect forPageWithTitle:frame.title() URL:frame.url()];
}

void UIDelegate::UIClient::pageDidScroll(WebPageProxy*)
{
    if (!m_uiDelegate)
        return;

    if (!m_uiDelegate->m_delegateMethods.webViewDidScroll)
        return;
    
    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return;
    
    [(id <WKUIDelegatePrivate>)delegate _webViewDidScroll:m_uiDelegate->m_webView.get().get()];
}

bool UIDelegate::UIClient::focusFromServiceWorker(WebKit::WebPageProxy& proxy)
{
    bool hasImplementation = m_uiDelegate && m_uiDelegate->m_delegateMethods.focusWebViewFromServiceWorker && m_uiDelegate->m_delegate.get();
    if (!hasImplementation) {
        auto* webView = m_uiDelegate ? m_uiDelegate->m_webView.get().get() : nullptr;
        if (!webView || !webView.window)
            return false;

#if PLATFORM(MAC)
        [webView.window makeKeyAndOrderFront:nil];
#else
        [webView.window makeKeyAndVisible];
#endif
        [[webView window] makeFirstResponder:webView];
        return true;
    }

    return [(id <WKUIDelegatePrivate>)m_uiDelegate->m_delegate.get() _focusWebViewFromServiceWorker:m_uiDelegate->m_webView.get().get()];
}

void UIDelegate::UIClient::focus(WebPageProxy*)
{
    if (!m_uiDelegate)
        return;

    if (!m_uiDelegate->m_delegateMethods.focusWebView)
        return;
    
    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    [(id <WKUIDelegatePrivate>)delegate _focusWebView:m_uiDelegate->m_webView.get().get()];
}

void UIDelegate::UIClient::unfocus(WebPageProxy*)
{
    if (!m_uiDelegate)
        return;

    if (!m_uiDelegate->m_delegateMethods.unfocusWebView)
        return;
    
    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return;
    
    [(id <WKUIDelegatePrivate>)delegate _unfocusWebView:m_uiDelegate->m_webView.get().get()];
}

void UIDelegate::UIClient::didNotHandleWheelEvent(WebPageProxy*, const NativeWebWheelEvent& event)
{
    if (!m_uiDelegate)
        return;

    if (!m_uiDelegate->m_delegateMethods.webViewDidNotHandleWheelEvent)
        return;
    
    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return;
    
    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate->m_webView.get().get() didNotHandleWheelEvent:event.nativeEvent()];
}

void UIDelegate::UIClient::setIsResizable(WebKit::WebPageProxy&, bool resizable)
{
    if (!m_uiDelegate)
        return;

    if (!m_uiDelegate->m_delegateMethods.webViewSetResizable)
        return;
    
    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return;
    
    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate->m_webView.get().get() setResizable:resizable];
}

void UIDelegate::UIClient::setWindowFrame(WebKit::WebPageProxy&, const WebCore::FloatRect& frame)
{
    if (!m_uiDelegate)
        return;

    if (!m_uiDelegate->m_delegateMethods.webViewSetWindowFrame)
        return;
    
    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return;
    
    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate->m_webView.get().get() setWindowFrame:frame];
}

void UIDelegate::UIClient::windowFrame(WebKit::WebPageProxy&, Function<void(WebCore::FloatRect)>&& completionHandler)
{
    if (!m_uiDelegate)
        return completionHandler({ });

    if (!m_uiDelegate->m_delegateMethods.webViewGetWindowFrameWithCompletionHandler)
        return completionHandler({ });
    
    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return completionHandler({ });
    
    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate->m_webView.get().get() getWindowFrameWithCompletionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:getWindowFrameWithCompletionHandler:))](CGRect frame) {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();
        completionHandler(frame);
    }).get()];
}

void UIDelegate::UIClient::toolbarsAreVisible(WebPageProxy&, Function<void(bool)>&& completionHandler)
{
    if (!m_uiDelegate)
        return completionHandler(true);

    if (!m_uiDelegate->m_delegateMethods.webViewGetToolbarsAreVisibleWithCompletionHandler)
        return completionHandler(true);
    
    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return completionHandler(true);
    
    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate->m_webView.get().get() getToolbarsAreVisibleWithCompletionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:getToolbarsAreVisibleWithCompletionHandler:))](BOOL visible) {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();
        completionHandler(visible);
    }).get()];
}

void UIDelegate::UIClient::didClickAutoFillButton(WebPageProxy&, API::Object* userInfo)
{
    if (!m_uiDelegate)
        return;

    if (!m_uiDelegate->m_delegateMethods.webViewDidClickAutoFillButtonWithUserInfo)
        return;
    
    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return;
    
    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate->m_webView.get().get() didClickAutoFillButtonWithUserInfo:userInfo ? static_cast<id <NSSecureCoding>>(userInfo->wrapper()) : nil];
}

void UIDelegate::UIClient::showPage(WebPageProxy*)
{
    if (!m_uiDelegate)
        return;

    if (!m_uiDelegate->m_delegateMethods.showWebView)
        return;

    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return;
    
    [(id <WKUIDelegatePrivate>)delegate _showWebView:m_uiDelegate->m_webView.get().get()];
}
    
void UIDelegate::UIClient::saveDataToFileInDownloadsFolder(WebPageProxy*, const WTF::String& suggestedFilename, const WTF::String& mimeType, const URL& originatingURL, API::Data& data)
{
    if (!m_uiDelegate)
        return;

    if (!m_uiDelegate->m_delegateMethods.webViewSaveDataToFileSuggestedFilenameMimeTypeOriginatingURL)
        return;
    
    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate->m_webView.get().get() saveDataToFile:wrapper(data) suggestedFilename:suggestedFilename mimeType:mimeType originatingURL:originatingURL];
}

Ref<API::InspectorConfiguration> UIDelegate::UIClient::configurationForLocalInspector(WebPageProxy&, WebInspectorUIProxy& inspector)
{
    if (!m_uiDelegate)
        return API::InspectorConfiguration::create();

    if (!m_uiDelegate->m_delegateMethods.webViewConfigurationForLocalInspector)
        return API::InspectorConfiguration::create();

    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return API::InspectorConfiguration::create();

    return static_cast<API::InspectorConfiguration&>([[(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate->m_webView.get().get() configurationForLocalInspector:wrapper(inspector)] _apiObject]);
}

void UIDelegate::UIClient::didAttachLocalInspector(WebPageProxy&, WebInspectorUIProxy& inspector)
{
    if (!m_uiDelegate)
        return;

    if (!m_uiDelegate->m_delegateMethods.webViewDidAttachLocalInspector)
        return;

    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate->m_webView.get().get() didAttachLocalInspector:wrapper(inspector)];
}

void UIDelegate::UIClient::willCloseLocalInspector(WebPageProxy&, WebInspectorUIProxy& inspector)
{
    if (!m_uiDelegate)
        return;

    if (!m_uiDelegate->m_delegateMethods.webViewWillCloseLocalInspector)
        return;

    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate->m_webView.get().get() willCloseLocalInspector:wrapper(inspector)];
}

bool UIDelegate::UIClient::runOpenPanel(WebPageProxy& page, WebFrameProxy* webFrameProxy, FrameInfoData&& frameInfo, API::OpenPanelParameters* openPanelParameters, WebOpenPanelResultListenerProxy* listener)
{
    if (!m_uiDelegate)
        return false;

    if (!m_uiDelegate->m_delegateMethods.webViewRunOpenPanelWithParametersInitiatedByFrameCompletionHandler)
        return false;

    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return false;

    auto frame = API::FrameInfo::create(WTFMove(frameInfo), &page);

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(webView:runOpenPanelWithParameters:initiatedByFrame:completionHandler:));

    [delegate webView:m_uiDelegate->m_webView.get().get() runOpenPanelWithParameters:wrapper(*openPanelParameters) initiatedByFrame:wrapper(frame) completionHandler:makeBlockPtr([checker = WTFMove(checker), openPanelParameters = Ref { *openPanelParameters }, listener = RefPtr { listener }] (NSArray *URLs) mutable {
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

        listener->chooseFiles(filenames, openPanelParameters->allowedMIMETypes()->toStringVector());
    }).get()];

    return true;
}
#endif

#if ENABLE(DEVICE_ORIENTATION)
void UIDelegate::UIClient::shouldAllowDeviceOrientationAndMotionAccess(WebKit::WebPageProxy& page, WebFrameProxy& webFrameProxy, FrameInfoData&& frameInfo, CompletionHandler<void(bool)>&& completionHandler)
{
    auto securityOrigin = WebCore::SecurityOrigin::createFromString(page.pageLoadState().activeURL());
    if (!m_uiDelegate || !m_uiDelegate->m_delegate.get() || !m_uiDelegate->m_delegateMethods.webViewRequestDeviceOrientationAndMotionPermissionForOriginDecisionHandler) {
        alertForPermission(page, MediaPermissionReason::DeviceOrientation, securityOrigin->data(), WTFMove(completionHandler));
        return;
    }

    auto delegate = m_uiDelegate->m_delegate.get();
    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(webView:requestDeviceOrientationAndMotionPermissionForOrigin:initiatedByFrame:decisionHandler:));
    auto decisionHandler = makeBlockPtr([completionHandler = WTFMove(completionHandler), securityOrigin = securityOrigin->data(), checker = WTFMove(checker), page = WeakPtr { page }](WKPermissionDecision decision) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();
        if (!page) {
            completionHandler(false);
            return;
        }
        switch (decision) {
        case WKPermissionDecisionPrompt:
            alertForPermission(*page, MediaPermissionReason::DeviceOrientation, securityOrigin, WTFMove(completionHandler));
            break;
        case WKPermissionDecisionGrant:
            completionHandler(true);
            break;
        case WKPermissionDecisionDeny:
            completionHandler(false);
            break;
        }
    });
    [delegate webView:m_uiDelegate->m_webView.get().get() requestDeviceOrientationAndMotionPermissionForOrigin:wrapper(API::SecurityOrigin::create(securityOrigin.get())) initiatedByFrame:wrapper(API::FrameInfo::create(WTFMove(frameInfo), &page)) decisionHandler:decisionHandler.get()];
}
#endif

void UIDelegate::UIClient::didChangeFontAttributes(const WebCore::FontAttributes& fontAttributes)
{
    if (!m_uiDelegate)
        return;

    if (!needsFontAttributes())
        return;

    auto privateUIDelegate = (id <WKUIDelegatePrivate>)m_uiDelegate->m_delegate.get();
    [privateUIDelegate _webView:m_uiDelegate->m_webView.get().get() didChangeFontAttributes:fontAttributes.createDictionary().get()];
}

void UIDelegate::UIClient::promptForDisplayCapturePermission(WebPageProxy& page, WebFrameProxy& frame, API::SecurityOrigin& userMediaOrigin, API::SecurityOrigin& topLevelOrigin, UserMediaPermissionRequestProxy& request)
{
    auto delegate = (id <WKUIDelegatePrivate>)m_uiDelegate->m_delegate.get();

    ASSERT([delegate respondsToSelector:@selector(_webView:requestDisplayCapturePermissionForOrigin:initiatedByFrame:withSystemAudio:decisionHandler:)]);
    ASSERT(request.canPromptForGetDisplayMedia());

    auto checker = CompletionHandlerCallChecker::create(delegate, @selector(_webView:requestDisplayCapturePermissionForOrigin:initiatedByFrame:withSystemAudio:decisionHandler:));
    auto decisionHandler = makeBlockPtr([protectedRequest = Ref { request }, checker = WTFMove(checker)](WKDisplayCapturePermissionDecision decision) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();

        ensureOnMainRunLoop([protectedRequest = WTFMove(protectedRequest), decision]() {
            switch (decision) {
            case WKDisplayCapturePermissionDecisionScreenPrompt:
                protectedRequest->promptForGetDisplayMedia(UserMediaPermissionRequestProxy::UserMediaDisplayCapturePromptType::Screen);
                break;
            case WKDisplayCapturePermissionDecisionWindowPrompt: {
                protectedRequest->promptForGetDisplayMedia(UserMediaPermissionRequestProxy::UserMediaDisplayCapturePromptType::Window);
                break;
            }
            case WKDisplayCapturePermissionDecisionDeny:
                protectedRequest->deny(UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::PermissionDenied);
                break;
            }
        });
    });

    std::optional<WebCore::FrameIdentifier> mainFrameID;
    if (auto* mainFrame = frame.page() ? frame.page()->mainFrame() : nullptr)
        mainFrameID = mainFrame->frameID();
    FrameInfoData frameInfo { frame.isMainFrame(), { }, userMediaOrigin.securityOrigin(), { }, frame.frameID(), mainFrameID };
    RetainPtr<WKFrameInfo> frameInfoWrapper = wrapper(API::FrameInfo::create(WTFMove(frameInfo), frame.page()));

    BOOL requestSystemAudio = !!request.requiresDisplayCaptureWithAudio();
    [delegate _webView:m_uiDelegate->m_webView.get().get() requestDisplayCapturePermissionForOrigin:wrapper(topLevelOrigin) initiatedByFrame:frameInfoWrapper.get() withSystemAudio:requestSystemAudio decisionHandler:decisionHandler.get()];

}
void UIDelegate::UIClient::decidePolicyForUserMediaPermissionRequest(WebPageProxy& page, WebFrameProxy& frame, API::SecurityOrigin& userMediaOrigin, API::SecurityOrigin& topLevelOrigin, UserMediaPermissionRequestProxy& request)
{
#if ENABLE(MEDIA_STREAM)
    if (!m_uiDelegate)
        return;

    auto delegate = (id <WKUIDelegatePrivate>)m_uiDelegate->m_delegate.get();
    if (!delegate) {
        request.doDefaultAction();
        return;
    }

    bool respondsToRequestMediaCapturePermission = [delegate respondsToSelector:@selector(webView:requestMediaCapturePermissionForOrigin:initiatedByFrame:type:decisionHandler:)];
    bool respondsToRequestUserMediaAuthorizationForDevices = [delegate respondsToSelector:@selector(_webView:requestUserMediaAuthorizationForDevices:url:mainFrameURL:decisionHandler:)];
    bool respondsToRequestDisplayCapturePermissionForOrigin = [delegate respondsToSelector:@selector(_webView:requestDisplayCapturePermissionForOrigin:initiatedByFrame:withSystemAudio:decisionHandler:)];

    if (!respondsToRequestMediaCapturePermission && !respondsToRequestUserMediaAuthorizationForDevices && !respondsToRequestDisplayCapturePermissionForOrigin) {
        request.doDefaultAction();
        return;
    }

    // FIXME: Provide a specific delegate for display capture.
    if (!request.requiresDisplayCapture() && respondsToRequestMediaCapturePermission) {
        auto checker = CompletionHandlerCallChecker::create(delegate, @selector(webView:requestMediaCapturePermissionForOrigin:initiatedByFrame:type:decisionHandler:));
        auto decisionHandler = makeBlockPtr([protectedRequest = Ref { request }, checker = WTFMove(checker)](WKPermissionDecision decision) mutable {
            if (checker->completionHandlerHasBeenCalled())
                return;
            checker->didCallCompletionHandler();

            ensureOnMainRunLoop([protectedRequest = WTFMove(protectedRequest), decision] {
                switch (decision) {
                case WKPermissionDecisionPrompt:
                    protectedRequest->promptForGetUserMedia();
                    break;
                case WKPermissionDecisionGrant: {
                    const String& videoDeviceUID = protectedRequest->requiresVideoCapture() ? protectedRequest->videoDeviceUIDs().first() : String();
                    const String& audioDeviceUID = protectedRequest->requiresAudioCapture() ? protectedRequest->audioDeviceUIDs().first() : String();
                    protectedRequest->allow(audioDeviceUID, videoDeviceUID);
                    break;
                }
                case WKPermissionDecisionDeny:
                    protectedRequest->deny(UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::PermissionDenied);
                    break;
                }
            });
        });

        std::optional<WebCore::FrameIdentifier> mainFrameID;
        if (auto* mainFrame = frame.page() ? frame.page()->mainFrame() : nullptr)
            mainFrameID = mainFrame->frameID();
        FrameInfoData frameInfo { frame.isMainFrame(), { }, userMediaOrigin.securityOrigin(), { }, frame.frameID(), mainFrameID };
        RetainPtr<WKFrameInfo> frameInfoWrapper = wrapper(API::FrameInfo::create(WTFMove(frameInfo), frame.page()));

        WKMediaCaptureType type = WKMediaCaptureTypeCamera;
        if (request.requiresAudioCapture())
            type = request.requiresVideoCapture() ? WKMediaCaptureTypeCameraAndMicrophone : WKMediaCaptureTypeMicrophone;
        [delegate webView:m_uiDelegate->m_webView.get().get() requestMediaCapturePermissionForOrigin:wrapper(topLevelOrigin) initiatedByFrame:frameInfoWrapper.get() type:type decisionHandler:decisionHandler.get()];
        return;
    }

    if (request.requiresDisplayCapture() && request.canPromptForGetDisplayMedia()) {
        if (respondsToRequestDisplayCapturePermissionForOrigin) {
            promptForDisplayCapturePermission(page, frame, userMediaOrigin, topLevelOrigin, request);
            return;
        }

        if (!respondsToRequestUserMediaAuthorizationForDevices) {
            request.promptForGetDisplayMedia();
            return;
        }
    }

    if (!respondsToRequestUserMediaAuthorizationForDevices) {
        request.doDefaultAction();
        return;
    }

    URL requestFrameURL { frame.url() };
    URL mainFrameURL { frame.page()->mainFrame()->url() };

    _WKCaptureDevices devices = 0;
    if (request.requiresAudioCapture())
        devices |= _WKCaptureDeviceMicrophone;
    if (request.requiresVideoCapture())
        devices |= _WKCaptureDeviceCamera;
    if (request.requiresDisplayCapture()) {
        devices |= _WKCaptureDeviceDisplay;
        ASSERT(!(devices & _WKCaptureDeviceCamera));
    }

    auto checker = CompletionHandlerCallChecker::create(delegate, @selector(_webView:requestUserMediaAuthorizationForDevices:url:mainFrameURL:decisionHandler:));
    auto decisionHandler = makeBlockPtr([protectedRequest = Ref { request }, checker = WTFMove(checker)](BOOL authorized) {
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
    [delegate _webView:m_uiDelegate->m_webView.get().get() requestUserMediaAuthorizationForDevices:devices url:requestFrameURL mainFrameURL:mainFrameURL decisionHandler:decisionHandler.get()];
#endif
}

void UIDelegate::UIClient::checkUserMediaPermissionForOrigin(WebPageProxy& page, WebFrameProxy& frame, API::SecurityOrigin& userMediaOrigin, API::SecurityOrigin& topLevelOrigin, UserMediaPermissionCheckProxy& request)
{
    if (!m_uiDelegate)
        return;

    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate || !m_uiDelegate->m_delegateMethods.webViewIsMediaCaptureAuthorizedForFrameDecisionHandler) {
        request.setUserMediaAccessInfo(false);
        return;
    }

    const auto* mainFrame = frame.page()->mainFrame();

    if ([delegate respondsToSelector:@selector(_webView:includeSensitiveMediaDeviceDetails:)]) {
        auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:includeSensitiveMediaDeviceDetails:));
        auto decisionHandler = makeBlockPtr([protectedRequest = Ref { request }, checker = WTFMove(checker)](BOOL includeSensitiveDetails) {
            if (checker->completionHandlerHasBeenCalled())
                return;
            checker->didCallCompletionHandler();

            protectedRequest->setUserMediaAccessInfo(includeSensitiveDetails);
        });

        [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate->m_webView.get().get() includeSensitiveMediaDeviceDetails:decisionHandler.get()];
        return;
    }

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:checkUserMediaPermissionForURL:mainFrameURL:frameIdentifier:decisionHandler:));
    auto decisionHandler = makeBlockPtr([protectedRequest = Ref { request }, checker = WTFMove(checker)](NSString*, BOOL authorized) {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();

        protectedRequest->setUserMediaAccessInfo(authorized);
    });

    URL requestFrameURL { frame.url() };
    URL mainFrameURL { mainFrame->url() };

    [(id<WKUIDelegatePrivate>)delegate _webView:m_uiDelegate->m_webView.get().get() checkUserMediaPermissionForURL:requestFrameURL mainFrameURL:mainFrameURL frameIdentifier:frame.frameID().object().toUInt64() decisionHandler:decisionHandler.get()];
}

void UIDelegate::UIClient::mediaCaptureStateDidChange(WebCore::MediaProducerMediaStateFlags state)
{
    if (!m_uiDelegate)
        return;

    auto webView = m_uiDelegate->m_webView;

    [webView didChangeValueForKey:@"mediaCaptureState"];

    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate || !m_uiDelegate->m_delegateMethods.webViewMediaCaptureStateDidChange)
        return;

    [(id <WKUIDelegatePrivate>)delegate _webView:webView.get().get() mediaCaptureStateDidChange:toWKMediaCaptureStateDeprecated(state)];
}

void UIDelegate::UIClient::reachedApplicationCacheOriginQuota(WebPageProxy*, const WebCore::SecurityOrigin& securityOrigin, uint64_t currentQuota, uint64_t totalBytesNeeded, Function<void (unsigned long long)>&& completionHandler)
{
    if (!m_uiDelegate)
        return completionHandler(currentQuota);

    if (!m_uiDelegate->m_delegateMethods.webViewDecideWebApplicationCacheQuotaForSecurityOriginCurrentQuotaTotalBytesNeeded) {
        completionHandler(currentQuota);
        return;
    }

    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate) {
        completionHandler(currentQuota);
        return;
    }

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:decideWebApplicationCacheQuotaForSecurityOrigin:currentQuota:totalBytesNeeded:decisionHandler:));
    auto apiOrigin = API::SecurityOrigin::create(securityOrigin);
    
    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate->m_webView.get().get() decideWebApplicationCacheQuotaForSecurityOrigin:wrapper(apiOrigin.get()) currentQuota:currentQuota totalBytesNeeded:totalBytesNeeded decisionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)](unsigned long long newQuota) {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();
        completionHandler(newQuota);
    }).get()];
}

void UIDelegate::UIClient::printFrame(WebPageProxy&, WebFrameProxy& webFrameProxy, const WebCore::FloatSize& pdfFirstPageSize, CompletionHandler<void()>&& completionHandler)
{
    if (!m_uiDelegate)
        return completionHandler();

    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return completionHandler();

    auto handle = API::FrameHandle::create(webFrameProxy.frameID());
    if (m_uiDelegate->m_delegateMethods.webViewPrintFramePDFFirstPageSizeCompletionHandler) {
        auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:printFrame:pdfFirstPageSize:completionHandler:));
        [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate->m_webView.get().get() printFrame:wrapper(handle) pdfFirstPageSize:static_cast<CGSize>(pdfFirstPageSize) completionHandler:makeBlockPtr([checker = WTFMove(checker), completionHandler = WTFMove(completionHandler)] () mutable {
            if (checker->completionHandlerHasBeenCalled())
                return;
            checker->didCallCompletionHandler();
            completionHandler();
        }).get()];
        return;
    }

    if (m_uiDelegate->m_delegateMethods.webViewPrintFrame)
        [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate->m_webView.get().get() printFrame:wrapper(handle)];
    completionHandler();
}

void UIDelegate::UIClient::close(WebPageProxy*)
{
    if (!m_uiDelegate)
        return;

    if (m_uiDelegate->m_delegateMethods.webViewClose) {
        auto delegate = m_uiDelegate->m_delegate.get();
        if (!delegate)
            return;

        [(id <WKUIDelegatePrivate>)delegate _webViewClose:m_uiDelegate->m_webView.get().get()];
        return;
    }

    if (!m_uiDelegate->m_delegateMethods.webViewDidClose)
        return;

    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    [delegate webViewDidClose:m_uiDelegate->m_webView.get().get()];
}

void UIDelegate::UIClient::fullscreenMayReturnToInline(WebPageProxy*)
{
    if (!m_uiDelegate)
        return;

    if (!m_uiDelegate->m_delegateMethods.webViewFullscreenMayReturnToInline)
        return;
    
    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return;
    
    [(id <WKUIDelegatePrivate>)delegate _webViewFullscreenMayReturnToInline:m_uiDelegate->m_webView.get().get()];
}

void UIDelegate::UIClient::didEnterFullscreen(WebPageProxy*)
{
    if (!m_uiDelegate)
        return;

    if (!m_uiDelegate->m_delegateMethods.webViewDidEnterFullscreen)
        return;

    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    [(id <WKUIDelegatePrivate>)delegate _webViewDidEnterFullscreen:m_uiDelegate->m_webView.get().get()];
}

void UIDelegate::UIClient::didExitFullscreen(WebPageProxy*)
{
    if (!m_uiDelegate)
        return;

    if (!m_uiDelegate->m_delegateMethods.webViewDidExitFullscreen)
        return;

    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    [(id <WKUIDelegatePrivate>)delegate _webViewDidExitFullscreen:m_uiDelegate->m_webView.get().get()];
}
    
#if PLATFORM(IOS_FAMILY)
#if HAVE(APP_LINKS)
bool UIDelegate::UIClient::shouldIncludeAppLinkActionsForElement(_WKActivatedElementInfo *elementInfo)
{
    if (!m_uiDelegate)
        return true;

    if (!m_uiDelegate->m_delegateMethods.webViewShouldIncludeAppLinkActionsForElement)
        return true;

    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return true;

    return [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate->m_webView.get().get() shouldIncludeAppLinkActionsForElement:elementInfo];
}
#endif

RetainPtr<NSArray> UIDelegate::UIClient::actionsForElement(_WKActivatedElementInfo *elementInfo, RetainPtr<NSArray> defaultActions)
{
    if (!m_uiDelegate)
        return defaultActions;

    if (!m_uiDelegate->m_delegateMethods.webViewActionsForElementDefaultActions)
        return defaultActions;

    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return defaultActions;

    return [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate->m_webView.get().get() actionsForElement:elementInfo defaultActions:defaultActions.get()];
}

void UIDelegate::UIClient::didNotHandleTapAsClick(const WebCore::IntPoint& point)
{
    if (!m_uiDelegate)
        return;

    if (!m_uiDelegate->m_delegateMethods.webViewDidNotHandleTapAsClickAtPoint)
        return;

    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    [static_cast<id <WKUIDelegatePrivate>>(delegate) _webView:m_uiDelegate->m_webView.get().get() didNotHandleTapAsClickAtPoint:point];
}

void UIDelegate::UIClient::statusBarWasTapped()
{
    if (!m_uiDelegate)
        return;

    if (!m_uiDelegate->m_delegateMethods.webViewStatusBarWasTapped)
        return;

    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    [static_cast<id <WKUIDelegatePrivate>>(delegate) _webViewStatusBarWasTapped:m_uiDelegate->m_webView.get().get()];
}
#endif // PLATFORM(IOS_FAMILY)

PlatformViewController *UIDelegate::UIClient::presentingViewController()
{
    if (!m_uiDelegate)
        return nullptr;

    if (!m_uiDelegate->m_delegateMethods.presentingViewControllerForWebView)
        return nullptr;

    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return nullptr;

    return [static_cast<id <WKUIDelegatePrivate>>(delegate) _presentingViewControllerForWebView:m_uiDelegate->m_webView.get().get()];
}

NSDictionary *UIDelegate::UIClient::dataDetectionContext()
{
    if (!m_uiDelegate)
        return nullptr;

    if (!m_uiDelegate->m_delegateMethods.dataDetectionContextForWebView)
        return nullptr;

    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return nullptr;

    return [static_cast<id <WKUIDelegatePrivate>>(delegate) _dataDetectionContextForWebView:m_uiDelegate->m_webView.get().get()];
}

#if ENABLE(POINTER_LOCK)

void UIDelegate::UIClient::requestPointerLock(WebPageProxy* page)
{
    if (!m_uiDelegate)
        return;

    if (!m_uiDelegate->m_delegateMethods.webViewRequestPointerLock && !m_uiDelegate->m_delegateMethods.webViewDidRequestPointerLockCompletionHandler)
        return;

    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    if (m_uiDelegate->m_delegateMethods.webViewRequestPointerLock) {
        [static_cast<id <WKUIDelegatePrivate>>(delegate) _webViewRequestPointerLock:m_uiDelegate->m_webView.get().get()];
        return;
    }

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webViewDidRequestPointerLock:completionHandler:));
    [static_cast<id <WKUIDelegatePrivate>>(delegate) _webViewDidRequestPointerLock:m_uiDelegate->m_webView.get().get() completionHandler:makeBlockPtr([checker = WTFMove(checker), page = RefPtr { page }] (BOOL allow) {
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
    if (!m_uiDelegate)
        return;

    if (!m_uiDelegate->m_delegateMethods.webViewDidLosePointerLock)
        return;

    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    [static_cast<id <WKUIDelegatePrivate>>(delegate) _webViewDidLosePointerLock:m_uiDelegate->m_webView.get().get()];
}

#endif
    
void UIDelegate::UIClient::didShowSafeBrowsingWarning()
{
    if (!m_uiDelegate)
        return;

    if (!m_uiDelegate->m_delegateMethods.webViewDidShowSafeBrowsingWarning)
        return;

    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    [static_cast<id <WKUIDelegatePrivate>>(delegate) _webViewDidShowSafeBrowsingWarning:m_uiDelegate->m_webView.get().get()];
}

void UIDelegate::UIClient::confirmPDFOpening(WebPageProxy& page, const WTF::URL& fileURL, FrameInfoData&& frameInfo, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!m_uiDelegate)
        return completionHandler(true);

    if (!m_uiDelegate->m_delegateMethods.webViewShouldAllowPDFAtURLToOpenFromFrameCompletionHandler)
        return completionHandler(true);

    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return completionHandler(true);

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:shouldAllowPDFAtURL:toOpenFromFrame:completionHandler:));
    [static_cast<id <WKUIDelegatePrivate>>(delegate) _webView:m_uiDelegate->m_webView.get().get() shouldAllowPDFAtURL:fileURL toOpenFromFrame:wrapper(API::FrameInfo::create(WTFMove(frameInfo), &page)) completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] (BOOL result) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();
        completionHandler(result);
    }).get()];
}

#if ENABLE(WEB_AUTHN)

static WebAuthenticationPanelResult webAuthenticationPanelResult(_WKWebAuthenticationPanelResult result)
{
    switch (result) {
    case _WKWebAuthenticationPanelResultUnavailable:
        return WebAuthenticationPanelResult::Unavailable;
    case _WKWebAuthenticationPanelResultPresented:
        return WebAuthenticationPanelResult::Presented;
    case _WKWebAuthenticationPanelResultDidNotPresent:
        return WebAuthenticationPanelResult::DidNotPresent;
    }
    ASSERT_NOT_REACHED();
    return WebAuthenticationPanelResult::Unavailable;
}

void UIDelegate::UIClient::runWebAuthenticationPanel(WebPageProxy& page, API::WebAuthenticationPanel& panel, WebFrameProxy&, FrameInfoData&& frameInfo, CompletionHandler<void(WebAuthenticationPanelResult)>&& completionHandler)
{
    if (!m_uiDelegate)
        return completionHandler(WebAuthenticationPanelResult::Unavailable);

    if (!m_uiDelegate->m_delegateMethods.webViewRunWebAuthenticationPanelInitiatedByFrameCompletionHandler) {
        completionHandler(WebAuthenticationPanelResult::Unavailable);
        return;
    }

    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate) {
        completionHandler(WebAuthenticationPanelResult::Unavailable);
        return;
    }

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:runWebAuthenticationPanel:initiatedByFrame:completionHandler:));
    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate->m_webView.get().get() runWebAuthenticationPanel:wrapper(panel) initiatedByFrame:wrapper(API::FrameInfo::create(WTFMove(frameInfo), &page)) completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] (_WKWebAuthenticationPanelResult result) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();
        completionHandler(webAuthenticationPanelResult(result));
    }).get()];
}

void UIDelegate::UIClient::requestWebAuthenticationNoGesture(API::SecurityOrigin& origin, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!m_uiDelegate)
        return completionHandler(false);

    if (!m_uiDelegate->m_delegateMethods.webViewRequestWebAuthenticationNoGestureForOriginCompletionHandler)
        return completionHandler(false);

    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return completionHandler(false);

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:requestWebAuthenticationNoGestureForOrigin:completionHandler:));
    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate->m_webView.get().get() requestWebAuthenticationNoGestureForOrigin: wrapper(origin) completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] (BOOL result) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();
        completionHandler(result);
    }).get()];
}

#endif // ENABLE(WEB_AUTHN)

void UIDelegate::UIClient::hasVideoInPictureInPictureDidChange(WebPageProxy*, bool hasVideoInPictureInPicture)
{
    if (!m_uiDelegate)
        return;

    if (!m_uiDelegate->m_delegateMethods.webViewHasVideoInPictureInPictureDidChange)
        return;
    
    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return;
    
    [static_cast<id <WKUIDelegatePrivate>>(delegate) _webView:m_uiDelegate->m_webView.get().get() hasVideoInPictureInPictureDidChange:hasVideoInPictureInPicture];
}

void UIDelegate::UIClient::imageOrMediaDocumentSizeChanged(const WebCore::IntSize& newSize)
{
    if (!m_uiDelegate)
        return;

    if (!m_uiDelegate->m_delegateMethods.webViewImageOrMediaDocumentSizeChanged)
        return;

    auto delegate = m_uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    [static_cast<id <WKUIDelegatePrivate>>(delegate) _webView:m_uiDelegate->m_webView.get().get() imageOrMediaDocumentSizeChanged:newSize];
}

void UIDelegate::UIClient::decidePolicyForSpeechRecognitionPermissionRequest(WebKit::WebPageProxy& page, API::SecurityOrigin& origin, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!m_uiDelegate) {
        page.requestSpeechRecognitionPermissionByDefaultAction(origin.securityOrigin(), WTFMove(completionHandler));
        return;
    }

    auto delegate = (id <WKUIDelegatePrivate>)m_uiDelegate->m_delegate.get();
    if (!delegate) {
        page.requestSpeechRecognitionPermissionByDefaultAction(origin.securityOrigin(), WTFMove(completionHandler));
        return;
    }

    if (![delegate respondsToSelector:@selector(_webView:requestSpeechRecognitionPermissionForOrigin:decisionHandler:)]) {
        page.requestSpeechRecognitionPermissionByDefaultAction(origin.securityOrigin(), WTFMove(completionHandler));
        return;
    }

    auto checker = CompletionHandlerCallChecker::create(delegate, @selector(_webView:requestSpeechRecognitionPermissionForOrigin:decisionHandler:));
    [delegate _webView:m_uiDelegate->m_webView.get().get() requestSpeechRecognitionPermissionForOrigin:wrapper(origin) decisionHandler:makeBlockPtr([completionHandler = std::exchange(completionHandler, { }), checker = WTFMove(checker)] (BOOL granted) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();
        completionHandler(granted);
    }).get()];
}

void UIDelegate::UIClient::queryPermission(const String& permissionName, API::SecurityOrigin& origin, CompletionHandler<void(std::optional<WebCore::PermissionState>)>&& callback)
{
    if (!m_uiDelegate) {
        callback(WebCore::PermissionState::Prompt);
        return;
    }

    auto delegate = (id <WKUIDelegatePrivate>)m_uiDelegate->m_delegate.get();
    if (!delegate) {
        callback(WebCore::PermissionState::Prompt);
        return;
    }

    if (![delegate respondsToSelector:@selector(_webView:queryPermission:forOrigin:completionHandler:)]) {
        callback(WebCore::PermissionState::Prompt);
        return;
    }

    auto checker = CompletionHandlerCallChecker::create(delegate, @selector(_webView:queryPermission:forOrigin:completionHandler:));
    [delegate _webView:m_uiDelegate->m_webView.get().get() queryPermission:permissionName forOrigin:wrapper(origin) completionHandler:makeBlockPtr([callback = WTFMove(callback), checker = WTFMove(checker)](WKPermissionDecision permissionState) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();
        switch (permissionState) {
        case WKPermissionDecisionPrompt:
            callback(WebCore::PermissionState::Prompt);
            break;
        case WKPermissionDecisionGrant:
            callback(WebCore::PermissionState::Granted);
            break;
        case WKPermissionDecisionDeny:
            callback(WebCore::PermissionState::Denied);
            break;
        }
    }).get()];
}

void UIDelegate::UIClient::didEnableInspectorBrowserDomain(WebPageProxy&)
{
    if (!m_uiDelegate)
        return;

    if (!m_uiDelegate->m_delegateMethods.webViewDidEnableInspectorBrowserDomain)
        return;

    auto delegate = (id <WKUIDelegatePrivate>)m_uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    [delegate _webViewDidEnableInspectorBrowserDomain:m_uiDelegate->m_webView.get().get()];
}

void UIDelegate::UIClient::didDisableInspectorBrowserDomain(WebPageProxy&)
{
    if (!m_uiDelegate)
        return;

    if (!m_uiDelegate->m_delegateMethods.webViewDidDisableInspectorBrowserDomain)
        return;

    auto delegate = (id <WKUIDelegatePrivate>)m_uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    [delegate _webViewDidDisableInspectorBrowserDomain:m_uiDelegate->m_webView.get().get()];
}

void UIDelegate::UIClient::updateAppBadge(WebPageProxy&, std::optional<uint64_t> badge)
{
    if (!m_uiDelegate)
        return;

    if (!m_uiDelegate->m_delegateMethods.webViewUpdatedAppBadge)
        return;

    auto delegate = (id <WKUIDelegatePrivate>)m_uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    NSNumber *nsBadge = nil;
    if (badge)
        nsBadge = @(*badge);

    [delegate _webView:m_uiDelegate->m_webView.get().get() updatedAppBadge:nsBadge];
}

void UIDelegate::UIClient::updateClientBadge(WebPageProxy&, std::optional<uint64_t> badge)
{
    if (!m_uiDelegate)
        return;

    if (!m_uiDelegate->m_delegateMethods.webViewUpdatedClientBadge)
        return;

    auto delegate = (id <WKUIDelegatePrivate>)m_uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    NSNumber *nsBadge = nil;
    if (badge)
        nsBadge = @(*badge);

    [delegate _webView:m_uiDelegate->m_webView.get().get() updatedClientBadge:nsBadge];
}

#if ENABLE(WEBXR)
static _WKXRSessionMode toWKXRSessionMode(PlatformXR::SessionMode mode)
{
    switch (mode) {
    case PlatformXR::SessionMode::Inline:
        return _WKXRSessionModeInline;
    case PlatformXR::SessionMode::ImmersiveVr:
        return _WKXRSessionModeImmersiveVr;
    case PlatformXR::SessionMode::ImmersiveAr:
        return _WKXRSessionModeImmersiveAr;
    }
}

static _WKXRSessionFeatureFlags toWKXRSessionFeatureFlags(PlatformXR::SessionFeature feature)
{
    switch (feature) {
    case PlatformXR::SessionFeature::ReferenceSpaceTypeViewer:
        return _WKXRSessionFeatureFlagsReferenceSpaceTypeViewer;
    case PlatformXR::SessionFeature::ReferenceSpaceTypeLocal:
        return _WKXRSessionFeatureFlagsReferenceSpaceTypeLocal;
    case PlatformXR::SessionFeature::ReferenceSpaceTypeLocalFloor:
        return _WKXRSessionFeatureFlagsReferenceSpaceTypeLocalFloor;
    case PlatformXR::SessionFeature::ReferenceSpaceTypeBoundedFloor:
        return _WKXRSessionFeatureFlagsReferenceSpaceTypeBoundedFloor;
    case PlatformXR::SessionFeature::ReferenceSpaceTypeUnbounded:
        return _WKXRSessionFeatureFlagsReferenceSpaceTypeUnbounded;
#if ENABLE(WEBXR_HANDS)
    case PlatformXR::SessionFeature::HandTracking:
        return _WKXRSessionFeatureFlagsHandTracking;
#endif
    }
}

static _WKXRSessionFeatureFlags toWKXRSessionFeatureFlags(const PlatformXR::Device::FeatureList& features)
{
    _WKXRSessionFeatureFlags flags = _WKXRSessionFeatureFlagsNone;
    for (auto feature : features)
        flags |= toWKXRSessionFeatureFlags(feature);
    return flags;
}

static std::optional<PlatformXR::Device::FeatureList> toPlatformXRFeatures(_WKXRSessionFeatureFlags featureFlags)
{
    if (featureFlags == _WKXRSessionFeatureFlagsNone)
        return std::nullopt;

    PlatformXR::Device::FeatureList features;
    if (featureFlags & _WKXRSessionFeatureFlagsReferenceSpaceTypeViewer)
        features.append(PlatformXR::SessionFeature::ReferenceSpaceTypeViewer);
    if (featureFlags & _WKXRSessionFeatureFlagsReferenceSpaceTypeLocal)
        features.append(PlatformXR::SessionFeature::ReferenceSpaceTypeLocal);
    if (featureFlags & _WKXRSessionFeatureFlagsReferenceSpaceTypeLocalFloor)
        features.append(PlatformXR::SessionFeature::ReferenceSpaceTypeLocalFloor);
    if (featureFlags & _WKXRSessionFeatureFlagsReferenceSpaceTypeBoundedFloor)
        features.append(PlatformXR::SessionFeature::ReferenceSpaceTypeBoundedFloor);
    if (featureFlags & _WKXRSessionFeatureFlagsReferenceSpaceTypeUnbounded)
        features.append(PlatformXR::SessionFeature::ReferenceSpaceTypeUnbounded);
#if ENABLE(WEBXR_HANDS)
    if (featureFlags & _WKXRSessionFeatureFlagsHandTracking)
        features.append(PlatformXR::SessionFeature::HandTracking);
#endif
    return features;
}

void UIDelegate::UIClient::requestPermissionOnXRSessionFeatures(WebPageProxy&, const WebCore::SecurityOriginData& securityOriginData, PlatformXR::SessionMode mode, const PlatformXR::Device::FeatureList& granted, const PlatformXR::Device::FeatureList& consentRequired, const PlatformXR::Device::FeatureList& consentOptional, CompletionHandler<void(std::optional<PlatformXR::Device::FeatureList>&&)>&& completionHandler)
{
    if (!m_uiDelegate || !m_uiDelegate->m_delegateMethods.webViewRequestPermissionForXRSessionOriginModeAndFeaturesWithCompletionHandler) {
        completionHandler(granted);
        return;
    }

    auto delegate = (id <WKUIDelegatePrivate>)m_uiDelegate->m_delegate.get();
    if (!delegate) {
        completionHandler(granted);
        return;
    }

    auto checker = CompletionHandlerCallChecker::create(delegate, @selector(_webView:requestPermissionForXRSessionOrigin:mode:grantedFeatures:consentRequiredFeatures:consentOptionalFeatures:completionHandler:));
    [delegate _webView:m_uiDelegate->m_webView.get().get() requestPermissionForXRSessionOrigin:securityOriginData.toString() mode:toWKXRSessionMode(mode) grantedFeatures:toWKXRSessionFeatureFlags(granted) consentRequiredFeatures:toWKXRSessionFeatureFlags(consentRequired) consentOptionalFeatures:toWKXRSessionFeatureFlags(consentOptional) completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] (_WKXRSessionFeatureFlags userGrantedFeatures) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();
        completionHandler(toPlatformXRFeatures(userGrantedFeatures));
    }).get()];
}

void UIDelegate::UIClient::startXRSession(WebPageProxy&, CompletionHandler<void(RetainPtr<id>)>&& completionHandler)
{
    if (!m_uiDelegate || !m_uiDelegate->m_delegateMethods.webViewStartXRSessionWithCompletionHandler) {
        completionHandler(nil);
        return;
    }

    auto delegate = (id <WKUIDelegatePrivate>)m_uiDelegate->m_delegate.get();
    if (!delegate) {
        completionHandler(nil);
        return;
    }

    auto checker = CompletionHandlerCallChecker::create(delegate, @selector(_webView:startXRSessionWithCompletionHandler:));
    [delegate _webView:m_uiDelegate->m_webView.get().get() startXRSessionWithCompletionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] (id result) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();
        completionHandler(result);
    }).get()];
}

void UIDelegate::UIClient::endXRSession(WebPageProxy&)
{
    if (!m_uiDelegate || !m_uiDelegate->m_delegateMethods.webViewEndXRSession)
        return;

    auto delegate = (id<WKUIDelegatePrivate>)m_uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    [delegate _webViewEndXRSession:m_uiDelegate->m_webView.get().get()];
}
#endif // ENABLE(WEBXR)

} // namespace WebKit
