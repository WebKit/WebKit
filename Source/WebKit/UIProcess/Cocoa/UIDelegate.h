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

#pragma once

#import "WKFoundation.h"

#import "APIContextMenuClient.h"
#import "APIUIClient.h"
#import <WebCore/PlatformViewController.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/WeakPtr.h>

@class _WKActivatedElementInfo;
@class WKWebView;
@protocol WKUIDelegate;

namespace API {
class FrameInfo;
class SecurityOrigin;
}

namespace WebCore {
class RegistrableDomain;
}

namespace WebKit {

enum class TapHandlingResult : uint8_t;

class UIDelegate : public CanMakeWeakPtr<UIDelegate> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit UIDelegate(WKWebView *);
    ~UIDelegate();

#if ENABLE(CONTEXT_MENUS)
    std::unique_ptr<API::ContextMenuClient> createContextMenuClient();
#endif
    std::unique_ptr<API::UIClient> createUIClient();

    RetainPtr<id <WKUIDelegate> > delegate();
    void setDelegate(id <WKUIDelegate>);

private:
#if ENABLE(CONTEXT_MENUS)
    class ContextMenuClient : public API::ContextMenuClient {
    public:
        explicit ContextMenuClient(UIDelegate&);
        ~ContextMenuClient();

    private:
        // API::ContextMenuClient
        void menuFromProposedMenu(WebPageProxy&, NSMenu *, const WebHitTestResultData&, API::Object*, CompletionHandler<void(RetainPtr<NSMenu>&&)>&&) override;

        WeakPtr<UIDelegate> m_uiDelegate;
    };
#endif

    class UIClient : public API::UIClient, public CanMakeWeakPtr<UIClient> {
    public:
        explicit UIClient(UIDelegate&);
        ~UIClient();

    private:
        // API::UIClient
        void createNewPage(WebKit::WebPageProxy&, WebCore::WindowFeatures&&, Ref<API::NavigationAction>&&, CompletionHandler<void(RefPtr<WebPageProxy>&&)>&&) final;
        void close(WebPageProxy*) final;
        void fullscreenMayReturnToInline(WebPageProxy*) final;
        void didEnterFullscreen(WebPageProxy*) final;
        void didExitFullscreen(WebPageProxy*) final;
        void runJavaScriptAlert(WebPageProxy&, const WTF::String&, WebFrameProxy*, FrameInfoData&&, Function<void()>&& completionHandler) final;
        void runJavaScriptConfirm(WebPageProxy&, const WTF::String&, WebFrameProxy*, FrameInfoData&&, Function<void(bool)>&& completionHandler) final;
        void runJavaScriptPrompt(WebPageProxy&, const WTF::String&, const WTF::String&, WebFrameProxy*, FrameInfoData&&, Function<void(const WTF::String&)>&&) final;
        void presentStorageAccessConfirmDialog(const WTF::String& requestingDomain, const WTF::String& currentDomain, CompletionHandler<void(bool)>&&);
        void requestStorageAccessConfirm(WebPageProxy&, WebFrameProxy*, const WebCore::RegistrableDomain& requestingDomain, const WebCore::RegistrableDomain& currentDomain, CompletionHandler<void(bool)>&&) final;
        void decidePolicyForGeolocationPermissionRequest(WebPageProxy&, WebFrameProxy&, const FrameInfoData&, Function<void(bool)>&) final;
        bool canRunBeforeUnloadConfirmPanel() const final;
        void runBeforeUnloadConfirmPanel(WebPageProxy&, const WTF::String&, WebFrameProxy*, FrameInfoData&&, Function<void(bool)>&& completionHandler) final;
        void exceededDatabaseQuota(WebPageProxy*, WebFrameProxy*, API::SecurityOrigin*, const WTF::String& databaseName, const WTF::String& displayName, unsigned long long currentQuota, unsigned long long currentOriginUsage, unsigned long long currentUsage, unsigned long long expectedUsage, Function<void(unsigned long long)>&& completionHandler) final;
        void reachedApplicationCacheOriginQuota(WebPageProxy*, const WebCore::SecurityOrigin&, uint64_t currentQuota, uint64_t totalBytesNeeded, Function<void(unsigned long long)>&& completionHandler) final;
        bool lockScreenOrientation(WebPageProxy&, WebCore::ScreenOrientationType) final;
        void unlockScreenOrientation(WebPageProxy&) final;
        void didResignInputElementStrongPasswordAppearance(WebPageProxy&, API::Object*) final;
        bool takeFocus(WebPageProxy*, WKFocusDirection) final;
        void handleAutoplayEvent(WebPageProxy&, WebCore::AutoplayEvent, OptionSet<WebCore::AutoplayEventFlags>) final;
        void decidePolicyForNotificationPermissionRequest(WebPageProxy&, API::SecurityOrigin&, CompletionHandler<void(bool allowed)>&&) final;
        void requestCookieConsent(CompletionHandler<void(WebCore::CookieConsentDecisionResult)>&&) final;
        void decidePolicyForModalContainer(OptionSet<WebCore::ModalContainerControlType>, CompletionHandler<void(WebCore::ModalContainerDecision)>&&) final;
#if PLATFORM(MAC) || HAVE(UIKIT_WITH_MOUSE_SUPPORT)
        void mouseDidMoveOverElement(WebPageProxy&, const WebHitTestResultData&, OptionSet<WebEventModifier>, API::Object*);
#endif

#if PLATFORM(MAC)
        void showPage(WebPageProxy*) final;
        void focus(WebPageProxy*) final;
        void unfocus(WebPageProxy*) final;
        bool focusFromServiceWorker(WebKit::WebPageProxy&) final;

