// Copyright © 2025  All rights reserved.

#import <Foundation/Foundation.h>


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
} NS_SWIFT_NAME(_WKWebExtension.BookmarkType) WK_API_AVAILABLE(macos(15.4), ios(18.4), visionos(2.4));

/*! @abstract A class conforming to the ``_WKWebExtensionBookmark`` protocol represents a single bookmark node (a bookmark, folder, or separator) to web extensions. */
WK_API_AVAILABLE(macos(15.4), ios(18.4), visionos(2.4)) WK_SWIFT_UI_ACTOR
@protocol _WKWebExtensionBookmark <NSObject>
@optional

/*!
 @abstract Called when the unique identifier for the bookmark node is needed.
 @param context The context in which the web extension is running.
 @return A string uniquely identifying this bookmark node.
 */
- (NSString *)identifierForWebExtensionContext:(WKWebExtensionContext *)context NS_SWIFT_NAME(identifier(for:));

/*!
 @abstract Called when the identifier of the parent folder is needed.
 @param context The context in which the web extension is running.
 @return The unique identifier of the parent folder, or `nil` if the node is at the root level.
 */
- (nullable NSString *)parentIdentifierForWebExtensionContext:(WKWebExtensionContext *)context NS_SWIFT_NAME(parentIdentifier(for:));

/*!
 @abstract Called when the title of the bookmark node is needed.
 @param context The context in which the web extension is running.
 @return The user-visible title of the bookmark or folder.
 */
- (nullable NSString *)titleForWebExtensionContext:(WKWebExtensionContext *)context NS_SWIFT_NAME(title(for:));

/*!
 @abstract Called when the URL of the bookmark is needed.
 @param context The context in which the web extension is running.
 @return The URL the bookmark points to. This should be `nil` for folders.
 */
- (nullable NSString *)urlForWebExtensionContext:(WKWebExtensionContext *)context NS_SWIFT_NAME(url(for:));

/*!
 @abstract Called when the type of the bookmark node is needed.
 @param context The context in which the web extension is running.
 @return The type of the bookmark node.
 */
- (_WKWebExtensionBookmarkType)bookmarkTypeForWebExtensionContext:(WKWebExtensionContext *)context NS_SWIFT_NAME(bookmarkType(for:));

/*!
 @abstract Called when the children of a folder are needed.
 @param context The context in which the web extension is running.
 @return An array of bookmark nodes contained within this folder. Should be `nil` if the node is not a folder.
 */
- (nullable NSArray<id <_WKWebExtensionBookmark>> *)childrenForWebExtensionContext:(WKWebExtensionContext *)context NS_SWIFT_NAME(children(for:));

/*!
 @abstract Called when the zero-based index of this node within its parent folder is needed.
 @param context The context in which the web extension is running.
 @return The index of the bookmark node.
 */
- (NSInteger)indexForWebExtensionContext:(WKWebExtensionContext *)context NS_SWIFT_NAME(index(for:));

@end

WK_HEADER_AUDIT_END(nullability, sendability)

