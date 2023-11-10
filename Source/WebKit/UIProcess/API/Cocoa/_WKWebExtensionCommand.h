/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#if TARGET_OS_IPHONE
#import <UIKit/UICommand.h>
#endif

@class _WKWebExtensionContext;

NS_ASSUME_NONNULL_BEGIN

/*!
 @abstract A `WKWebExtensionCommand` object encapsulates the properties for an individual web extension command.
 @discussion Provides access to command properties such as a unique identifier, a descriptive title, and shortcut keys. Commands
 can be used by a web extension to perform specific actions within a web extension context, such toggling features, or interacting with
 web content. These commands enhance the functionality of the extension by allowing users to invoke actions quickly.
 */
WK_CLASS_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
NS_SWIFT_NAME(WKWebExtension.Command)
@interface _WKWebExtensionCommand : NSObject

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

/*! @abstract The web extension context associated with the command. */
@property (nonatomic, readonly, weak) _WKWebExtensionContext *webExtensionContext;

/*! @abstract Unique identifier for the command. */
@property (nonatomic, readonly, copy) NSString *identifier;

/*!
 @abstract Descriptive title for the command aiding discoverability.
 @discussion This title can be displayed in user interface elements such as keyboard shortcuts lists or menu items to help users understand its purpose.
 */
@property (nonatomic, readonly, copy) NSString *discoverabilityTitle;

/*!
 @abstract The primary key used to trigger the command, distinct from any modifier flags.
 @discussion This property can be customized within the app to avoid conflicts with existing shortcuts or to enable user personalization.
 It should accurately represent the activation key as used by the app, which the extension can use to display the complete shortcut in its interface.
 If no shortcut is desired for the command, the property should be set to `nil`.
 */
@property (nonatomic, nullable, copy) NSString *activationKey;

/*!
 @abstract The modifier flags used with the activation key to trigger the command.
 @discussion This property can be customized within the app to avoid conflicts with existing shortcuts or to enable user personalization. It
 should accurately represent the modifier keys as used by the app, which the extension can use to display the complete shortcut in its interface.
 If no modifiers are desired for the command, the property should be set to `0`.
 */
#if TARGET_OS_IPHONE
@property (nonatomic) UIKeyModifierFlags modifierFlags;
#else
@property (nonatomic) NSEventModifierFlags modifierFlags;
#endif

@end

NS_ASSUME_NONNULL_END
