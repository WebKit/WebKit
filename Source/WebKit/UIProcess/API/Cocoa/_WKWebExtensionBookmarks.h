// Copyright © 2025  All rights reserved.

#import <Foundation/Foundation.h>
#import <WebKit/WKFoundation.h>


@class WKWebExtensionContext;
@protocol _WKWebExtensionBookmark;

WK_HEADER_AUDIT_BEGIN(nullability, sendability)

/*!
 @abstract Constants used by ``_WKWebExtensionBookmark`` to indicate the type of a bookmark node.
 @constant _WKWebExtensionBookmarkTypeBookmark  Indicates the node is a bookmark with a URL.
 @constant _WKWebExtensionBookmarkTypeFolder  Indicates the node is a folder that can contain other bookmarks or folders.
 @constant _WKWebExtensionBookmarkTypeSeparator  Indicates the node is a separator.
 */
typedef NS_ENUM(NSInteger, _WKWebExtensionBookmarkType) {
    _WKWebExtensionBookmarkTypeBookmark,
    _WKWebExtensionBookmarkTypeFolder,
} NS_SWIFT_NAME(_WKWebExtension.BookmarkType) WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA), visionos(WK_XROS_TBA));

/*! @abstract A class conforming to the ``_WKWebExtensionBookmark`` protocol represents a single bookmark node (a bookmark or folder) to web extensions. */
WK_SWIFT_UI_ACTOR
@protocol _WKWebExtensionBookmark <NSObject>
@optional

/*!
 @abstract Called when the unique identifier for the bookmark node is needed.
 @param context The context in which the web extension is running.
 @return A string uniquely identifying this bookmark node.
 */
- (NSString *)identifierForWebExtensionContext:(WKWebExtensionContext * _Nonnull)context NS_SWIFT_NAME(identifier(for:));

/*!
 @abstract Called when the identifier of the parent folder is needed.
 @param context The context in which the web extension is running.
 @return The unique identifier of the parent folder, or `nil` if the node is at the root level.
 */
- (nullable NSString *)parentIdentifierForWebExtensionContext:(WKWebExtensionContext * _Nonnull)context NS_SWIFT_NAME(parentIdentifier(for:));

/*!
 @abstract Called when the title of the bookmark node is needed.
 @param context The context in which the web extension is running.
 @return The user-visible title of the bookmark or folder.
 */
- (nullable NSString *)titleForWebExtensionContext:(WKWebExtensionContext * _Nonnull)context NS_SWIFT_NAME(title(for:));

/*!
 @abstract Called when the URL of the bookmark is needed.
 @param context The context in which the web extension is running.
 @return The URL the bookmark points to. This should be `nil` for folders.
 */
- (nullable NSString *)urlStringForWebExtensionContext:(WKWebExtensionContext * _Nonnull)context NS_SWIFT_NAME(url(for:));

/*!
 @abstract Called when the type of the bookmark node is needed.
 @param context The context in which the web extension is running.
 @return The type of the bookmark node.
 */
- (_WKWebExtensionBookmarkType)bookmarkTypeForWebExtensionContext:(WKWebExtensionContext * _Nonnull)context NS_SWIFT_NAME(bookmarkType(for:));

/*!
 @abstract Called when the children of a folder are needed.
 @param context The context in which the web extension is running.
 @return An array of bookmark nodes contained within this folder. Should be `nil` if the node is not a folder.
 */
- (nullable NSArray<id<_WKWebExtensionBookmark>> *)childrenForWebExtensionContext:(WKWebExtensionContext * _Nonnull)context NS_SWIFT_NAME(children(for:));

/*!
 @abstract Called when the zero-based index of this node within its parent folder is needed.
 @param context The context in which the web extension is running.
 @return The index of the bookmark node.
 */
- (NSInteger)indexForWebExtensionContext:(WKWebExtensionContext * _Nonnull)context NS_SWIFT_NAME(index(for:));

/*!
 @abstract Called when the date the bookmark was added is needed.
 @param context The context in which the web extension is running.
 @return The date the bookmark was added. Should be `nil` for folders or separators.
 */
- (nullable NSDate *)dateAddedForWebExtensionContext:(WKWebExtensionContext * _Nonnull)context NS_SWIFT_NAME(dateAdded(for:));

@end

WK_HEADER_AUDIT_END(nullability, sendability)

