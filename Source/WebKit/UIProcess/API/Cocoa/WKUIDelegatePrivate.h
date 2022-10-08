/*
 * Copyright (C) 2014-2020 Apple Inc. All rights reserved.
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

#import <WebKit/WKDragDestinationAction.h>
#import <WebKit/WKSecurityOrigin.h>
#import <WebKit/WKUIDelegate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKActivatedElementInfo.h>
#import <WebKit/_WKTapHandlingResult.h>
#import <WebKit/_WKWebAuthenticationPanel.h>

@class NSData;
@class UIScrollView;
@class UIViewController;
@class WKFrameInfo;
@class _WKContextMenuElementInfo;
@class _WKActivatedElementInfo;
@class _WKElementAction;
@class _WKFrameHandle;
@class _WKInspector;
@class _WKInspectorConfiguration;
@class _WKInspectorDebuggableInfo;
@class _WKModalContainerInfo;

#if TARGET_OS_IOS

@class UIContextMenuConfiguration;
@class UIDragItem;
@class UITargetedDragPreview;
@class WKContextMenuElementInfo;

@protocol UIContextMenuInteractionCommitAnimating;
@protocol UIDragSession;
@protocol UIDropSession;
@protocol UIEditMenuInteractionAnimating;

typedef NS_ENUM(NSInteger, _WKScreenOrientationLockType) {
    _WKScreenOrientationLockTypeAny,
    _WKScreenOrientationLockTypeLandscape,
    _WKScreenOrientationLockTypeLandscapePrimary,
    _WKScreenOrientationLockTypeLandscapeSecondary,
    _WKScreenOrientationLockTypePortrait,
    _WKScreenOrientationLockTypePortraitPrimary,
    _WKScreenOrientationLockTypePortraitSecondary
} WK_API_AVAILABLE(ios(WK_IOS_TBA));

#else

typedef NS_ENUM(NSInteger, _WKResourceLimit) {
    _WKResourceLimitMemory,
    _WKResourceLimitCPU,
} WK_API_AVAILABLE(macos(10.13.4));

typedef NS_ENUM(NSInteger, _WKPlugInUnavailabilityReason) {
    _WKPlugInUnavailabilityReasonPluginMissing,
    _WKPlugInUnavailabilityReasonPluginCrashed,
    _WKPlugInUnavailabilityReasonInsecurePluginVersion
} WK_API_AVAILABLE(macos(10.13.4));

#endif

typedef NS_ENUM(NSInteger, _WKAutoplayEvent) {
    _WKAutoplayEventDidPreventFromAutoplaying,
    _WKAutoplayEventDidPlayMediaWithUserGesture,
    _WKAutoplayEventDidAutoplayMediaPastThresholdWithoutUserInterference,
    _WKAutoplayEventUserDidInterfereWithPlayback,
} WK_API_AVAILABLE(macos(10.13.4), ios(14.0));

typedef NS_OPTIONS(NSUInteger, _WKAutoplayEventFlags) {
    _WKAutoplayEventFlagsNone = 0,
    _WKAutoplayEventFlagsHasAudio = 1 << 0,
    _WKAutoplayEventFlagsPlaybackWasPrevented = 1 << 1,
    _WKAutoplayEventFlagsMediaIsMainContent = 1 << 2,
} WK_API_AVAILABLE(macos(10.13.4), ios(14.0));

typedef NS_ENUM(NSInteger, _WKFocusDirection) {
    _WKFocusDirectionBackward,
    _WKFocusDirectionForward,
} WK_API_AVAILABLE(macos(10.13.4), ios(12.2));

typedef NS_ENUM(NSInteger, _WKXRSessionMode) {
    _WKXRSessionModeInline,
    _WKXRSessionModeImmersiveVr,
    _WKXRSessionModeImmersiveAr,
} WK_API_AVAILABLE(macos(13.0), ios(16.0));

typedef NS_OPTIONS(NSUInteger, _WKXRSessionFeatureFlags) {
    _WKXRSessionFeatureFlagsNone = 0,
    _WKXRSessionFeatureFlagsReferenceSpaceTypeViewer = 1 << 0,
    _WKXRSessionFeatureFlagsReferenceSpaceTypeLocal = 1 << 1,
    _WKXRSessionFeatureFlagsReferenceSpaceTypeLocalFloor = 1 << 2,
    _WKXRSessionFeatureFlagsReferenceSpaceTypeBoundedFloor = 1 << 3,
    _WKXRSessionFeatureFlagsReferenceSpaceTypeUnbounded = 1 << 4,
    _WKXRSessionFeatureFlagsHandTracking = 1 << 5,
} WK_API_AVAILABLE(macos(13.0), ios(16.0));

typedef NS_ENUM(NSInteger, _WKModalContainerDecision) {
    _WKModalContainerDecisionShow,
    _WKModalContainerDecisionHideAndIgnore,
    _WKModalContainerDecisionHideAndAllow,
    _WKModalContainerDecisionHideAndDisallow,
} WK_API_AVAILABLE(macos(13.0), ios(16.0));

typedef NS_ENUM(NSInteger, WKDisplayCapturePermissionDecision) {
    WKDisplayCapturePermissionDecisionDeny,
    WKDisplayCapturePermissionDecisionScreenPrompt,
    WKDisplayCapturePermissionDecisionWindowPrompt,
} WK_API_AVAILABLE(macos(13.0), ios(16.0));

@protocol WKUIDelegatePrivate <WKUIDelegate>

#ifdef FOUNDATION_HAS_DIRECTIONAL_GEOMETRY
typedef NSEdgeInsets UIEdgeInsets;
#else
struct UIEdgeInsets;
#endif

@optional

// FIXME: This should be handled by the WKWebsiteDataStore delegate.
- (void)_webView:(WKWebView *)webView decideDatabaseQuotaForSecurityOrigin:(WKSecurityOrigin *)securityOrigin currentQuota:(unsigned long long)currentQuota currentOriginUsage:(unsigned long long)currentOriginUsage currentDatabaseUsage:(unsigned long long)currentUsage expectedUsage:(unsigned long long)expectedUsage decisionHandler:(void (^)(unsigned long long newQuota))decisionHandler;

// FIXME: This should be handled by the WKWebsiteDataStore delegate.
- (void)_webView:(WKWebView *)webView decideDatabaseQuotaForSecurityOrigin:(WKSecurityOrigin *)securityOrigin databaseName:(NSString *)databaseName displayName:(NSString *)displayName currentQuota:(unsigned long long)currentQuota currentOriginUsage:(unsigned long long)currentOriginUsage currentDatabaseUsage:(unsigned long long)currentUsage expectedUsage:(unsigned long long)expectedUsage decisionHandler:(void (^)(unsigned long long newQuota))decisionHandler;

// FIXME: This should be handled by the WKWebsiteDataStore delegate.
- (void)_webView:(WKWebView *)webView decideWebApplicationCacheQuotaForSecurityOrigin:(WKSecurityOrigin *)securityOrigin currentQuota:(unsigned long long)currentQuota totalBytesNeeded:(unsigned long long)totalBytesNeeded decisionHandler:(void (^)(unsigned long long newQuota))decisionHandler;

- (void)_webView:(WKWebView *)webView printFrame:(_WKFrameHandle *)frame;
- (void)_webView:(WKWebView *)webView printFrame:(_WKFrameHandle *)frame pdfFirstPageSize:(CGSize)size completionHandler:(void (^)(void))completionHandler WK_API_AVAILABLE(macos(12.0), ios(15.0));

- (void)_webViewClose:(WKWebView *)webView;
- (void)_webViewFullscreenMayReturnToInline:(WKWebView *)webView;
- (void)_webViewDidEnterFullscreen:(WKWebView *)webView WK_API_AVAILABLE(macos(10.11), ios(8.3));
- (void)_webViewDidExitFullscreen:(WKWebView *)webView WK_API_AVAILABLE(macos(10.11), ios(8.3));
- (void)_webViewRequestPointerLock:(WKWebView *)webView WK_API_AVAILABLE(macos(10.12.3));
- (void)_webViewDidRequestPointerLock:(WKWebView *)webView completionHandler:(void (^)(BOOL))completionHandler WK_API_AVAILABLE(macos(10.14.4), ios(12.2));
- (void)_webViewDidShowSafeBrowsingWarning:(WKWebView *)webView WK_API_AVAILABLE(macos(10.14.4), ios(12.2));
- (void)_webViewDidLosePointerLock:(WKWebView *)webView WK_API_AVAILABLE(macos(10.12.3));
- (void)_webView:(WKWebView *)webView hasVideoInPictureInPictureDidChange:(BOOL)hasVideoInPictureInPicture WK_API_AVAILABLE(macos(10.13), ios(11.0));
- (void)_webView:(WKWebView *)webView shouldAllowPDFAtURL:(NSURL *)fileURL toOpenFromFrame:(WKFrameInfo *)frame completionHandler:(void (^)(BOOL))completionHandler WK_API_AVAILABLE(macos(11.0), ios(14.0));

- (void)_webView:(WKWebView *)webView imageOrMediaDocumentSizeChanged:(CGSize)size WK_API_AVAILABLE(macos(10.12), ios(10.0));
- (NSDictionary *)_dataDetectionContextForWebView:(WKWebView *)webView WK_API_AVAILABLE(macos(10.12), ios(10.0));
- (void)_webView:(WKWebView *)webView includeSensitiveMediaDeviceDetails:(void (^)(BOOL includeSensitiveDetails))decisionHandler WK_API_AVAILABLE(macos(10.15), ios(13.0));
- (void)_webView:(WKWebView *)webView requestDisplayCapturePermissionForOrigin:(WKSecurityOrigin *)securityOrigin initiatedByFrame:(WKFrameInfo *)frame withSystemAudio:(BOOL)withSystemAudio decisionHandler:(void (^)(WKDisplayCapturePermissionDecision decision))decisionHandler WK_API_AVAILABLE(macos(13.0), ios(16.0));
- (void)_webView:(WKWebView *)webView requestUserMediaAuthorizationForDevices:(_WKCaptureDevices)devices url:(NSURL *)url mainFrameURL:(NSURL *)mainFrameURL decisionHandler:(void (^)(BOOL authorized))decisionHandler WK_API_AVAILABLE(macos(10.13), ios(11.0));
- (void)_webView:(WKWebView *)webView checkUserMediaPermissionForURL:(NSURL *)url mainFrameURL:(NSURL *)mainFrameURL frameIdentifier:(NSUInteger)frameIdentifier decisionHandler:(void (^)(NSString *salt, BOOL authorized))decisionHandler WK_API_AVAILABLE(macos(10.12.3), ios(10.3));
- (void)_webView:(WKWebView *)webView mediaCaptureStateDidChange:(_WKMediaCaptureStateDeprecated)state WK_API_AVAILABLE(macos(10.13), ios(11.0));
- (WKDragDestinationAction)_webView:(WKWebView *)webView dragDestinationActionMaskForDraggingInfo:(id)draggingInfo WK_API_AVAILABLE(macos(10.13), ios(11.0));
- (void)_webView:(WKWebView *)webView createWebViewWithConfiguration:(WKWebViewConfiguration *)configuration forNavigationAction:(WKNavigationAction *)navigationAction windowFeatures:(WKWindowFeatures *)windowFeatures completionHandler:(void (^)(WKWebView *webView))completionHandler WK_API_AVAILABLE(macos(10.13), ios(11.0));
- (void)_webView:(WKWebView *)webView requestGeolocationPermissionForFrame:(WKFrameInfo *)frame decisionHandler:(void (^)(BOOL allowed))decisionHandler WK_API_AVAILABLE(macos(10.13.4), ios(11.3));
- (void)_webView:(WKWebView *)webView requestGeolocationPermissionForOrigin:(WKSecurityOrigin*)origin initiatedByFrame:(WKFrameInfo *)frame decisionHandler:(void (^)(WKPermissionDecision decision))decisionHandler WK_API_AVAILABLE(macos(12.0), ios(15.0));
- (void)_webView:(WKWebView *)webView queryPermission:(NSString*)name forOrigin:(WKSecurityOrigin*)origin completionHandler:(void (^)(WKPermissionDecision permissionState))completionHandler WK_API_AVAILABLE(macos(13.0), ios(16.0));
- (void)_webView:(WKWebView *)webView runBeforeUnloadConfirmPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(BOOL result))completionHandler WK_API_AVAILABLE(macos(10.13), ios(11.0));
- (void)_webView:(WKWebView *)webView editorStateDidChange:(NSDictionary *)editorState WK_API_AVAILABLE(macos(10.13.4), ios(11.3));

- (void)_webView:(WKWebView *)webView didRemoveAttachment:(_WKAttachment *)attachment WK_API_AVAILABLE(macos(10.13.4), ios(11.3));
- (void)_webView:(WKWebView *)webView didInsertAttachment:(_WKAttachment *)attachment withSource:(NSString *)source WK_API_AVAILABLE(macos(10.14), ios(12.0));
- (void)_webView:(WKWebView *)webView didInvalidateDataForAttachment:(_WKAttachment *)attachment WK_API_AVAILABLE(macos(10.14.4), ios(12.2));

- (void)_webView:(WKWebView *)webView didResignInputElementStrongPasswordAppearanceWithUserInfo:(id <NSSecureCoding>)userInfo WK_API_AVAILABLE(macos(10.14), ios(12.0));

- (void)_webView:(WKWebView *)webView requestStorageAccessPanelForDomain:(NSString *)requestingDomain underCurrentDomain:(NSString *)currentDomain completionHandler:(void (^)(BOOL result))completionHandler WK_API_AVAILABLE(macos(10.14), ios(12.0));

- (void)_webView:(WKWebView *)webView didChangeFontAttributes:(NSDictionary<NSString *, id> *)fontAttributes WK_API_AVAILABLE(macos(10.14.4), ios(12.2));

- (void)_webView:(WKWebView *)webView takeFocus:(_WKFocusDirection)direction WK_API_AVAILABLE(macos(10.13.4), ios(12.2));

- (void)_webView:(WKWebView *)webView requestWebAuthenticationNoGestureForOrigin:(WKSecurityOrigin *)orgin completionHandler:(void (^)(BOOL))completionHandler WK_API_AVAILABLE(macos(10.15.4), ios(13.4));

- (void)_webView:(WKWebView *)webView runWebAuthenticationPanel:(_WKWebAuthenticationPanel *)panel initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(_WKWebAuthenticationPanelResult))completionHandler WK_API_AVAILABLE(macos(10.15.4), ios(13.4));

- (void)_webView:(WKWebView *)webView handleAutoplayEvent:(_WKAutoplayEvent)event withFlags:(_WKAutoplayEventFlags)flags WK_API_AVAILABLE(macos(10.13.4), ios(14.0));

- (void)_webView:(WKWebView *)webView willShareActivityItems:(NSArray *)activityItems WK_API_AVAILABLE(macos(11.0), ios(14.0));

- (void)_webView:(WKWebView *)webView requestSpeechRecognitionPermissionForOrigin:(WKSecurityOrigin *)origin decisionHandler:(void (^)(BOOL authorized))decisionHandler WK_API_AVAILABLE(macos(12.0), ios(15.0));

- (void)_webViewDidEnableInspectorBrowserDomain:(WKWebView *)webView WK_API_AVAILABLE(macos(12.0), ios(15.0));
- (void)_webViewDidDisableInspectorBrowserDomain:(WKWebView *)webView WK_API_AVAILABLE(macos(12.0), ios(15.0));

- (void)_webView:(WKWebView *)webView requestPermissionForXRSessionOrigin:(NSString *)originString mode:(_WKXRSessionMode)mode grantedFeatures:(_WKXRSessionFeatureFlags)grantedFeatures consentRequiredFeatures:(_WKXRSessionFeatureFlags)consentRequiredFeatures consentOptionalFeatures:(_WKXRSessionFeatureFlags)consentOptionalFeatures completionHandler:(void (^)(_WKXRSessionFeatureFlags))completionHandler WK_API_AVAILABLE(macos(13.0), ios(16.0));
- (void)_webView:(WKWebView *)webView startXRSessionWithCompletionHandler:(void (^)(id))completionHandler WK_API_AVAILABLE(macos(12.0), ios(15.0));
- (void)_webView:(WKWebView *)webView requestNotificationPermissionForSecurityOrigin:(WKSecurityOrigin *)securityOrigin decisionHandler:(void (^)(BOOL))decisionHandler WK_API_AVAILABLE(macos(10.13.4), ios(16.0));
- (void)_webViewEndXRSession:(WKWebView *)webView WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

- (void)_webView:(WKWebView *)webView requestCookieConsentWithMoreInfoHandler:(void (^)(void))moreInfoHandler decisionHandler:(void (^)(BOOL))decisionHandler WK_API_AVAILABLE(macos(13.0), ios(16.0));

- (void)_webView:(WKWebView *)webView decidePolicyForModalContainer:(_WKModalContainerInfo *)containerInfo decisionHandler:(void (^)(_WKModalContainerDecision))decisionHandler WK_API_AVAILABLE(macos(13.0), ios(16.0));

#if TARGET_OS_IPHONE

- (BOOL)_webView:(WKWebView *)webView shouldIncludeAppLinkActionsForElement:(_WKActivatedElementInfo *)element WK_API_AVAILABLE(ios(9.0));
- (NSArray *)_webView:(WKWebView *)webView actionsForElement:(_WKActivatedElementInfo *)element defaultActions:(NSArray<_WKElementAction *> *)defaultActions;
- (void)_webView:(WKWebView *)webView didNotHandleTapAsClickAtPoint:(CGPoint)point;
- (void)_webViewStatusBarWasTapped:(WKWebView *)webView;
- (void)_webView:(WKWebView *)webView requestGeolocationAuthorizationForURL:(NSURL *)url frame:(WKFrameInfo *)frame decisionHandler:(void (^)(BOOL authorized))decisionHandler WK_API_AVAILABLE(ios(11.0));
- (BOOL)_webView:(WKWebView *)webView fileUploadPanelContentIsManagedWithInitiatingFrame:(WKFrameInfo *)frame;

- (UIViewController *)_webView:(WKWebView *)webView previewViewControllerForURL:(NSURL *)url WK_API_DEPRECATED_WITH_REPLACEMENT("webView:contextMenuConfigurationForElement:completionHandler:", ios(9.0, 13.0));
- (void)_webView:(WKWebView *)webView commitPreviewedViewController:(UIViewController *)previewedViewController WK_API_DEPRECATED_WITH_REPLACEMENT("webView:contextMenuForElement:willCommitWithAnimator:", ios(9.0, 13.0));
- (void)_webView:(WKWebView *)webView willPreviewImageWithURL:(NSURL *)imageURL WK_API_DEPRECATED_WITH_REPLACEMENT("webView:contextMenuConfigurationForElement:completionHandler:", ios(9.0, 13.0));
- (void)_webView:(WKWebView *)webView commitPreviewedImageWithURL:(NSURL *)imageURL WK_API_DEPRECATED_WITH_REPLACEMENT("webView:contextMenuForElement:willCommitWithAnimator:", ios(9.0, 13.0));
- (void)_webView:(WKWebView *)webView didDismissPreviewViewController:(UIViewController *)previewedViewController committing:(BOOL)committing WK_API_DEPRECATED_WITH_REPLACEMENT("webView:contextMenuDidEndForElement:", ios(9.0, 13.0));
- (void)_webView:(WKWebView *)webView didDismissPreviewViewController:(UIViewController *)previewedViewController WK_API_DEPRECATED_WITH_REPLACEMENT("webView:contextMenuDidEndForElement:", ios(9.0, 13.0));

#if TARGET_OS_IOS
// This needs to be removed once there is an API version to continue to do callbacks for image element context menus.
- (void)_webView:(WKWebView *)webView contextMenuConfigurationForElement:(WKContextMenuElementInfo *)elementInfo completionHandler:(void(^)(UIContextMenuConfiguration *))completionHandler WK_API_AVAILABLE(ios(13.0));

// These can be removed once there is an API version.
- (void)_webView:(WKWebView *)webView contextMenuForElement:(WKContextMenuElementInfo *)elementInfo willCommitWithAnimator:(id<UIContextMenuInteractionCommitAnimating>)animator WK_API_AVAILABLE(ios(13.0));
- (void)_webView:(WKWebView *)webView contextMenuWillPresentForElement:(WKContextMenuElementInfo *)elementInfo WK_API_AVAILABLE(ios(13.0));
- (UIViewController *)_webView:(WKWebView *)webView contextMenuContentPreviewForElement:(WKContextMenuElementInfo *)elementInfo WK_API_AVAILABLE(ios(15.0));
- (void)_webView:(WKWebView *)webView contextMenuDidEndForElement:(WKContextMenuElementInfo *)elementInfo WK_API_AVAILABLE(ios(13.0));
- (void)_webView:(WKWebView *)webview mouseDidMoveOverElement:(_WKHitTestResult *)hitTestResult withFlags:(UIKeyModifierFlags)flags userInfo:(id <NSSecureCoding>)userInfo WK_API_AVAILABLE(ios(16.0));
#endif

- (BOOL)_webView:(WKWebView *)webView showCustomSheetForElement:(_WKActivatedElementInfo *)element WK_API_DEPRECATED_WITH_REPLACEMENT("_webView:contextMenuConfigurationForElement:completionHandler:", ios(10.0, 13.0));
- (void)_webView:(WKWebView *)webView alternateActionForURL:(NSURL *)url WK_API_AVAILABLE(ios(10.0));
- (NSArray *)_attachmentListForWebView:(WKWebView *)webView WK_API_AVAILABLE(ios(10.0));
- (NSArray *)_attachmentListForWebView:(WKWebView *)webView sourceIsManaged:(BOOL*)sourceIsManaged WK_API_AVAILABLE(ios(10.3));
- (NSUInteger)_webView:(WKWebView *)webView indexIntoAttachmentListForElement:(_WKActivatedElementInfo *)element WK_API_AVAILABLE(ios(10.3));
- (UIEdgeInsets)_webView:(WKWebView *)webView finalObscuredInsetsForScrollView:(UIScrollView *)scrollView withVelocity:(CGPoint)velocity targetContentOffset:(inout CGPoint *)targetContentOffset WK_API_AVAILABLE(ios(9.0));
- (UIView *)_contextMenuHintPreviewContainerViewForWebView:(WKWebView *)webView WK_API_AVAILABLE(ios(15.0));
- (UIViewController *)_webView:(WKWebView *)webView previewViewControllerForURL:(NSURL *)url defaultActions:(NSArray<_WKElementAction *> *)actions elementInfo:(_WKActivatedElementInfo *)elementInfo WK_API_DEPRECATED_WITH_REPLACEMENT("webView:contextMenuConfigurationForElement:completionHandler:", ios(9.0, 13.0));
- (UIViewController *)_webView:(WKWebView *)webView previewViewControllerForAnimatedImageAtURL:(NSURL *)url defaultActions:(NSArray<_WKElementAction *> *)actions elementInfo:(_WKActivatedElementInfo *)elementInfo imageSize:(CGSize)imageSize WK_API_DEPRECATED_WITH_REPLACEMENT("webView:contextMenuConfigurationForElement:completionHandler:", ios(9.0, 13.0));
- (UIViewController *)_presentingViewControllerForWebView:(WKWebView *)webView WK_API_AVAILABLE(ios(10.0));
- (void)_webView:(WKWebView *)webView getAlternateURLFromImage:(UIImage *)image completionHandler:(void (^)(NSURL *alternateURL, NSDictionary *userInfo))completionHandler WK_API_AVAILABLE(ios(11.0));
- (NSURL *)_webView:(WKWebView *)webView alternateURLFromImage:(UIImage *)image userInfo:(NSDictionary **)userInfo WK_API_AVAILABLE(ios(11.0));
- (UIViewController *)_webView:(WKWebView *)webView previewViewControllerForImage:(UIImage *)image alternateURL:(NSURL *)url defaultActions:(NSArray<_WKElementAction *> *)actions elementInfo:(_WKActivatedElementInfo *)elementInfo WK_API_DEPRECATED_WITH_REPLACEMENT("webView:contextMenuConfigurationForElement:completionHandler:", ios(11.0, 13.0));
- (NSArray *)_webView:(WKWebView *)webView adjustedDataInteractionItemProviders:(NSArray *)originalItemProviders WK_API_AVAILABLE(ios(11.0));
- (NSArray *)_webView:(WKWebView *)webView adjustedDataInteractionItemProvidersForItemProvider:(id)itemProvider representingObjects:(NSArray *)representingObjects additionalData:(NSDictionary *)additionalData WK_API_AVAILABLE(ios(11.0));
- (BOOL)_webView:(WKWebView *)webView performDataInteractionOperationWithItemProviders:(NSArray *)itemProviders WK_API_AVAILABLE(ios(11.0));
- (void)_webView:(WKWebView *)webView dataInteraction:(id)interaction sessionWillBegin:(id)session WK_API_AVAILABLE(ios(11.0));
- (void)_webView:(WKWebView *)webView dataInteraction:(id)interaction session:(id)session didEndWithOperation:(NSUInteger)operation WK_API_AVAILABLE(ios(11.0));
- (void)_webView:(WKWebView *)webView dataInteractionOperationWasHandled:(BOOL)handled forSession:(id)session itemProviders:(NSArray *)itemProviders WK_API_AVAILABLE(ios(11.0));
- (NSUInteger)_webView:(WKWebView *)webView willUpdateDataInteractionOperationToOperation:(NSUInteger)operation forSession:(id)session WK_API_AVAILABLE(ios(11.0));

#if TARGET_OS_IOS
- (UIDropProposal *)_webView:(WKWebView *)webView willUpdateDropProposalToProposal:(UIDropProposal *)proposal forSession:(id <UIDropSession>)session WK_API_AVAILABLE(ios(12.2));
- (UITargetedDragPreview *)_webView:(WKWebView *)webView previewForLiftingItem:(UIDragItem *)item session:(id <UIDragSession>)session WK_API_AVAILABLE(ios(11.0));
- (UITargetedDragPreview *)_webView:(WKWebView *)webView previewForCancellingItem:(UIDragItem *)item withDefault:(UITargetedDragPreview *)defaultPreview WK_API_AVAILABLE(ios(11.0));
- (NSArray<UIDragItem *> *)_webView:(WKWebView *)webView willPerformDropWithSession:(id <UIDropSession>)session WK_API_AVAILABLE(ios(11.0));
- (NSInteger)_webView:(WKWebView *)webView dataOwnerForDropSession:(id <UIDropSession>)session WK_API_AVAILABLE(ios(11.0));
- (NSInteger)_webView:(WKWebView *)webView dataOwnerForDragSession:(id <UIDragSession>)session WK_API_AVAILABLE(ios(11.0));

- (void)_webViewLockScreenOrientation:(WKWebView *)webView lockType:(_WKScreenOrientationLockType)lockType WK_API_AVAILABLE(ios(WK_IOS_TBA));
- (void)_webViewUnlockScreenOrientation:(WKWebView *)webView WK_API_AVAILABLE(ios(WK_IOS_TBA));
#endif

- (void)_webView:(WKWebView *)webView didChangeSafeAreaShouldAffectObscuredInsets:(BOOL)safeAreaShouldAffectObscuredInsets WK_API_AVAILABLE(ios(11.0));
- (void)_webView:(WKWebView *)webView didPresentFocusedElementViewController:(UIViewController *)controller WK_API_AVAILABLE(ios(12.0));
- (void)_webView:(WKWebView *)webView didDismissFocusedElementViewController:(UIViewController *)controller WK_API_AVAILABLE(ios(12.0));
- (BOOL)_webView:(WKWebView *)webView gestureRecognizerCouldPinch:(UIGestureRecognizer *)gestureRecognizer WK_API_AVAILABLE(ios(13.0));

- (BOOL)_webViewCanBecomeFocused:(WKWebView *)webView WK_API_AVAILABLE(ios(15.0));
- (BOOL)_webView:(WKWebView *)webView touchEventsMustRequireGestureRecognizerToFail:(UIGestureRecognizer *)gestureRecognizer WK_API_AVAILABLE(ios(15.0));

#else // !TARGET_OS_IPHONE

- (NSViewController *)_presentingViewControllerForWebView:(WKWebView *)webView WK_API_AVAILABLE(macos(13.0));

- (void)_prepareForImmediateActionAnimationForWebView:(WKWebView *)webView WK_API_AVAILABLE(macos(10.13.4));
- (void)_cancelImmediateActionAnimationForWebView:(WKWebView *)webView WK_API_AVAILABLE(macos(10.13.4));
- (void)_completeImmediateActionAnimationForWebView:(WKWebView *)webView WK_API_AVAILABLE(macos(10.13.4));
- (void)_showWebView:(WKWebView *)webView WK_API_AVAILABLE(macos(10.13.4));
- (void)_focusWebView:(WKWebView *)webView WK_API_AVAILABLE(macos(10.13.4));
- (bool)_focusWebViewFromServiceWorker:(WKWebView *)webView WK_API_AVAILABLE(macos(13.0), ios(16.0));
- (void)_unfocusWebView:(WKWebView *)webView WK_API_AVAILABLE(macos(10.13.4));
- (void)_webViewDidScroll:(WKWebView *)webView WK_API_AVAILABLE(macos(10.13.4));
- (void)_webViewRunModal:(WKWebView *)webView WK_API_AVAILABLE(macos(10.13.4));
- (void)_webView:(WKWebView *)webView didNotHandleWheelEvent:(NSEvent *)event WK_API_AVAILABLE(macos(10.13.4));
- (void)_webView:(WKWebView *)webView didClickAutoFillButtonWithUserInfo:(id <NSSecureCoding>)userInfo WK_API_AVAILABLE(macos(10.13.4));
- (void)_webView:(WKWebView *)webView getToolbarsAreVisibleWithCompletionHandler:(void(^)(BOOL))completionHandler WK_API_AVAILABLE(macos(10.13.4));
- (void)_webView:(WKWebView *)webView saveDataToFile:(NSData *)data suggestedFilename:(NSString *)suggestedFilename mimeType:(NSString *)mimeType originatingURL:(NSURL *)url WK_API_AVAILABLE(macos(10.13.4));
- (CGFloat)_webViewHeaderHeight:(WKWebView *)webView WK_API_AVAILABLE(macos(10.13.4));
- (CGFloat)_webViewFooterHeight:(WKWebView *)webView WK_API_AVAILABLE(macos(10.13.4));
- (void)_webView:(WKWebView *)webView drawHeaderInRect:(CGRect)rect forPageWithTitle:(NSString *)title URL:(NSURL *)url WK_API_AVAILABLE(macos(10.13.4));
- (void)_webView:(WKWebView *)webView drawFooterInRect:(CGRect)rect forPageWithTitle:(NSString *)title URL:(NSURL *)url WK_API_AVAILABLE(macos(10.13.4));
- (void)_webView:(WKWebView *)webview mouseDidMoveOverElement:(_WKHitTestResult *)hitTestResult withFlags:(NSEventModifierFlags)flags userInfo:(id <NSSecureCoding>)userInfo;
- (void)_webView:(WKWebView *)webView setResizable:(BOOL)isResizable WK_API_AVAILABLE(macos(10.13.4));
- (void)_webView:(WKWebView *)webView getWindowFrameWithCompletionHandler:(void (^)(CGRect))completionHandler WK_API_AVAILABLE(macos(10.13.4));
- (void)_webView:(WKWebView *)webView setWindowFrame:(CGRect)frame WK_API_AVAILABLE(macos(10.13.4));
- (void)_webView:(WKWebView *)webView unavailablePlugInButtonClickedWithReason:(_WKPlugInUnavailabilityReason)reason plugInInfo:(NSDictionary *)plugInInfo;
- (NSMenu *)_webView:(WKWebView *)webView contextMenu:(NSMenu *)menu forElement:(_WKContextMenuElementInfo *)element WK_API_DEPRECATED_WITH_REPLACEMENT("_webView:getContextMenuFromProposedMenu:forElement:userInfo:completionHandler:", macos(10.12, 10.14.4));
- (NSMenu *)_webView:(WKWebView *)webView contextMenu:(NSMenu *)menu forElement:(_WKContextMenuElementInfo *)element userInfo:(id <NSSecureCoding>)userInfo WK_API_DEPRECATED_WITH_REPLACEMENT("_webView:getContextMenuFromProposedMenu:forElement:userInfo:completionHandler:", macos(10.12, 10.14.4));
- (void)_webView:(WKWebView *)webView getContextMenuFromProposedMenu:(NSMenu *)menu forElement:(_WKContextMenuElementInfo *)element userInfo:(id <NSSecureCoding>)userInfo completionHandler:(void (^)(NSMenu *))completionHandler WK_API_AVAILABLE(macos(10.14));
- (void)_webView:(WKWebView *)webView didPerformDragOperation:(BOOL)handled WK_API_AVAILABLE(macos(10.14.4));

/*! @abstract Called when the _WKInspector for this WKWebView is about to be displayed. The client can
    provide a custom _WKInspectorConfiguration that should be used when creating the Web Inspector.
    @param inspector The Web Inspector instance that is about to be initialized.
 */
- (_WKInspectorConfiguration *)_webView:(WKWebView *)webView configurationForLocalInspector:(_WKInspector *)inspector WK_API_AVAILABLE(macos(12.0));

/*! @abstract Called when a Web Inspector instance is attached to this WKWebView. This is not called in the case of remote inspection.
    @param webView The WKWebView instance being inspected.
    @param inspector The Web Inspector instance attached to this WKWebView.
 */
- (void)_webView:(WKWebView *)webView didAttachLocalInspector:(_WKInspector *)inspector WK_API_AVAILABLE(macos(12.0));

/*! @abstract Called before closing the Web Inspector instance for this WKWebView. This is not called in the case of remote inspection.
    @param webView The WKWebView instance being inspected.
    @param inspector The Web Inspector instance being closed.
 */
- (void)_webView:(WKWebView *)webView willCloseLocalInspector:(_WKInspector *)inspector WK_API_AVAILABLE(macos(12.0));

#endif // !TARGET_OS_IPHONE

@end
