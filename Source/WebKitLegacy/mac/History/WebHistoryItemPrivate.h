/*
 * Copyright (C) 2005, 2006, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <WebKitLegacy/WebHistoryItem.h>

#if TARGET_OS_IPHONE
extern NSString *WebViewportInitialScaleKey;
extern NSString *WebViewportMinimumScaleKey;
extern NSString *WebViewportMaximumScaleKey;
extern NSString *WebViewportUserScalableKey;
extern NSString *WebViewportShrinkToFitKey;
extern NSString *WebViewportFitKey;
extern NSString *WebViewportWidthKey;
extern NSString *WebViewportHeightKey;

extern NSString *WebViewportFitAutoValue;
extern NSString *WebViewportFitContainValue;
extern NSString *WebViewportFitCoverValue;
#endif

@interface WebHistoryItem (WebPrivate)

#if !TARGET_OS_IPHONE
+ (void)_releaseAllPendingPageCaches;
#endif

- (id)initWithURL:(NSURL *)URL title:(NSString *)title;

- (NSURL *)URL;
- (BOOL)lastVisitWasFailure;

- (NSString *)RSSFeedReferrer;
- (void)setRSSFeedReferrer:(NSString *)referrer;

- (NSArray *)_redirectURLs;

- (NSString *)target;
- (BOOL)isTargetItem;
- (NSArray *)children;
- (NSDictionary *)dictionaryRepresentation;
#if TARGET_OS_IPHONE
- (NSDictionary *)dictionaryRepresentationIncludingChildren:(BOOL)includesChildren;
#endif

#if TARGET_OS_IPHONE
- (void)_setScale:(float)scale isInitial:(BOOL)aFlag;
- (float)_scale;
- (BOOL)_scaleIsInitial;
- (NSDictionary *)_viewportArguments;
- (void)_setViewportArguments:(NSDictionary *)arguments;
- (CGPoint)_scrollPoint;
- (void)_setScrollPoint:(CGPoint)scrollPoint;
#endif

- (BOOL)_isInPageCache;
- (BOOL)_hasCachedPageExpired;

@end
