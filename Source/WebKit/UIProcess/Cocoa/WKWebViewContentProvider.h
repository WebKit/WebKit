/*
 * Copyright (C) 2014, 2018 Apple Inc. All rights reserved.
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

#import <WebKit/WKFoundation.h>

#if PLATFORM(IOS_FAMILY)

#import <WebKit/WKPageLoadTypes.h>
#import <WebKit/_WKFindOptions.h>

@class NSData;
@class UIEvent;
@class UIScrollView;
@class UIView;
@class WKWebView;
@protocol NSObject;
struct CGSize;

#ifdef FOUNDATION_HAS_DIRECTIONAL_GEOMETRY
typedef NSEdgeInsets UIEdgeInsets;
#else
struct UIEdgeInsets;
#endif

@protocol WKWebViewContentProvider <NSObject>

- (instancetype)web_initWithFrame:(CGRect)frame webView:(WKWebView *)webView mimeType:(NSString *)mimeType __attribute__((objc_method_family(init)));
- (void)web_setContentProviderData:(NSData *)data suggestedFilename:(NSString *)filename;
- (void)web_setMinimumSize:(CGSize)size;
- (void)web_setOverlaidAccessoryViewsInset:(CGSize)inset;
- (void)web_computedContentInsetDidChange;
- (void)web_setFixedOverlayView:(UIView *)fixedOverlayView;
- (void)web_didSameDocumentNavigation:(WKSameDocumentNavigationType)navigationType;
- (void)web_countStringMatches:(NSString *)string options:(_WKFindOptions)options maxCount:(NSUInteger)maxCount;
- (void)web_findString:(NSString *)string options:(_WKFindOptions)options maxCount:(NSUInteger)maxCount;
- (void)web_hideFindUI;
@property (nonatomic, readonly) UIView *web_contentView;
@property (nonatomic, readonly, class) BOOL web_requiresCustomSnapshotting;

@optional
- (void)web_scrollViewDidScroll:(UIScrollView *)scrollView;
- (void)web_scrollViewWillBeginZooming:(UIScrollView *)scrollView withView:(UIView *)view;
- (void)web_scrollViewDidEndZooming:(UIScrollView *)scrollView withView:(UIView *)view atScale:(CGFloat)scale;
- (void)web_scrollViewDidZoom:(UIScrollView *)scrollView;
- (void)web_beginAnimatedResizeWithUpdates:(void (^)(void))updateBlock;
- (BOOL)web_handleKeyEvent:(UIEvent *)event;
- (void)web_snapshotRectInContentViewCoordinates:(CGRect)contentViewCoordinates snapshotWidth:(CGFloat)snapshotWidth completionHandler:(void (^)(CGImageRef))completionHandler;
@property (nonatomic, readonly) NSData *web_dataRepresentation;
@property (nonatomic, readonly) NSString *web_suggestedFilename;
@property (nonatomic, readonly) BOOL web_isBackground;

@end

#endif // PLATFORM(IOS_FAMILY)

