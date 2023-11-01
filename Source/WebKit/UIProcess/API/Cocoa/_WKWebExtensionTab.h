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

@class WKWebView;
@class _WKWebExtensionContext;
@class _WKWebExtensionTabCreationOptions;
@protocol _WKWebExtensionWindow;

#if TARGET_OS_IPHONE
@class UIImage;
#else
@class NSImage;
#endif

NS_ASSUME_NONNULL_BEGIN

/*!
 @abstract Constants used by @link WKWebExtensionController @/link to indicate tab changes.
 @constant WKWebExtensionTabChangedPropertiesNone  Indicates nothing changed.
 @constant WKWebExtensionTabChangedPropertiesAudible  Indicates the audible state changed.
 @constant WKWebExtensionTabChangedPropertiesLoading  Indicates the loading state changed.
 @constant WKWebExtensionTabChangedPropertiesMuted  Indicates the muted state changed.
 @constant WKWebExtensionTabChangedPropertiesPinned  Indicates the pinned state changed.
 @constant WKWebExtensionTabChangedPropertiesReaderMode  Indicates the reader mode state changed.
 @constant WKWebExtensionTabChangedPropertiesSize  Indicates the size changed.
 @constant WKWebExtensionTabChangedPropertiesTitle  Indicates the title changed.
 @constant WKWebExtensionTabChangedPropertiesURL  Indicates the URL changed.
 @constant WKWebExtensionTabChangedPropertiesZoomFactor  Indicates the zoom factor changed.
 @constant WKWebExtensionTabChangedPropertiesAll  Indicates all properties changed.
 */
typedef NS_OPTIONS(NSUInteger, _WKWebExtensionTabChangedProperties) {
    _WKWebExtensionTabChangedPropertiesNone       = 0,
    _WKWebExtensionTabChangedPropertiesAudible    = 1 << 1,
    _WKWebExtensionTabChangedPropertiesLoading    = 1 << 2,
    _WKWebExtensionTabChangedPropertiesMuted      = 1 << 3,
    _WKWebExtensionTabChangedPropertiesPinned     = 1 << 4,
    _WKWebExtensionTabChangedPropertiesReaderMode = 1 << 5,
    _WKWebExtensionTabChangedPropertiesSize       = 1 << 6,
    _WKWebExtensionTabChangedPropertiesTitle      = 1 << 7,
    _WKWebExtensionTabChangedPropertiesURL        = 1 << 8,
    _WKWebExtensionTabChangedPropertiesZoomFactor = 1 << 9,
    _WKWebExtensionTabChangedPropertiesAll        = NSUIntegerMax,
} WK_API_AVAILABLE(macos(13.3), ios(16.4));

/*! @abstract A class conforming to the `WKWebExtensionTab` protocol represents a tab to web extensions. */
WK_API_AVAILABLE(macos(13.3), ios(16.4))
@protocol _WKWebExtensionTab <NSObject>
@optional

/*!
 @abstract Called when the window containing the tab is needed.
 @param context The context in which the web extension is running.
 @return The window containing the tab.
 @discussion Defaults to `nil` if not implemented.
 */
- (nullable id <_WKWebExtensionWindow>)windowForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called when the parent tab for the tab is needed.
 @param context The context in which the web extension is running.
 @return The parent tab of the tab, if the tab was opened from another tab.
 @discussion Defaults to `nil` if not implemented.
 @seealso setParentTab:forWebExtensionContext:completionHandler:
 */
- (nullable id <_WKWebExtensionTab>)parentTabForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called to set or clear the parent tab for the tab.
 @param parentTab The tab that should be set as the parent of the tab. If \c nil is provided, the current
 parent tab should be cleared.
 @param context The context in which the web extension is running.
 @param completionHandler A block that must be called upon completion. It takes a single error argument,
 which should be provided if any errors occurred.
 @discussion No action is performed if not implemented.
 @seealso parentTabForWebExtensionContext:
 */
- (void)setParentTab:(nullable id <_WKWebExtensionTab>)parentTab forWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError * _Nullable error))completionHandler;

/*!
 @abstract Called when the main web view for the tab is needed.
 @param context The context in which the web extension is running.
 @return The main web view for the tab.
 @discussion Defaults to `nil` if not implemented.
 */
