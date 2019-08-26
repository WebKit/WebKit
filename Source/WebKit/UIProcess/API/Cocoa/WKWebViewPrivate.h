/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#import <WebKit/WKWebView.h>

#import <WebKit/WKDataDetectorTypes.h>
#import <WebKit/_WKActivatedElementInfo.h>
#import <WebKit/_WKAttachment.h>
#import <WebKit/_WKFindOptions.h>
#import <WebKit/_WKLayoutMode.h>
#import <WebKit/_WKOverlayScrollbarStyle.h>
#import <WebKit/_WKRenderingProgressEvents.h>

typedef NS_ENUM(NSInteger, _WKPaginationMode) {
    _WKPaginationModeUnpaginated,
    _WKPaginationModeLeftToRight,
    _WKPaginationModeRightToLeft,
    _WKPaginationModeTopToBottom,
    _WKPaginationModeBottomToTop,
} WK_API_AVAILABLE(macos(10.10), ios(8.0));

typedef NS_OPTIONS(NSUInteger, _WKMediaCaptureState) {
    _WKMediaCaptureStateNone = 0,
    _WKMediaCaptureStateActiveMicrophone = 1 << 0,
    _WKMediaCaptureStateActiveCamera = 1 << 1,
    _WKMediaCaptureStateMutedMicrophone = 1 << 2,
    _WKMediaCaptureStateMutedCamera = 1 << 3,
} WK_API_AVAILABLE(macos(10.13), ios(11.0));

typedef NS_OPTIONS(NSUInteger, _WKMediaMutedState) {
    _WKMediaNoneMuted = 0,
    _WKMediaAudioMuted = 1 << 0,
    _WKMediaCaptureDevicesMuted = 1 << 1,
    _WKMediaScreenCaptureMuted = 1 << 2,
} WK_API_AVAILABLE(macos(10.13), ios(11.0));

typedef NS_OPTIONS(NSUInteger, _WKCaptureDevices) {
    _WKCaptureDeviceMicrophone = 1 << 0,
    _WKCaptureDeviceCamera = 1 << 1,
    _WKCaptureDeviceDisplay = 1 << 2,
} WK_API_AVAILABLE(macos(10.13), ios(11.0));

typedef NS_OPTIONS(NSUInteger, _WKSelectionAttributes) {
    _WKSelectionAttributeNoSelection = 0,
    _WKSelectionAttributeIsCaret = 1 << 0,
    _WKSelectionAttributeIsRange = 1 << 1,
    _WKSelectionAttributeAtStartOfSentence = 1 << 2,
} WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

#if TARGET_OS_IPHONE

typedef NS_ENUM(NSUInteger, _WKDragInteractionPolicy) {
    _WKDragInteractionPolicyDefault = 0,
    _WKDragInteractionPolicyAlwaysEnable,
    _WKDragInteractionPolicyAlwaysDisable
} WK_API_AVAILABLE(ios(11.0));

#else
#import <WebKit/WKBase.h>
#endif

#if !TARGET_OS_IPHONE

typedef NS_ENUM(NSInteger, _WKImmediateActionType) {
    _WKImmediateActionNone,
    _WKImmediateActionLinkPreview,
    _WKImmediateActionDataDetectedItem,
    _WKImmediateActionLookupText,
    _WKImmediateActionMailtoLink,
    _WKImmediateActionTelLink
} WK_API_AVAILABLE(macos(10.12));

typedef NS_OPTIONS(NSUInteger, _WKRectEdge) {
    _WKRectEdgeNone = 0,
    _WKRectEdgeLeft = 1 << CGRectMinXEdge,
    _WKRectEdgeTop = 1 << CGRectMinYEdge,
    _WKRectEdgeRight = 1 << CGRectMaxXEdge,
    _WKRectEdgeBottom = 1 << CGRectMaxYEdge,
    _WKRectEdgeAll = _WKRectEdgeLeft | _WKRectEdgeTop | _WKRectEdgeRight | _WKRectEdgeBottom,
} WK_API_AVAILABLE(macos(10.13.4));

#endif

@class WKBrowsingContextHandle;
@class _WKApplicationManifest;
@class _WKFrameHandle;
@class _WKHitTestResult;
@class _WKIconLoadingDelegate;
@class _WKInspector;
@class _WKRemoteObjectRegistry;
@class _WKSafeBrowsingWarning;
@class _WKSessionState;
@class _WKTextInputContext;
@class _WKThumbnailView;
@class _WKWebsitePolicies;
@class _WKWebViewPrintFormatter;

@protocol WKHistoryDelegatePrivate;
@protocol _WKDiagnosticLoggingDelegate;
@protocol _WKFindDelegate;
@protocol _WKIconLoadingDelegate;
@protocol _WKInputDelegate;
@protocol _WKFullscreenDelegate;

@interface WKWebView (WKPrivate)

// FIXME: This should return a _WKRemoteObjectRegistry *.
@property (nonatomic, readonly) id _remoteObjectRegistry;
@property (nonatomic, readonly) WKBrowsingContextHandle *_handle;

@property (nonatomic, setter=_setObservedRenderingProgressEvents:) _WKRenderingProgressEvents _observedRenderingProgressEvents;

@property (nonatomic, weak, setter=_setHistoryDelegate:) id <WKHistoryDelegatePrivate> _historyDelegate;
@property (nonatomic, weak, setter=_setIconLoadingDelegate:) id <_WKIconLoadingDelegate> _iconLoadingDelegate;