        bool canRunModal() const final;
        void runModal(WebPageProxy&) final;
        void pageDidScroll(WebPageProxy*) final;
        void setIsResizable(WebPageProxy&, bool) final;
        void setWindowFrame(WebPageProxy&, const WebCore::FloatRect&) final;
        void windowFrame(WebPageProxy&, Function<void(WebCore::FloatRect)>&&) final;
        void didNotHandleWheelEvent(WebPageProxy*, const NativeWebWheelEvent&) final;

        // Printing.
        float headerHeight(WebPageProxy&, WebFrameProxy&) final;
        float footerHeight(WebPageProxy&, WebFrameProxy&) final;
        void drawHeader(WebPageProxy&, WebFrameProxy&, WebCore::FloatRect&&) final;
        void drawFooter(WebPageProxy&, WebFrameProxy&, WebCore::FloatRect&&) final;

        void didClickAutoFillButton(WebPageProxy&, API::Object*) final;
        void toolbarsAreVisible(WebPageProxy&, Function<void(bool)>&&) final;
        bool runOpenPanel(WebPageProxy&, WebFrameProxy*, FrameInfoData&&, API::OpenPanelParameters*, WebOpenPanelResultListenerProxy*) final;
        void saveDataToFileInDownloadsFolder(WebPageProxy*, const WTF::String&, const WTF::String&, const URL&, API::Data&) final;
        Ref<API::InspectorConfiguration> configurationForLocalInspector(WebPageProxy&, WebInspectorUIProxy&) final;
        void didAttachLocalInspector(WebPageProxy&, WebInspectorUIProxy&) final;
        void willCloseLocalInspector(WebPageProxy&, WebInspectorUIProxy&) final;
#endif
#if ENABLE(DEVICE_ORIENTATION)
        void shouldAllowDeviceOrientationAndMotionAccess(WebKit::WebPageProxy&, WebFrameProxy&, FrameInfoData&&, CompletionHandler<void(bool)>&&) final;
#endif
        bool needsFontAttributes() const final { return m_uiDelegate ? m_uiDelegate->m_delegateMethods.webViewDidChangeFontAttributes : false; }
        void didChangeFontAttributes(const WebCore::FontAttributes&) final;
        void decidePolicyForUserMediaPermissionRequest(WebPageProxy&, WebFrameProxy&, API::SecurityOrigin&, API::SecurityOrigin&, UserMediaPermissionRequestProxy&) final;
        void checkUserMediaPermissionForOrigin(WebPageProxy&, WebFrameProxy&, API::SecurityOrigin&, API::SecurityOrigin&, UserMediaPermissionCheckProxy&) final;
        void mediaCaptureStateDidChange(WebCore::MediaProducerMediaStateFlags) final;
        void promptForDisplayCapturePermission(WebPageProxy&, WebFrameProxy&, API::SecurityOrigin&, API::SecurityOrigin&, UserMediaPermissionRequestProxy&);
        void printFrame(WebPageProxy&, WebFrameProxy&, const WebCore::FloatSize& pdfFirstPageSize, CompletionHandler<void()>&&) final;
#if PLATFORM(IOS_FAMILY)
#if HAVE(APP_LINKS)
        bool shouldIncludeAppLinkActionsForElement(_WKActivatedElementInfo *) final;
#endif
        RetainPtr<NSArray> actionsForElement(_WKActivatedElementInfo *, RetainPtr<NSArray> defaultActions) final;
        void didNotHandleTapAsClick(const WebCore::IntPoint&) final;
        void statusBarWasTapped() final;
#endif // PLATFORM(IOS_FAMILY)
        PlatformViewController *presentingViewController() final;

