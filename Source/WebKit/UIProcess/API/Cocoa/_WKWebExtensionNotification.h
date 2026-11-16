/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

@class WKWebExtensionContext;

NS_HEADER_AUDIT_BEGIN(nullability, sendability)

/*!
 @abstract A ``_WKWebExtensionNotificationButton`` object encapsulates the properties of a single action button shown on a web extension notification.
 @discussion Buttons are identified by their position within the ``_WKWebExtensionNotification/buttons`` array; the app reports which button the user activated using that index.
 */
WK_CLASS_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA), visionos(WK_XROS_TBA))
@interface _WKWebExtensionNotificationButton : NSObject

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)new NS_UNAVAILABLE;

/*! @abstract The title displayed on the button. */
@property (nonatomic, readonly, copy) NSString *title;

@end

/*!
 @abstract A ``_WKWebExtensionNotification`` object encapsulates the properties of a notification requested by a web extension.
 @discussion Instances are created by WebKit in response to the `browser.notifications` JavaScript APIs and passed to the app through the notification methods of ``WKWebExtensionControllerDelegatePrivate``. WebKit owns the notification's identity and state, including merging updates and firing events; the app is responsible only for presenting the notification with its native facilities and for reporting the user's interaction back to WebKit.
 */
WK_CLASS_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA), visionos(WK_XROS_TBA))
@interface _WKWebExtensionNotification : NSObject

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)new NS_UNAVAILABLE;

/*! @abstract The extension context to which this notification is related. */
@property (nonatomic, nullable, readonly, weak) WKWebExtensionContext *webExtensionContext;

/*! @abstract A unique identifier for the notification within its extension context. */
@property (nonatomic, readonly, copy) NSString *identifier;

/*! @abstract The primary title of the notification. */
@property (nonatomic, readonly, copy) NSString *title;

/*! @abstract Secondary text shown below the title, or `nil` if the extension specified none. */
@property (nonatomic, readonly, copy, nullable) NSString *subtitle;

/*! @abstract The main message body of the notification. */
@property (nonatomic, readonly, copy) NSString *body;

/*! @abstract The action buttons to display on the notification, or an empty array if the extension specified none. */
@property (nonatomic, readonly, copy) NSArray<_WKWebExtensionNotificationButton *> *buttons;

@end

NS_HEADER_AUDIT_END(nullability, sendability)