@property (nonatomic, readonly) NSURL *_unreachableURL;
@property (nonatomic, readonly) NSURL *_mainFrameURL WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

- (void)_loadAlternateHTMLString:(NSString *)string baseURL:(NSURL *)baseURL forUnreachableURL:(NSURL *)unreachableURL;
- (WKNavigation *)_loadData:(NSData *)data MIMEType:(NSString *)MIMEType characterEncodingName:(NSString *)characterEncodingName baseURL:(NSURL *)baseURL userData:(id)userData WK_API_AVAILABLE(macos(10.12), ios(10.0));
- (WKNavigation *)_loadRequest:(NSURLRequest *)request shouldOpenExternalURLs:(BOOL)shouldOpenExternalURLs WK_API_AVAILABLE(macos(10.13), ios(11.0));

@property (nonatomic, readonly) NSArray *_certificateChain WK_API_DEPRECATED_WITH_REPLACEMENT("certificateChain", macos(10.10, 10.11), ios(8.0, 9.0));
@property (nonatomic, readonly) NSURL *_committedURL;
@property (nonatomic, readonly) NSString *_MIMEType;
@property (nonatomic, readonly) NSString *_userAgent WK_API_AVAILABLE(macos(10.11), ios(9.0));

@property (nonatomic, readonly, getter=_isPlayingAudio) BOOL _playingAudio WK_API_AVAILABLE(macos(10.13.4), ios(11.3));

@property (copy, setter=_setApplicationNameForUserAgent:) NSString *_applicationNameForUserAgent;
@property (copy, setter=_setCustomUserAgent:) NSString *_customUserAgent;

@property (nonatomic, setter=_setUserContentExtensionsEnabled:) BOOL _userContentExtensionsEnabled WK_API_AVAILABLE(macos(10.11), ios(9.0));

@property (nonatomic, readonly) pid_t _webProcessIdentifier;
@property (nonatomic, readonly) pid_t _provisionalWebProcessIdentifier WK_API_AVAILABLE(macos(10.14.4), ios(12.2));

@property (nonatomic, getter=_isEditable, setter=_setEditable:) BOOL _editable WK_API_AVAILABLE(macos(10.11), ios(9.0));

// FIXME: Remove these once nobody is using them.
@property (nonatomic, readonly) NSData *_sessionStateData;
- (void)_restoreFromSessionStateData:(NSData *)sessionStateData;

@property (nonatomic, readonly) _WKSessionState *_sessionState;
- (WKNavigation *)_restoreSessionState:(_WKSessionState *)sessionState andNavigate:(BOOL)navigate;
- (_WKSessionState *)_sessionStateWithFilter:(BOOL (^)(WKBackForwardListItem *item))filter;

@property (nonatomic, setter=_setAllowsRemoteInspection:) BOOL _allowsRemoteInspection;
@property (nonatomic, copy, setter=_setRemoteInspectionNameOverride:) NSString *_remoteInspectionNameOverride WK_API_AVAILABLE(macos(10.12), ios(10.0));

@property (nonatomic, setter=_setAddsVisitedLinks:) BOOL _addsVisitedLinks;

@property (nonatomic, readonly) BOOL _networkRequestsInProgress;

@property (nonatomic, readonly, getter=_isShowingNavigationGestureSnapshot) BOOL _showingNavigationGestureSnapshot;

@property (nonatomic, readonly) NSURL *_resourceDirectoryURL WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

- (void)_close;

- (void)_updateWebsitePolicies:(_WKWebsitePolicies *)websitePolicies WK_API_AVAILABLE(macos(10.13), ios(11.0));

- (void)_evaluateJavaScriptWithoutUserGesture:(NSString *)javaScriptString completionHandler:(void (^)(id, NSError *))completionHandler WK_API_AVAILABLE(macos(10.13), ios(11.0));

@property (nonatomic, setter=_setLayoutMode:) _WKLayoutMode _layoutMode;
// For use with _layoutMode = _WKLayoutModeFixedSize:
@property (nonatomic, setter=_setFixedLayoutSize:) CGSize _fixedLayoutSize;

@property (nonatomic, setter=_setViewportSizeForCSSViewportUnits:) CGSize _viewportSizeForCSSViewportUnits WK_API_AVAILABLE(macos(10.13), ios(11.0));

@property (nonatomic, setter=_setViewScale:) CGFloat _viewScale WK_API_AVAILABLE(macos(10.11), ios(9.0));

@property (nonatomic, setter=_setMinimumEffectiveDeviceWidth:) CGFloat _minimumEffectiveDeviceWidth WK_API_AVAILABLE(macos(10.14.4), ios(12.2));

@property (nonatomic, setter=_setBackgroundExtendsBeyondPage:) BOOL _backgroundExtendsBeyondPage WK_API_AVAILABLE(macos(10.13.4), ios(8.0));

- (_WKAttachment *)_insertAttachmentWithFilename:(NSString *)filename contentType:(NSString *)contentType data:(NSData *)data options:(_WKAttachmentDisplayOptions *)options completion:(void(^)(BOOL success))completionHandler WK_API_DEPRECATED_WITH_REPLACEMENT("-_insertAttachmentWithFileWrapper:contentType:options:completion:", macos(10.13.4, 10.14.4), ios(11.3, 12.2));
- (_WKAttachment *)_insertAttachmentWithFileWrapper:(NSFileWrapper *)fileWrapper contentType:(NSString *)contentType options:(_WKAttachmentDisplayOptions *)options completion:(void(^)(BOOL success))completionHandler WK_API_DEPRECATED_WITH_REPLACEMENT("-_insertAttachmentWithFileWrapper:contentType:completion:", macos(10.14.4, 10.14.4), ios(12.2, 12.2));
- (_WKAttachment *)_insertAttachmentWithFileWrapper:(NSFileWrapper *)fileWrapper contentType:(NSString *)contentType completion:(void(^)(BOOL success))completionHandler WK_API_AVAILABLE(macos(10.14.4), ios(12.2));
- (_WKAttachment *)_attachmentForIdentifier:(NSString *)identifier WK_API_AVAILABLE(macos(10.14.4), ios(12.2));

