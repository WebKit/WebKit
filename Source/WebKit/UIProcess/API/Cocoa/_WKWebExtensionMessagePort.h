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

NS_ASSUME_NONNULL_BEGIN

/*! @abstract Indicates a `_WKWebExtensionMessagePort` error. */
WK_EXTERN NSErrorDomain const _WKWebExtensionMessagePortErrorDomain WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

/*!
 @abstract Constants used by NSError to indicate errors in the `_WKWebExtensionMessagePort` domain.
 @constant WKWebExtensionMessagePortErrorUnknown  Indicates that an unknown error occurred.
 @constant WKWebExtensionMessagePortErrorNotConnected  Indicates that the message port is disconnected.
 */
typedef NS_ERROR_ENUM(_WKWebExtensionMessagePortErrorDomain, _WKWebExtensionMessagePortError) {
    _WKWebExtensionMessagePortErrorUnknown,
    _WKWebExtensionMessagePortErrorNotConnected,
} NS_SWIFT_NAME(_WKWebExtensionMessagePort.Error) WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

/*!
 @abstract A `WKWebExtensionMessagePort` object manages message-based communication with a web extension.
 @discussion Contains properties and methods to handle message exchanges with a web extension.
*/
WK_CLASS_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
NS_SWIFT_NAME(_WKWebExtension.MessagePort)
@interface _WKWebExtensionMessagePort : NSObject

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

/*!
 @abstract The unique identifier for the app to which this port should be connected.
 @discussion This identifier is provided by the web extension and may or may not be used by the app.
 It's up to the app to decide how to interpret this identifier.
 */
@property (nonatomic, readonly, nullable) NSString *applicationIdentifier;

/*!
 @abstract The block to be executed when a message is received from the web extension.
 @discussion The block takes two parameters: the message and an optional error object in case of an error.
 */
@property (nonatomic, copy, nullable) void (^messageHandler)(id _Nullable message, NSError * _Nullable error);

/*!
 @abstract The block to be executed when the port disconnects.
 @discussion This block has one parameter: an optional error object in case an error caused the disconnection.
 */
@property (nonatomic, copy, nullable) void (^disconnectHandler)(NSError * _Nullable error);

/*! @abstract Indicates whether the message port is disconnected. */
@property (nonatomic, readonly, getter=isDisconnected) BOOL disconnected;

/*!
 @abstract Sends a message to the connected web extension.
 @param message The message that needs to be sent, which must be JSON-serializable.
 @param completionHandler A block to be invoked after the message is sent, taking a boolean and an optional error object as parameters.
 */
- (void)sendMessage:(id)message completionHandler:(void (^)(BOOL success, NSError * _Nullable error))completionHandler;

/*!
 @abstract Disconnects the port, terminating all further messages.
 @param error An optional error object indicating the reason for disconnection.
 */
- (void)disconnectWithError:(nullable NSError *)error;

@end

NS_ASSUME_NONNULL_END
