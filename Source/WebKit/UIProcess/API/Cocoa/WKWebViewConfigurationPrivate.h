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

#import <WebKit/WKBase.h>
#import <WebKit/WKWebViewConfiguration.h>

#if TARGET_OS_IPHONE
typedef NS_ENUM(NSUInteger, _WKDragLiftDelay) {
    _WKDragLiftDelayShort = 0,
    _WKDragLiftDelayMedium,
    _WKDragLiftDelayLong
} WK_API_AVAILABLE(ios(11.0));

@protocol _UIClickInteractionDriving;
#endif

typedef NS_ENUM(NSUInteger, _WKWebViewCategory) {
    _WKWebViewCategoryAppBoundDomain,
    _WKWebViewCategoryHybridApp,
    _WKWebViewCategoryInAppBrowser,
    _WKWebViewCategoryWebBrowser
} WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

@class WKWebView;
@class _WKApplicationManifest;
@class _WKVisitedLinkStore;
@class _WKWebsiteDataStore;

@interface WKWebViewConfiguration (WKPrivate)

@property (nonatomic, weak, setter=_setRelatedWebView:) WKWebView *_relatedWebView;
@property (nonatomic, copy, setter=_setGroupIdentifier:) NSString *_groupIdentifier;

@property (nonatomic, strong, setter=_setVisitedLinkStore:) _WKVisitedLinkStore *_visitedLinkStore;

@property (nonatomic, weak, setter=_setAlternateWebViewForNavigationGestures:) WKWebView *_alternateWebViewForNavigationGestures;

@property (nonatomic, setter=_setRespectsImageOrientation:) BOOL _respectsImageOrientation WK_API_AVAILABLE(macos(10.12), ios(10.0));
@property (nonatomic, setter=_setPrintsBackgrounds:) BOOL _printsBackgrounds WK_API_AVAILABLE(macos(10.12), ios(10.0));
@property (nonatomic, setter=_setIncrementalRenderingSuppressionTimeout:) NSTimeInterval _incrementalRenderingSuppressionTimeout WK_API_AVAILABLE(macos(10.12), ios(10.0));
@property (nonatomic, setter=_setAllowsJavaScriptMarkup:) BOOL _allowsJavaScriptMarkup WK_API_AVAILABLE(macos(10.12), ios(10.0));
@property (nonatomic, setter=_setConvertsPositionStyleOnCopy:) BOOL _convertsPositionStyleOnCopy WK_API_AVAILABLE(macos(10.12), ios(10.0));
@property (nonatomic, setter=_setAllowsMetaRefresh:) BOOL _allowsMetaRefresh WK_API_AVAILABLE(macos(10.12), ios(10.0));
@property (nonatomic, setter=_setAllowUniversalAccessFromFileURLs:) BOOL _allowUniversalAccessFromFileURLs WK_API_AVAILABLE(macos(10.12), ios(10.0));
@property (nonatomic, setter=_setAllowTopNavigationToDataURLs:) BOOL _allowTopNavigationToDataURLs WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));
@property (nonatomic, setter=_setNeedsStorageAccessFromFileURLsQuirk:) BOOL _needsStorageAccessFromFileURLsQuirk WK_API_AVAILABLE(macos(10.12.3), ios(10.3));
@property (nonatomic, setter=_setMainContentUserGestureOverrideEnabled:) BOOL _mainContentUserGestureOverrideEnabled WK_API_AVAILABLE(macos(10.12), ios(10.0));
@property (nonatomic, setter=_setInvisibleAutoplayNotPermitted:) BOOL _invisibleAutoplayNotPermitted WK_API_AVAILABLE(macos(10.12), ios(10.0));
@property (nonatomic, setter=_setMediaDataLoadsAutomatically:) BOOL _mediaDataLoadsAutomatically WK_API_AVAILABLE(macos(10.12), ios(10.0));
@property (nonatomic, setter=_setAttachmentElementEnabled:) BOOL _attachmentElementEnabled WK_API_AVAILABLE(macos(10.12), ios(10.0));
@property (nonatomic, setter=_setAttachmentFileWrapperClass:) Class _attachmentFileWrapperClass WK_API_AVAILABLE(macos(10.14.4), ios(12.2));
@property (nonatomic, setter=_setInitialCapitalizationEnabled:) BOOL _initialCapitalizationEnabled WK_API_AVAILABLE(macos(10.12), ios(10.0));
@property (nonatomic, setter=_setApplePayEnabled:) BOOL _applePayEnabled WK_API_AVAILABLE(macos(10.12), ios(10.0));
@property (nonatomic, setter=_setWaitsForPaintAfterViewDidMoveToWindow:) BOOL _waitsForPaintAfterViewDidMoveToWindow WK_API_AVAILABLE(macos(10.12.3), ios(10.3));
@property (nonatomic, setter=_setControlledByAutomation:, getter=_isControlledByAutomation) BOOL _controlledByAutomation WK_API_AVAILABLE(macos(10.12.3), ios(10.3));
@property (nonatomic, setter=_setApplicationManifest:) _WKApplicationManifest *_applicationManifest WK_API_AVAILABLE(macos(10.13.4), ios(11.3));
@property (nonatomic, setter=_setColorFilterEnabled:) BOOL _colorFilterEnabled WK_API_AVAILABLE(macos(10.14), ios(12.0));
@property (nonatomic, setter=_setIncompleteImageBorderEnabled:) BOOL _incompleteImageBorderEnabled WK_API_AVAILABLE(macos(10.14), ios(12.0));
@property (nonatomic, setter=_setDrawsBackground:) BOOL _drawsBackground WK_API_AVAILABLE(macos(10.14), ios(12.0));
@property (nonatomic, setter=_setShouldDeferAsynchronousScriptsUntilAfterDocumentLoad:) BOOL _shouldDeferAsynchronousScriptsUntilAfterDocumentLoad WK_API_AVAILABLE(macos(10.14), ios(12.0));