- (void)_simulateDeviceOrientationChangeWithAlpha:(double)alpha beta:(double)beta gamma:(double)gamma WK_API_AVAILABLE(macos(10.14.4), ios(12.2));

+ (BOOL)_handlesSafeBrowsing WK_API_AVAILABLE(macos(10.14.4), ios(12.2));
+ (NSURL *)_confirmMalwareSentinel WK_API_AVAILABLE(macos(10.14.4), ios(12.2));
+ (NSURL *)_visitUnsafeWebsiteSentinel WK_API_AVAILABLE(macos(10.14.4), ios(12.2));
- (void)_showSafeBrowsingWarningWithTitle:(NSString *)title warning:(NSString *)warning details:(NSAttributedString *)details completionHandler:(void(^)(BOOL))completionHandler WK_API_AVAILABLE(macos(10.14.4), ios(12.2));
- (void)_showSafeBrowsingWarningWithURL:(NSURL *)url title:(NSString *)title warning:(NSString *)warning details:(NSAttributedString *)details completionHandler:(void(^)(BOOL))completionHandler WK_API_AVAILABLE(macos(10.14.4), ios(12.2));

- (void)_isJITEnabled:(void(^)(BOOL))completionHandler WK_API_AVAILABLE(macos(10.14.4), ios(12.2));
- (void)_removeDataDetectedLinks:(dispatch_block_t)completion WK_API_AVAILABLE(macos(10.14.4), ios(12.2));

- (IBAction)_alignCenter:(id)sender WK_API_AVAILABLE(macos(10.14.4), ios(12.2));
- (IBAction)_alignJustified:(id)sender WK_API_AVAILABLE(macos(10.14.4), ios(12.2));
- (IBAction)_alignLeft:(id)sender WK_API_AVAILABLE(macos(10.14.4), ios(12.2));
- (IBAction)_alignRight:(id)sender WK_API_AVAILABLE(macos(10.14.4), ios(12.2));
- (IBAction)_indent:(id)sender WK_API_AVAILABLE(macos(10.14.4), ios(12.2));
- (IBAction)_outdent:(id)sender WK_API_AVAILABLE(macos(10.14.4), ios(12.2));
- (IBAction)_toggleStrikeThrough:(id)sender WK_API_AVAILABLE(macos(10.14.4), ios(12.2));
- (IBAction)_insertOrderedList:(id)sender WK_API_AVAILABLE(macos(10.14.4), ios(12.2));
- (IBAction)_insertUnorderedList:(id)sender WK_API_AVAILABLE(macos(10.14.4), ios(12.2));
- (IBAction)_insertNestedOrderedList:(id)sender WK_API_AVAILABLE(macos(10.14.4), ios(12.2));
- (IBAction)_insertNestedUnorderedList:(id)sender WK_API_AVAILABLE(macos(10.14.4), ios(12.2));
- (IBAction)_increaseListLevel:(id)sender WK_API_AVAILABLE(macos(10.14.4), ios(12.2));
- (IBAction)_decreaseListLevel:(id)sender WK_API_AVAILABLE(macos(10.14.4), ios(12.2));
- (IBAction)_changeListType:(id)sender WK_API_AVAILABLE(macos(10.14.4), ios(12.2));
- (IBAction)_pasteAsQuotation:(id)sender WK_API_AVAILABLE(macos(10.14.4), ios(12.2));
- (IBAction)_pasteAndMatchStyle:(id)sender WK_API_AVAILABLE(macos(10.14.4), ios(12.2));
- (IBAction)_takeFindStringFromSelection:(id)sender WK_API_AVAILABLE(macos(10.14.4), ios(12.2));

@property (class, nonatomic, copy, setter=_setStringForFind:) NSString *_stringForFind WK_API_AVAILABLE(macos(10.14.4), ios(12.2));
@property (nonatomic, readonly) _WKSelectionAttributes _selectionAttributes WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

#if TARGET_OS_IPHONE

- (void)_setFont:(UIFont *)font sender:(id)sender WK_API_AVAILABLE(ios(12.2));
- (void)_setFontSize:(CGFloat)fontSize sender:(id)sender WK_API_AVAILABLE(ios(12.2));
- (void)_setTextColor:(UIColor *)color sender:(id)sender WK_API_AVAILABLE(ios(12.2));

- (void)_detectDataWithTypes:(WKDataDetectorTypes)types completionHandler:(dispatch_block_t)completion WK_API_AVAILABLE(ios(12.2));

// DERECATED: The setters of the three following function are deprecated, please use overrideLayoutParameters.
// Define the smallest size a page take with a regular viewport.
@property (nonatomic, readonly) CGSize _minimumLayoutSizeOverride;
// Define the largest size the unobscured area can get for the current view bounds. This value is used to define viewport units.
@property (nonatomic, readonly) CGSize _maximumUnobscuredSizeOverride;

