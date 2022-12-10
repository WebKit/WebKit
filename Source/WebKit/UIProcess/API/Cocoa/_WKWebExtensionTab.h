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
@protocol _WKWebExtensionWindow;

NS_ASSUME_NONNULL_BEGIN

/*!
 @abstract Constants used by @link WKWebExtensionController @/link to indicate tab changes.
 @constant WKWebExtensionTabChangedPropertiesNone  Indicates nothing changed.
 @constant WKWebExtensionTabChangedPropertiesLoading  Indicates the loading state changed.
 @constant WKWebExtensionTabChangedPropertiesTitle  Indicates the title changed.
 @constant WKWebExtensionTabChangedPropertiesURL  Indicates the URL changed.
 @constant WKWebExtensionTabChangedPropertiesSize  Indicates the size changed.
 @constant WKWebExtensionTabChangedPropertiesZoomFactor  Indicates the zoom factor changed.
 @constant WKWebExtensionTabChangedPropertiesAudible  Indicates the audible state changed.
 @constant WKWebExtensionTabChangedPropertiesMuted  Indicates the muted state changed.
 @constant WKWebExtensionTabChangedPropertiesPinned  Indicates the pinned state changed.
 @constant WKWebExtensionTabChangedPropertiesReaderMode  Indicates the reader mode state changed.
 @constant WKWebExtensionTabChangedPropertiesAll  Indicates all properties changed.
 */
typedef NS_OPTIONS(NSUInteger, _WKWebExtensionTabChangedProperties) {
    _WKWebExtensionTabChangedPropertiesNone       = 0,
    _WKWebExtensionTabChangedPropertiesLoading    = 1 << 1,
    _WKWebExtensionTabChangedPropertiesTitle      = 1 << 2,
    _WKWebExtensionTabChangedPropertiesURL        = 1 << 3,
    _WKWebExtensionTabChangedPropertiesSize       = 1 << 4,
    _WKWebExtensionTabChangedPropertiesZoomFactor = 1 << 5,
    _WKWebExtensionTabChangedPropertiesAudible    = 1 << 6,
    _WKWebExtensionTabChangedPropertiesMuted      = 1 << 7,
    _WKWebExtensionTabChangedPropertiesPinned     = 1 << 8,
    _WKWebExtensionTabChangedPropertiesReaderMode = 1 << 9,
    _WKWebExtensionTabChangedPropertiesAll        = NSUIntegerMax,
} WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
@protocol _WKWebExtensionTab <NSObject>
@optional

/*!
 @abstract Called when the parent tab for the tab is needed.
 @param context The context in which the web extension is running.
 @return The parent tab of the tab, if the tab was opened from another tab.
 @discussion Defaults to `nil` if not implemented.
 */
- (id <_WKWebExtensionTab>)parentTabForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called when the window containing the tab is needed.
 @param context The context in which the web extension is running.
 @return The window containing the tab.
 @discussion Defaults to `nil` if not implemented.
 */
- (id <_WKWebExtensionWindow>)windowForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called when the main web view for the window is needed.
 @param context The context in which the web extension is running.
 @return The main web view for the tab.
 @discussion Defaults to `nil` if not implemented.
 */
- (WKWebView *)mainWebViewForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called when the web views for the window are needed.
 @param context The context in which the web extension is running.
 @return An array of web views for the tab.
 @discussion Defaults to an empty array if not implemented.
 */
- (NSArray<WKWebView *> *)webViewsForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called when the title of the tab is needed.
 @param context The context in which the web extension is running.
 @return The title of the tab.
 @discussion Defaults to an empty string if not implemented.
 */
- (NSString *)tabTitleForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called when the selected state of the tab is needed.
 @param context The context in which the web extension is running.
 @return `YES` if the tab is selected, `NO` otherwise.
 @discussion Defaults to `NO` if not implemented.
 */
- (BOOL)isSelectedForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called when the pinned state of the tab is needed.
 @param context The context in which the web extension is running.
 @return `YES` if the tab is pinned, `NO` otherwise.
 @discussion Defaults to `NO` if not implemented.
 */
- (BOOL)isPinnedTabForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called when the ephemeral state of the tab is needed.
 @param context The context in which the web extension is running.
 @return `YES` if the tab is ephemeral, `NO` otherwise.
 @discussion Used to indicated "private browsing" windows. Defaults to `NO` if not implemented.
 */
