/*
 * Copyright (C) 2014-2016 Apple Inc. All rights reserved.
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

#import <Cocoa/Cocoa.h>

extern NSString * const kUserAgentChangedNotificationName;

@interface SettingsController : NSObject

- (instancetype)initWithMenu:(NSMenu *)menu;

@property (nonatomic, readonly) BOOL useWebKit2ByDefault;
@property (nonatomic, readonly) BOOL createEditorByDefault;
@property (nonatomic, readonly) BOOL useTransparentWindows;
@property (nonatomic, readonly) BOOL usePaginatedMode;
@property (nonatomic, readonly) BOOL layerBordersVisible;
@property (nonatomic, readonly) BOOL legacyLineLayoutVisualCoverageEnabled;
@property (nonatomic, readonly) BOOL incrementalRenderingSuppressed;
@property (nonatomic, readonly) BOOL tiledScrollingIndicatorVisible;
@property (nonatomic, readonly, getter=isSpaceReservedForBanners) BOOL spaceReservedForBanners;
@property (nonatomic, readonly) BOOL resourceUsageOverlayVisible;
@property (nonatomic, readonly) BOOL nonFastScrollableRegionOverlayVisible;
@property (nonatomic, readonly) BOOL wheelEventHandlerRegionOverlayVisible;
@property (nonatomic, readonly) BOOL interactionRegionOverlayVisible;
@property (nonatomic, readonly) BOOL useUISideCompositing;
@property (nonatomic, readonly) BOOL perWindowWebProcessesDisabled;
@property (nonatomic, readonly) BOOL acceleratedDrawingEnabled;
@property (nonatomic, readonly) BOOL displayListDrawingEnabled;
@property (nonatomic, readonly) BOOL resourceLoadStatisticsEnabled;
@property (nonatomic, readonly) BOOL largeImageAsyncDecodingEnabled;
@property (nonatomic, readonly) BOOL animatedImageAsyncDecodingEnabled;
@property (nonatomic, readonly) BOOL appleColorFilterEnabled;
@property (nonatomic, readonly) BOOL punchOutWhiteBackgroundsInDarkMode;
@property (nonatomic, readonly) BOOL useSystemAppearance;
@property (nonatomic, readonly) BOOL loadsAllSiteIcons;
@property (nonatomic, readonly) BOOL usesGameControllerFramework;
@property (nonatomic, readonly) BOOL networkCacheSpeculativeRevalidationDisabled;
@property (nonatomic, readonly) BOOL processSwapOnWindowOpenWithOpenerEnabled;

@property (nonatomic, readonly) NSString *defaultURL;
@property (nonatomic, readonly) NSString *customUserAgent;

@property (nonatomic) BOOL webViewFillsWindow;

@end
