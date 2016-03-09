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
@class _WKVisitedLinkProvider;
@class _WKVisitedLinkStore;
@class _WKWebsiteDataStore;

@interface WKWebViewConfiguration (WKPrivate)

@property (nonatomic, weak, setter=_setRelatedWebView:) WKWebView *_relatedWebView;
@property (nonatomic, copy, setter=_setGroupIdentifier:) NSString *_groupIdentifier;

@property (nonatomic, strong, setter=_setVisitedLinkStore:) _WKVisitedLinkStore *_visitedLinkStore;

@property (nonatomic, weak, setter=_setAlternateWebViewForNavigationGestures:) WKWebView *_alternateWebViewForNavigationGestures;

@property (nonatomic, setter=_setTreatsSHA1SignedCertificatesAsInsecure:) BOOL _treatsSHA1SignedCertificatesAsInsecure WK_AVAILABLE(NA, 9_0);

@property (nonatomic, setter=_setRespectsImageOrientation:) BOOL _respectsImageOrientation WK_AVAILABLE(WK_MAC_TBA, WK_IOS_TBA);
@property (nonatomic, setter=_setPrintsBackgrounds:) BOOL _printsBackgrounds WK_AVAILABLE(WK_MAC_TBA, WK_IOS_TBA);
@property (nonatomic, setter=_setIncrementalRenderingSuppressionTimeout:) CGFloat _incrementalRenderingSuppressionTimeout WK_AVAILABLE(WK_MAC_TBA, WK_IOS_TBA);
@property (nonatomic, setter=_setAllowsJavaScriptMarkup:) BOOL _allowsJavaScriptMarkup WK_AVAILABLE(WK_MAC_TBA, WK_IOS_TBA);
@property (nonatomic, setter=_setConvertsPositionStyleOnCopy:) BOOL _convertsPositionStyleOnCopy WK_AVAILABLE(WK_MAC_TBA, WK_IOS_TBA);
@property (nonatomic, setter=_setAllowsMetaRefresh:) BOOL _allowsMetaRefresh WK_AVAILABLE(WK_MAC_TBA, WK_IOS_TBA);
@property (nonatomic, setter=_setAllowUniversalAccessFromFileURLs:) BOOL _allowUniversalAccessFromFileURLs WK_AVAILABLE(WK_MAC_TBA, WK_IOS_TBA);

#if TARGET_OS_IPHONE
@property (nonatomic, setter=_setAlwaysRunsAtForegroundPriority:) BOOL _alwaysRunsAtForegroundPriority WK_AVAILABLE(NA, 9_0);

@property (nonatomic, setter=_setRequiresUserActionForAudioPlayback:) BOOL _requiresUserActionForAudioPlayback WK_AVAILABLE(NA, WK_IOS_TBA);
@property (nonatomic, setter=_setInlineMediaPlaybackRequiresPlaysInlineAttribute:) BOOL _inlineMediaPlaybackRequiresPlaysInlineAttribute WK_AVAILABLE(NA, WK_IOS_TBA);
@property (nonatomic, setter=_setInvisibleAutoplayNotPermitted:) BOOL _invisibleAutoplayNotPermitted WK_AVAILABLE(NA, WK_IOS_TBA);
@property (nonatomic, setter=_setMediaDataLoadsAutomatically:) BOOL _mediaDataLoadsAutomatically WK_AVAILABLE(NA, WK_IOS_TBA);
#else
@property (nonatomic, setter=_setShowsURLsInToolTips:) BOOL _showsURLsInToolTips WK_AVAILABLE(WK_MAC_TBA, NA);
@property (nonatomic, setter=_setServiceControlsEnabled:) BOOL _serviceControlsEnabled WK_AVAILABLE(WK_MAC_TBA, NA);
@property (nonatomic, setter=_setImageControlsEnabled:) BOOL _imageControlsEnabled WK_AVAILABLE(WK_MAC_TBA, NA);
#endif

@property (nonatomic, strong, setter=_setVisitedLinkProvider:) _WKVisitedLinkProvider *_visitedLinkProvider WK_DEPRECATED(10_10, WK_MAC_TBA, 8_0, WK_IOS_TBA, "Please use _visitedLinkStore instead");

@property (nonatomic, strong, setter=_setWebsiteDataStore:) _WKWebsiteDataStore *_websiteDataStore WK_DEPRECATED(10_10, 10_11, 8_0, 9_0, "Please use websiteDataStore instead");

@end

#endif
