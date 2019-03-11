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
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>

@class _WKActivatedElementInfo;
@class WKWebView;
@protocol WKUIDelegate;

namespace API {
class FrameInfo;
class SecurityOrigin;
}

namespace WebKit {

class UIDelegate {
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

        UIDelegate& m_uiDelegate;
    };
#endif

    class UIClient : public API::UIClient {
    public:
        explicit UIClient(UIDelegate&);
        ~UIClient();

    private:
        // API::UIClient
        void createNewPage(WebPageProxy&, Ref<API::FrameInfo>&&, WebCore::ResourceRequest&&, WebCore::WindowFeatures&&, NavigationActionData&&, CompletionHandler<void(RefPtr<WebPageProxy>&&)>&&) final;
        void close(WebPageProxy*) final;
        void fullscreenMayReturnToInline(WebPageProxy*) final;
        void didEnterFullscreen(WebPageProxy*) final;
        void didExitFullscreen(WebPageProxy*) final;
        void runJavaScriptAlert(WebPageProxy*, const WTF::String&, WebFrameProxy*, const WebCore::SecurityOriginData&, Function<void()>&& completionHandler) final;
        void runJavaScriptConfirm(WebPageProxy*, const WTF::String&, WebFrameProxy*, const WebCore::SecurityOriginData&, Function<void(bool)>&& completionHandler) final;
        void runJavaScriptPrompt(WebPageProxy*, const WTF::String&, const WTF::String&, WebFrameProxy*, const WebCore::SecurityOriginData&, Function<void(const WTF::String&)>&&) final;
        void requestStorageAccessConfirm(WebPageProxy&, WebFrameProxy*, const WTF::String& requestingDomain, const WTF::String& currentDomain, CompletionHandler<void(bool)>&&) final;
        void decidePolicyForGeolocationPermissionRequest(WebPageProxy&, WebFrameProxy&, API::SecurityOrigin&, Function<void(bool)>&) final;
        bool canRunBeforeUnloadConfirmPanel() const final;
        void runBeforeUnloadConfirmPanel(WebPageProxy*, const WTF::String&, WebFrameProxy*, const WebCore::SecurityOriginData&, Function<void(bool)>&& completionHandler) final;
        void exceededDatabaseQuota(WebPageProxy*, WebFrameProxy*, API::SecurityOrigin*, const WTF::String& databaseName, const WTF::String& displayName, unsigned long long currentQuota, unsigned long long currentOriginUsage, unsigned long long currentUsage, unsigned long long expectedUsage, Function<void(unsigned long long)>&& completionHandler) final;
        void reachedApplicationCacheOriginQuota(WebPageProxy*, const WebCore::SecurityOrigin&, uint64_t currentQuota, uint64_t totalBytesNeeded, Function<void(unsigned long long)>&& completionHandler) final;
        void didResignInputElementStrongPasswordAppearance(WebPageProxy&, API::Object*) final;
        bool takeFocus(WebPageProxy*, WKFocusDirection) final;
#if PLATFORM(MAC)
        void showPage(WebPageProxy*) final;
        void focus(WebPageProxy*) final;
        void unfocus(WebPageProxy*) final;
        bool canRunModal() const final;
        void runModal(WebPageProxy&) final;
        void pageDidScroll(WebPageProxy*) final;
        void setIsResizable(WebPageProxy&, bool) final;
        void setWindowFrame(WebPageProxy&, const WebCore::FloatRect&) final;
        void windowFrame(WebPageProxy&, Function<void(WebCore::FloatRect)>&&) final;
        void didNotHandleWheelEvent(WebPageProxy*, const NativeWebWheelEvent&) final;
        float headerHeight(WebPageProxy&, WebFrameProxy&) final;
        float footerHeight(WebPageProxy&, WebFrameProxy&) final;
        void drawHeader(WebPageProxy&, WebFrameProxy&, WebCore::FloatRect&&) final;
        void drawFooter(WebPageProxy&, WebFrameProxy&, WebCore::FloatRect&&) final;
        void decidePolicyForNotificationPermissionRequest(WebPageProxy&, API::SecurityOrigin&, Function<void(bool)>&&) final;
        void handleAutoplayEvent(WebPageProxy&, WebCore::AutoplayEvent, OptionSet<WebCore::AutoplayEventFlags>) final;
        void unavailablePluginButtonClicked(WebPageProxy&, WKPluginUnavailabilityReason, API::Dictionary&) final;
        void mouseDidMoveOverElement(WebPageProxy&, const WebHitTestResultData&, OptionSet<WebEvent::Modifier>, API::Object*);
        void didClickAutoFillButton(WebPageProxy&, API::Object*) final;
        void toolbarsAreVisible(WebPageProxy&, Function<void(bool)>&&) final;
        bool runOpenPanel(WebPageProxy*, WebFrameProxy*, const WebCore::SecurityOriginData&, API::OpenPanelParameters*, WebOpenPanelResultListenerProxy*) final;
        void didExceedBackgroundResourceLimitWhileInForeground(WebPageProxy&, WKResourceLimit) final;
        void saveDataToFileInDownloadsFolder(WebPageProxy*, const WTF::String&, const WTF::String&, const URL&, API::Data&) final;
#endif
#if ENABLE(DEVICE_ORIENTATION)
        void shouldAllowDeviceOrientationAndMotionAccess(WebKit::WebPageProxy&, API::SecurityOrigin&, CompletionHandler<void(bool)>&&) final;
#endif
        bool needsFontAttributes() const final { return m_uiDelegate.m_delegateMethods.webViewDidChangeFontAttributes; }
        void didChangeFontAttributes(const WebCore::FontAttributes&) final;
        void decidePolicyForUserMediaPermissionRequest(WebPageProxy&, WebFrameProxy&, API::SecurityOrigin&, API::SecurityOrigin&, UserMediaPermissionRequestProxy&) final;
        void checkUserMediaPermissionForOrigin(WebPageProxy&, WebFrameProxy&, API::SecurityOrigin&, API::SecurityOrigin&, UserMediaPermissionCheckProxy&) final;
        void mediaCaptureStateDidChange(WebCore::MediaProducer::MediaStateFlags) final;
        void printFrame(WebPageProxy&, WebFrameProxy&) final;
#if PLATFORM(IOS_FAMILY)
#if HAVE(APP_LINKS)
        bool shouldIncludeAppLinkActionsForElement(_WKActivatedElementInfo *) final;
#endif
        RetainPtr<NSArray> actionsForElement(_WKActivatedElementInfo *, RetainPtr<NSArray> defaultActions) final;
        void didNotHandleTapAsClick(const WebCore::IntPoint&) final;
        UIViewController *presentingViewController() final;
#endif // PLATFORM(IOS_FAMILY)

