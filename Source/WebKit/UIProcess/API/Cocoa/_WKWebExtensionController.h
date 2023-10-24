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
#import <WebKit/_WKWebExtensionTab.h>
#import <WebKit/_WKWebExtensionWindow.h>

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
 @abstract Returns a loaded extension context matching the specified URL.
 @param URL The URL to lookup.
 @result An extension context or `nil` if no match was found.
 @discussion This method is useful for determining the extension context to use when about to navigate to an extension URL. For example,
 you could use this method to retrieve the appropriate extension context and then use its `webViewConfiguration` property to configure a
 web view for loading that URL.
 */
- (nullable _WKWebExtensionContext *)extensionContextForURL:(NSURL *)URL NS_SWIFT_NAME(extensionContext(for:));

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

/*!
 @abstract Should be called by the app when a new window is opened to fire appropriate events with all loaded web extensions.
 @param newWindow The newly opened window.
 @discussion This method informs all loaded extensions of the opening of a new window, ensuring consistent understanding across extensions.
 If the intention is to inform only a specific extension, you should use the respective method on that extension's context instead.
 @seealso didCloseWindow:
 */
- (void)didOpenWindow:(id <_WKWebExtensionWindow>)newWindow;

/*!
 @abstract Should be called by the app when a window is closed to fire appropriate events with all loaded web extensions.
 @param newWindow The window that was closed.
 @discussion This method informs all loaded extensions of the closure of a window, ensuring consistent understanding across extensions.
 If the intention is to inform only a specific extension, you should use the respective method on that extension's context instead.
 @seealso didOpenWindow:
 */
- (void)didCloseWindow:(id <_WKWebExtensionWindow>)closedWindow;

/*!
 @abstract Should be called by the app when a window gains focus to fire appropriate events with all loaded web extensions.
 @param focusedWindow The window that gained focus, or \c nil if no window has focus or a window has focus that is not visible to extensions.
 @discussion This method informs all loaded extensions of the focused window, ensuring consistent understanding across extensions.
 If the intention is to inform only a specific extension, you should use the respective method on that extension's context instead.
 */
- (void)didFocusWindow:(nullable id <_WKWebExtensionWindow>)focusedWindow;

/*!
 @abstract Should be called by the app when a new tab is opened to fire appropriate events with all loaded web extensions.
 @param newTab The newly opened tab.
 @discussion This method informs all loaded extensions of the opening of a new tab, ensuring consistent understanding across extensions.
 If the intention is to inform only a specific extension, you should use the respective method on that extension's context instead.
 @seealso didCloseTab:
 */
- (void)didOpenTab:(id <_WKWebExtensionTab>)newTab;

/*!
 @abstract Should be called by the app when a tab is closed to fire appropriate events with all loaded web extensions.
 @param closedTab The tab that was closed.
 @param windowIsClosing A boolean value indicating whether the window containing the tab is also closing.
 @discussion This method informs all loaded extensions of the closing of a tab, ensuring consistent understanding across extensions.
 If the intention is to inform only a specific extension, you should use the respective method on that extension's context instead.
 @seealso didOpenTab:
 */
- (void)didCloseTab:(id <_WKWebExtensionTab>)closedTab windowIsClosing:(BOOL)windowIsClosing;

/*!
 @abstract Should be called by the app when a tab is activated to notify all loaded web extensions.
 @param activatedTab The tab that has become active.
 @param previousTab The tab that was active before. This parameter can be \c nil if there was no previously active tab.
 @discussion This method informs all loaded extensions of the tab activation, ensuring consistent state awareness across extensions.
 If the intention is to inform only a specific extension, use the respective method on that extension's context instead.
 */
- (void)didActivateTab:(id<_WKWebExtensionTab>)activatedTab previousActiveTab:(nullable id<_WKWebExtensionTab>)previousTab;

/*!
 @abstract Should be called by the app when tabs are selected to fire appropriate events with all loaded web extensions.
 @param selectedTabs The set of tabs that were selected.
 @discussion This method informs all loaded extensions that tabs have been selected, ensuring consistent understanding across extensions.
 If the intention is to inform only a specific extension, you should use the respective method on that extension's context instead.
 */
- (void)didSelectTabs:(NSSet<id <_WKWebExtensionTab>> *)selectedTabs;

/*!
 @abstract Should be called by the app when tabs are deselected to fire appropriate events with all loaded web extensions.
 @param deselectedTabs The set of tabs that were deselected.
 @discussion This method informs all loaded extensions that tabs have been deselected, ensuring consistent understanding across extensions.
 If the intention is to inform only a specific extension, you should use the respective method on that extension's context instead.
 */
- (void)didDeselectTabs:(NSSet<id <_WKWebExtensionTab>> *)deselectedTabs;

/*!
 @abstract Should be called by the app when a tab is moved to fire appropriate events with all loaded web extensions.
 @param movedTab The tab that was moved.
 @param index The old index of the tab within the window.
 @param oldWindow The window that the tab was moved from, or \c nil if the window stayed the same.
 @discussion This method informs all loaded extensions of the movement of a tab, ensuring consistent understanding across extensions.
 If the intention is to inform only a specific extension, you should use the respective method on that extension's context instead.
 */
- (void)didMoveTab:(id <_WKWebExtensionTab>)movedTab fromIndex:(NSUInteger)index inWindow:(nullable id <_WKWebExtensionWindow>)oldWindow NS_SWIFT_NAME(didMoveTab(_:from:in:));

/*!
 @abstract Should be called by the app when a tab is replaced by another tab to fire appropriate events with all loaded web extensions.
 @param oldTab The tab that was replaced.
 @param newTab The tab that replaced the old tab.
 @discussion This method informs all loaded extensions of the replacement of a tab, ensuring consistent understanding across extensions.
 If the intention is to inform only a specific extension, you should use the respective method on that extension's context instead.
 */
- (void)didReplaceTab:(id <_WKWebExtensionTab>)oldTab withTab:(id <_WKWebExtensionTab>)newTab NS_SWIFT_NAME(didReplaceTab(_:with:));

/*!
 @abstract Should be called by the app when the properties of a tab are changed to fire appropriate events with all loaded web extensions.
 @param properties The properties of the tab that were changed.
 @param changedTab The tab whose properties were changed.
 @discussion This method informs all loaded extensions of changes to tab properties, ensuring a unified understanding across extensions.
 If the intention is to inform only a specific extension, you should use the respective method on that extension's context instead.
 */
- (void)didChangeTabProperties:(_WKWebExtensionTabChangedProperties)properties forTab:(id <_WKWebExtensionTab>)changedTab NS_SWIFT_NAME(didChangeTabProperties(_:for:));

@end

NS_ASSUME_NONNULL_END
