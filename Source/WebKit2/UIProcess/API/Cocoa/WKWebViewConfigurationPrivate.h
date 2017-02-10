/*
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
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

#import <WebKit/WKWebViewConfiguration.h>

#if WK_API_ENABLED

@class WKWebView;
@class _WKVisitedLinkStore;
@class _WKWebsiteDataStore;

@interface WKWebViewConfiguration (WKPrivate)

@property (nonatomic, weak, setter=_setRelatedWebView:) WKWebView *_relatedWebView;
@property (nonatomic, copy, setter=_setGroupIdentifier:) NSString *_groupIdentifier;

@property (nonatomic, strong, setter=_setVisitedLinkStore:) _WKVisitedLinkStore *_visitedLinkStore;

@property (nonatomic, weak, setter=_setAlternateWebViewForNavigationGestures:) WKWebView *_alternateWebViewForNavigationGestures;

@property (nonatomic, setter=_setTreatsSHA1SignedCertificatesAsInsecure:) BOOL _treatsSHA1SignedCertificatesAsInsecure WK_API_AVAILABLE(ios(9_0));

@property (nonatomic, setter=_setRespectsImageOrientation:) BOOL _respectsImageOrientation WK_API_AVAILABLE(macosx(10.12), ios(10.0));
@property (nonatomic, setter=_setPrintsBackgrounds:) BOOL _printsBackgrounds WK_API_AVAILABLE(macosx(10.12), ios(10.0));
@property (nonatomic, setter=_setIncrementalRenderingSuppressionTimeout:) NSTimeInterval _incrementalRenderingSuppressionTimeout WK_API_AVAILABLE(macosx(10.12), ios(10.0));
@property (nonatomic, setter=_setAllowsJavaScriptMarkup:) BOOL _allowsJavaScriptMarkup WK_API_AVAILABLE(macosx(10.12), ios(10.0));
@property (nonatomic, setter=_setConvertsPositionStyleOnCopy:) BOOL _convertsPositionStyleOnCopy WK_API_AVAILABLE(macosx(10.12), ios(10.0));
@property (nonatomic, setter=_setAllowsMetaRefresh:) BOOL _allowsMetaRefresh WK_API_AVAILABLE(macosx(10.12), ios(10.0));
@property (nonatomic, setter=_setAllowUniversalAccessFromFileURLs:) BOOL _allowUniversalAccessFromFileURLs WK_API_AVAILABLE(macosx(10.12), ios(10.0));
@property (nonatomic, setter=_setNeedsStorageAccessFromFileURLsQuirk:) BOOL _needsStorageAccessFromFileURLsQuirk WK_API_AVAILABLE(macosx(WK_MAC_TBA), ios(WK_IOS_TBA));
@property (nonatomic, setter=_setMainContentUserGestureOverrideEnabled:) BOOL _mainContentUserGestureOverrideEnabled WK_API_AVAILABLE(macosx(10.12), ios(10.0));
@property (nonatomic, setter=_setInvisibleAutoplayNotPermitted:) BOOL _invisibleAutoplayNotPermitted WK_API_AVAILABLE(macosx(10.12), ios(10.0));
@property (nonatomic, setter=_setMediaDataLoadsAutomatically:) BOOL _mediaDataLoadsAutomatically WK_API_AVAILABLE(macosx(10.12), ios(10.0));
@property (nonatomic, setter=_setAttachmentElementEnabled:) BOOL _attachmentElementEnabled WK_API_AVAILABLE(macosx(10.12), ios(10.0));
@property (nonatomic, setter=_setInitialCapitalizationEnabled:) BOOL _initialCapitalizationEnabled WK_API_AVAILABLE(macosx(10.12), ios(10.0));
@property (nonatomic, setter=_setApplePayEnabled:) BOOL _applePayEnabled WK_API_AVAILABLE(macosx(10.12), ios(10.0));
@property (nonatomic, setter=_setWaitsForPaintAfterViewDidMoveToWindow:) BOOL _waitsForPaintAfterViewDidMoveToWindow WK_API_AVAILABLE(macosx(WK_MAC_TBA), ios(WK_IOS_TBA));
@property (nonatomic, setter=_setControlledByAutomation:, getter=_isControlledByAutomation) BOOL _controlledByAutomation WK_API_AVAILABLE(macosx(WK_MAC_TBA), ios(WK_IOS_TBA));
@property (nonatomic, setter=_setMediaStreamEnabled:) BOOL _mediaStreamEnabled WK_API_AVAILABLE(macosx(WK_MAC_TBA), ios(WK_IOS_TBA));

#if TARGET_OS_IPHONE
@property (nonatomic, setter=_setAlwaysRunsAtForegroundPriority:) BOOL _alwaysRunsAtForegroundPriority WK_API_AVAILABLE(ios(9_0));
@property (nonatomic, setter=_setInlineMediaPlaybackRequiresPlaysInlineAttribute:) BOOL _inlineMediaPlaybackRequiresPlaysInlineAttribute WK_API_AVAILABLE(ios(10.0));
@property (nonatomic, setter=_setAllowsInlineMediaPlaybackAfterFullscreen:) BOOL _allowsInlineMediaPlaybackAfterFullscreen  WK_API_AVAILABLE(ios(10.0));
#else
@property (nonatomic, setter=_setShowsURLsInToolTips:) BOOL _showsURLsInToolTips WK_API_AVAILABLE(macosx(10.12));
@property (nonatomic, setter=_setServiceControlsEnabled:) BOOL _serviceControlsEnabled WK_API_AVAILABLE(macosx(10.12));
@property (nonatomic, setter=_setImageControlsEnabled:) BOOL _imageControlsEnabled WK_API_AVAILABLE(macosx(10.12));
@property (nonatomic, readwrite, setter=_setRequiresUserActionForEditingControlsManager:) BOOL _requiresUserActionForEditingControlsManager WK_API_AVAILABLE(macosx(10.12));
#endif

@property (nonatomic, strong, setter=_setWebsiteDataStore:) _WKWebsiteDataStore *_websiteDataStore WK_API_DEPRECATED_WITH_REPLACEMENT("websiteDataStore", macosx(10.10, 10.11), ios(8.0, 9.0));
@property (nonatomic, setter=_setRequiresUserActionForAudioPlayback:) BOOL _requiresUserActionForAudioPlayback WK_API_DEPRECATED_WITH_REPLACEMENT("mediaTypesRequiringUserActionForPlayback", macosx(10.12, 10.12), ios(10.0, 10.0));
@property (nonatomic, setter=_setRequiresUserActionForVideoPlayback:) BOOL _requiresUserActionForVideoPlayback WK_API_DEPRECATED_WITH_REPLACEMENT("mediaTypesRequiringUserActionForPlayback", macosx(10.12, 10.12), ios(10.0, 10.0));

@property (nonatomic, setter=_setOverrideContentSecurityPolicy:) NSString *_overrideContentSecurityPolicy WK_API_AVAILABLE(macosx(WK_MAC_TBA), ios(WK_IOS_TBA));

@end

#endif
