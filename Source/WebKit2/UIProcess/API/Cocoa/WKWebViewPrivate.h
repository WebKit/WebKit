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

#import <WebKit2/WKWebView.h>

#if WK_API_ENABLED

typedef NS_OPTIONS(NSUInteger, _WKRenderingProgressEvents) {
    _WKRenderingProgressEventFirstLayout = 1 << 0,
    _WKRenderingProgressEventFirstPaintWithSignificantArea = 1 << 2,
};

typedef NS_ENUM(NSUInteger, _WKPaginationMode) {
    _WKPaginationModeUnpaginated,
    _WKPaginationModeLeftToRight,
    _WKPaginationModeRightToLeft,
    _WKPaginationModeTopToBottom,
    _WKPaginationModeBottomToTop,
};

@class WKBrowsingContextHandle;
@class WKRemoteObjectRegistry;
@protocol WKHistoryDelegatePrivate;

@interface WKWebView (WKPrivate)

@property (nonatomic, readonly) WKRemoteObjectRegistry *_remoteObjectRegistry;
@property (nonatomic, readonly) WKBrowsingContextHandle *_handle;

@property (nonatomic, setter=_setObservedRenderingProgressEvents:) _WKRenderingProgressEvents _observedRenderingProgressEvents;

@property (nonatomic, weak, setter=_setHistoryDelegate:) id <WKHistoryDelegatePrivate> _historyDelegate;

@property (nonatomic, readonly) NSURL *_unreachableURL;

- (void)_loadAlternateHTMLString:(NSString *)string baseURL:(NSURL *)baseURL forUnreachableURL:(NSURL *)unreachableURL;

- (WKNavigation *)_reload;

@property (nonatomic, readonly) NSArray *_certificateChain;
@property (nonatomic, readonly) NSURL *_committedURL;

@property (copy, setter=_setApplicationNameForUserAgent:) NSString *_applicationNameForUserAgent;
@property (copy, setter=_setCustomUserAgent:) NSString *_customUserAgent;

@property (nonatomic, readonly) pid_t _webProcessIdentifier;

@property (nonatomic, readonly) NSData *_sessionState;
- (void)_restoreFromSessionState:(NSData *)sessionState;

@property (nonatomic, setter=_setPrivateBrowsingEnabled:) BOOL _privateBrowsingEnabled;

- (void)_close;

#if TARGET_OS_IPHONE
@property (nonatomic, setter=_setMinimumLayoutSizeOverride:) CGSize _minimumLayoutSizeOverride;

// Define the inset of the scrollview unusable by the web page.
@property (nonatomic, setter=_setObscuredInsets:) UIEdgeInsets _obscuredInsets;

@property (nonatomic, setter=_setBackgroundExtendsBeyondPage:) BOOL _backgroundExtendsBeyondPage;

// This is deprecated and should be removed entirely: <rdar://problem/16294704>.
@property (readonly) UIColor *_pageExtendedBackgroundColor;

- (void)_beginInteractiveObscuredInsetsChange;
- (void)_endInteractiveObscuredInsetsChange;
#else
@property (readonly) NSColor *_pageExtendedBackgroundColor;
@property (nonatomic, setter=_setDrawsTransparentBackground:) BOOL _drawsTransparentBackground;
#endif

- (void)_runJavaScriptInMainFrame:(NSString *)scriptString;

@property (nonatomic, setter=_setPaginationMode:) _WKPaginationMode _paginationMode;
// Whether the column-break-{before,after} properties are respected instead of the
// page-break-{before,after} properties.
@property (nonatomic, setter=_setPaginationBehavesLikeColumns:) BOOL _paginationBehavesLikeColumns;
// Set to 0 to have the page length equal the view length.
@property (nonatomic, setter=_setPageLength:) CGFloat _pageLength;
@property (nonatomic, setter=_setGapBetweenPages:) CGFloat _gapBetweenPages;
@property (readonly) NSUInteger _pageCount;

@end

#endif
