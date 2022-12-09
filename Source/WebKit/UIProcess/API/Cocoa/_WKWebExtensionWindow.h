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

/*!
 @abstract Constants used by @link WKWebExtensionWindow @/link to indicate the type of a window.
 @constant WKWebExtensionWindowTypeNormal  Indicates a normal window.
 @constant WKWebExtensionWindowTypePopup  Indicates a popup window.
 */
typedef NS_ENUM(NSInteger, _WKWebExtensionWindowType) {
    _WKWebExtensionWindowTypeNormal,
    _WKWebExtensionWindowTypePopup,
} WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

/*!
 @abstract Constants used by @link WKWebExtensionWindow @/link to indicate possible states of a window.
 @constant WKWebExtensionWindowStateNormal  Indicates a window is in its normal state.
 @constant WKWebExtensionWindowStateMinimized  Indicates a window is minimized.
 @constant WKWebExtensionWindowStateMaximized  Indicates a window is maximized.
 @constant WKWebExtensionWindowStateFullscreen  Indicates a window is in fullscreen mode.
 */
typedef NS_ENUM(NSInteger, _WKWebExtensionWindowState) {
    _WKWebExtensionWindowStateNormal,
    _WKWebExtensionWindowStateMinimized,
    _WKWebExtensionWindowStateMaximized,
    _WKWebExtensionWindowStateFullscreen,
} WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

/*! @abstract A class conforming to the `WKWebExtensionWindow` protocol represents a window to web extensions. */
WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
@protocol _WKWebExtensionWindow <NSObject>
@optional

/*!
 @abstract Called when an array of tabs is needed for the window.
 @param context The context in which the web extension is running.
 @return An array of tabs in the window.
 @discussion Defaults to an empty array if not implemented.
 */
- (NSArray<id <_WKWebExtensionTab>> *)tabsForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called when the active tab is needed for the window.
 @param context The context in which the web extension is running.
 @return The active tab in the window.
 @discussion Defaults to `nil` if not implemented.
 */
- (id <_WKWebExtensionTab>)activeTabForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called when the type of the window is needed.
 @param context The context in which the web extension is running.
 @return The type of the window.
 @discussion Defaults to`WKWebExtensionWindowTypeNormal` if not implemented.
 */
- (_WKWebExtensionWindowType)windowTypeForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called when the state of the window is needed.
 @param context The context in which the web extension is running.
 @return The state of the window.
 @discussion Defaults to`WKWebExtensionWindowStateNormal` if not implemented.
 */
- (_WKWebExtensionWindowState)windowStateForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called when the focused state of the window is needed.
 @param context The context in which the web extension is running.
 @return `YES` if the window is focused, `NO` otherwise.
 @discussion Defaults to `NO` if not implemented.
 */
- (BOOL)isFocusedForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called when the ephemeral state of the window is needed.
 @param context The context in which the web extension is running.
 @return `YES` if the window is ephemeral, `NO` otherwise.
 @discussion Used to indicated "private browsing" windows. Defaults to `NO` if not implemented.
 */
- (BOOL)isEphemeralForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called when the frame of the window is needed.
 @param context The context in which the web extension is running.
 @return The frame of the window.
 @discussion The frame is the bounding rectangle of the window, in screen coordinates. Defaults to `CGRectZero` if not implemented.
 */
- (CGRect)frameForWebExtensionContext:(_WKWebExtensionContext *)context;

@end

NS_ASSUME_NONNULL_END