@property (nonatomic, readonly) WKWebsiteDataStore *_websiteDataStoreIfExists WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));
@property (nonatomic, copy, setter=_setCORSDisablingPatterns:) NSArray<NSString *> *_corsDisablingPatterns WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));
@property (nonatomic, setter=_setDeferrableUserScriptsShouldWaitUntilNotification:) BOOL _deferrableUserScriptsShouldWaitUntilNotification WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));
@property (nonatomic, setter=_setCrossOriginAccessControlCheckEnabled:) BOOL _crossOriginAccessControlCheckEnabled WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

@property (nonatomic, setter=_setLoadsFromNetwork:) BOOL _loadsFromNetwork WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));
@property (nonatomic, setter=_setLoadsSubresources:) BOOL _loadsSubresources WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

#if TARGET_OS_IPHONE
@property (nonatomic, setter=_setClientNavigationsRunAtForegroundPriority:) BOOL _clientNavigationsRunAtForegroundPriority WK_API_AVAILABLE(ios(WK_IOS_TBA));
@property (nonatomic, setter=_setAlwaysRunsAtForegroundPriority:) BOOL _alwaysRunsAtForegroundPriority WK_API_DEPRECATED_WITH_REPLACEMENT("_clientNavigationsRunAtForegroundPriority", ios(9.0, WK_IOS_TBA));
@property (nonatomic, setter=_setInlineMediaPlaybackRequiresPlaysInlineAttribute:) BOOL _inlineMediaPlaybackRequiresPlaysInlineAttribute WK_API_AVAILABLE(ios(10.0));
@property (nonatomic, setter=_setAllowsInlineMediaPlaybackAfterFullscreen:) BOOL _allowsInlineMediaPlaybackAfterFullscreen  WK_API_AVAILABLE(ios(10.0));
@property (nonatomic, setter=_setDragLiftDelay:) _WKDragLiftDelay _dragLiftDelay WK_API_AVAILABLE(ios(11.0));
@property (nonatomic, setter=_setTextInteractionGesturesEnabled:) BOOL _textInteractionGesturesEnabled WK_API_AVAILABLE(ios(12.0));
@property (nonatomic, setter=_setLongPressActionsEnabled:) BOOL _longPressActionsEnabled WK_API_AVAILABLE(ios(12.0));
@property (nonatomic, setter=_setSystemPreviewEnabled:) BOOL _systemPreviewEnabled WK_API_AVAILABLE(ios(12.0));
@property (nonatomic, setter=_setShouldDecidePolicyBeforeLoadingQuickLookPreview:) BOOL _shouldDecidePolicyBeforeLoadingQuickLookPreview WK_API_AVAILABLE(ios(13.0));
@property (nonatomic, setter=_setCanShowWhileLocked:) BOOL _canShowWhileLocked WK_API_AVAILABLE(ios(13.0));
@property (nonatomic, setter=_setClickInteractionDriverForTesting:) id <_UIClickInteractionDriving> _clickInteractionDriverForTesting WK_API_AVAILABLE(ios(13.0));
#else
@property (nonatomic, setter=_setShowsURLsInToolTips:) BOOL _showsURLsInToolTips WK_API_AVAILABLE(macos(10.12));
@property (nonatomic, setter=_setServiceControlsEnabled:) BOOL _serviceControlsEnabled WK_API_AVAILABLE(macos(10.12));
@property (nonatomic, setter=_setImageControlsEnabled:) BOOL _imageControlsEnabled WK_API_AVAILABLE(macos(10.12));
@property (nonatomic, readwrite, setter=_setRequiresUserActionForEditingControlsManager:) BOOL _requiresUserActionForEditingControlsManager WK_API_AVAILABLE(macos(10.12));
@property (nonatomic, readwrite, setter=_setCPULimit:) double _cpuLimit WK_API_AVAILABLE(macos(10.13.4));
@property (nonatomic, readwrite, setter=_setPageGroup:) WKPageGroupRef _pageGroup WK_API_AVAILABLE(macos(10.13.4));
#endif

