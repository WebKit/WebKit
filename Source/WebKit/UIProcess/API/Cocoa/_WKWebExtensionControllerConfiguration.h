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

NS_ASSUME_NONNULL_BEGIN

/*!
 @abstract A `WKWebExtensionControllerConfiguration` object with which to initialize a web extension controller.
 @discussion Contains properties used to configure a @link WKWebExtensionController @/link.
*/
WK_CLASS_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
@interface _WKWebExtensionControllerConfiguration : NSObject <NSSecureCoding, NSCopying>

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

/*!
 @abstract Returns a new default configuration that is persistent and not unique.
 @discussion If a @link WKWebExtensionController @/link is associated with a persistent configuration,
 data will be written to the file system in a common location. When using multiple extension controllers, each
 controller should use a unique configuration to avoid conflicts.
 @seealso configurationWithIdentifier:
*/
+ (instancetype)defaultConfiguration;

/*!
 @abstract Returns a new non-persistent configuration.
 @discussion If a @link WKWebExtensionController @/link is associated with a non-persistent configuration,
 no data will be written to the file system. This is useful for extensions in "private browsing" situations.
*/
+ (instancetype)nonPersistentConfiguration;

/*!
 @abstract Returns a new configuration that is persistent and unique for the specified identifier.
 @discussion If a @link WKWebExtensionController @/link is associated with a unique persistent configuration,
 data will be written to the file system in a unique location based on the specified identifier.
 @seealso defaultConfiguration
*/
+ (instancetype)configurationWithIdentifier:(NSUUID *)identifier;

/*! @abstract A Boolean value indicating if this context will write data to the the file system. */
@property (nonatomic, readonly, getter=isPersistent) BOOL persistent;

/*! @abstract A unique identifier used for persistent configuration storage, or `nil` when it is the default or not persistent. */
@property (nonatomic, nullable, readonly) NSUUID *identifier;

@end

NS_ASSUME_NONNULL_END
