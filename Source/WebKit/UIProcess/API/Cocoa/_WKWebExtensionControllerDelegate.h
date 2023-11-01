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

@class _WKWebExtensionAction;
@class _WKWebExtensionContext;
@class _WKWebExtensionController;
@class _WKWebExtensionMatchPattern;
@class _WKWebExtensionMessagePort;
@class _WKWebExtensionTabCreationOptions;
@class _WKWebExtensionWindowCreationOptions;
@protocol _WKWebExtensionTab;
@protocol _WKWebExtensionWindow;

NS_ASSUME_NONNULL_BEGIN

WK_API_AVAILABLE(macos(13.3), ios(16.4))
@protocol _WKWebExtensionControllerDelegate <NSObject>
@optional

/*!
 @abstract Called when an extension context requests the list of ordered open windows.
 @param controller The web extension controller that is managing the extension.
 @param extensionContext The context in which the web extension is running.
 @return The array of ordered open windows.
 @discussion This method should be implemented by the app to provide the extension with a list of ordered open windows. Depending on your
 app's requirements, you may return different windows for each extension or the same windows for all extensions. The first window in the returned
 windows must correspond to the focused window and match the result of `webExtensionController:focusedWindowForExtensionContext:`.
 If `webExtensionController:focusedWindowForExtensionContext:` returns `nil`, indicating that no window has focus or the focused
 window is not visible the extension, the first window in the list returned by this method will be considered the presumed focused window. An empty result
 indicates no open windows are available for the extension. Defaults to empty array if not implemented.
 @seealso webExtensionController:focusedWindowForExtensionContext:
 */
- (NSArray<id <_WKWebExtensionWindow>> *)webExtensionController:(_WKWebExtensionController *)controller openWindowsForExtensionContext:(_WKWebExtensionContext *)extensionContext;

/*!
 @abstract Called when an extension context requests the currently focused window.
 @param controller The web extension controller that is managing the extension.
 @param extensionContext The context in which the web extension is running.
 @return The window that has focus, or \c nil if no window has focus or a window has focus that is not visible to the extension.
 @discussion This method can be optionally implemented by the app to designate the window currently in focus to the extension.
 If not implemented, the first window in the result of `webExtensionController:openWindowsForExtensionContext:` is used.
 @seealso webExtensionController:openWindowsForExtensionContext:
 */
- (nullable id <_WKWebExtensionWindow>)webExtensionController:(_WKWebExtensionController *)controller focusedWindowForExtensionContext:(_WKWebExtensionContext *)extensionContext;

/*!
 @abstract Called when an extension context requests a new window to be opened.
 @param controller The web extension controller that is managing the extension.
 @param options The set of options specifying how the new window should be created.
 @param extensionContext The context in which the web extension is running.
 @param completionHandler A block to be called with the newly created window or \c nil if the window wasn't created. An error should be
 provided if any errors occurred.
 @discussion This method should be implemented by the app to handle requests to open new windows. The app can decide how to handle
 the creation based on the provided options and existing windows. Once handled, the app should call the completion block with the created window
 or `nil` if the creation was declined or failed. If not implemented, the extension can't open new windows.
 */
- (void)webExtensionController:(_WKWebExtensionController *)controller openNewWindowWithOptions:(_WKWebExtensionWindowCreationOptions *)options forExtensionContext:(_WKWebExtensionContext *)extensionContext completionHandler:(void (^)(id <_WKWebExtensionWindow> WK_NULLABLE_RESULT newWindow, NSError * _Nullable error))completionHandler;

/*!
 @abstract Called when an extension context requests a new tab to be opened.
 @param controller The web extension controller that is managing the extension.
 @param options The set of options specifying how the new tab should be created.
 @param extensionContext The context in which the web extension is running.
 @param completionHandler A block to be called with the newly created tab or \c nil if the tab wasn't created. An error should be
 provided if any errors occurred.
 @discussion This method should be implemented by the app to handle requests to open new tabs. The app can decide how to handle
 the creation based on the provided options and existing tabs. Once handled, the app should call the completion block with the created tab
 or `nil` if the creation was declined or failed. If not implemented, the extension can't open new tabs.
 */
- (void)webExtensionController:(_WKWebExtensionController *)controller openNewTabWithOptions:(_WKWebExtensionTabCreationOptions *)options forExtensionContext:(_WKWebExtensionContext *)extensionContext completionHandler:(void (^)(id <_WKWebExtensionTab> WK_NULLABLE_RESULT newTab, NSError * _Nullable error))completionHandler;