@property (nonatomic, strong, setter=_setWebsiteDataStore:) _WKWebsiteDataStore *_websiteDataStore WK_API_DEPRECATED_WITH_REPLACEMENT("websiteDataStore", macos(10.10, 10.11), ios(8.0, 9.0));
@property (nonatomic, setter=_setRequiresUserActionForAudioPlayback:) BOOL _requiresUserActionForAudioPlayback WK_API_DEPRECATED_WITH_REPLACEMENT("mediaTypesRequiringUserActionForPlayback", macos(10.12, 10.12), ios(10.0, 10.0));
@property (nonatomic, setter=_setRequiresUserActionForVideoPlayback:) BOOL _requiresUserActionForVideoPlayback WK_API_DEPRECATED_WITH_REPLACEMENT("mediaTypesRequiringUserActionForPlayback", macos(10.12, 10.12), ios(10.0, 10.0));

@property (nonatomic, setter=_setOverrideContentSecurityPolicy:) NSString *_overrideContentSecurityPolicy WK_API_AVAILABLE(macos(10.12.3), ios(10.3));
@property (nonatomic, setter=_setMediaContentTypesRequiringHardwareSupport:) NSString *_mediaContentTypesRequiringHardwareSupport WK_API_AVAILABLE(macos(10.13), ios(11.0));
@property (nonatomic, setter=_setLegacyEncryptedMediaAPIEnabled:) BOOL _legacyEncryptedMediaAPIEnabled WK_API_AVAILABLE(macos(10.13), ios(11.0));
@property (nonatomic, setter=_setAllowMediaContentTypesRequiringHardwareSupportAsFallback:) BOOL _allowMediaContentTypesRequiringHardwareSupportAsFallback WK_API_AVAILABLE(macos(10.13), ios(11.0));

// The input of this SPI is an array of image UTI (Uniform Type Identifier).
@property (nonatomic, copy, setter=_setAdditionalSupportedImageTypes:) NSArray<NSString *> *_additionalSupportedImageTypes WK_API_AVAILABLE(macos(10.14.4), ios(12.2));

@property (nonatomic, setter=_setEditableImagesEnabled:) BOOL _editableImagesEnabled WK_API_AVAILABLE(macos(10.14.4), ios(12.2));
@property (nonatomic, setter=_setUndoManagerAPIEnabled:) BOOL _undoManagerAPIEnabled WK_API_AVAILABLE(macos(10.15), ios(13.0));
@property (nonatomic, setter=_setWebViewCategory:) _WKWebViewCategory _webViewCategory WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));
@property (nonatomic, setter=_setIgnoresAppBoundDomains:) BOOL _ignoresAppBoundDomains WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));
@property (nonatomic, setter=_setShouldRelaxThirdPartyCookieBlocking:) BOOL _shouldRelaxThirdPartyCookieBlocking WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));
@property (nonatomic, setter=_setProcessDisplayName:) NSString *_processDisplayName WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

@end
