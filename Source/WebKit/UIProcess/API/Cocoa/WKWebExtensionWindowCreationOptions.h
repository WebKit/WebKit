/*
 * Copyright (C) 2023-2024 Apple Inc. All rights reserved.
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

#import <Foundation/Foundation.h>
#import <WebKit/WKFoundation.h>

#import <WebKit/WKWebExtensionWindow.h>

@protocol WKWebExtensionTab;

WK_HEADER_AUDIT_BEGIN(nullability, sendability)

/*!
 @abstract A `WKWebExtensionWindowCreationOptions` object encapsulates new window creation options for an extension.
 @discussion This class holds the various options that influence the behavior and initial state of a newly created window.
 The app retains the discretion to disregard any or all of these options, or even opt not to create a new window.
 */
WK_CLASS_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA), visionos(WK_XROS_TBA))
WK_SWIFT_UI_ACTOR NS_SWIFT_NAME(WKWebExtension.WindowCreationOptions)
@interface WKWebExtensionWindowCreationOptions : NSObject

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

/*! @abstract Indicates the window type for the new window. */
@property (nonatomic, readonly) WKWebExtensionWindowType windowType;

/*! @abstract Indicates the window state for the new window. */
@property (nonatomic, readonly) WKWebExtensionWindowState windowState;

/*!
 @abstract Indicates the frame where the new window should be positioned on the main screen.
 @discussion This frame should override the app's default window position and size.
 Individual components (e.g., `origin.x`, `size.width`) will be `NaN` if not specified.
 */
@property (nonatomic, readonly) CGRect frame;

/*!
 @abstract Indicates a list of URLs that the new window should initially load as new tabs.
 @discussion If the array is empty, and `tabs` is empty, the app's default "start page" should appear in a new tab.
 @seealso tabs
 */
@property (nonatomic, readonly, copy) NSArray<NSURL *> *tabURLs;

/*!
 @abstract Indicates a list of existing tabs that should be moved to the new window.
 @discussion If the array is empty, and `tabURLs` is empty, the app's default "start page" should appear in a new tab.
 @seealso URLs
 */
@property (nonatomic, readonly, copy) NSArray<id <WKWebExtensionTab>> *tabs;

/*! @abstract Indicates whether the new window should be focused. */
@property (nonatomic, readonly, getter=isFocused) BOOL focused;

/*!
 @abstract Indicates whether the new window should use private browsing.
 @note To ensure proper isolation between private and non-private browsing, web views associated with private browsing windows must
 use a different `WKUserContentController`. Likewise, to be identified as a private web view and to ensure that cookies and other
 website data is not shared, private web views must be configured to use a non-persistent `WKWebsiteDataStore`.
 */
@property (nonatomic, readonly) BOOL usePrivateBrowsing;

@end

WK_HEADER_AUDIT_END(nullability, sendability)
