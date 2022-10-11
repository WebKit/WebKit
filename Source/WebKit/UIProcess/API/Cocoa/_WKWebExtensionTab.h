/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#import <Foundation/Foundation.h>

@class WKWebView;
@class _WKWebExtensionContext;
@protocol _WKWebExtensionWindow;

NS_ASSUME_NONNULL_BEGIN

typedef NS_OPTIONS(NSUInteger, _WKWebExtensionTabChangedProperties) {
    _WKWebExtensionTabChangedPropertiesNone       = 0,
    _WKWebExtensionTabChangedPropertiesLoading    = 1 << 1,
    _WKWebExtensionTabChangedPropertiesTitle      = 1 << 2,
    _WKWebExtensionTabChangedPropertiesURL        = 1 << 3,
    _WKWebExtensionTabChangedPropertiesSize       = 1 << 4,
    _WKWebExtensionTabChangedPropertiesZoomFactor = 1 << 5,
    _WKWebExtensionTabChangedPropertiesAudible    = 1 << 6,
    _WKWebExtensionTabChangedPropertiesMuted      = 1 << 7,
    _WKWebExtensionTabChangedPropertiesPinned     = 1 << 8,
    _WKWebExtensionTabChangedPropertiesReaderMode = 1 << 9,
    _WKWebExtensionTabChangedPropertiesAll        = NSUIntegerMax,
} WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
@protocol _WKWebExtensionTab <NSObject>
@optional

- (id <_WKWebExtensionTab>)parentTabForWebExtensionContext:(_WKWebExtensionContext *)context;
- (id <_WKWebExtensionWindow>)windowForWebExtensionContext:(_WKWebExtensionContext *)context;

- (WKWebView *)mainWebViewForWebExtensionContext:(_WKWebExtensionContext *)context;
- (NSArray<WKWebView *> *)webViewsForWebExtensionContext:(_WKWebExtensionContext *)context;

- (NSString *)tabTitleForWebExtensionContext:(_WKWebExtensionContext *)context;

- (BOOL)isSelectedForWebExtensionContext:(_WKWebExtensionContext *)context;
- (BOOL)isPinnedTabForWebExtensionContext:(_WKWebExtensionContext *)context;
- (BOOL)isEphemeralForWebExtensionContext:(_WKWebExtensionContext *)context;

- (BOOL)isReaderModeAvailableForWebExtensionContext:(_WKWebExtensionContext *)context;
- (BOOL)isShowingReaderModeForWebExtensionContext:(_WKWebExtensionContext *)context;

- (void)toggleReaderModeForWebExtensionContext:(_WKWebExtensionContext *)context;

- (BOOL)isAudibleForWebExtensionContext:(_WKWebExtensionContext *)context;
- (BOOL)isMutedForWebExtensionContext:(_WKWebExtensionContext *)context;

- (void)muteForWebExtensionContext:(_WKWebExtensionContext *)context;
- (void)unmuteForWebExtensionContext:(_WKWebExtensionContext *)context;

- (CGSize)sizeForWebExtensionContext:(_WKWebExtensionContext *)context;
- (double)zoomFactorForWebExtensionContext:(_WKWebExtensionContext *)context;

- (NSURL *)urlForWebExtensionContext:(_WKWebExtensionContext *)context;
- (NSURL *)pendingURLForWebExtensionContext:(_WKWebExtensionContext *)context;

- (BOOL)isLoadingCompleteForWebExtensionContext:(_WKWebExtensionContext *)context;

- (void)detectWebpageLocaleForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSLocale * _Nullable locale))completionHandler;

- (void)loadURL:(NSURL *)url forWebExtensionContext:(_WKWebExtensionContext *)context NS_SWIFT_NAME(load(url:forWebExtensionContext:));
- (void)reloadForWebExtensionContext:(_WKWebExtensionContext *)context;
- (void)reloadFromOriginForWebExtensionContext:(_WKWebExtensionContext *)context;

- (void)goBackForWebExtensionContext:(_WKWebExtensionContext *)context;
- (void)goForwardForWebExtensionContext:(_WKWebExtensionContext *)context;

- (void)closeForWebExtensionContext:(_WKWebExtensionContext *)context;

- (void)selectForWebExtensionContext:(_WKWebExtensionContext *)context;

@end

NS_ASSUME_NONNULL_END