// Define the inset of the scrollview unusable by the web page.
@property (nonatomic, setter=_setObscuredInsets:) UIEdgeInsets _obscuredInsets;

@property (nonatomic, setter=_setUnobscuredSafeAreaInsets:) UIEdgeInsets _unobscuredSafeAreaInsets WK_API_AVAILABLE(ios(11.0));
@property (nonatomic, readonly) BOOL _safeAreaShouldAffectObscuredInsets WK_API_AVAILABLE(ios(11.0));
@property (nonatomic, setter=_setObscuredInsetEdgesAffectedBySafeArea:) UIRectEdge _obscuredInsetEdgesAffectedBySafeArea WK_API_AVAILABLE(ios(11.0));

// An ancestor view whose bounds will be intersected with those of this WKWebView to determine the visible region of content to render.
@property (nonatomic, readonly) UIView *_enclosingViewForExposedRectComputation WK_API_AVAILABLE(ios(11.0));

// Override the interface orientation. Clients using _beginAnimatedResizeWithUpdates: must update the interface orientation
// in the update block.
@property (nonatomic, setter=_setInterfaceOrientationOverride:) UIInterfaceOrientation _interfaceOrientationOverride;

@property (nonatomic, setter=_setAllowsViewportShrinkToFit:) BOOL _allowsViewportShrinkToFit;

// FIXME: Remove these three properties once we expose WKWebViewContentProvider as API.
@property (nonatomic, readonly, getter=_isDisplayingPDF) BOOL _displayingPDF;
@property (nonatomic, readonly) NSData *_dataForDisplayedPDF;
// FIXME: This can be removed once WKNavigation's response property is implemented.
@property (nonatomic, readonly) NSString *_suggestedFilenameForDisplayedPDF;

@property (nonatomic, readonly) _WKWebViewPrintFormatter *_webViewPrintFormatter;

@property (nonatomic, setter=_setDragInteractionPolicy:) _WKDragInteractionPolicy _dragInteractionPolicy WK_API_AVAILABLE(ios(11.0));
@property (nonatomic, readonly) BOOL _shouldAvoidResizingWhenInputViewBoundsChange WK_API_AVAILABLE(ios(WK_IOS_TBA));

- (void)_beginInteractiveObscuredInsetsChange;
- (void)_endInteractiveObscuredInsetsChange;
- (void)_hideContentUntilNextUpdate;

- (void)_beginAnimatedResizeWithUpdates:(void (^)(void))updateBlock;
- (void)_endAnimatedResize;
- (void)_resizeWhileHidingContentWithUpdates:(void (^)(void))updateBlock;

- (void)_snapshotRect:(CGRect)rectInViewCoordinates intoImageOfWidth:(CGFloat)imageWidth completionHandler:(void(^)(CGImageRef))completionHandler;

- (void)_overrideLayoutParametersWithMinimumLayoutSize:(CGSize)minimumLayoutSize maximumUnobscuredSizeOverride:(CGSize)maximumUnobscuredSizeOverride WK_API_AVAILABLE(ios(9_0));
- (void)_overrideViewportWithArguments:(NSDictionary<NSString *, NSString *> *)arguments WK_API_AVAILABLE(ios(WK_IOS_TBA));

- (void)_clearOverrideLayoutParameters WK_API_AVAILABLE(ios(11.0));
- (void)_clearInterfaceOrientationOverride WK_API_AVAILABLE(ios(11.0));

- (UIView *)_viewForFindUI;

- (void)_setOverlaidAccessoryViewsInset:(CGSize)inset;

// Puts the view into a state where being taken out of the view hierarchy and resigning first responder
// will not count as becoming inactive and unfocused. The returned block must be called to exit the state.
- (void (^)(void))_retainActiveFocusedState WK_API_AVAILABLE(ios(9_0));

- (void)_becomeFirstResponderWithSelectionMovingForward:(BOOL)selectingForward completionHandler:(void (^)(BOOL didBecomeFirstResponder))completionHandler WK_API_AVAILABLE(ios(9_0));

- (id)_snapshotLayerContentsForBackForwardListItem:(WKBackForwardListItem *)item WK_API_AVAILABLE(ios(9_0));
- (NSArray *)_dataDetectionResults;

- (void)_accessibilityRetrieveSpeakSelectionContent WK_API_AVAILABLE(ios(11.0));
- (void)_accessibilityDidGetSpeakSelectionContent:(NSString *)content WK_API_AVAILABLE(ios(11.0));

@property (nonatomic, readonly) UIView *_safeBrowsingWarning WK_API_AVAILABLE(macos(10.14.4), ios(12.2));
#else
@property (nonatomic, readonly) NSView *_safeBrowsingWarning WK_API_AVAILABLE(macos(10.14.4), ios(12.2));

@property (nonatomic, readonly) _WKRectEdge _pinnedState WK_API_AVAILABLE(macos(10.13.4));
@property (nonatomic, setter=_setRubberBandingEnabled:) _WKRectEdge _rubberBandingEnabled WK_API_AVAILABLE(macos(10.13.4));

@property (readonly) NSColor *_pageExtendedBackgroundColor;
@property (nonatomic, setter=_setBackgroundColor:) NSColor *_backgroundColor WK_API_AVAILABLE(macos(10.14));
@property (nonatomic, setter=_setDrawsBackground:) BOOL _drawsBackground;
@property (nonatomic, setter=_setTopContentInset:) CGFloat _topContentInset;