/*!
 @abstract Called when an extension context requests permissions.
 @param controller The web extension controller that is managing the extension.
 @param permissions The set of permissions being requested by the extension.
 @param tab The tab in which the extension is running, or \c nil if the request are not specific to a tab.
 @param extensionContext The context in which the web extension is running.
 @param completionHandler A block to be called with the set of allowed permissions.
 @discussion This method should be implemented by the app to prompt the user for permission and call the completion block with the
 set of permissions that were granted. If not implemented or the completion block is not called within a reasonable amount of time, the
 request is assumed to have been denied.
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
 set of URLs that were granted access to. If not implemented or the completion block is not called within a reasonable amount of time, the
 request is assumed to have been denied.
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
 set of match patterns that were granted access to. If not implemented or the completion block is not called within a reasonable amount of time,
 the request is assumed to have been denied.
 */
- (void)webExtensionController:(_WKWebExtensionController *)controller promptForPermissionMatchPatterns:(NSSet<_WKWebExtensionMatchPattern *> *)matchPatterns inTab:(nullable id <_WKWebExtensionTab>)tab forExtensionContext:(_WKWebExtensionContext *)extensionContext completionHandler:(void (^)(NSSet<_WKWebExtensionMatchPattern *> *allowedMatchPatterns))completionHandler NS_SWIFT_NAME(webExtensionController(_:promptForPermissionMatchPatterns:in:for:completionHandler:));

/*!
 @abstract Called when a popup is requested to be displayed for a specific action.
 @param controller The web extension controller initiating the request.
 @param action The action for which the popup is requested.
 @param context The context within which the web extension is running.
 @param completionHandler A block to be called once the popup display operation is completed.
 @discussion This method is called in response to the extension's scripts or when invoking `performActionForTab:` if the action has a popup.
 The associated tab, if applicable, can be located through the `associatedTab` property of the `action` parameter. This delegate method is
 called when the web view for the popup is fully loaded and ready to display. Implementing this method is needed if the app intends to support
 programmatically showing the popup by the extension, although it is recommended for handling both programmatic and user-initiated cases.
 */
- (void)webExtensionController:(_WKWebExtensionController *)controller presentActionPopup:(_WKWebExtensionAction *)action forExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError * _Nullable error))completionHandler;

/*!
 @abstract Called when an extension context wants to send a one-time message to an application.
 @param controller The web extension controller that is managing the extension.
 @param message The message to be sent.
 @param applicationIdentifier The unique identifier for the application, or \c nil if none was specified.
 @param extensionContext The context in which the web extension is running.
 @param replyHandler A block to be called with a JSON-serializable reply message or an error.
 @discussion This method should be implemented by the app to handle one-off messages to applications.
 If not implemented, the default behavior is to pass the message to the app extension handler within the extension's bundle,
 if the extension was loaded from an app extension bundle; otherwise, no action is performed if not implemented.
 */
- (void)webExtensionController:(_WKWebExtensionController *)controller sendMessage:(id)message toApplicationIdentifier:(nullable NSString *)applicationIdentifier forExtensionContext:(_WKWebExtensionContext *)extensionContext replyHandler:(void (^)(id WK_NULLABLE_RESULT replyMessage, NSError * _Nullable error))replyHandler WK_SWIFT_ASYNC(5) NS_SWIFT_NAME(webExtensionController(_:sendMessage:to:for:replyHandler:));

/*!
 @abstract Called when an extension context wants to establish a persistent connection to an application.
 @param controller The web extension controller that is managing the extension.
 @param extensionContext The context in which the web extension is running.
 @param port A port object for handling the message exchange.
 @param completionHandler A block to be called when the connection is ready to use, taking an optional error object
 as a parameter. If the connection is successfully established, the error parameter should be \c nil.
 @discussion This method should be implemented by the app to handle establishing connections to applications.
 The provided `WKWebExtensionPort` object can be used to handle message sending, receiving, and disconnection.
 You should retain the port object for as long as the connection remains active. Releasing the port will disconnect it.
 If not implemented, the default behavior is to pass the messages to the app extension handler within the extension's bundle,
 if the extension was loaded from an app extension bundle; otherwise, no action is performed if not implemented.
 */
- (void)webExtensionController:(_WKWebExtensionController *)controller connectUsingMessagePort:(_WKWebExtensionMessagePort *)port forExtensionContext:(_WKWebExtensionContext *)extensionContext completionHandler:(void (^)(NSError * _Nullable error))completionHandler NS_SWIFT_NAME(webExtensionController(_:connectUsingMessagePort:for:completionHandler:));

@end

NS_ASSUME_NONNULL_END