        NSDictionary *dataDetectionContext() final;

#if ENABLE(POINTER_LOCK)
        void requestPointerLock(WebPageProxy*) final;
        void didLosePointerLock(WebPageProxy*) final;
#endif
        
        void hasVideoInPictureInPictureDidChange(WebPageProxy*, bool) final;

        void imageOrMediaDocumentSizeChanged(const WebCore::IntSize&) final;
        void didShowSafeBrowsingWarning() final;

        UIDelegate& m_uiDelegate;
    };

    WKWebView *m_webView;
    WeakObjCPtr<id <WKUIDelegate> > m_delegate;

    struct {
        bool webViewCreateWebViewWithConfigurationForNavigationActionWindowFeatures : 1;
        bool webViewCreateWebViewWithConfigurationForNavigationActionWindowFeaturesAsync : 1;
        bool webViewRunJavaScriptAlertPanelWithMessageInitiatedByFrameCompletionHandler : 1;
        bool webViewRunJavaScriptConfirmPanelWithMessageInitiatedByFrameCompletionHandler : 1;
        bool webViewRunJavaScriptTextInputPanelWithPromptDefaultTextInitiatedByFrameCompletionHandler : 1;
        bool webViewRequestStorageAccessPanelForTopPrivatelyControlledDomainUnderFirstPartyTopPrivatelyControlledDomainCompletionHandler : 1;
        bool webViewRunBeforeUnloadConfirmPanelWithMessageInitiatedByFrameCompletionHandler : 1;
        bool webViewRequestGeolocationPermissionForFrameDecisionHandler : 1;
        bool webViewDidResignInputElementStrongPasswordAppearanceWithUserInfo : 1;
        bool webViewTakeFocus : 1;
#if PLATFORM(MAC)
        bool showWebView : 1;
        bool focusWebView : 1;
        bool unfocusWebView : 1;
        bool webViewRunModal : 1;
        bool webViewDidScroll : 1;
        bool webViewHeaderHeight : 1;
        bool webViewFooterHeight : 1;
        bool webViewSetResizable : 1;
        bool webViewSetWindowFrame : 1;
        bool webViewDidNotHandleWheelEvent : 1;
        bool webViewHandleAutoplayEventWithFlags : 1;
        bool webViewUnavailablePlugInButtonClicked : 1;
        bool webViewDidClickAutoFillButtonWithUserInfo : 1;
        bool webViewDrawHeaderInRectForPageWithTitleURL : 1;
        bool webViewDrawFooterInRectForPageWithTitleURL : 1;
        bool webViewGetWindowFrameWithCompletionHandler : 1;
        bool webViewMouseDidMoveOverElementWithFlagsUserInfo : 1;
        bool webViewGetToolbarsAreVisibleWithCompletionHandler : 1;
        bool webViewDidExceedBackgroundResourceLimitWhileInForeground : 1;
        bool webViewSaveDataToFileSuggestedFilenameMimeTypeOriginatingURL : 1;
        bool webViewRunOpenPanelWithParametersInitiatedByFrameCompletionHandler : 1;
        bool webViewRequestNotificationPermissionForSecurityOriginDecisionHandler : 1;
#endif
#if ENABLE(DEVICE_ORIENTATION)
        bool webViewShouldAllowDeviceOrientationAndMotionAccessForSecurityOriginDecisionHandler : 1;
#endif
        bool webViewDecideDatabaseQuotaForSecurityOriginCurrentQuotaCurrentOriginUsageCurrentDatabaseUsageExpectedUsageDecisionHandler : 1;
        bool webViewDecideDatabaseQuotaForSecurityOriginDatabaseNameDisplayNameCurrentQuotaCurrentOriginUsageCurrentDatabaseUsageExpectedUsageDecisionHandler : 1;
        bool webViewDecideWebApplicationCacheQuotaForSecurityOriginCurrentQuotaTotalBytesNeeded : 1;
        bool webViewPrintFrame : 1;
        bool webViewDidClose : 1;
        bool webViewClose : 1;
        bool webViewFullscreenMayReturnToInline : 1;
        bool webViewDidEnterFullscreen : 1;
        bool webViewDidExitFullscreen : 1;
        bool webViewRequestMediaCaptureAuthorizationForFrameDecisionHandler : 1;
        bool webViewIsMediaCaptureAuthorizedForFrameDecisionHandler : 1;
        bool webViewMediaCaptureStateDidChange : 1;
        bool webViewDidChangeFontAttributes : 1;
#if PLATFORM(IOS_FAMILY)
#if HAVE(APP_LINKS)
        bool webViewShouldIncludeAppLinkActionsForElement : 1;
#endif
        bool webViewActionsForElementDefaultActions : 1;
        bool webViewDidNotHandleTapAsClickAtPoint : 1;
        bool presentingViewControllerForWebView : 1;
#endif
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
    } m_delegateMethods;
};

} // namespace WebKit