- (nullable WKWebView *)mainWebViewForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called when the web views for the tab are needed.
 @param context The context in which the web extension is running.
 @return An array of web views for the tab.
 @discussion Defaults to an array containing the main web view if not implemented.
 @seealso mainWebViewForWebExtensionContext:
 */
- (NSArray<WKWebView *> *)webViewsForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called when the title of the tab is needed.
 @param context The context in which the web extension is running.
 @return The title of the tab.
 @discussion Defaults to `title` for the main web view if not implemented.
 */
- (nullable NSString *)tabTitleForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called when the pinned state of the tab is needed.
 @param context The context in which the web extension is running.
 @return `YES` if the tab is pinned, `NO` otherwise.
 @discussion Defaults to `NO` if not implemented.
 */
- (BOOL)isPinnedForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called to pin the tab.
 @param context The context in which the web extension is running.
 @param completionHandler A block that must be called upon completion. It takes a single error argument,
 which should be provided if any errors occurred.
 @discussion This is equivalent to the user selecting to pin the tab through a menu item. When a tab is pinned,
 it should be moved to the front of the tab bar and usually reduced in size. No action is performed if not implemented.
 @seealso isPinnedForWebExtensionContext:
 @seealso pinForWebExtensionContext:completionHandler:
 */
- (void)pinForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError * _Nullable error))completionHandler;

/*!
 @abstract Called to unpin the tab.
 @param context The context in which the web extension is running.
 @param completionHandler A block that must be called upon completion. It takes a single error argument,
 which should be provided if any errors occurred.
 @discussion This is equivalent to the user selecting to unpin the tab through a menu item. When a tab is unpinned,
 it should be restored to a normal size and position in the tab bar. No action is performed if not implemented.
 @seealso isPinnedForWebExtensionContext:
 @seealso pinForWebExtensionContext:completionHandler:
 */
- (void)unpinForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError * _Nullable error))completionHandler;

/*!
 @abstract Called to check if reader mode is available for the tab.
 @param context The context in which the web extension is running.
 @return `YES` if reader mode is available for the tab, `NO` otherwise.
 @discussion Defaults to `NO` if not implemented.
 @seealso isShowingReaderModeForWebExtensionContext:
 */
- (BOOL)isReaderModeAvailableForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called to check if the tab is currently showing reader mode.
 @param context The context in which the web extension is running.
 @return `YES` if the tab is showing reader mode, `NO` otherwise.
 @discussion Defaults to `NO` if not implemented.
 @seealso isReaderModeAvailableForWebExtensionContext:
 */
- (BOOL)isShowingReaderModeForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called to toggle reader mode for the tab.
 @param context The context in which the web extension is running.
 @param completionHandler A block that must be called upon completion. It takes a single error argument,
 which should be provided if any errors occurred.
 @discussion No action is performed if not implemented.
 @seealso isReaderModeAvailableForWebExtensionContext:
 @seealso isShowingReaderModeForWebExtensionContext:
 */
- (void)toggleReaderModeForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError * _Nullable error))completionHandler;

/*!
 @abstract Called to check if the tab is currently playing audio.
 @param context The context in which the web extension is running.
 @return `YES` if the tab is playing audio, `NO` otherwise.
 @discussion Defaults to `NO` if not implemented.
 */
- (BOOL)isAudibleForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called to check if the tab is currently muted.
 @param context The context in which the web extension is running.
 @return `YES` if the tab is muted, `NO` otherwise.
 @discussion Defaults to `NO` if not implemented.
 */
- (BOOL)isMutedForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called to mute the tab.
 @param context The context in which the web extension is running.
 @param completionHandler A block that must be called upon completion. It takes a single error argument,
 which should be provided if any errors occurred.
 @discussion No action is performed if not implemented.
 @seealso isMutedForWebExtensionContext:
 @seealso unmuteForWebExtensionContext:completionHandler:
 */
- (void)muteForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError * _Nullable error))completionHandler;

/*!
 @abstract Called to unmute the tab.
 @param context The context in which the web extension is running.
 @param completionHandler A block that must be called upon completion. It takes a single error argument,
 which should be provided if any errors occurred.
 @discussion No action is performed if not implemented.
 @seealso isMutedForWebExtensionContext:
 @seealso muteForWebExtensionContext:completionHandler:
 */