        NSDictionary *dataDetectionContext() final;

#if ENABLE(POINTER_LOCK)
        void requestPointerLock(WebPageProxy*) final;
        void didLosePointerLock(WebPageProxy*) final;
#endif
        
        void hasVideoInPictureInPictureDidChange(WebPageProxy*, bool) final;

        void imageOrMediaDocumentSizeChanged(const WebCore::IntSize&) final;
        void didShowSafeBrowsingWarning() final;
        void confirmPDFOpening(WebPageProxy&, const WTF::URL&, FrameInfoData&&, CompletionHandler<void(bool)>&&) final;
#if ENABLE(WEB_AUTHN)
        void runWebAuthenticationPanel(WebPageProxy&, API::WebAuthenticationPanel&, WebFrameProxy&, FrameInfoData&&, CompletionHandler<void(WebAuthenticationPanelResult)>&&) final;
        void requestWebAuthenticationNoGesture(API::SecurityOrigin&, CompletionHandler<void(bool)>&&) final;
#endif
        void decidePolicyForSpeechRecognitionPermissionRequest(WebPageProxy&, API::SecurityOrigin&, CompletionHandler<void(bool)>&&) final;
        void queryPermission(const String&, API::SecurityOrigin&, CompletionHandler<void(std::optional<WebCore::PermissionState>)>&&) final;
        void didEnableInspectorBrowserDomain(WebPageProxy&) final;
        void didDisableInspectorBrowserDomain(WebPageProxy&) final;

#if ENABLE(WEBXR)
        void requestPermissionOnXRSessionFeatures(WebPageProxy&, const WebCore::SecurityOriginData&, PlatformXR::SessionMode, const PlatformXR::Device::FeatureList& /* granted */, const PlatformXR::Device::FeatureList& /* consentRequired */, const PlatformXR::Device::FeatureList& /* consentOptional */, CompletionHandler<void(std::optional<PlatformXR::Device::FeatureList>&&)>&&) final;
        void startXRSession(WebPageProxy&, CompletionHandler<void(RetainPtr<id>)>&&) final;
        void endXRSession(WebPageProxy&) final;
#endif

        WeakPtr<UIDelegate> m_uiDelegate;
    };

    WeakObjCPtr<WKWebView> m_webView;
    WeakObjCPtr<id <WKUIDelegate> > m_delegate;