@property (nonatomic, setter=_setAutomaticallyAdjustsContentInsets:) BOOL _automaticallyAdjustsContentInsets;

// Default value is 0. A value of 0 means the window's backing scale factor will be used and automatically update when the window moves screens.
@property (nonatomic, setter=_setOverrideDeviceScaleFactor:) CGFloat _overrideDeviceScaleFactor WK_API_AVAILABLE(macos(10.11));

@property (nonatomic, setter=_setWindowOcclusionDetectionEnabled:) BOOL _windowOcclusionDetectionEnabled;

@property (nonatomic, readonly) NSInteger _spellCheckerDocumentTag WK_API_AVAILABLE(macos(10.14));

- (void)_setShouldSuppressFirstResponderChanges:(BOOL)shouldSuppress;

// Clients that want to maintain default behavior can return nil. To disable the immediate action entirely, return NSNull. And to
// do something custom, return an object that conforms to the NSImmediateActionAnimationController protocol.
- (id)_immediateActionAnimationControllerForHitTestResult:(_WKHitTestResult *)hitTestResult withType:(_WKImmediateActionType)type userData:(id<NSSecureCoding>)userData;

@property (nonatomic, setter=_setMinimumLayoutWidth:) CGFloat _minimumLayoutWidth WK_API_AVAILABLE(macos(10.12));

// When using _minimumLayoutWidth, the web content will lay out to the intrinsic height
// of the content; use this property to force it to lay out to the height of the view instead.
@property (nonatomic, setter=_setShouldExpandContentToViewHeightForAutoLayout:) BOOL _shouldExpandContentToViewHeightForAutoLayout WK_API_AVAILABLE(macos(10.12));

@property (nonatomic, setter=_setAlwaysShowsHorizontalScroller:) BOOL _alwaysShowsHorizontalScroller WK_API_AVAILABLE(macos(10.13.4));
@property (nonatomic, setter=_setAlwaysShowsVerticalScroller:) BOOL _alwaysShowsVerticalScroller WK_API_AVAILABLE(macos(10.13.4));

- (NSPrintOperation *)_printOperationWithPrintInfo:(NSPrintInfo *)printInfo;
- (NSPrintOperation *)_printOperationWithPrintInfo:(NSPrintInfo *)printInfo forFrame:(_WKFrameHandle *)frameHandle WK_API_AVAILABLE(macos(10.12), ios(10.0));

// FIXME: This SPI should become a part of the WKUIDelegate. rdar://problem/26561537
@property (nonatomic, readwrite, setter=_setWantsMediaPlaybackControlsView:) BOOL _wantsMediaPlaybackControlsView WK_API_AVAILABLE(macos(10.12.3));
@property (nonatomic, readonly) id _mediaPlaybackControlsView WK_API_AVAILABLE(macos(10.13));
- (void)_addMediaPlaybackControlsView:(id)mediaPlaybackControlsView WK_API_AVAILABLE(macos(10.13));
- (void)_removeMediaPlaybackControlsView WK_API_AVAILABLE(macos(10.12.3));

- (void)_prepareForMoveToWindow:(NSWindow *)targetWindow completionHandler:(void(^)(void))completionHandler WK_API_AVAILABLE(macos(10.13));

@property (nonatomic, setter=_setOverlayScrollbarStyle:) _WKOverlayScrollbarStyle _overlayScrollbarStyle WK_API_AVAILABLE(macos(10.13.4));
@property (strong, nonatomic, setter=_setInspectorAttachmentView:) NSView *_inspectorAttachmentView WK_API_AVAILABLE(macos(10.13.4));

@property (nonatomic, setter=_setThumbnailView:) _WKThumbnailView *_thumbnailView WK_API_AVAILABLE(macos(10.13.4));
@property (nonatomic, setter=_setIgnoresAllEvents:) BOOL _ignoresAllEvents WK_API_AVAILABLE(macos(10.13.4));

// Defaults to YES; if set to NO, WebKit will draw the grey wash and highlights itself.
@property (nonatomic, setter=_setUsePlatformFindUI:) BOOL _usePlatformFindUI WK_API_AVAILABLE(macos(WK_MAC_TBA));

#endif

- (WKNavigation *)_reloadWithoutContentBlockers WK_API_AVAILABLE(macos(10.12), ios(10.0));
- (WKNavigation *)_reloadExpiredOnly WK_API_AVAILABLE(macos(10.13), ios(11.0));

- (void)_killWebContentProcess;
- (void)_killWebContentProcessAndResetState;

- (void)_getMainResourceDataWithCompletionHandler:(void (^)(NSData *, NSError *))completionHandler;
- (void)_getWebArchiveDataWithCompletionHandler:(void (^)(NSData *, NSError *))completionHandler;
- (void)_getContentsAsStringWithCompletionHandler:(void (^)(NSString *, NSError *))completionHandler WK_API_AVAILABLE(macos(10.13), ios(11.0));
- (void)_getContentsAsAttributedStringWithCompletionHandler:(void (^)(NSAttributedString *, NSDictionary<NSAttributedStringDocumentAttributeKey, id> *, NSError *))completionHandler WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

- (void)_getApplicationManifestWithCompletionHandler:(void (^)(_WKApplicationManifest *))completionHandler WK_API_AVAILABLE(macos(10.13.4), ios(11.3));