- (void)unmuteForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError * _Nullable error))completionHandler;

/*!
 @abstract Called when the size of the tab is needed.
 @param context The context in which the web extension is running.
 @return The size of the tab.
 @discussion Defaults to size of the main web view if not implemented.
 */
- (CGSize)sizeForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called when the zoom factor of the tab is needed.
 @param context The context in which the web extension is running.
 @return The zoom factor of the tab.
 @discussion Defaults to `pageZoom` for the main web view if not implemented.
 @seealso setZoomFactor:forWebExtensionContext:completionHandler:
 */
- (double)zoomFactorForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called to set the zoom factor of the tab.
 @param zoomFactor The desired zoom factor for the tab.
 @param context The context in which the web extension is running.
 @param completionHandler A block that must be called upon completion. It takes a single error argument,
 which should be provided if any errors occurred.
 @discussion Sets `pageZoom` for the main web view if not implemented.
 @seealso zoomFactorForWebExtensionContext:
 */
- (void)setZoomFactor:(double)zoomFactor forWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError * _Nullable error))completionHandler;

/*!
 @abstract Called when the URL of the tab is needed.
 @param context The context in which the web extension is running.
 @return The URL of the tab.
 @discussion Defaults to `URL` for the main web view if not implemented.
 */
- (nullable NSURL *)urlForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called when the pending URL of the tab is needed.
 @param context The context in which the web extension is running.
 @return The pending URL of the tab.
 @discussion The pending URL is the URL of a page that is in the process of loading. If there is no pending URL, return `nil`.
 Defaults to `nil` if not implemented.
 */
- (nullable NSURL *)pendingURLForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called to check if the tab has finished loading.
 @param context The context in which the web extension is running.
 @return `YES` if the tab has finished loading, `NO` otherwise.
 @discussion Defaults to `isLoading` for the main web view if not implemented.
 */
- (BOOL)isLoadingCompleteForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called to detect the locale of the webpage currently loaded in the tab.
 @param context The context in which the web extension is running.
 @param completionHandler A block that must be called upon completion. The block takes two arguments:
 the detected locale (or \c nil if the locale is unknown) and an error, which should be provided if any errors occurred.
 @discussion No action is performed if not implemented.
 */
- (void)detectWebpageLocaleForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSLocale * WK_NULLABLE_RESULT locale, NSError * _Nullable error))completionHandler;

/*!
 @abstract Called to capture the visible area of the current webpage as an image.
 @param context The context in which the web extension is running.
 @param completionHandler A block that must be called upon completion. The block takes two arguments:
 the captured image the webpage (or \c nil if capturing failed) and an error, which should be provided if any errors occurred.
 @discussion Defaults to capturing the visible area for the main web view if not implemented.
 */
#if TARGET_OS_IPHONE
- (void)captureVisibleWebpageForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(UIImage * WK_NULLABLE_RESULT visibleWebpageImage, NSError * _Nullable error))completionHandler;
#else
- (void)captureVisibleWebpageForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSImage * WK_NULLABLE_RESULT visibleWebpageImage, NSError * _Nullable error))completionHandler;
#endif

/*!
 @abstract Called to load a URL in the tab.
 @param url The URL to be loaded in the tab.
 @param context The context in which the web extension is running.
 @param completionHandler A block that must be called upon completion. It takes a single error argument,
 which should be provided if any errors occurred.
 @discussion If the tab is already loading a page, calling this method should stop the current page from loading and start
 loading the new URL. Loads the URL in the main web view via `loadRequest:` if not implemented.
 */
- (void)loadURL:(NSURL *)url forWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError * _Nullable error))completionHandler;

/*!
 @abstract Called to reload the current page in the tab.
 @param context The context in which the web extension is running.
 @param completionHandler A block that must be called upon completion. It takes a single error argument,
 which should be provided if any errors occurred.
 @discussion Reloads the main web view via `reload` if not implemented.
 */
- (void)reloadForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError * _Nullable error))completionHandler;

