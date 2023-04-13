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

#import <WebKit/_WKWebExtensionPermission.h>

@class _WKWebExtensionContext;
@class _WKWebExtensionMatchPattern;
@class _WKWebExtensionController;
@protocol _WKWebExtensionTab;

NS_ASSUME_NONNULL_BEGIN

WK_API_AVAILABLE(macos(13.3), ios(16.4))
@protocol _WKWebExtensionControllerDelegate <NSObject>
@optional

/*!
 @abstract Called when an extension context requests permissions.
 @param controller The web extension controller that is managing the extension.
 @param permissions The set of permissions being requested by the extension.
 @param tab The tab in which the extension is running, or \c nil if the request are not specific to a tab.
 @param extensionContext The context in which the web extension is running.
 @param completionHandler A block to be called with the set of allowed permissions.
 @discussion This method should be implemented by the app to prompt the user for permission and call the completion block with the
 set of permissions that were granted. If the completion block is not called within a reasonable amount of time, the request
 is assumed to have been denied.
 */
- (void)webExtensionController:(_WKWebExtensionController *)controller promptForPermissions:(NSSet<_WKWebExtensionPermission> *)permissions inTab:(nullable id <_WKWebExtensionTab>)tab forExtensionContext:(_WKWebExtensionContext *)extensionContext completionHandler:(void (^)(NSSet<_WKWebExtensionPermission> *allowedPermissions))completionHandler NS_SWIFT_NAME(webExtensionController(_:promptForPermissions:in:for:completionHandler:));

/*!
 @abstract Called when an extension context requests access to a set of URLs.
 @param controller The web extension controller that is managing the extension.
 @param urls The set of URLs that the extension is requesting access to.
 @param tab The tab in which the extension is running, or \c nil if the request is not specific to a tab.
 @param extensionContext The context in which the web extension is running.
 @param completionHandler A block to be called with the set of allowed URLs.
 @discussion This method should be implemented by the app to prompt the user for permission and call the completion block with the
 set of URLs that were granted access to. If the completion block is not called within a reasonable amount of time, the request
 is assumed to have been denied.
 */
- (void)webExtensionController:(_WKWebExtensionController *)controller promptForPermissionToAccessURLs:(NSSet<NSURL *> *)urls inTab:(nullable id <_WKWebExtensionTab>)tab forExtensionContext:(_WKWebExtensionContext *)extensionContext completionHandler:(void (^)(NSSet<NSURL *> *allowedURLs))completionHandler NS_SWIFT_NAME(webExtensionController(_:promptForPermissionToAccess:in:for:completionHandler:));

/*!
 @abstract Called when an extension context requests access to a set of match patterns.
 @param controller The web extension controller that is managing the extension.
 @param matchPatterns The set of match patterns that the extension is requesting access to.
 @param tab The tab in which the extension is running, or \c nil if the request is not specific to a tab.
 @param extensionContext The context in which the web extension is running.
 @param completionHandler A block to be called with the set of allowed match patterns.
 @discussion This method should be implemented by the app to prompt the user for permission and call the completion block with the
 set of match patterns that were granted access to. If the completion block is not called within a reasonable amount of time, the request
 is assumed to have been denied.
 */
- (void)webExtensionController:(_WKWebExtensionController *)controller promptForPermissionMatchPatterns:(NSSet<_WKWebExtensionMatchPattern *> *)matchPatterns inTab:(nullable id <_WKWebExtensionTab>)tab forExtensionContext:(_WKWebExtensionContext *)extensionContext completionHandler:(void (^)(NSSet<_WKWebExtensionMatchPattern *> *allowedMatchPatterns))completionHandler NS_SWIFT_NAME(webExtensionController(_:promptForPermissionMatchPatterns:in:for:completionHandler:));

@end

NS_ASSUME_NONNULL_END