@property (nonatomic, setter=_setPaginationMode:) _WKPaginationMode _paginationMode;
// Whether the column-break-{before,after} properties are respected instead of the
// page-break-{before,after} properties.
@property (nonatomic, setter=_setPaginationBehavesLikeColumns:) BOOL _paginationBehavesLikeColumns;
// Set to 0 to have the page length equal the view length.
@property (nonatomic, setter=_setPageLength:) CGFloat _pageLength;
@property (nonatomic, setter=_setGapBetweenPages:) CGFloat _gapBetweenPages;
@property (nonatomic, setter=_setPaginationLineGridEnabled:) BOOL _paginationLineGridEnabled;
@property (readonly) NSUInteger _pageCount;

@property (nonatomic, readonly) BOOL _supportsTextZoom;
@property (nonatomic, setter=_setTextZoomFactor:) double _textZoomFactor;
@property (nonatomic, setter=_setPageZoomFactor:) double _pageZoomFactor;

@property (nonatomic, weak, setter=_setDiagnosticLoggingDelegate:) id <_WKDiagnosticLoggingDelegate> _diagnosticLoggingDelegate WK_API_AVAILABLE(macos(10.11), ios(9.0));
@property (nonatomic, weak, setter=_setFindDelegate:) id <_WKFindDelegate> _findDelegate;
- (void)_findString:(NSString *)string options:(_WKFindOptions)options maxCount:(NSUInteger)maxCount;
- (void)_countStringMatches:(NSString *)string options:(_WKFindOptions)options maxCount:(NSUInteger)maxCount;
- (void)_hideFindUI;

@property (nonatomic, weak, setter=_setInputDelegate:) id <_WKInputDelegate> _inputDelegate WK_API_AVAILABLE(macos(10.12), ios(10.0));

@property (nonatomic, readonly, getter=_isDisplayingStandaloneImageDocument) BOOL _displayingStandaloneImageDocument;
@property (nonatomic, readonly, getter=_isDisplayingStandaloneMediaDocument) BOOL _displayingStandaloneMediaDocument;

@property (nonatomic, setter=_setScrollPerformanceDataCollectionEnabled:) BOOL _scrollPerformanceDataCollectionEnabled WK_API_AVAILABLE(macos(10.11), ios(9.0));
@property (nonatomic, readonly) NSArray *_scrollPerformanceData WK_API_AVAILABLE(macos(10.11), ios(9.0));

- (void)_saveBackForwardSnapshotForItem:(WKBackForwardListItem *)item WK_API_AVAILABLE(macos(10.11), ios(9.0));

@property (nonatomic, getter=_allowsMediaDocumentInlinePlayback, setter=_setAllowsMediaDocumentInlinePlayback:) BOOL _allowsMediaDocumentInlinePlayback;

@property (nonatomic, readonly) BOOL _webProcessIsResponsive WK_API_AVAILABLE(macos(10.12), ios(10.0));

@property (nonatomic, setter=_setFullscreenDelegate:) id<_WKFullscreenDelegate> _fullscreenDelegate WK_API_AVAILABLE(macos(10.13), ios(11.0));
@property (nonatomic, readonly) BOOL _isInFullscreen WK_API_AVAILABLE(macos(10.12.3));

@property (nonatomic, readonly) _WKMediaCaptureState _mediaCaptureState WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

- (void)_setPageMuted:(_WKMediaMutedState)mutedState WK_API_AVAILABLE(macos(10.13), ios(11.0));

@property (nonatomic, setter=_setMediaCaptureEnabled:) BOOL _mediaCaptureEnabled WK_API_AVAILABLE(macos(10.13), ios(11.0));

@property (nonatomic, readonly) BOOL _canTogglePictureInPicture;
@property (nonatomic, readonly) BOOL _isPictureInPictureActive;
- (void)_updateMediaPlaybackControlsManager;
- (void)_togglePictureInPicture;
- (void)_stopAllMediaPlayback;
- (void)_suspendAllMediaPlayback;
- (void)_resumeAllMediaPlayback;
- (void)_closeAllMediaPresentations;

- (void)_requestTextInputContextsInRect:(CGRect)rect completionHandler:(void(^)(NSArray<_WKTextInputContext *> *))completionHandler WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));
- (void)_focusTextInputContext:(_WKTextInputContext *)textInputElement completionHandler:(void(^)(BOOL))completionHandler WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

@end

#if TARGET_OS_IPHONE && !TARGET_OS_WATCH
@interface WKWebView (FullScreenAPI_Private)
-(BOOL)hasFullScreenWindowController;
-(void)closeFullScreenWindowController;
@end
#endif

#if TARGET_OS_IPHONE

@interface WKWebView () <UIResponderStandardEditActions>
@end

#else

@interface WKWebView (WKNSTextFinderClient) <NSTextFinderClient>
@end

#endif

@interface WKWebView (WKTesting)

- (NSDictionary *)_contentsOfUserInterfaceItem:(NSString *)userInterfaceItem WK_API_AVAILABLE(macos(10.12.3), ios(10.3));

#if TARGET_OS_IPHONE

@property (nonatomic, readonly) CGRect _contentVisibleRect WK_API_AVAILABLE(ios(10.0));
- (CGPoint)_convertPointFromContentsToView:(CGPoint)point WK_API_AVAILABLE(ios(10.0));
- (CGPoint)_convertPointFromViewToContents:(CGPoint)point WK_API_AVAILABLE(ios(10.0));

