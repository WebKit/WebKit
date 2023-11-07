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

@class WKWebView;
@class _WKWebExtensionContext;
@protocol _WKWebExtensionTab;

#if TARGET_OS_IPHONE
@class UIImage;
#else
@class NSImage;
#endif

NS_ASSUME_NONNULL_BEGIN

/*! @abstract This notification is sent whenever a @link WKWebExtensionAction has changed properties. */
WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
WK_EXTERN NSNotificationName const _WKWebExtensionActionPropertiesDidChangeNotification NS_SWIFT_NAME(_WKWebExtensionAction.propertiesDidChangeNotification);

/*! @abstract This notification is sent when the `intrinsicContentSize` of the popup web view associated with a @link WKWebExtensionAction changes. */
WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
WK_EXTERN NSNotificationName const _WKWebExtensionActionPopupWebViewContentSizeDidChangeNotification NS_SWIFT_NAME(_WKWebExtensionAction.popupWebViewContentSizeDidChangeNotification);

/*! @abstract This notification is sent when the popup web view associated with a @link WKWebExtensionAction is closed. */
WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
WK_EXTERN NSNotificationName const _WKWebExtensionActionPopupWebViewDidCloseNotification NS_SWIFT_NAME(_WKWebExtensionAction.popupWebViewDidCloseNotification);

/*!
 @abstract A `WKWebExtensionAction` object encapsulates the properties for an individual web extension action.
 @discussion Provides access to action properties such as popup, icon, and title, with tab-specific values.
 */
WK_CLASS_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
NS_SWIFT_NAME(_WKWebExtension.Action)
@interface _WKWebExtensionAction : NSObject

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)new NS_UNAVAILABLE;

/*! @abstract The extension context to which this action is related. */
@property (nonatomic, readonly, weak) _WKWebExtensionContext *webExtensionContext;

/*!
 @abstract The tab that this action is associated with, or `nil` if it is the default action.
 @discussion When this property is `nil`, it indicates that the action is the default action and not associated with a specific tab.
 */
@property (nonatomic, readonly, nullable, weak) id <_WKWebExtensionTab> associatedTab;

/*!
 @abstract Returns the action icon for the specified size.
 @param size The size to use when looking up the action icon.
 @result The action icon, or `nil` if the icon was unable to be loaded.
 @discussion This icon should represent the extension in action sheets or toolbars. The returned image will be the best match for the specified
 size that is available in the extension's action icon set. If no matching icon is available, the method will fall back to the extension's icon.
 */
#if TARGET_OS_IPHONE
- (nullable UIImage *)iconForSize:(CGSize)size;
#else
- (nullable NSImage *)iconForSize:(CGSize)size;
#endif

/*! @abstract The localized display label for the action. */
@property (nonatomic, readonly, copy) NSString *label;

/*! @abstract The badge text for the action. */
@property (nonatomic, readonly, copy) NSString *badgeText;

/*! @abstract A Boolean value indicating whether the action is enabled. */
@property (nonatomic, readonly, getter=isEnabled) BOOL enabled;

/*!
 @abstract A Boolean value indicating whether the action has a popup.
 @discussion Use this property to check if the action has a popup before attempting to access the `popupWebView` property.
 @seealso popupWebView
 */
@property (nonatomic, readonly) BOOL presentsPopup;

/*!
 @abstract A web view loaded with the popup page for this action, or `nil` if no popup is specified.
 @discussion The web view will be preloaded with the popup page upon first access or after it has been closed. Use the `hasPopup`
 property to determine whether a popup should be displayed before accessing this property.
 @seealso hasPopup
 */
@property (nonatomic, readonly, nullable) WKWebView *popupWebView;

/*!
 @abstract Should be called by the app to close the popup's web view.
 @discussion This method should be called when the popup web view is no longer presented to the user, indicating that it can safely be closed.
 */
- (void)closePopupWebView;

@end

NS_ASSUME_NONNULL_END