/*!
 @abstract Called to reload the current page in the tab, bypassing the cache.
 @param context The context in which the web extension is running.
 @param completionHandler A block that must be called upon completion. It takes a single error argument,
 which should be provided if any errors occurred.
 @discussion Reloads the main web view via `reloadFromOrigin` if not implemented.
 */
- (void)reloadFromOriginForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError * _Nullable error))completionHandler;

/*!
 @abstract Called to navigate the tab to the previous page in its history.
 @param context The context in which the web extension is running.
 @param completionHandler A block that must be called upon completion. It takes a single error argument,
 which should be provided if any errors occurred.
 @discussion Navigates to the previous page in the main web view via `goBack` if not implemented.
 */
- (void)goBackForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError * _Nullable error))completionHandler;

/*!
 @abstract Called to navigate the tab to the next page in its history.
 @param context The context in which the web extension is running.
 @param completionHandler A block that must be called upon completion. It takes a single error argument,
 which should be provided if any errors occurred.
 @discussion Navigates to the next page in the main web view via `goForward` if not implemented.
 */
- (void)goForwardForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError * _Nullable error))completionHandler;

/*!
 @abstract Called to activate the tab, making it frontmost.
 @param context The context in which the web extension is running.
 @param completionHandler A block that must be called upon completion. It takes a single error argument,
 which should be provided if any errors occurred.
 @discussion Upon activation, the tab should become the frontmost and either be the sole selected tab or
 be included among the selected tabs. No action is performed if not implemented.
 @seealso selectForWebExtensionContext:extendSelection:completionHandler:
 */
- (void)activateForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError * _Nullable error))completionHandler;

/*!
 @abstract Called when the selected state of the tab is needed.
 @param context The context in which the web extension is running.
 @return `YES` if the tab is selected, `NO` otherwise.
 @discussion Defaults to `YES` for the active tab and `NO` for other tabs if not implemented.
 */
- (BOOL)isSelectedForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called to select the tab, adding it to the current tab selection.
 @param context The context in which the web extension is running.
 @param completionHandler A block that must be called upon completion. It takes a single error argument,
 which should be provided if any errors occurred.
 @discussion This is equivalent to the user command-clicking on the tab to add it to a selection.
 The method should add the tab to the current selection without changing the active tab. No action is performed if not implemented.
 @seealso isSelectedForWebExtensionContext:
 @seealso deselectForWebExtensionContext:completionHandler:
 */
- (void)selectForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError * _Nullable error))completionHandler;

/*!
 @abstract Called to deselect the tab, removing it from the current tab selection.
 @param context The context in which the web extension is running.
 @param completionHandler A block that must be called upon completion. It takes a single error argument,
 which should be provided if any errors occurred.
 @discussion This is equivalent to the user command-clicking on an already selected tab to remove it from the selection.
 The method should remove the tab from the current selection without changing the active tab. If the tab is the active tab, it should
 remain selected and active or another tab should become selected and active in its place. No action is performed if not implemented.
 @seealso isSelectedForWebExtensionContext:
 @seealso selectForWebExtensionContext:completionHandler:
 */
- (void)deselectForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError * _Nullable error))completionHandler;

/*!
 @abstract Called to duplicate the tab.
 @param context The context in which the web extension is running.
 @param options The tab creation options influencing the duplicated tab's properties.
 @param completionHandler A block that must be called upon completion. It takes two arguments:
 the duplicated tab (or \c nil if no tab was created) and an error, which should be provided if any errors occurred.
 @discussion This is equivalent to the user selecting to duplicate the tab through a menu item, with the specified options.
 No action is performed if not implemented.
 */
- (void)duplicateForWebExtensionContext:(_WKWebExtensionContext *)context withOptions:(_WKWebExtensionTabCreationOptions *)options completionHandler:(void (^)(id <_WKWebExtensionTab> WK_NULLABLE_RESULT duplicatedTab, NSError * _Nullable error))completionHandler;

/*!
 @abstract Called to close the tab.
 @param context The context in which the web extension is running.
 @param completionHandler A block that must be called upon completion. It takes a single error argument,
 which should be provided if any errors occurred.
 @discussion No action is performed if not implemented.
 */
- (void)closeForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError * _Nullable error))completionHandler;

@end

NS_ASSUME_NONNULL_END