- (void)keyboardAccessoryBarNext WK_API_AVAILABLE(ios(10.0));
- (void)keyboardAccessoryBarPrevious WK_API_AVAILABLE(ios(10.0));
- (void)dismissFormAccessoryView WK_API_AVAILABLE(ios(10.3));
- (void)_dismissFilePicker WK_API_AVAILABLE(ios(WK_IOS_TBA));
- (void)selectFormAccessoryPickerRow:(int)rowIndex WK_API_AVAILABLE(ios(10.3));
@property (nonatomic, readonly) NSString *textContentTypeForTesting WK_API_AVAILABLE(ios(12.0));
@property (nonatomic, readonly) NSString *selectFormPopoverTitle WK_API_AVAILABLE(ios(12.0));
@property (nonatomic, readonly) NSString *formInputLabel WK_API_AVAILABLE(ios(12.0));
- (void)setTimePickerValueToHour:(NSInteger)hour minute:(NSInteger)minute WK_API_AVAILABLE(ios(12.0));

- (void)applyAutocorrection:(NSString *)newString toString:(NSString *)oldString withCompletionHandler:(void (^)(void))completionHandler WK_API_AVAILABLE(ios(11.0));

- (void)didStartFormControlInteraction WK_API_AVAILABLE(ios(10.3));
- (void)didEndFormControlInteraction WK_API_AVAILABLE(ios(10.3));

- (void)_didShowForcePressPreview WK_API_AVAILABLE(ios(10.3));
- (void)_didDismissForcePressPreview WK_API_AVAILABLE(ios(10.3));
- (void)_doAfterNextStablePresentationUpdate:(dispatch_block_t)updateBlock WK_API_AVAILABLE(ios(10.3));
- (void)_doAfterResettingSingleTapGesture:(dispatch_block_t)action WK_API_AVAILABLE(ios(WK_IOS_TBA));

@property (nonatomic, readonly) NSArray<NSValue *> *_uiTextSelectionRects WK_API_AVAILABLE(ios(10.3));
@property (nonatomic, readonly) CGRect _uiTextCaretRect WK_API_AVAILABLE(ios(10.3));
@property (nonatomic, readonly) CGRect _inputViewBounds WK_API_AVAILABLE(ios(10.3));

@property (nonatomic, readonly) NSString *_scrollingTreeAsText WK_API_AVAILABLE(ios(10.3));

@property (nonatomic, readonly) NSNumber *_stableStateOverride WK_API_AVAILABLE(ios(10.3));

- (NSDictionary *)_propertiesOfLayerWithID:(unsigned long long)layerID WK_API_AVAILABLE(ios(11.0));
- (void)_simulateLongPressActionAtLocation:(CGPoint)location WK_API_AVAILABLE(ios(11.0));
- (void)_simulateTextEntered:(NSString *)text WK_API_AVAILABLE(ios(12.0));

@property (nonatomic, readonly) CGRect _dragCaretRect WK_API_AVAILABLE(ios(11.0));

- (void)_doAfterReceivingEditDragSnapshotForTesting:(dispatch_block_t)action WK_API_AVAILABLE(ios(WK_IOS_TBA));

- (void)_requestActivatedElementAtPosition:(CGPoint)position completionBlock:(void (^)(_WKActivatedElementInfo *))block WK_API_AVAILABLE(ios(11.0));
- (void)_accessibilityRetrieveRectsAtSelectionOffset:(NSInteger)offset withText:(NSString *)text completionHandler:(void (^)(NSArray<NSValue *> *rects))completionHandler WK_API_AVAILABLE(ios(11.3));
- (void)_accessibilityStoreSelection WK_API_AVAILABLE(ios(11.3));
- (void)_accessibilityClearSelection WK_API_AVAILABLE(ios(11.3));
- (UIView *)_fullScreenPlaceholderView WK_API_AVAILABLE(ios(12.0));

