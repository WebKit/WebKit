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

@class _WKWebExtensionContext;
@protocol _WKWebExtensionTab;

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, _WKWebExtensionWindowType) {
    _WKWebExtensionWindowTypeNormal,
    _WKWebExtensionWindowTypePopup,
} WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

typedef NS_ENUM(NSInteger, _WKWebExtensionWindowState) {
    _WKWebExtensionWindowStateNormal,
    _WKWebExtensionWindowStateMinimized,
    _WKWebExtensionWindowStateMaximized,
    _WKWebExtensionWindowStateFullscreen,
} WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
@protocol _WKWebExtensionWindow <NSObject>
@optional

- (NSArray<id <_WKWebExtensionTab>> *)tabsForWebExtensionContext:(_WKWebExtensionContext *)context;
- (id <_WKWebExtensionTab>)activeTabForWebExtensionContext:(_WKWebExtensionContext *)context;

- (_WKWebExtensionWindowType)windowTypeForWebExtensionContext:(_WKWebExtensionContext *)context;
- (_WKWebExtensionWindowState)windowStateForWebExtensionContext:(_WKWebExtensionContext *)context;

- (BOOL)isFocusedForWebExtensionContext:(_WKWebExtensionContext *)context;
- (BOOL)isEphemeralForWebExtensionContext:(_WKWebExtensionContext *)context;
- (BOOL)isPopupWindowForWebExtensionContext:(_WKWebExtensionContext *)context;

- (NSRect)frameForWebExtensionContext:(_WKWebExtensionContext *)context;

@end

NS_ASSUME_NONNULL_END
