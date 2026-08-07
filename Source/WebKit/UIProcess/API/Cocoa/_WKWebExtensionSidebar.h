/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
@class WKWebView;
@protocol WKWebExtensionTab;
@protocol WKWebExtensionWindow;

#if TARGET_OS_IPHONE
@class UIImage;
@class UIViewController;
#else
@class NSImage;
@class NSViewController;
#endif

WK_HEADER_AUDIT_BEGIN(nullability, sendability)

/*!
 @abstract A `_WKWebExtensionSidebar` object encapsulates the properties of a web extension sidebar.
 @discussion An extension may specify a sidebar for a particular tab or for a whole window. Use
 ``-[WKWebExtensionContext sidebarForTab:]`` to obtain the sidebar which applies to a tab. Every tab in a
 window which the extension has not set unique properties for is given the same sidebar object, so comparing successive
 results by identity tells the app whether anything needs to change when the user switches tabs. The sidebar pane
 itself is shown or hidden per window; when a tab has a sidebar of its own, the app swaps between sidebars by
 closing the one being replaced and opening the one taking its place.
 */
WK_CLASS_AVAILABLE(macos(15.2), ios(18.2), visionos(2.2))
@interface _WKWebExtensionSidebar : NSObject

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)new NS_UNAVAILABLE;

/*! @abstract The extension context to which this sidebar is related. */
@property (nonatomic, nullable, readonly, weak) WKWebExtensionContext *webExtensionContext;

/*!
 @abstract The window this sidebar belongs to.
 */
@property (nonatomic, readonly, weak) id <WKWebExtensionWindow> associatedWindow;

/*!
 @abstract The tab this sidebar is specific to, or `nil` if it applies to its whole window.
 @discussion When this property is nil, this sidebar should be the global default for ``associatedWindow``. Otherwise,
 it should only be showed alongside ``associatedTab``.
 */
@property (nonatomic, nullable, readonly, weak) id <WKWebExtensionTab> associatedTab;

/*! @abstract The title of this sidebar. */
@property (nonatomic, readonly, copy) NSString *title;

/*!
 @abstract Get the sidebar icon of the given size.
 @param size The size to use when looking up the sidebar icon.
 @result The sidebar icon, or the action icon if the sidebar specifies no icon, or `nil` if the action icon was unable to be loaded.
 */
#if TARGET_OS_IPHONE
- (nullable UIImage *)iconForSize:(CGSize)size;
#else
- (nullable NSImage *)iconForSize:(CGSize)size;
#endif

/*! @abstract Whether this sidebar is enabled or not. */
@property (nonatomic, readonly, getter=isEnabled) BOOL enabled;

/*!
 @abstract The web view which renders this sidebar's content.
 @discussion Sidebars which show the same document share a web view, so this may return the same web view for
 several sidebars. Other properties, such as title and icon, may still differ in cases where different sidebars share the
 same WKWebView.
 */
@property (nonatomic, readonly) WKWebView *webView;

#if TARGET_OS_IPHONE
/*!
 @abstract A view controller which presents this sidebar's web view.
 @discussion As with ``webView``, this may return the same view controller for several sidebars.
 */
@property (nonatomic, readonly) UIViewController *viewController;
#endif

#if TARGET_OS_OSX
/*!
 @abstract A view controller which presents this sidebar's web view.
 @discussion As with ``webView``, this may return the same view controller for several sidebars.
 */
@property (nonatomic, readonly) NSViewController *viewController;
#endif

/*!
 @abstract Indicate that the sidebar will be opened.
 @param fromUserInteraction Whether the sidebar is opening due to the user.
 @discussion This method should be invoked by the browser before this sidebar is displayed. If it is not called, then the
 ``WKWebView`` associated with this sidebar may not have a document loaded. It also marks this particular sidebar as the one
 on screen, so when the app replaces one sidebar with another -- because the user moved to a tab which has a sidebar of its
 own, for instance -- it should call ``willCloseSidebar`` on the one being replaced and this on the one taking its place.
 Pass `YES` for `fromUserInteraction` only when the sidebar is opening due to direct user action, since that will grant
 the extension the `activeTab` permission for the tab this sidebar applies to. Pass `NO` when the sidebar is opening as a side
 effect of some other action (e.g. switching tabs), or generally not due to direct user intent.
 */
- (void)willOpenSidebarFromUserInteraction:(BOOL)fromUserInteraction;

/*!
 @abstract Indicate that the sidebar will be closed
 @discussion This method should be invoked by the browser when the sidebar will be closed -- i.e., its associated ``WKWebView`` will cease
 to be displayed. If this method is not called when the sidebar is closed, then the sidebar's associated ``WKWebView`` may remain active longer than
 necessary. Note that calling this method does not guarantee that the ``WKWebView`` associated with a particular sidebar will be deallocated, as the
 web view may be shared between mutliple sidebars. This should also be called on a sidebar which is being replaced by another (e.g. due to the user
 switching tabs).
 */
- (void)willCloseSidebar;

@end // interface _WKWebExtensionSidebar

WK_HEADER_AUDIT_END(nullability, sendability)