- (BOOL)isEphemeralForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called to check if reader mode is available for the tab.
 @param context The context in which the web extension is running.
 @return `YES` if reader mode is available for the tab, `NO` otherwise.
 @discussion Defaults to `NO` if not implemented.
 */
- (BOOL)isReaderModeAvailableForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called to check if the tab is currently showing reader mode.
 @param context The context in which the web extension is running.
 @return `YES` if the tab is showing reader mode, `NO` otherwise.
 @discussion Defaults to `NO` if not implemented.
 */
- (BOOL)isShowingReaderModeForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called to toggle reader mode for the tab.
 @param context The context in which the web extension is running.
 @discussion No action is performed if not implemented.
 */
- (void)toggleReaderModeForWebExtensionContext:(_WKWebExtensionContext *)context;

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
 @discussion No action is performed if not implemented.
 */
- (void)muteForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called to unmute the tab.
 @param context The context in which the web extension is running.
 @discussion No action is performed if not implemented.
 */
- (void)unmuteForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called when the size of the tab in the window is needed.
 @param context The context in which the web extension is running.
 @return The size of the tab.
 @discussion Defaults to `CGSizeZero` if not implemented.
 */
- (CGSize)sizeForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called when the zoom factor of the tab is needed.
 @param context The context in which the web extension is running.
 @return The zoom factor of the tab.
 @discussion Defaults to `1.0` if not implemented.
 */
- (double)zoomFactorForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called when the URL of the tab is needed.
 @param context The context in which the web extension is running.
 @return The URL of the tab.
 @discussion Defaults to `nil` if not implemented.
 */
- (NSURL *)urlForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called when the pending URL of the tab is needed.
 @param context The context in which the web extension is running.
 @return The pending URL of the tab.
 @discussion The pending URL is the URL of a page that is in the process of loading. If there is no pending URL, return `nil`.
 Defaults to `nil` if not implemented.
 */
- (NSURL *)pendingURLForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called to check if the tab has finished loading.
 @param context The context in which the web extension is running.
 @return `YES` if the tab has finished loading, `NO` otherwise.
 @discussion Defaults to `YES` if not implemented.
 */
- (BOOL)isLoadingCompleteForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called to detect the locale of the webpage currently loaded in the tab.
 @param context The context in which the web extension is running.
 @param completionHandler A block to be called when the locale has been detected. The block takes a single argument, the
 detected locale, which may be `nil` if no locale could be detected.
 @discussion No language detection is performed if not implemented.
 */
- (void)detectWebpageLocaleForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSLocale * _Nullable locale))completionHandler;

/*!
 @abstract Called to load a URL in the tab.
 @param url The URL to be loaded in the tab.
 @param context The context in which the web extension is running.
 @discussion If the tab is already loading a page, calling this method should stop the current page from loading and start loading the new URL.
 Loads the URL in the main web view if not implemented.
 */
- (void)loadURL:(NSURL *)url forWebExtensionContext:(_WKWebExtensionContext *)context NS_SWIFT_NAME(load(url:forWebExtensionContext:));

/*!
 @abstract Called to reload the current page in the tab.
 @param context The context in which the web extension is running.
 @discussion Reloads the main web view if not implemented.
 */
- (void)reloadForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called to reload the current page in the tab, bypassing the cache.
 @param context The context in which the web extension is running.
 @discussion Reloads the main web view, bypassing the cache, if not implemented.
 */
- (void)reloadFromOriginForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called to navigate the tab to the previous page in its history.
 @param context The context in which the web extension is running.
 @discussion Navigates to the previous page in the main web view if not implemented.
 */
- (void)goBackForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called to navigate the tab to the next page in its history.
 @param context The context in which the web extension is running.
 @discussion Navigates to the next page in the main web view if not implemented.
 */
- (void)goForwardForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called to close the tab.
 @param context The context in which the web extension is running.
 @discussion No action is performed if not implemented.
 */
- (void)closeForWebExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Called to select the tab.
 @param context The context in which the web extension is running.
 @discussion This is equivalent to the user clicking on the tab in a tab bar. No action is performed if not implemented.
 */
- (void)selectForWebExtensionContext:(_WKWebExtensionContext *)context;

@end

NS_ASSUME_NONNULL_END