@property (nonatomic, readonly) BOOL _contentViewIsFirstResponder WK_API_AVAILABLE(ios(12.2));
#else
- (void)_dismissContentRelativeChildWindows WK_API_AVAILABLE(macos(10.13.4));
- (void)_setFrame:(NSRect)rect andScrollBy:(NSSize)offset WK_API_AVAILABLE(macos(10.13.4));
- (void)_beginDeferringViewInWindowChanges WK_API_AVAILABLE(macos(10.13.4));
- (void)_endDeferringViewInWindowChanges WK_API_AVAILABLE(macos(10.13.4));
- (void)_endDeferringViewInWindowChangesSync WK_API_AVAILABLE(macos(10.13.4));
- (void)_gestureEventWasNotHandledByWebCore:(NSEvent *)event WK_API_AVAILABLE(macos(10.13.4));
- (void)_setCustomSwipeViews:(NSArray *)customSwipeViews WK_API_AVAILABLE(macos(10.13.4));
- (void)_setCustomSwipeViewsTopContentInset:(float)topContentInset WK_API_AVAILABLE(macos(10.13.4));
- (NSView *)_fullScreenPlaceholderView WK_API_AVAILABLE(macos(10.13.4));
- (NSWindow *)_fullScreenWindow WK_API_AVAILABLE(macos(10.13.4));
- (void)_disableFrameSizeUpdates WK_API_AVAILABLE(macos(10.13.4));
- (void)_enableFrameSizeUpdates WK_API_AVAILABLE(macos(10.13.4));
- (void)_prepareForImmediateActionAnimation WK_API_AVAILABLE(macos(10.13.4));
- (void)_cancelImmediateActionAnimation WK_API_AVAILABLE(macos(10.13.4));
- (void)_completeImmediateActionAnimation WK_API_AVAILABLE(macos(10.13.4));
- (BOOL)_canChangeFrameLayout:(_WKFrameHandle *)frameHandle WK_API_AVAILABLE(macos(10.13.4));
- (BOOL)_tryToSwipeWithEvent:(NSEvent *)event ignoringPinnedState:(BOOL)ignoringPinnedState WK_API_AVAILABLE(macos(10.13.4));
- (void)_setDidMoveSwipeSnapshotCallback:(void(^)(CGRect))callback WK_API_AVAILABLE(macos(10.13.4));
@property (nonatomic, setter=_setTotalHeightOfBanners:) CGFloat _totalHeightOfBanners WK_API_AVAILABLE(macos(10.13.4));
@property (nonatomic, copy, setter=_setUnderlayColor:) NSColor *_underlayColor WK_API_AVAILABLE(macos(10.13.4));
@property (nonatomic, readwrite, setter=_setIgnoresNonWheelEvents:) BOOL _ignoresNonWheelEvents WK_API_AVAILABLE(macos(10.13.4));
@property (nonatomic, readonly) WKPageRef _pageRefForTransitionToWKWebView  WK_API_AVAILABLE(macos(10.13.4));
@property (nonatomic, readonly) BOOL _hasActiveVideoForControlsManager WK_API_AVAILABLE(macos(10.12));
- (void)_requestControlledElementID WK_API_AVAILABLE(macos(10.12.3));
- (void)_handleControlledElementIDResponse:(NSString *)identifier WK_API_AVAILABLE(macos(10.12.3));
- (void)_handleAcceptedCandidate:(NSTextCheckingResult *)candidate WK_API_AVAILABLE(macos(10.12.3));
- (void)_didHandleAcceptedCandidate WK_API_AVAILABLE(macos(10.12.3));
- (void)_forceRequestCandidates WK_API_AVAILABLE(macos(10.12.3));
- (void)_didUpdateCandidateListVisibility:(BOOL)visible WK_API_AVAILABLE(macos(10.12.3));
@property (nonatomic, readonly) BOOL _shouldRequestCandidates WK_API_AVAILABLE(macos(10.12.3));
- (void)_insertText:(id)string replacementRange:(NSRange)replacementRange WK_API_AVAILABLE(macos(10.12.3));
- (NSRect)_candidateRect WK_API_AVAILABLE(macos(10.13));
@property (nonatomic, readwrite, setter=_setUseSystemAppearance:) BOOL _useSystemAppearance WK_API_AVAILABLE(macos(10.14));

- (void)_setHeaderBannerHeight:(int)height WK_API_AVAILABLE(macos(10.12.3));
- (void)_setFooterBannerHeight:(int)height WK_API_AVAILABLE(macos(10.12.3));
- (void)_doAfterProcessingAllPendingMouseEvents:(dispatch_block_t)action WK_API_AVAILABLE(macos(10.14.4));
#endif

- (void)_requestActiveNowPlayingSessionInfo:(void(^)(BOOL, BOOL, NSString*, double, double, NSInteger))callback WK_API_AVAILABLE(macos(10.13.4), ios(11.3));

- (void)_setPageScale:(CGFloat)scale withOrigin:(CGPoint)origin WK_API_AVAILABLE(ios(10.3));
- (CGFloat)_pageScale WK_API_AVAILABLE(ios(10.3));

- (void)_doAfterNextPresentationUpdate:(void (^)(void))updateBlock WK_API_AVAILABLE(macos(10.12), ios(10.0));
- (void)_doAfterNextPresentationUpdateWithoutWaitingForAnimatedResizeForTesting:(void (^)(void))updateBlock WK_API_AVAILABLE(macos(10.14), ios(12.0));
- (void)_doAfterNextPresentationUpdateWithoutWaitingForPainting:(void (^)(void))updateBlock WK_API_AVAILABLE(macos(10.12.3), ios(10.3));
- (void)_doAfterNextVisibleContentRectUpdate:(void (^)(void))updateBlock WK_API_AVAILABLE(macos(10.13), ios(11.0));

- (void)_disableBackForwardSnapshotVolatilityForTesting WK_API_AVAILABLE(macos(10.12.3), ios(10.3));
- (void)_executeEditCommand:(NSString *)command argument:(NSString *)argument completion:(void (^)(BOOL))completion WK_API_AVAILABLE(macos(10.13.4), ios(11.3));

- (void)_denyNextUserMediaRequest WK_API_AVAILABLE(macos(10.14), ios(12.0));

- (BOOL)_beginBackSwipeForTesting;
- (BOOL)_completeBackSwipeForTesting;
- (void)_setDefersLoadingForTesting:(BOOL)defersLoading;

- (void)_setShareSheetCompletesImmediatelyWithResolutionForTesting:(BOOL)resolved WK_API_AVAILABLE(macos(10.14.4), ios(12.2));

@property (nonatomic, readonly) BOOL _hasInspectorFrontend WK_API_AVAILABLE(macos(10.14.4), ios(12.2));
@property (nonatomic, readonly) _WKInspector *_inspector WK_API_AVAILABLE(macos(10.14.4), ios(12.2));
@property (nonatomic, readonly) _WKFrameHandle *_mainFrame WK_API_AVAILABLE(macos(10.14.4), ios(12.2));

@property (nonatomic, setter=_setScrollingUpdatesDisabledForTesting:) BOOL _scrollingUpdatesDisabledForTesting WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

- (void)_processWillSuspendImminentlyForTesting;
- (void)_processDidResumeForTesting;

@end
