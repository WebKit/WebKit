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

#import <WebKit/_WKWebExtensionControllerDelegate.h>

@class _WKWebExtension;
@class _WKWebExtensionContext;
@class _WKWebExtensionControllerConfiguration;

NS_ASSUME_NONNULL_BEGIN

/*!
 @abstract A `WKWebExtensionController` object manages a set of loaded extension contexts.
 @discussion You can have one or more extension controller instances, allowing different parts of the app to use different sets of extensions.
 A controller is associated with @link WKWebView @/link via the `webExtensionController` property on @link WKWebViewConfiguration @/link.
 */
WK_CLASS_AVAILABLE(macos(13.3), ios(16.4))
@interface _WKWebExtensionController : NSObject

/*!
 @abstract Returns a web extension controller initialized with the default configuration.
 @result An initialized web extension controller, or nil if the object could not be initialized.
 @discussion This is a designated initializer. You can use @link -initWithConfiguration: @/link to
 initialize an instance with a configuration.
 @seealso initWithConfiguration:
*/
- (instancetype)init NS_DESIGNATED_INITIALIZER;

/*!
 @abstract Returns a web extension controller initialized with the specified configuration.
 @param configuration The configuration for the new web extension controller.
 @result An initialized web extension controller, or nil if the object could not be initialized.
 @discussion This is a designated initializer. You can use @link -init: @/link to initialize an
 instance with the default configuration. The initializer copies the specified configuration, so mutating
 the configuration after invoking the initializer has no effect on the web extension controller.
 @seealso init
*/
- (instancetype)initWithConfiguration:(_WKWebExtensionControllerConfiguration *)configuration NS_DESIGNATED_INITIALIZER;

/*! @abstract The extension controller delegate. */
@property (nonatomic, weak) id <_WKWebExtensionControllerDelegate> delegate;

/*!
 @abstract A copy of the configuration with which the web extension controller was initialized.
 @discussion Mutating the configuration has no effect on the web extension controller.
*/
@property (nonatomic, readonly, copy) _WKWebExtensionControllerConfiguration *configuration;

/*!
 @abstract Loads the specified extension context.
 @discussion Causes the context to start, loading any background content, and injecting any content into relevant tabs.
 @param error Set to \c nil or an \c NSError instance if an error occurred.
 @result A Boolean value indicating if the context was successfully loaded.
 @seealso loadExtensionContext:
*/
- (BOOL)loadExtensionContext:(_WKWebExtensionContext *)extensionContext error:(NSError **)error;

/*!
 @abstract Unloads the specified extension context.
 @discussion Causes the context to stop running.
 @param error Set to \c nil or an \c NSError instance if an error occurred.
 @result A Boolean value indicating if the context was successfully unloaded.
 @seealso unloadExtensionContext:
*/
- (BOOL)unloadExtensionContext:(_WKWebExtensionContext *)extensionContext error:(NSError **)error;

/*!
 @abstract Returns a loaded extension context for the specified extension.
 @param extension An extension to lookup.
 @result An extension context or `nil` if no match was found.
 @seealso extensions
*/
- (nullable _WKWebExtensionContext *)extensionContextForExtension:(_WKWebExtension *)extension NS_SWIFT_NAME(extensionContext(for:));

/*!
 @abstract A set of all the currently loaded extensions.
 @seealso extensionContexts
*/
@property (nonatomic, readonly, copy) NSSet<_WKWebExtension *> *extensions;

/*!
 @abstract A set of all the currently loaded extension contexts.
 @seealso extensions
*/
@property (nonatomic, readonly, copy) NSSet<_WKWebExtensionContext *> *extensionContexts;

@end

NS_ASSUME_NONNULL_END