    struct {
        bool webViewCreateWebViewWithConfigurationForNavigationActionWindowFeatures : 1;
        bool webViewCreateWebViewWithConfigurationForNavigationActionWindowFeaturesAsync : 1;
        bool webViewRunJavaScriptAlertPanelWithMessageInitiatedByFrameCompletionHandler : 1;
        bool webViewRunJavaScriptConfirmPanelWithMessageInitiatedByFrameCompletionHandler : 1;
        bool webViewRunJavaScriptTextInputPanelWithPromptDefaultTextInitiatedByFrameCompletionHandler : 1;
        bool webViewRequestStorageAccessPanelUnderFirstPartyCompletionHandler : 1;
        bool webViewRunBeforeUnloadConfirmPanelWithMessageInitiatedByFrameCompletionHandler : 1;
        bool webViewRequestGeolocationPermissionForFrameDecisionHandler : 1;
        bool webViewRequestGeolocationPermissionForOriginDecisionHandler : 1;
        bool webViewDidResignInputElementStrongPasswordAppearanceWithUserInfo : 1;
        bool webViewTakeFocus : 1;
        bool webViewHandleAutoplayEventWithFlags : 1;
#if PLATFORM(MAC) || HAVE(UIKIT_WITH_MOUSE_SUPPORT)
        bool webViewMouseDidMoveOverElementWithFlagsUserInfo : 1;
#endif
#if PLATFORM(MAC)
        bool showWebView : 1;
        bool focusWebView : 1;
        bool focusWebViewFromServiceWorker : 1;
        bool unfocusWebView : 1;
        bool webViewRunModal : 1;
        bool webViewDidScroll : 1;
        bool webViewHeaderHeight : 1;
        bool webViewFooterHeight : 1;
        bool webViewSetResizable : 1;
        bool webViewSetWindowFrame : 1;
        bool webViewDidNotHandleWheelEvent : 1;
        bool webViewUnavailablePlugInButtonClicked : 1;
        bool webViewDidClickAutoFillButtonWithUserInfo : 1;
        bool webViewDrawHeaderInRectForPageWithTitleURL : 1;
        bool webViewDrawFooterInRectForPageWithTitleURL : 1;
        bool webViewGetWindowFrameWithCompletionHandler : 1;
        bool webViewGetToolbarsAreVisibleWithCompletionHandler : 1;
        bool webViewSaveDataToFileSuggestedFilenameMimeTypeOriginatingURL : 1;
        bool webViewRunOpenPanelWithParametersInitiatedByFrameCompletionHandler : 1;
        bool webViewConfigurationForLocalInspector : 1;
        bool webViewDidAttachLocalInspector : 1;
        bool webViewWillCloseLocalInspector : 1;
#endif
#if ENABLE(DEVICE_ORIENTATION)
        bool webViewRequestDeviceOrientationAndMotionPermissionForOriginDecisionHandler : 1;
#endif
        bool webViewDecideDatabaseQuotaForSecurityOriginCurrentQuotaCurrentOriginUsageCurrentDatabaseUsageExpectedUsageDecisionHandler : 1;
        bool webViewDecideDatabaseQuotaForSecurityOriginDatabaseNameDisplayNameCurrentQuotaCurrentOriginUsageCurrentDatabaseUsageExpectedUsageDecisionHandler : 1;
#if PLATFORM(IOS)
        bool webViewLockScreenOrientation : 1;
        bool webViewUnlockScreenOrientation : 1;
#endif
        bool webViewDecideWebApplicationCacheQuotaForSecurityOriginCurrentQuotaTotalBytesNeeded : 1;
        bool webViewPrintFrame : 1;
        bool webViewPrintFramePDFFirstPageSizeCompletionHandler : 1;
        bool webViewDidClose : 1;
        bool webViewClose : 1;
        bool webViewFullscreenMayReturnToInline : 1;
        bool webViewDidEnterFullscreen : 1;
        bool webViewDidExitFullscreen : 1;
        bool webViewIsMediaCaptureAuthorizedForFrameDecisionHandler : 1;
        bool webViewMediaCaptureStateDidChange : 1;
        bool webViewDidChangeFontAttributes : 1;
#if PLATFORM(IOS_FAMILY)
#if HAVE(APP_LINKS)
        bool webViewShouldIncludeAppLinkActionsForElement : 1;
#endif
        bool webViewActionsForElementDefaultActions : 1;
        bool webViewDidNotHandleTapAsClickAtPoint : 1;
        bool webViewStatusBarWasTapped : 1;
#endif
        bool presentingViewControllerForWebView : 1;
        bool dataDetectionContextForWebView : 1;
        bool webViewImageOrMediaDocumentSizeChanged : 1;
#if ENABLE(POINTER_LOCK)
        bool webViewRequestPointerLock : 1;
        bool webViewDidRequestPointerLockCompletionHandler : 1;
        bool webViewDidLosePointerLock : 1;
#endif
#if ENABLE(CONTEXT_MENUS)
        bool webViewContextMenuForElement : 1;
        bool webViewContextMenuForElementUserInfo : 1;
        bool webViewGetContextMenuFromProposedMenuForElementUserInfoCompletionHandler : 1;
#endif
        bool webViewHasVideoInPictureInPictureDidChange : 1;
        bool webViewDidShowSafeBrowsingWarning : 1;
        bool webViewShouldAllowPDFAtURLToOpenFromFrameCompletionHandler : 1;
#if ENABLE(WEB_AUTHN)
        bool webViewRunWebAuthenticationPanelInitiatedByFrameCompletionHandler : 1;
        bool webViewRequestWebAuthenticationNoGestureForOriginCompletionHandler : 1;
#endif
        bool webViewDidEnableInspectorBrowserDomain : 1;
        bool webViewDidDisableInspectorBrowserDomain : 1;
#if ENABLE(WEBXR)
        bool webViewRequestPermissionForXRSessionOriginModeAndFeaturesWithCompletionHandler: 1;
        bool webViewStartXRSessionWithCompletionHandler : 1;
        bool webViewEndXRSession : 1;
#endif
        bool webViewRequestNotificationPermissionForSecurityOriginDecisionHandler : 1;
        bool webViewRequestCookieConsentWithMoreInfoHandlerDecisionHandler : 1;
        bool webViewDecidePolicyForModalContainerDecisionHandler : 1;
    } m_delegateMethods;
};

} // namespace WebKit
