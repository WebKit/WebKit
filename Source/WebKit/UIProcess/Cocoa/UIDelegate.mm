/*
 * Copyright (C) 2014-2023 Apple Inc. All rights reserved.
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
#import "APIContextMenuElementInfoMac.h"
#import "APIFrameInfo.h"
#import "APIHitTestResult.h"
#import "APIInspectorConfiguration.h"
#import "CompletionHandlerCallChecker.h"
#import "FrameProcess.h"
#import "MediaPermissionUtilities.h"
#import "MediaUtilities.h"
#import "NativeWebWheelEvent.h"
#import "NavigationActionData.h"
#import "PageLoadState.h"
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
#import "WebFrameProxy.h"
#import "WebOpenPanelResultListenerProxy.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import "_WKContextMenuElementInfoInternal.h"
#import "_WKFrameHandleInternal.h"
#import "_WKHitTestResultInternal.h"
#import "_WKInspectorConfigurationInternal.h"
#import "_WKInspectorInternal.h"
#import "_WKModalContainerInfoInternal.h"
#import "_WKWebAuthenticationPanelInternal.h"
#import <AVFoundation/AVCaptureDevice.h>
#import <AVFoundation/AVMediaFormat.h>
#import <WebCore/AutoplayEvent.h>
#import <WebCore/DataDetection.h>
#import <WebCore/FontAttributes.h>
#import <WebCore/SecurityOrigin.h>
#import <wtf/BlockPtr.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/URL.h>
#import <wtf/cocoa/VectorCocoa.h>

#if PLATFORM(IOS_FAMILY)
#import "TapHandlingResult.h"
#import "WKWebViewIOS.h"
#import "WebIOSEventFactory.h"
#endif

#import <pal/cocoa/AVFoundationSoftLink.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(UIDelegate);
WTF_MAKE_TZONE_ALLOCATED_IMPL_NESTED(UIDelegateUIClient, UIDelegate::UIClient);
#if ENABLE(CONTEXT_MENUS)
WTF_MAKE_TZONE_ALLOCATED_IMPL_NESTED(UIDelegateContextMenuClient, UIDelegate::ContextMenuClient);
#endif

UIDelegate::UIDelegate(WKWebView *webView)
    : m_webView(webView)
{
}

UIDelegate::~UIDelegate() = default;

void UIDelegate::ref() const
{
    [m_webView retain];
}

void UIDelegate::deref() const
{
    [m_webView release];
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
    m_delegateMethods.webViewRequestStorageAccessPanelForDomainUnderCurrentDomainForQuirkDomainsCompletionHandler = [delegate respondsToSelector:@selector(_webView:requestStorageAccessPanelForDomain:underCurrentDomain:forQuirkDomains:completionHandler:)];
    m_delegateMethods.webViewRunBeforeUnloadConfirmPanelWithMessageInitiatedByFrameCompletionHandler = [delegate respondsToSelector:@selector(_webView:runBeforeUnloadConfirmPanelWithMessage:initiatedByFrame:completionHandler:)];
    m_delegateMethods.webViewRequestGeolocationPermissionForOriginDecisionHandler = [delegate respondsToSelector:@selector(_webView:requestGeolocationPermissionForOrigin:initiatedByFrame:decisionHandler:)];
    m_delegateMethods.webViewRequestGeolocationPermissionForFrameDecisionHandler = [delegate respondsToSelector:@selector(_webView:requestGeolocationPermissionForFrame:decisionHandler:)];
    m_delegateMethods.webViewDidResignInputElementStrongPasswordAppearanceWithUserInfo = [delegate respondsToSelector:@selector(_webView:didResignInputElementStrongPasswordAppearanceWithUserInfo:)];
    m_delegateMethods.webViewTakeFocus = [delegate respondsToSelector:@selector(_webView:takeFocus:)];
    m_delegateMethods.webViewHandleAutoplayEventWithFlags = [delegate respondsToSelector:@selector(_webView:handleAutoplayEvent:withFlags:)];
    m_delegateMethods.focusWebViewFromServiceWorker = [delegate respondsToSelector:@selector(_focusWebViewFromServiceWorker:)];
    m_delegateMethods.webViewRunOpenPanelWithParametersInitiatedByFrameCompletionHandler = [delegate respondsToSelector:@selector(webView:runOpenPanelWithParameters:initiatedByFrame:completionHandler:)];
#if PLATFORM(MAC) || HAVE(UIKIT_WITH_MOUSE_SUPPORT)
    m_delegateMethods.webViewMouseDidMoveOverElementWithFlagsUserInfo = [delegate respondsToSelector:@selector(_webView:mouseDidMoveOverElement:withFlags:userInfo:)];
#endif

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
    m_delegateMethods.webViewDidClickAutoFillButtonWithUserInfo = [delegate respondsToSelector:@selector(_webView:didClickAutoFillButtonWithUserInfo:)];
    m_delegateMethods.webViewDrawHeaderInRectForPageWithTitleURL = [delegate respondsToSelector:@selector(_webView:drawHeaderInRect:forPageWithTitle:URL:)];
    m_delegateMethods.webViewDrawFooterInRectForPageWithTitleURL = [delegate respondsToSelector:@selector(_webView:drawFooterInRect:forPageWithTitle:URL:)];
    m_delegateMethods.webViewHeaderHeight = [delegate respondsToSelector:@selector(_webViewHeaderHeight:)];
    m_delegateMethods.webViewFooterHeight = [delegate respondsToSelector:@selector(_webViewFooterHeight:)];
    m_delegateMethods.webViewSaveDataToFileSuggestedFilenameMimeTypeOriginatingURL = [delegate respondsToSelector:@selector(_webView:saveDataToFile:suggestedFilename:mimeType:originatingURL:)];
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
    m_delegateMethods.webViewSetShouldKeepScreenAwake = [delegate respondsToSelector:@selector(_webView:setShouldKeepScreenAwake:)];
#endif
#if PLATFORM(IOS) || PLATFORM(VISION)
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
    m_delegateMethods.webViewRequestWebAuthenticationConditionalMediationRegistrationForUserCompletionHandler = [delegate respondsToSelector:@selector(_webView:requestWebAuthenticationConditionalMediationRegistrationForUser:completionHandler:)];
#endif
    
    m_delegateMethods.webViewDidEnableInspectorBrowserDomain = [delegate respondsToSelector:@selector(_webViewDidEnableInspectorBrowserDomain:)];
    m_delegateMethods.webViewDidDisableInspectorBrowserDomain = [delegate respondsToSelector:@selector(_webViewDidDisableInspectorBrowserDomain:)];

#if ENABLE(WEBXR)
    if ([delegate respondsToSelector:@selector(_webView:requestPermissionForXRSessionOrigin:mode:grantedFeatures:consentRequiredFeatures:consentOptionalFeatures:requiredFeaturesRequested:optionalFeaturesRequested:completionHandler:)])
        m_delegateMethods.webViewRequestPermissionForXRSessionOriginModeAndFeaturesWithCompletionHandler = true;
    else
        m_delegateMethods.webViewRequestPermissionForXRSessionOriginModeAndFeaturesWithCompletionHandler = [delegate respondsToSelector:@selector(_webView:requestPermissionForXRSessionOrigin:mode:grantedFeatures:consentRequiredFeatures:consentOptionalFeatures:completionHandler:)];

    m_delegateMethods.webViewSupportedXRSessionFeatures = [delegate respondsToSelector:@selector(_webView:supportedXRSessionFeatures:arFeatures:)];

#if PLATFORM(IOS_FAMILY)
    if ([delegate respondsToSelector:@selector(_webView:startXRSessionWithFeatures:completionHandler:)])
        m_delegateMethods.webViewStartXRSessionWithCompletionHandler = true;
    else
#endif
        m_delegateMethods.webViewStartXRSessionWithCompletionHandler = [delegate respondsToSelector:@selector(_webView:startXRSessionWithCompletionHandler:)];

    m_delegateMethods.webViewEndXRSession = [delegate respondsToSelector:@selector(_webViewEndXRSession:withReason:)] || [delegate respondsToSelector:@selector(_webViewEndXRSession:)];
#endif // ENABLE(WEBXR)

    m_delegateMethods.webViewRequestNotificationPermissionForSecurityOriginDecisionHandler = [delegate respondsToSelector:@selector(_webView:requestNotificationPermissionForSecurityOrigin:decisionHandler:)];
    m_delegateMethods.webViewRequestCookieConsentWithMoreInfoHandlerDecisionHandler = [delegate respondsToSelector:@selector(_webView:requestCookieConsentWithMoreInfoHandler:decisionHandler:)];

    m_delegateMethods.webViewUpdatedAppBadge = [delegate respondsToSelector:@selector(_webView:updatedAppBadge:fromSecurityOrigin:)];
    m_delegateMethods.webViewUpdatedClientBadge = [delegate respondsToSelector:@selector(_webView:updatedClientBadge:fromSecurityOrigin:)];

    m_delegateMethods.webViewDidAdjustVisibilityWithSelectors = [delegate respondsToSelector:@selector(_webView:didAdjustVisibilityWithSelectors:)];

#if ENABLE(GAMEPAD)
    m_delegateMethods.webViewRecentlyAccessedGamepadsForTesting = [delegate respondsToSelector:@selector(_webViewRecentlyAccessedGamepadsForTesting:)];
    m_delegateMethods.webViewStoppedAccessingGamepadsForTesting = [delegate respondsToSelector:@selector(_webViewStoppedAccessingGamepadsForTesting:)];
#endif
}

#if ENABLE(CONTEXT_MENUS)
UIDelegate::ContextMenuClient::ContextMenuClient(UIDelegate& uiDelegate)
    : m_uiDelegate(uiDelegate)
{
}

UIDelegate::ContextMenuClient::~ContextMenuClient()
{
}

void UIDelegate::ContextMenuClient::menuFromProposedMenu(WebPageProxy& page, NSMenu *menu, const ContextMenuContextData& data, API::Object* userInfo, CompletionHandler<void(RetainPtr<NSMenu>&&)>&& completionHandler)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return completionHandler(menu);

    if (!uiDelegate->m_delegateMethods.webViewContextMenuForElement
        && !uiDelegate->m_delegateMethods.webViewContextMenuForElementUserInfo
        && !uiDelegate->m_delegateMethods.webViewGetContextMenuFromProposedMenuForElementUserInfoCompletionHandler)
        return completionHandler(menu);

    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return completionHandler(menu);

    auto contextMenuElementInfo = API::ContextMenuElementInfoMac::create(data, page);
    if (uiDelegate->m_delegateMethods.webViewGetContextMenuFromProposedMenuForElementUserInfoCompletionHandler) {
        auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:getContextMenuFromProposedMenu:forElement:userInfo:completionHandler:));
        [(id<WKUIDelegatePrivate>)delegate _webView:uiDelegate->m_webView.get().get() getContextMenuFromProposedMenu:menu forElement:wrapper(contextMenuElementInfo.get()) userInfo:userInfo ? static_cast<id<NSSecureCoding>>(userInfo->wrapper()) : nil completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] (NSMenu *menu) mutable {
            if (checker->completionHandlerHasBeenCalled())
                return;
            checker->didCallCompletionHandler();
            completionHandler(menu);
        }).get()];
        return;
    }
    
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (uiDelegate->m_delegateMethods.webViewContextMenuForElement)
        return completionHandler([(id<WKUIDelegatePrivate>)delegate _webView:uiDelegate->m_webView.get().get() contextMenu:menu forElement:wrapper(contextMenuElementInfo.get())]);

    completionHandler([(id<WKUIDelegatePrivate>)delegate _webView:uiDelegate->m_webView.get().get() contextMenu:menu forElement:wrapper(contextMenuElementInfo.get()) userInfo:userInfo ? static_cast<id<NSSecureCoding>>(userInfo->wrapper()) : nil]);
ALLOW_DEPRECATED_DECLARATIONS_END
}
#endif

UIDelegate::UIClient::UIClient(UIDelegate& uiDelegate)
    : m_uiDelegate(uiDelegate)
{
}

UIDelegate::UIClient::~UIClient() = default;

#if PLATFORM(MAC) || HAVE(UIKIT_WITH_MOUSE_SUPPORT)
void UIDelegate::UIClient::mouseDidMoveOverElement(WebPageProxy& page, const WebHitTestResultData& data, OptionSet<WebEventModifier> modifiers, API::Object* userInfo)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return;

    if (!uiDelegate->m_delegateMethods.webViewMouseDidMoveOverElementWithFlagsUserInfo)
        return;

    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    auto apiHitTestResult = API::HitTestResult::create(data, &page);
#if PLATFORM(MAC)
    auto modifierFlags = WebEventFactory::toNSEventModifierFlags(modifiers);
#else
    auto modifierFlags = WebKit::WebIOSEventFactory::toUIKeyModifierFlags(modifiers);
#endif
    [(id <WKUIDelegatePrivate>)delegate _webView:uiDelegate->m_webView.get().get() mouseDidMoveOverElement:wrapper(apiHitTestResult.get()) withFlags:modifierFlags userInfo:userInfo ? static_cast<id <NSSecureCoding>>(userInfo->wrapper()) : nil];
}
#endif

void UIDelegate::UIClient::createNewPage(WebKit::WebPageProxy&, Ref<API::PageConfiguration>&& configuration, Ref<API::NavigationAction>&& navigationAction, CompletionHandler<void(RefPtr<WebPageProxy>&&)>&& completionHandler)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return completionHandler(nullptr);

    auto delegate = uiDelegate->m_delegate.get();
    ASSERT(delegate);

    ASSERT(configuration->windowFeatures());
    auto apiWindowFeatures = API::WindowFeatures::create(*configuration->windowFeatures());
    auto openerInfo = configuration->openerInfo();

    if (uiDelegate->m_delegateMethods.webViewCreateWebViewWithConfigurationForNavigationActionWindowFeaturesAsync) {
        auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:createWebViewWithConfiguration:forNavigationAction:windowFeatures:completionHandler:));

        [(id<WKUIDelegatePrivate>)delegate _webView:uiDelegate->m_webView.get().get() createWebViewWithConfiguration:wrapper(configuration) forNavigationAction:wrapper(navigationAction) windowFeatures:wrapper(apiWindowFeatures) completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker), openerInfo] (WKWebView *webView) mutable {
            if (checker->completionHandlerHasBeenCalled())
                return;
            checker->didCallCompletionHandler();

            if (!webView)
                return completionHandler(nullptr);

            // FIXME: Move this to WebPageProxy once rdar://134317255 and rdar://134317400 are resolved.
            if (openerInfo != webView->_configuration->_pageConfiguration->openerInfo())
                [NSException raise:NSInternalInconsistencyException format:@"Returned WKWebView was not created with the given configuration."];

            completionHandler(webView->_page.get());
        }).get()];
        return;
    }
    if (!uiDelegate->m_delegateMethods.webViewCreateWebViewWithConfigurationForNavigationActionWindowFeatures)
        return completionHandler(nullptr);

    RetainPtr<WKWebView> webView = [delegate webView:uiDelegate->m_webView.get().get() createWebViewWithConfiguration:wrapper(configuration) forNavigationAction:wrapper(navigationAction) windowFeatures:wrapper(apiWindowFeatures)];
    if (!webView)
        return completionHandler(nullptr);

    // FIXME: Move this to WebPageProxy once rdar://134317255 and rdar://134317400 are resolved.
    if (openerInfo != webView.get()->_configuration->_pageConfiguration->openerInfo())
        [NSException raise:NSInternalInconsistencyException format:@"Returned WKWebView was not created with the given configuration."];
    completionHandler(webView->_page.get());
}

void UIDelegate::UIClient::runJavaScriptAlert(WebPageProxy& page, const WTF::String& message, WebFrameProxy*, FrameInfoData&& frameInfo, Function<void()>&& completionHandler)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return completionHandler();

    if (!uiDelegate->m_delegateMethods.webViewRunJavaScriptAlertPanelWithMessageInitiatedByFrameCompletionHandler) {
        completionHandler();
        return;
    }

    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate) {
        completionHandler();
        return;
    }

    page.makeViewBlankIfUnpaintedSinceLastLoadCommit();

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(webView:runJavaScriptAlertPanelWithMessage:initiatedByFrame:completionHandler:));
    [delegate webView:uiDelegate->m_webView.get().get() runJavaScriptAlertPanelWithMessage:message initiatedByFrame:wrapper(API::FrameInfo::create(WTFMove(frameInfo), &page)).get() completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] {
        if (checker->completionHandlerHasBeenCalled())
            return;
        completionHandler();
        checker->didCallCompletionHandler();
    }).get()];
}

void UIDelegate::UIClient::runJavaScriptConfirm(WebPageProxy& page, const WTF::String& message, WebFrameProxy* webFrameProxy, FrameInfoData&& frameInfo, Function<void(bool)>&& completionHandler)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return completionHandler(false);

    if (!uiDelegate->m_delegateMethods.webViewRunJavaScriptConfirmPanelWithMessageInitiatedByFrameCompletionHandler) {
        completionHandler(false);
        return;
    }

    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate) {
        completionHandler(false);
        return;
    }

    page.makeViewBlankIfUnpaintedSinceLastLoadCommit();

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(webView:runJavaScriptConfirmPanelWithMessage:initiatedByFrame:completionHandler:));
    [delegate webView:uiDelegate->m_webView.get().get() runJavaScriptConfirmPanelWithMessage:message initiatedByFrame:wrapper(API::FrameInfo::create(WTFMove(frameInfo), &page)).get() completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] (BOOL result) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        completionHandler(result);
        checker->didCallCompletionHandler();
    }).get()];
}

void UIDelegate::UIClient::runJavaScriptPrompt(WebPageProxy& page, const WTF::String& message, const WTF::String& defaultValue, WebFrameProxy*, FrameInfoData&& frameInfo, Function<void(const WTF::String&)>&& completionHandler)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return completionHandler({ });

    if (!uiDelegate->m_delegateMethods.webViewRunJavaScriptTextInputPanelWithPromptDefaultTextInitiatedByFrameCompletionHandler) {
        completionHandler(String());
        return;
    }

    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate) {
        completionHandler(String());
        return;
    }

    page.makeViewBlankIfUnpaintedSinceLastLoadCommit();

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(webView:runJavaScriptTextInputPanelWithPrompt:defaultText:initiatedByFrame:completionHandler:));
    [delegate webView:uiDelegate->m_webView.get().get() runJavaScriptTextInputPanelWithPrompt:message defaultText:defaultValue initiatedByFrame:wrapper(API::FrameInfo::create(WTFMove(frameInfo), &page)).get() completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] (NSString *result) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        completionHandler(result);
        checker->didCallCompletionHandler();
    }).get()];
}

void UIDelegate::UIClient::requestStorageAccessConfirm(WebPageProxy& webPageProxy, WebFrameProxy*, const WebCore::RegistrableDomain& requestingDomain, const WebCore::RegistrableDomain& currentDomain, std::optional<WebCore::OrganizationStorageAccessPromptQuirk>&& organizationStorageAccessPromptQuirk, CompletionHandler<void(bool)>&& completionHandler)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return completionHandler(false);

    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate) {
        completionHandler(false);
        return;
    }

    // Some sites have quirks where multiple login domains require storage access.
    auto additionalLoginDomain = WebCore::NetworkStorageSession::findAdditionalLoginDomain(currentDomain, requestingDomain);

    if (organizationStorageAccessPromptQuirk || additionalLoginDomain) {
        if (uiDelegate->m_delegateMethods.webViewRequestStorageAccessPanelForDomainUnderCurrentDomainForQuirkDomainsCompletionHandler) {

            NSMutableDictionary<NSString *, NSArray<NSString *> *> *quirkDomains = [NSMutableDictionary dictionaryWithCapacity:1];
            if (organizationStorageAccessPromptQuirk) {
                for (auto& [topFrameDomain, subFrameDomains] : organizationStorageAccessPromptQuirk->quirkDomains) {
                    NSMutableArray<NSString *> *mutableSubFrameDomains = [NSMutableArray arrayWithCapacity:subFrameDomains.size()];
                    for (auto& subFrameDomain : subFrameDomains)
                        [mutableSubFrameDomains addObject:subFrameDomain.string()];
                    [quirkDomains setObject:mutableSubFrameDomains forKey:topFrameDomain.string()];
                }
            } else
                [quirkDomains setObject:@[additionalLoginDomain->string()] forKey:currentDomain.string()];

            auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:requestStorageAccessPanelForDomain:underCurrentDomain:forQuirkDomains:completionHandler:));
            [(id<WKUIDelegatePrivate>)delegate _webView:uiDelegate->m_webView.get().get() requestStorageAccessPanelForDomain:requestingDomain.string() underCurrentDomain:currentDomain.string() forQuirkDomains:quirkDomains completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] (BOOL result) mutable {
                if (checker->completionHandlerHasBeenCalled())
                    return;
                completionHandler(result);
                checker->didCallCompletionHandler();
            }).get()];
            return;
        }
    }

    if (organizationStorageAccessPromptQuirk) {
#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
        presentStorageAccessAlertSSOQuirk(uiDelegate->m_webView.get().get(), organizationStorageAccessPromptQuirk->organizationName, organizationStorageAccessPromptQuirk->quirkDomains, WTFMove(completionHandler));
#endif
        return;
    }

    // Some sites have quirks where multiple login domains require storage access.
    if (additionalLoginDomain) {
#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
        presentStorageAccessAlertQuirk(uiDelegate->m_webView.get().get(), requestingDomain, *additionalLoginDomain, currentDomain, WTFMove(completionHandler));
#endif
        return;
    }

    if (!uiDelegate->m_delegateMethods.webViewRequestStorageAccessPanelUnderFirstPartyCompletionHandler) {
#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
        presentStorageAccessAlert(uiDelegate->m_webView.get().get(), requestingDomain, currentDomain, WTFMove(completionHandler));
#endif
        return;
    }

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:requestStorageAccessPanelForDomain:underCurrentDomain:completionHandler:));
    [(id <WKUIDelegatePrivate>)delegate _webView:uiDelegate->m_webView.get().get() requestStorageAccessPanelForDomain:requestingDomain.string() underCurrentDomain:currentDomain.string() completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] (BOOL result) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        completionHandler(result);
        checker->didCallCompletionHandler();
    }).get()];
}

void UIDelegate::UIClient::decidePolicyForGeolocationPermissionRequest(WebKit::WebPageProxy& page, WebKit::WebFrameProxy& frame, const FrameInfoData& frameInfo, Function<void(bool)>& completionHandler)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate || (!uiDelegate->m_delegateMethods.webViewRequestGeolocationPermissionForFrameDecisionHandler && !uiDelegate->m_delegateMethods.webViewRequestGeolocationPermissionForOriginDecisionHandler))
        return;

    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    if (uiDelegate->m_delegateMethods.webViewRequestGeolocationPermissionForOriginDecisionHandler) {
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
        [(id<WKUIDelegatePrivate>)delegate _webView:uiDelegate->m_webView.get().get() requestGeolocationPermissionForOrigin:wrapper(API::SecurityOrigin::create(securityOrigin.get())).get() initiatedByFrame:wrapper(API::FrameInfo::create(FrameInfoData { frameInfo }, &page)).get() decisionHandler:decisionHandler.get()];
        return;
    }

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:requestGeolocationPermissionForFrame:decisionHandler:));
    [(id<WKUIDelegatePrivate>)delegate _webView:uiDelegate->m_webView.get().get() requestGeolocationPermissionForFrame:wrapper(API::FrameInfo::create(FrameInfoData { frameInfo }, &page)).get() decisionHandler:makeBlockPtr([completionHandler = std::exchange(completionHandler, nullptr), checker = WTFMove(checker)] (BOOL result) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();
        completionHandler(result);
    }).get()];
}

void UIDelegate::UIClient::didResignInputElementStrongPasswordAppearance(WebPageProxy&, API::Object* userInfo)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return;
    if (!uiDelegate->m_delegateMethods.webViewDidResignInputElementStrongPasswordAppearanceWithUserInfo)
        return;

    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    [(id <WKUIDelegatePrivate>)delegate _webView:uiDelegate->m_webView.get().get() didResignInputElementStrongPasswordAppearanceWithUserInfo:userInfo ? static_cast<id <NSSecureCoding>>(userInfo->wrapper()) : nil];
}

bool UIDelegate::UIClient::canRunBeforeUnloadConfirmPanel() const
{
    RefPtr uiDelegate = m_uiDelegate.get();
    return uiDelegate && uiDelegate->m_delegateMethods.webViewRunBeforeUnloadConfirmPanelWithMessageInitiatedByFrameCompletionHandler;
}

void UIDelegate::UIClient::runBeforeUnloadConfirmPanel(WebPageProxy& page, const WTF::String& message, WebFrameProxy*, FrameInfoData&& frameInfo, Function<void(bool)>&& completionHandler)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return completionHandler(false);

    if (!uiDelegate->m_delegateMethods.webViewRunBeforeUnloadConfirmPanelWithMessageInitiatedByFrameCompletionHandler) {
        completionHandler(false);
        return;
    }

    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate) {
        completionHandler(false);
        return;
    }

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:runBeforeUnloadConfirmPanelWithMessage:initiatedByFrame:completionHandler:));
    [(id<WKUIDelegatePrivate>)delegate _webView:uiDelegate->m_webView.get().get() runBeforeUnloadConfirmPanelWithMessage:message initiatedByFrame:wrapper(API::FrameInfo::create(WTFMove(frameInfo), &page)).get() completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] (BOOL result) mutable {
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

    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return completionHandler(defaultPerOriginDatabaseQuota);
    if (!uiDelegate->m_delegateMethods.webViewDecideDatabaseQuotaForSecurityOriginCurrentQuotaCurrentOriginUsageCurrentDatabaseUsageExpectedUsageDecisionHandler && !uiDelegate->m_delegateMethods.webViewDecideDatabaseQuotaForSecurityOriginDatabaseNameDisplayNameCurrentQuotaCurrentOriginUsageCurrentDatabaseUsageExpectedUsageDecisionHandler) {

        completionHandler(defaultPerOriginDatabaseQuota);
        return;
    }

    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate) {
        completionHandler(defaultPerOriginDatabaseQuota);
        return;
    }

    ASSERT(securityOrigin);

    if (uiDelegate->m_delegateMethods.webViewDecideDatabaseQuotaForSecurityOriginDatabaseNameDisplayNameCurrentQuotaCurrentOriginUsageCurrentDatabaseUsageExpectedUsageDecisionHandler) {
        auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:decideDatabaseQuotaForSecurityOrigin:databaseName:displayName:currentQuota:currentOriginUsage:currentDatabaseUsage:expectedUsage:decisionHandler:));
        [(id <WKUIDelegatePrivate>)delegate _webView:uiDelegate->m_webView.get().get() decideDatabaseQuotaForSecurityOrigin:wrapper(*securityOrigin) databaseName:databaseName displayName:displayName currentQuota:currentQuota currentOriginUsage:currentOriginUsage currentDatabaseUsage:currentUsage expectedUsage:expectedUsage decisionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)](unsigned long long newQuota) {
            if (checker->completionHandlerHasBeenCalled())
                return;
            checker->didCallCompletionHandler();
            completionHandler(newQuota);
        }).get()];
        return;
    }

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:decideDatabaseQuotaForSecurityOrigin:currentQuota:currentOriginUsage:currentDatabaseUsage:expectedUsage:decisionHandler:));
    [(id <WKUIDelegatePrivate>)delegate _webView:uiDelegate->m_webView.get().get() decideDatabaseQuotaForSecurityOrigin:wrapper(*securityOrigin) currentQuota:currentQuota currentOriginUsage:currentOriginUsage currentDatabaseUsage:currentUsage expectedUsage:expectedUsage decisionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)](unsigned long long newQuota) {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();
        completionHandler(newQuota);
    }).get()];
}

#if PLATFORM(IOS) || PLATFORM(VISION)
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
#if PLATFORM(IOS) || PLATFORM(VISION)
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return false;
    if (!uiDelegate->m_delegateMethods.webViewLockScreenOrientation)
        return false;
    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return false;

    return [(id<WKUIDelegatePrivate>)delegate _webViewLockScreenOrientation:uiDelegate->m_webView.get().get() lockType:toWKScreenOrientationType(orientation)];
#else
    UNUSED_PARAM(orientation);
    return false;
#endif
}

void UIDelegate::UIClient::unlockScreenOrientation(WebPageProxy&)
{
#if PLATFORM(IOS) || PLATFORM(VISION)
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return;
    if (!uiDelegate->m_delegateMethods.webViewUnlockScreenOrientation)
        return;
    if (auto delegate = uiDelegate->m_delegate.get())
        [(id<WKUIDelegatePrivate>)delegate _webViewUnlockScreenOrientation:uiDelegate->m_webView.get().get()];
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
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return false;

    if (!uiDelegate->m_delegateMethods.webViewTakeFocus)
        return false;
    
    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return false;
    
    [(id <WKUIDelegatePrivate>)delegate _webView:uiDelegate->m_webView.get().get() takeFocus:toWKFocusDirection(direction)];
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
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return;

    if (!uiDelegate->m_delegateMethods.webViewHandleAutoplayEventWithFlags)
        return;
    
    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return;
    
    [(id <WKUIDelegatePrivate>)delegate _webView:uiDelegate->m_webView.get().get() handleAutoplayEvent:toWKAutoplayEvent(event) withFlags:toWKAutoplayEventFlags(flags)];
}

void UIDelegate::UIClient::decidePolicyForNotificationPermissionRequest(WebKit::WebPageProxy&, API::SecurityOrigin& securityOrigin, CompletionHandler<void(bool allowed)>&& completionHandler)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return completionHandler(false);

    if (!uiDelegate->m_delegateMethods.webViewRequestNotificationPermissionForSecurityOriginDecisionHandler)
        return completionHandler(false);

    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return completionHandler(false);

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:requestNotificationPermissionForSecurityOrigin:decisionHandler:));
    [(id <WKUIDelegatePrivate>)delegate _webView:uiDelegate->m_webView.get().get() requestNotificationPermissionForSecurityOrigin:wrapper(securityOrigin) decisionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] (BOOL result) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();

        completionHandler(result);
    }).get()];
}

void UIDelegate::UIClient::requestCookieConsent(CompletionHandler<void(WebCore::CookieConsentDecisionResult)>&& completion)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return completion(WebCore::CookieConsentDecisionResult::NotSupported);

    if (!uiDelegate->m_delegateMethods.webViewRequestCookieConsentWithMoreInfoHandlerDecisionHandler)
        return completion(WebCore::CookieConsentDecisionResult::NotSupported);

    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return completion(WebCore::CookieConsentDecisionResult::NotSupported);

    // FIXME: Add support for the 'more info' handler.
    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:requestCookieConsentWithMoreInfoHandler:decisionHandler:));
    [(id <WKUIDelegatePrivate>)delegate _webView:uiDelegate->m_webView.get().get() requestCookieConsentWithMoreInfoHandler:nil decisionHandler:makeBlockPtr([completion = WTFMove(completion), checker = WTFMove(checker)] (BOOL decision) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();
        completion(decision ? WebCore::CookieConsentDecisionResult::Consent : WebCore::CookieConsentDecisionResult::Dissent);
    }).get()];
}

bool UIDelegate::UIClient::focusFromServiceWorker(WebKit::WebPageProxy& proxy)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    bool hasImplementation = uiDelegate && uiDelegate->m_delegateMethods.focusWebViewFromServiceWorker && uiDelegate->m_delegate.get();
    if (!hasImplementation) {
#if PLATFORM(MAC)
        auto* webView = uiDelegate ? uiDelegate->m_webView.get().get() : nullptr;
        if (!webView || !webView.window)
            return false;


        [webView.window makeKeyAndOrderFront:nil];
        [[webView window] makeFirstResponder:webView];
        return true;
#else
        return false;
#endif
    }
    return [(id<WKUIDelegatePrivate>)uiDelegate->m_delegate.get() _focusWebViewFromServiceWorker:uiDelegate->m_webView.get().get()];
}

bool UIDelegate::UIClient::runOpenPanel(WebPageProxy& page, WebFrameProxy* webFrameProxy, FrameInfoData&& frameInfo, API::OpenPanelParameters* openPanelParameters, WebOpenPanelResultListenerProxy* listener)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return false;

    if (!uiDelegate->m_delegateMethods.webViewRunOpenPanelWithParametersInitiatedByFrameCompletionHandler)
        return false;

    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return false;

    Ref frame = API::FrameInfo::create(WTFMove(frameInfo), &page);

    Ref checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(webView:runOpenPanelWithParameters:initiatedByFrame:completionHandler:));

    [delegate webView:uiDelegate->m_webView.get().get() runOpenPanelWithParameters:wrapper(*openPanelParameters) initiatedByFrame:wrapper(frame) completionHandler:makeBlockPtr([checker = WTFMove(checker), openPanelParameters = Ref { *openPanelParameters }, listener = RefPtr { listener }] (NSArray *URLs) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();

        if (!URLs) {
            listener->cancel();
            return;
        }

        Vector<String> filenames;
#if PLATFORM(IOS)
        RetainPtr<NSFileCoordinator> uploadFileCoordinator = adoptNS([[NSFileCoordinator alloc] init]);
        RetainPtr<NSFileManager> uploadFileManager = adoptNS([[NSFileManager alloc] init]);
        for (NSURL *url in URLs) {
            auto [operationResult, maybeMovedURL, temporaryURL] = [WKFileUploadPanel _moveToNewTemporaryDirectory:url fileCoordinator:uploadFileCoordinator.get() fileManager:uploadFileManager.get() asCopy:YES];
            if (operationResult == WebKit::MovedSuccessfully::Yes)
                filenames.append(maybeMovedURL.get().path);
        }
#else
        for (NSURL *url in URLs)
            filenames.append(url.path);
#endif
        listener->chooseFiles(filenames, openPanelParameters->allowedMIMETypes()->toStringVector());
    }).get()];

    return true;
}

#if PLATFORM(MAC)
bool UIDelegate::UIClient::canRunModal() const
{
    RefPtr uiDelegate = m_uiDelegate.get();
    return uiDelegate && uiDelegate->m_delegateMethods.webViewRunModal;
}

void UIDelegate::UIClient::runModal(WebPageProxy&)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return;

    if (!uiDelegate->m_delegateMethods.webViewRunModal)
        return;
    
    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    [(id <WKUIDelegatePrivate>)delegate _webViewRunModal:uiDelegate->m_webView.get().get()];
}

float UIDelegate::UIClient::headerHeight(WebPageProxy&, WebFrameProxy& webFrameProxy)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return 0;

    if (!uiDelegate->m_delegateMethods.webViewHeaderHeight)
        return 0;
    
    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return 0;
    
    return [(id <WKUIDelegatePrivate>)delegate _webViewHeaderHeight:uiDelegate->m_webView.get().get()];
}

float UIDelegate::UIClient::footerHeight(WebPageProxy&, WebFrameProxy&)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return 0;

    if (!uiDelegate->m_delegateMethods.webViewFooterHeight)
        return 0;
    
    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return 0;
    
    return [(id <WKUIDelegatePrivate>)delegate _webViewFooterHeight:uiDelegate->m_webView.get().get()];
}

void UIDelegate::UIClient::drawHeader(WebPageProxy&, WebFrameProxy& frame, WebCore::FloatRect&& rect)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return;

    if (!uiDelegate->m_delegateMethods.webViewDrawHeaderInRectForPageWithTitleURL)
        return;
    
    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return;
    
    [(id <WKUIDelegatePrivate>)delegate _webView:uiDelegate->m_webView.get().get() drawHeaderInRect:rect forPageWithTitle:frame.title() URL:frame.url()];
}

void UIDelegate::UIClient::drawFooter(WebPageProxy&, WebFrameProxy& frame, WebCore::FloatRect&& rect)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return;

    if (!uiDelegate->m_delegateMethods.webViewDrawFooterInRectForPageWithTitleURL)
        return;
    
    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return;
    
    [(id <WKUIDelegatePrivate>)delegate _webView:uiDelegate->m_webView.get().get() drawFooterInRect:rect forPageWithTitle:frame.title() URL:frame.url()];
}

void UIDelegate::UIClient::pageDidScroll(WebPageProxy*)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return;

    if (!uiDelegate->m_delegateMethods.webViewDidScroll)
        return;
    
    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return;
    
    [(id <WKUIDelegatePrivate>)delegate _webViewDidScroll:uiDelegate->m_webView.get().get()];
}

void UIDelegate::UIClient::focus(WebPageProxy*)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return;

    if (!uiDelegate->m_delegateMethods.focusWebView)
        return;
    
    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    [(id <WKUIDelegatePrivate>)delegate _focusWebView:uiDelegate->m_webView.get().get()];
}

void UIDelegate::UIClient::unfocus(WebPageProxy*)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return;

    if (!uiDelegate->m_delegateMethods.unfocusWebView)
        return;
    
    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return;
    
    [(id <WKUIDelegatePrivate>)delegate _unfocusWebView:uiDelegate->m_webView.get().get()];
}

void UIDelegate::UIClient::didNotHandleWheelEvent(WebPageProxy*, const NativeWebWheelEvent& event)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return;

    if (!uiDelegate->m_delegateMethods.webViewDidNotHandleWheelEvent)
        return;
    
    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return;
    
    [(id <WKUIDelegatePrivate>)delegate _webView:uiDelegate->m_webView.get().get() didNotHandleWheelEvent:event.nativeEvent()];
}

void UIDelegate::UIClient::setIsResizable(WebKit::WebPageProxy&, bool resizable)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return;

    if (!uiDelegate->m_delegateMethods.webViewSetResizable)
        return;
    
    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return;
    
    [(id <WKUIDelegatePrivate>)delegate _webView:uiDelegate->m_webView.get().get() setResizable:resizable];
}

void UIDelegate::UIClient::setWindowFrame(WebKit::WebPageProxy&, const WebCore::FloatRect& frame)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return;

    if (!uiDelegate->m_delegateMethods.webViewSetWindowFrame)
        return;
    
    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return;
    
    [(id <WKUIDelegatePrivate>)delegate _webView:uiDelegate->m_webView.get().get() setWindowFrame:frame];
}

void UIDelegate::UIClient::windowFrame(WebKit::WebPageProxy&, Function<void(WebCore::FloatRect)>&& completionHandler)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return completionHandler({ });

    if (!uiDelegate->m_delegateMethods.webViewGetWindowFrameWithCompletionHandler)
        return completionHandler({ });
    
    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return completionHandler({ });
    
    [(id <WKUIDelegatePrivate>)delegate _webView:uiDelegate->m_webView.get().get() getWindowFrameWithCompletionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:getWindowFrameWithCompletionHandler:))](CGRect frame) {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();
        completionHandler(frame);
    }).get()];
}

void UIDelegate::UIClient::toolbarsAreVisible(WebPageProxy&, Function<void(bool)>&& completionHandler)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return completionHandler(true);

    if (!uiDelegate->m_delegateMethods.webViewGetToolbarsAreVisibleWithCompletionHandler)
        return completionHandler(true);
    
    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return completionHandler(true);
    
    [(id <WKUIDelegatePrivate>)delegate _webView:uiDelegate->m_webView.get().get() getToolbarsAreVisibleWithCompletionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:getToolbarsAreVisibleWithCompletionHandler:))](BOOL visible) {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();
        completionHandler(visible);
    }).get()];
}

void UIDelegate::UIClient::didClickAutoFillButton(WebPageProxy&, API::Object* userInfo)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return;

    if (!uiDelegate->m_delegateMethods.webViewDidClickAutoFillButtonWithUserInfo)
        return;
    
    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return;
    
    [(id <WKUIDelegatePrivate>)delegate _webView:uiDelegate->m_webView.get().get() didClickAutoFillButtonWithUserInfo:userInfo ? static_cast<id <NSSecureCoding>>(userInfo->wrapper()) : nil];
}

void UIDelegate::UIClient::showPage(WebPageProxy*)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return;

    if (!uiDelegate->m_delegateMethods.showWebView)
        return;

    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return;
    
    [(id <WKUIDelegatePrivate>)delegate _showWebView:uiDelegate->m_webView.get().get()];
}
    
void UIDelegate::UIClient::saveDataToFileInDownloadsFolder(WebPageProxy*, const WTF::String& suggestedFilename, const WTF::String& mimeType, const URL& originatingURL, API::Data& data)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return;

    if (!uiDelegate->m_delegateMethods.webViewSaveDataToFileSuggestedFilenameMimeTypeOriginatingURL)
        return;
    
    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    [(id <WKUIDelegatePrivate>)delegate _webView:uiDelegate->m_webView.get().get() saveDataToFile:wrapper(data) suggestedFilename:suggestedFilename mimeType:mimeType originatingURL:originatingURL];
}

Ref<API::InspectorConfiguration> UIDelegate::UIClient::configurationForLocalInspector(WebPageProxy&, WebInspectorUIProxy& inspector)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return API::InspectorConfiguration::create();

    if (!uiDelegate->m_delegateMethods.webViewConfigurationForLocalInspector)
        return API::InspectorConfiguration::create();

    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return API::InspectorConfiguration::create();

    return static_cast<API::InspectorConfiguration&>([[(id <WKUIDelegatePrivate>)delegate _webView:uiDelegate->m_webView.get().get() configurationForLocalInspector:wrapper(inspector)] _apiObject]);
}

void UIDelegate::UIClient::didAttachLocalInspector(WebPageProxy&, WebInspectorUIProxy& inspector)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return;

    if (!uiDelegate->m_delegateMethods.webViewDidAttachLocalInspector)
        return;

    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    [(id <WKUIDelegatePrivate>)delegate _webView:uiDelegate->m_webView.get().get() didAttachLocalInspector:wrapper(inspector)];
}

void UIDelegate::UIClient::willCloseLocalInspector(WebPageProxy&, WebInspectorUIProxy& inspector)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return;

    if (!uiDelegate->m_delegateMethods.webViewWillCloseLocalInspector)
        return;

    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    [(id <WKUIDelegatePrivate>)delegate _webView:uiDelegate->m_webView.get().get() willCloseLocalInspector:wrapper(inspector)];
}

#endif

#if ENABLE(DEVICE_ORIENTATION)
void UIDelegate::UIClient::shouldAllowDeviceOrientationAndMotionAccess(WebKit::WebPageProxy& page, WebFrameProxy& webFrameProxy, FrameInfoData&& frameInfo, CompletionHandler<void(bool)>&& completionHandler)
{
    Ref securityOrigin = WebCore::SecurityOrigin::createFromString(page.pageLoadState().activeURL());
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate || !uiDelegate->m_delegate.get() || !uiDelegate->m_delegateMethods.webViewRequestDeviceOrientationAndMotionPermissionForOriginDecisionHandler) {
        alertForPermission(page, MediaPermissionReason::DeviceOrientation, securityOrigin->data(), WTFMove(completionHandler));
        return;
    }

    auto delegate = uiDelegate->m_delegate.get();
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
    [delegate webView:uiDelegate->m_webView.get().get() requestDeviceOrientationAndMotionPermissionForOrigin:wrapper(API::SecurityOrigin::create(securityOrigin.get())).get() initiatedByFrame:wrapper(API::FrameInfo::create(WTFMove(frameInfo), &page)).get() decisionHandler:decisionHandler.get()];
}
#endif

void UIDelegate::UIClient::didChangeFontAttributes(const WebCore::FontAttributes& fontAttributes)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return;

    if (!needsFontAttributes())
        return;

    auto privateUIDelegate = (id <WKUIDelegatePrivate>)uiDelegate->m_delegate.get();
    [privateUIDelegate _webView:uiDelegate->m_webView.get().get() didChangeFontAttributes:fontAttributes.createDictionary().get()];
}

void UIDelegate::UIClient::callDisplayCapturePermissionDelegate(WebPageProxy& page, WebFrameProxy& frame, API::SecurityOrigin& userMediaOrigin, API::SecurityOrigin& topLevelOrigin, UserMediaPermissionRequestProxy& request)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    auto delegate = (id<WKUIDelegatePrivate>)uiDelegate->m_delegate.get();
    ASSERT([delegate respondsToSelector:@selector(_webView:requestDisplayCapturePermissionForOrigin:initiatedByFrame:withSystemAudio:decisionHandler:)]);

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
    FrameInfoData frameInfo { frame.isMainFrame(), FrameType::Local, { }, userMediaOrigin.securityOrigin(), { }, frame.frameID(), mainFrameID, frame.process().processID(), frame.isFocused() };
    RetainPtr<WKFrameInfo> frameInfoWrapper = wrapper(API::FrameInfo::create(WTFMove(frameInfo), frame.page()));

    BOOL requestSystemAudio = !!request.requiresDisplayCaptureWithAudio();
    [delegate _webView:uiDelegate->m_webView.get().get() requestDisplayCapturePermissionForOrigin:wrapper(topLevelOrigin) initiatedByFrame:frameInfoWrapper.get() withSystemAudio:requestSystemAudio decisionHandler:decisionHandler.get()];

}
void UIDelegate::UIClient::decidePolicyForUserMediaPermissionRequest(WebPageProxy& page, WebFrameProxy& frame, API::SecurityOrigin& userMediaOrigin, API::SecurityOrigin& topLevelOrigin, UserMediaPermissionRequestProxy& request)
{
#if ENABLE(MEDIA_STREAM)
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return;

    auto delegate = (id <WKUIDelegatePrivate>)uiDelegate->m_delegate.get();
    if (!delegate) {
        ensureOnMainRunLoop([protectedRequest = Ref { request }]() {
            protectedRequest->doDefaultAction();
        });
        return;
    }

    if (request.requiresDisplayCapture()) {
        bool respondsToRequestDisplayCapturePermission = [delegate respondsToSelector:@selector(_webView:requestDisplayCapturePermissionForOrigin:initiatedByFrame:withSystemAudio:decisionHandler:)];
        if (!respondsToRequestDisplayCapturePermission || request.canRequestDisplayCapturePermission()) {
            request.promptForGetDisplayMedia(UserMediaPermissionRequestProxy::UserMediaDisplayCapturePromptType::UserChoose);
            return;
        }

        callDisplayCapturePermissionDelegate(page, frame, userMediaOrigin, topLevelOrigin, request);
        return;
    }

    bool respondsToRequestMediaCapturePermission = [delegate respondsToSelector:@selector(webView:requestMediaCapturePermissionForOrigin:initiatedByFrame:type:decisionHandler:)];
    bool respondsToRequestUserMediaAuthorizationForDevices = [delegate respondsToSelector:@selector(_webView:requestUserMediaAuthorizationForDevices:url:mainFrameURL:decisionHandler:)];

    if (!respondsToRequestMediaCapturePermission && !respondsToRequestUserMediaAuthorizationForDevices) {
        ensureOnMainRunLoop([protectedRequest = Ref { request }]() {
            protectedRequest->doDefaultAction();
        });
        return;
    }

    ASSERT(!request.requiresDisplayCapture());
    if (respondsToRequestMediaCapturePermission) {
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
        FrameInfoData frameInfo { frame.isMainFrame(), FrameType::Local, { }, userMediaOrigin.securityOrigin(), { }, frame.frameID(), mainFrameID, frame.process().processID(), frame.isFocused() };
        RetainPtr<WKFrameInfo> frameInfoWrapper = wrapper(API::FrameInfo::create(WTFMove(frameInfo), frame.page()));

        WKMediaCaptureType type = WKMediaCaptureTypeCamera;
        if (request.requiresAudioCapture())
            type = request.requiresVideoCapture() ? WKMediaCaptureTypeCameraAndMicrophone : WKMediaCaptureTypeMicrophone;
        [delegate webView:uiDelegate->m_webView.get().get() requestMediaCapturePermissionForOrigin:wrapper(topLevelOrigin) initiatedByFrame:frameInfoWrapper.get() type:type decisionHandler:decisionHandler.get()];
        return;
    }

    if (!respondsToRequestUserMediaAuthorizationForDevices) {
        ensureOnMainRunLoop([protectedRequest = Ref { request }]() {
            protectedRequest->doDefaultAction();
        });
        return;
    }

    URL requestFrameURL { frame.url() };
    URL mainFrameURL { frame.page()->mainFrame()->url() };

    _WKCaptureDevices devices = 0;
    if (request.requiresAudioCapture())
        devices |= _WKCaptureDeviceMicrophone;
    if (request.requiresVideoCapture())
        devices |= _WKCaptureDeviceCamera;

    auto checker = CompletionHandlerCallChecker::create(delegate, @selector(_webView:requestUserMediaAuthorizationForDevices:url:mainFrameURL:decisionHandler:));
    auto decisionHandler = makeBlockPtr([protectedRequest = Ref { request }, checker = WTFMove(checker)](BOOL authorized) {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();

        if (!authorized) {
            protectedRequest->deny(UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::PermissionDenied);
            return;
        }
        const String& videoDeviceUID = protectedRequest->requiresVideoCapture() ? protectedRequest->videoDeviceUIDs().first() : String();
        const String& audioDeviceUID = protectedRequest->requiresAudioCapture() ? protectedRequest->audioDeviceUIDs().first() : String();
        protectedRequest->allow(audioDeviceUID, videoDeviceUID);
    });
    [delegate _webView:uiDelegate->m_webView.get().get() requestUserMediaAuthorizationForDevices:devices url:requestFrameURL mainFrameURL:mainFrameURL decisionHandler:decisionHandler.get()];
#endif
}

void UIDelegate::UIClient::decidePolicyForScreenCaptureUnmuting(WebPageProxy& page, WebFrameProxy& frame, API::SecurityOrigin& userMediaOrigin, API::SecurityOrigin& topLevelOrigin, CompletionHandler<void(bool isAllowed)>&& completionHandler)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate) {
        completionHandler(false);
        return;
    }

    auto delegate = (id<WKUIDelegatePrivate>)uiDelegate->m_delegate.get();
    if (!delegate) {
        completionHandler(false);
        return;
    }

    bool respondsToRequestScreenCaptureUnmute = [delegate respondsToSelector:@selector(_webView:decidePolicyForScreenCaptureUnmutingForOrigin:initiatedByFrame:decisionHandler:)];

    if (!respondsToRequestScreenCaptureUnmute) {
        alertForPermission(page, MediaPermissionReason::ScreenCapture, topLevelOrigin.securityOrigin(), WTFMove(completionHandler));
        return;
    }

    auto checker = CompletionHandlerCallChecker::create(delegate, @selector(_webView:decidePolicyForScreenCaptureUnmutingForOrigin:initiatedByFrame:decisionHandler:));
    auto decisionHandler = makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] (BOOL isAllowed) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();

        completionHandler(isAllowed);
    });

    std::optional<WebCore::FrameIdentifier> mainFrameID;
    if (auto* mainFrame = frame.page() ? frame.page()->mainFrame() : nullptr)
        mainFrameID = mainFrame->frameID();
    FrameInfoData frameInfo { frame.isMainFrame(), FrameType::Local, { }, userMediaOrigin.securityOrigin(), { }, frame.frameID(), mainFrameID, frame.process().processID(), frame.isFocused() };
    RetainPtr<WKFrameInfo> frameInfoWrapper = wrapper(API::FrameInfo::create(WTFMove(frameInfo), frame.page()));

    [delegate _webView:uiDelegate->m_webView.get().get() decidePolicyForScreenCaptureUnmutingForOrigin:wrapper(topLevelOrigin) initiatedByFrame:frameInfoWrapper.get() decisionHandler:decisionHandler.get()];
}

void UIDelegate::UIClient::checkUserMediaPermissionForOrigin(WebPageProxy& page, WebFrameProxy& frame, API::SecurityOrigin& userMediaOrigin, API::SecurityOrigin& topLevelOrigin, UserMediaPermissionCheckProxy& request)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return;

    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate || !uiDelegate->m_delegateMethods.webViewIsMediaCaptureAuthorizedForFrameDecisionHandler) {
        request.setUserMediaAccessInfo(false);
        return;
    }

    RefPtr mainFrame = frame.page()->mainFrame();

    if ([delegate respondsToSelector:@selector(_webView:includeSensitiveMediaDeviceDetails:)]) {
        auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:includeSensitiveMediaDeviceDetails:));
        auto decisionHandler = makeBlockPtr([protectedRequest = Ref { request }, checker = WTFMove(checker)](BOOL includeSensitiveDetails) {
            if (checker->completionHandlerHasBeenCalled())
                return;
            checker->didCallCompletionHandler();

            protectedRequest->setUserMediaAccessInfo(includeSensitiveDetails);
        });

        [(id <WKUIDelegatePrivate>)delegate _webView:uiDelegate->m_webView.get().get() includeSensitiveMediaDeviceDetails:decisionHandler.get()];
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

    [(id<WKUIDelegatePrivate>)delegate _webView:uiDelegate->m_webView.get().get() checkUserMediaPermissionForURL:requestFrameURL mainFrameURL:mainFrameURL frameIdentifier:frame.frameID().object().toUInt64() decisionHandler:decisionHandler.get()];
}

void UIDelegate::UIClient::mediaCaptureStateDidChange(WebCore::MediaProducerMediaStateFlags state)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return;

    auto webView = uiDelegate->m_webView;

    [webView didChangeValueForKey:@"mediaCaptureState"];

    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate || !uiDelegate->m_delegateMethods.webViewMediaCaptureStateDidChange)
        return;

    [(id <WKUIDelegatePrivate>)delegate _webView:webView.get().get() mediaCaptureStateDidChange:toWKMediaCaptureStateDeprecated(state)];
}

void UIDelegate::UIClient::printFrame(WebPageProxy&, WebFrameProxy& webFrameProxy, const WebCore::FloatSize& pdfFirstPageSize, CompletionHandler<void()>&& completionHandler)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return completionHandler();

    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return completionHandler();

    auto handle = API::FrameHandle::create(webFrameProxy.frameID());
    if (uiDelegate->m_delegateMethods.webViewPrintFramePDFFirstPageSizeCompletionHandler) {
        auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:printFrame:pdfFirstPageSize:completionHandler:));
        [(id <WKUIDelegatePrivate>)delegate _webView:uiDelegate->m_webView.get().get() printFrame:wrapper(handle) pdfFirstPageSize:static_cast<CGSize>(pdfFirstPageSize) completionHandler:makeBlockPtr([checker = WTFMove(checker), completionHandler = WTFMove(completionHandler)] () mutable {
            if (checker->completionHandlerHasBeenCalled())
                return;
            checker->didCallCompletionHandler();
            completionHandler();
        }).get()];
        return;
    }

    if (uiDelegate->m_delegateMethods.webViewPrintFrame)
        [(id <WKUIDelegatePrivate>)delegate _webView:uiDelegate->m_webView.get().get() printFrame:wrapper(handle)];
    completionHandler();
}

void UIDelegate::UIClient::close(WebPageProxy*)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return;

    if (uiDelegate->m_delegateMethods.webViewClose) {
        auto delegate = uiDelegate->m_delegate.get();
        if (!delegate)
            return;

        [(id <WKUIDelegatePrivate>)delegate _webViewClose:uiDelegate->m_webView.get().get()];
        return;
    }

    if (!uiDelegate->m_delegateMethods.webViewDidClose)
        return;

    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    [delegate webViewDidClose:uiDelegate->m_webView.get().get()];
}

void UIDelegate::UIClient::fullscreenMayReturnToInline(WebPageProxy*)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return;

    if (!uiDelegate->m_delegateMethods.webViewFullscreenMayReturnToInline)
        return;
    
    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return;
    
    [(id <WKUIDelegatePrivate>)delegate _webViewFullscreenMayReturnToInline:uiDelegate->m_webView.get().get()];
}

void UIDelegate::UIClient::didEnterFullscreen(WebPageProxy*)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return;

    if (!uiDelegate->m_delegateMethods.webViewDidEnterFullscreen)
        return;

    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    [(id <WKUIDelegatePrivate>)delegate _webViewDidEnterFullscreen:uiDelegate->m_webView.get().get()];
}

void UIDelegate::UIClient::didExitFullscreen(WebPageProxy*)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return;

    if (!uiDelegate->m_delegateMethods.webViewDidExitFullscreen)
        return;

    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    [(id <WKUIDelegatePrivate>)delegate _webViewDidExitFullscreen:uiDelegate->m_webView.get().get()];
}
    
#if PLATFORM(IOS_FAMILY)
#if HAVE(APP_LINKS)
bool UIDelegate::UIClient::shouldIncludeAppLinkActionsForElement(_WKActivatedElementInfo *elementInfo)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return true;

    if (!uiDelegate->m_delegateMethods.webViewShouldIncludeAppLinkActionsForElement)
        return true;

    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return true;

    return [(id <WKUIDelegatePrivate>)delegate _webView:uiDelegate->m_webView.get().get() shouldIncludeAppLinkActionsForElement:elementInfo];
}
#endif

RetainPtr<NSArray> UIDelegate::UIClient::actionsForElement(_WKActivatedElementInfo *elementInfo, RetainPtr<NSArray> defaultActions)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return defaultActions;

    if (!uiDelegate->m_delegateMethods.webViewActionsForElementDefaultActions)
        return defaultActions;

    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return defaultActions;

    return [(id <WKUIDelegatePrivate>)delegate _webView:uiDelegate->m_webView.get().get() actionsForElement:elementInfo defaultActions:defaultActions.get()];
}

void UIDelegate::UIClient::didNotHandleTapAsClick(const WebCore::IntPoint& point)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return;

    if (!uiDelegate->m_delegateMethods.webViewDidNotHandleTapAsClickAtPoint)
        return;

    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    [static_cast<id <WKUIDelegatePrivate>>(delegate) _webView:uiDelegate->m_webView.get().get() didNotHandleTapAsClickAtPoint:point];
}

void UIDelegate::UIClient::statusBarWasTapped()
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return;

    if (!uiDelegate->m_delegateMethods.webViewStatusBarWasTapped)
        return;

    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    [static_cast<id <WKUIDelegatePrivate>>(delegate) _webViewStatusBarWasTapped:uiDelegate->m_webView.get().get()];
}

bool UIDelegate::UIClient::setShouldKeepScreenAwake(bool shouldKeepScreenAwake)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return false;

    if (!uiDelegate->m_delegateMethods.webViewSetShouldKeepScreenAwake)
        return false;

    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return false;

    [static_cast<id <WKUIDelegatePrivate>>(delegate) _webView:uiDelegate->m_webView.get().get() setShouldKeepScreenAwake:shouldKeepScreenAwake];
    return true;
}
#endif // PLATFORM(IOS_FAMILY)

PlatformViewController *UIDelegate::UIClient::presentingViewController()
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return nullptr;

    if (!uiDelegate->m_delegateMethods.presentingViewControllerForWebView)
        return nullptr;

    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return nullptr;

    return [static_cast<id <WKUIDelegatePrivate>>(delegate) _presentingViewControllerForWebView:uiDelegate->m_webView.get().get()];
}

std::optional<double> UIDelegate::UIClient::dataDetectionReferenceDate()
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return std::nullopt;

    if (!uiDelegate->m_delegateMethods.dataDetectionContextForWebView)
        return std::nullopt;

    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return std::nullopt;

#if ENABLE(DATA_DETECTION)
    return WebCore::DataDetection::extractReferenceDate([static_cast<id<WKUIDelegatePrivate>>(delegate) _dataDetectionContextForWebView:uiDelegate->m_webView.get().get()]);
#else
    return std::nullopt;
#endif
}

#if ENABLE(POINTER_LOCK)

void UIDelegate::UIClient::requestPointerLock(WebPageProxy* page)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return;

    if (!uiDelegate->m_delegateMethods.webViewRequestPointerLock && !uiDelegate->m_delegateMethods.webViewDidRequestPointerLockCompletionHandler)
        return;

    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    if (uiDelegate->m_delegateMethods.webViewRequestPointerLock) {
        [static_cast<id <WKUIDelegatePrivate>>(delegate) _webViewRequestPointerLock:uiDelegate->m_webView.get().get()];
        return;
    }

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webViewDidRequestPointerLock:completionHandler:));
    [static_cast<id <WKUIDelegatePrivate>>(delegate) _webViewDidRequestPointerLock:uiDelegate->m_webView.get().get() completionHandler:makeBlockPtr([checker = WTFMove(checker), page = RefPtr { page }] (BOOL allow) {
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
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return;

    if (!uiDelegate->m_delegateMethods.webViewDidLosePointerLock)
        return;

    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    [static_cast<id <WKUIDelegatePrivate>>(delegate) _webViewDidLosePointerLock:uiDelegate->m_webView.get().get()];
}

#endif
    
void UIDelegate::UIClient::didShowSafeBrowsingWarning()
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return;

    if (!uiDelegate->m_delegateMethods.webViewDidShowSafeBrowsingWarning)
        return;

    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    [static_cast<id <WKUIDelegatePrivate>>(delegate) _webViewDidShowSafeBrowsingWarning:uiDelegate->m_webView.get().get()];
}

void UIDelegate::UIClient::confirmPDFOpening(WebPageProxy& page, const WTF::URL& fileURL, FrameInfoData&& frameInfo, CompletionHandler<void(bool)>&& completionHandler)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return completionHandler(true);

    if (!uiDelegate->m_delegateMethods.webViewShouldAllowPDFAtURLToOpenFromFrameCompletionHandler)
        return completionHandler(true);

    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return completionHandler(true);

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:shouldAllowPDFAtURL:toOpenFromFrame:completionHandler:));
    [static_cast<id<WKUIDelegatePrivate>>(delegate) _webView:uiDelegate->m_webView.get().get() shouldAllowPDFAtURL:fileURL toOpenFromFrame:wrapper(API::FrameInfo::create(WTFMove(frameInfo), &page)).get() completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] (BOOL result) mutable {
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
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return completionHandler(WebAuthenticationPanelResult::Unavailable);

    if (!uiDelegate->m_delegateMethods.webViewRunWebAuthenticationPanelInitiatedByFrameCompletionHandler) {
        completionHandler(WebAuthenticationPanelResult::Unavailable);
        return;
    }

    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate) {
        completionHandler(WebAuthenticationPanelResult::Unavailable);
        return;
    }

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:runWebAuthenticationPanel:initiatedByFrame:completionHandler:));
    [(id<WKUIDelegatePrivate>)delegate _webView:uiDelegate->m_webView.get().get() runWebAuthenticationPanel:wrapper(panel) initiatedByFrame:wrapper(API::FrameInfo::create(WTFMove(frameInfo), &page)).get() completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] (_WKWebAuthenticationPanelResult result) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();
        completionHandler(webAuthenticationPanelResult(result));
    }).get()];
}

void UIDelegate::UIClient::requestWebAuthenticationConditonalMediationRegistration(WTF::String&& username, CompletionHandler<void(bool)>&& completionHandler)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return completionHandler(false);

    if (!uiDelegate->m_delegateMethods.webViewRequestWebAuthenticationConditionalMediationRegistrationForUserCompletionHandler)
        return completionHandler(false);

    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return completionHandler(false);

    auto checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:requestWebAuthenticationConditionalMediationRegistrationForUser:completionHandler:));
    [(id<WKUIDelegatePrivate>)delegate _webView:uiDelegate->m_webView.get().get() requestWebAuthenticationConditionalMediationRegistrationForUser:username completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] (BOOL result) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();
        completionHandler(result);
    }).get()];
}
#endif // ENABLE(WEB_AUTHN)

void UIDelegate::UIClient::hasVideoInPictureInPictureDidChange(WebPageProxy*, bool hasVideoInPictureInPicture)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return;

    if (!uiDelegate->m_delegateMethods.webViewHasVideoInPictureInPictureDidChange)
        return;
    
    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return;
    
    [static_cast<id <WKUIDelegatePrivate>>(delegate) _webView:uiDelegate->m_webView.get().get() hasVideoInPictureInPictureDidChange:hasVideoInPictureInPicture];
}

void UIDelegate::UIClient::imageOrMediaDocumentSizeChanged(const WebCore::IntSize& newSize)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return;

    if (!uiDelegate->m_delegateMethods.webViewImageOrMediaDocumentSizeChanged)
        return;

    auto delegate = uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    [static_cast<id <WKUIDelegatePrivate>>(delegate) _webView:uiDelegate->m_webView.get().get() imageOrMediaDocumentSizeChanged:newSize];
}

void UIDelegate::UIClient::queryPermission(const String& permissionName, API::SecurityOrigin& origin, CompletionHandler<void(std::optional<WebCore::PermissionState>)>&& callback)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate) {
        callback(WebCore::PermissionState::Prompt);
        return;
    }

    auto delegate = (id <WKUIDelegatePrivate>)uiDelegate->m_delegate.get();
    if (!delegate) {
        callback(WebCore::PermissionState::Prompt);
        return;
    }

    if (![delegate respondsToSelector:@selector(_webView:queryPermission:forOrigin:completionHandler:)]) {
        callback(WebCore::PermissionState::Prompt);
        return;
    }

    auto checker = CompletionHandlerCallChecker::create(delegate, @selector(_webView:queryPermission:forOrigin:completionHandler:));
    [delegate _webView:uiDelegate->m_webView.get().get() queryPermission:permissionName forOrigin:wrapper(origin) completionHandler:makeBlockPtr([callback = WTFMove(callback), checker = WTFMove(checker)](WKPermissionDecision permissionState) mutable {
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
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return;

    if (!uiDelegate->m_delegateMethods.webViewDidEnableInspectorBrowserDomain)
        return;

    auto delegate = (id <WKUIDelegatePrivate>)uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    [delegate _webViewDidEnableInspectorBrowserDomain:uiDelegate->m_webView.get().get()];
}

void UIDelegate::UIClient::didDisableInspectorBrowserDomain(WebPageProxy&)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return;

    if (!uiDelegate->m_delegateMethods.webViewDidDisableInspectorBrowserDomain)
        return;

    auto delegate = (id <WKUIDelegatePrivate>)uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    [delegate _webViewDidDisableInspectorBrowserDomain:uiDelegate->m_webView.get().get()];
}

void UIDelegate::UIClient::updateAppBadge(WebPageProxy&, const WebCore::SecurityOriginData& origin, std::optional<uint64_t> badge)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return;

    if (!uiDelegate->m_delegateMethods.webViewUpdatedAppBadge)
        return;

    auto delegate = (id <WKUIDelegatePrivate>)uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    NSNumber *nsBadge = nil;
    if (badge)
        nsBadge = @(*badge);


    auto apiOrigin = API::SecurityOrigin::create(origin);
    [delegate _webView:uiDelegate->m_webView.get().get() updatedAppBadge:nsBadge fromSecurityOrigin:wrapper(apiOrigin.get())];
}

void UIDelegate::UIClient::updateClientBadge(WebPageProxy&, const WebCore::SecurityOriginData& origin, std::optional<uint64_t> badge)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return;

    if (!uiDelegate->m_delegateMethods.webViewUpdatedClientBadge)
        return;

    auto delegate = (id <WKUIDelegatePrivate>)uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    NSNumber *nsBadge = nil;
    if (badge)
        nsBadge = @(*badge);

    auto apiOrigin = API::SecurityOrigin::create(origin);
    [delegate _webView:uiDelegate->m_webView.get().get() updatedClientBadge:nsBadge fromSecurityOrigin:wrapper(apiOrigin.get())];
}

void UIDelegate::UIClient::didAdjustVisibilityWithSelectors(WebPageProxy&, Vector<String>&& selectors)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return;

    if (!uiDelegate->m_delegateMethods.webViewDidAdjustVisibilityWithSelectors)
        return;

    auto delegate = (id<WKUIDelegatePrivate>)uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    RetainPtr nsSelectors = createNSArray(WTFMove(selectors));
    [delegate _webView:uiDelegate->m_webView.get().get() didAdjustVisibilityWithSelectors:nsSelectors.get()];
}

#if ENABLE(GAMEPAD)
void UIDelegate::UIClient::recentlyAccessedGamepadsForTesting(WebPageProxy&)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return;

    if (!uiDelegate->m_delegateMethods.webViewRecentlyAccessedGamepadsForTesting)
        return;

    auto delegate = (id<WKUIDelegatePrivate>)uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    [delegate _webViewRecentlyAccessedGamepadsForTesting:uiDelegate->m_webView.get().get()];
}

void UIDelegate::UIClient::stoppedAccessingGamepadsForTesting(WebPageProxy&)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate)
        return;

    if (!uiDelegate->m_delegateMethods.webViewStoppedAccessingGamepadsForTesting)
        return;

    auto delegate = (id<WKUIDelegatePrivate>)uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    [delegate _webViewStoppedAccessingGamepadsForTesting:uiDelegate->m_webView.get().get()];
}
#endif

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

void UIDelegate::UIClient::requestPermissionOnXRSessionFeatures(WebPageProxy&, const WebCore::SecurityOriginData& securityOriginData, PlatformXR::SessionMode mode, const PlatformXR::Device::FeatureList& granted, const PlatformXR::Device::FeatureList& consentRequired, const PlatformXR::Device::FeatureList& consentOptional, const PlatformXR::Device::FeatureList& requiredFeaturesRequested, const PlatformXR::Device::FeatureList& optionalFeaturesRequested, CompletionHandler<void(std::optional<PlatformXR::Device::FeatureList>&&)>&& completionHandler)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate || !uiDelegate->m_delegateMethods.webViewRequestPermissionForXRSessionOriginModeAndFeaturesWithCompletionHandler) {
        completionHandler(granted);
        return;
    }

    auto delegate = (id <WKUIDelegatePrivate>)uiDelegate->m_delegate.get();
    if (!delegate) {
        completionHandler(granted);
        return;
    }

    SEL requestPermissionSelector = @selector(_webView:requestPermissionForXRSessionOrigin:mode:grantedFeatures:consentRequiredFeatures:consentOptionalFeatures:requiredFeaturesRequested:optionalFeaturesRequested:completionHandler:);
    bool usingOldDelegateMethod = false;
    if (![delegate respondsToSelector:requestPermissionSelector]) {
        requestPermissionSelector = @selector(_webView:requestPermissionForXRSessionOrigin:mode:grantedFeatures:consentRequiredFeatures:consentOptionalFeatures:completionHandler:);
        usingOldDelegateMethod = true;
    }

    auto checker = CompletionHandlerCallChecker::create(delegate, requestPermissionSelector);
    if (!usingOldDelegateMethod) {
        [delegate _webView:uiDelegate->m_webView.get().get() requestPermissionForXRSessionOrigin:securityOriginData.toString() mode:toWKXRSessionMode(mode) grantedFeatures:toWKXRSessionFeatureFlags(granted) consentRequiredFeatures:toWKXRSessionFeatureFlags(consentRequired) consentOptionalFeatures:toWKXRSessionFeatureFlags(consentOptional) requiredFeaturesRequested:toWKXRSessionFeatureFlags(requiredFeaturesRequested) optionalFeaturesRequested:toWKXRSessionFeatureFlags(optionalFeaturesRequested) completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] (_WKXRSessionFeatureFlags userGrantedFeatures) mutable {
            if (checker->completionHandlerHasBeenCalled())
                return;
            checker->didCallCompletionHandler();
            completionHandler(toPlatformXRFeatures(userGrantedFeatures));
        }).get()];
    } else {
        [delegate _webView:uiDelegate->m_webView.get().get() requestPermissionForXRSessionOrigin:securityOriginData.toString() mode:toWKXRSessionMode(mode) grantedFeatures:toWKXRSessionFeatureFlags(granted) consentRequiredFeatures:toWKXRSessionFeatureFlags(consentRequired) consentOptionalFeatures:toWKXRSessionFeatureFlags(consentOptional) completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] (_WKXRSessionFeatureFlags userGrantedFeatures) mutable {
            if (checker->completionHandlerHasBeenCalled())
                return;
            checker->didCallCompletionHandler();
            completionHandler(toPlatformXRFeatures(userGrantedFeatures));
        }).get()];
    }
}

void UIDelegate::UIClient::supportedXRSessionFeatures(PlatformXR::Device::FeatureList& vrFeatures, PlatformXR::Device::FeatureList& arFeatures)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (uiDelegate && uiDelegate->m_delegateMethods.webViewSupportedXRSessionFeatures) {
        auto delegate = (id<WKUIDelegatePrivate>)uiDelegate->m_delegate.get();
        if (delegate) {
            _WKXRSessionFeatureFlags wkVRfeatures = _WKXRSessionFeatureFlagsNone;
            _WKXRSessionFeatureFlags wkARfeatures = _WKXRSessionFeatureFlagsNone;

            [delegate _webView:uiDelegate->m_webView.get().get() supportedXRSessionFeatures:&wkVRfeatures arFeatures:&wkARfeatures];

            vrFeatures = toPlatformXRFeatures(wkVRfeatures).value_or(PlatformXR::Device::FeatureList());
            arFeatures = toPlatformXRFeatures(wkARfeatures).value_or(PlatformXR::Device::FeatureList());
        }
    }
}

#if PLATFORM(IOS_FAMILY)
void UIDelegate::UIClient::startXRSession(WebPageProxy&, const PlatformXR::Device::FeatureList& sessionFeatures, CompletionHandler<void(RetainPtr<id>, PlatformViewController *)>&& completionHandler)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate || !uiDelegate->m_delegateMethods.webViewStartXRSessionWithCompletionHandler) {
        completionHandler(nil, nil);
        return;
    }

    auto delegate = (id <WKUIDelegatePrivate>)uiDelegate->m_delegate.get();
    if (!delegate) {
        completionHandler(nil, nil);
        return;
    }

    if ([delegate respondsToSelector:@selector(_webView:startXRSessionWithFeatures:completionHandler:)]) {
        auto checker = CompletionHandlerCallChecker::create(delegate, @selector(_webView:startXRSessionWithFeatures:completionHandler:));
        [delegate _webView:uiDelegate->m_webView.get().get() startXRSessionWithFeatures:toWKXRSessionFeatureFlags(sessionFeatures) completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] (id result, UIViewController *viewController) mutable {
            if (checker->completionHandlerHasBeenCalled())
                return;
            checker->didCallCompletionHandler();
            completionHandler(result, viewController);
        }).get()];
        return;
    }

    auto checker = CompletionHandlerCallChecker::create(delegate, @selector(_webView:startXRSessionWithCompletionHandler:));
    [delegate _webView:uiDelegate->m_webView.get().get() startXRSessionWithCompletionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] (id result) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();
        completionHandler(result, nil);
    }).get()];
}

void UIDelegate::UIClient::endXRSession(WebPageProxy&, PlatformXRSessionEndReason reason)
{
    RefPtr uiDelegate = m_uiDelegate.get();
    if (!uiDelegate || !uiDelegate->m_delegateMethods.webViewEndXRSession)
        return;

    auto delegate = (id<WKUIDelegatePrivate>)uiDelegate->m_delegate.get();
    if (!delegate)
        return;

    if ([delegate respondsToSelector:@selector(_webViewEndXRSession:withReason:)]) {
        [delegate _webViewEndXRSession:uiDelegate->m_webView.get().get() withReason:static_cast<_WKXRSessionEndReason>(reason)];
        return;
    }

    [delegate _webViewEndXRSession:uiDelegate->m_webView.get().get()];
}
#endif // PLATFORM(IOS_FAMILY)

#endif // ENABLE(WEBXR)

} // namespace WebKit
