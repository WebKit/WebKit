/*
 * Copyright (C) 2025-2026 Apple Inc. All rights reserved.
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
@class _WKWebExtensionBookmarkConfiguration;
@protocol _WKWebExtensionBookmark;

NS_HEADER_AUDIT_BEGIN(nullability, sendability)

/*!
 @abstract Constants used by ``_WKWebExtensionBookmark`` to indicate the type of a bookmark node.
 @constant _WKWebExtensionBookmarkTypeBookmark Indicates the node is a bookmark with a URL.
 @constant _WKWebExtensionBookmarkTypeFolder Indicates the node is a folder that can contain other bookmarks or folders.
 */
typedef NS_ENUM(NSInteger, _WKWebExtensionBookmarkType) {
    _WKWebExtensionBookmarkTypeBookmark,
    _WKWebExtensionBookmarkTypeFolder,
} NS_SWIFT_NAME(_WKWebExtension.BookmarkType) WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA), visionos(WK_XROS_TBA));

/*!
 @abstract Constants used by ``_WKWebExtensionBookmark`` to indicate which built-in folder a bookmark node is.
 @constant _WKWebExtensionBookmarkFolderTypeNone Indicates the node is a bookmark or an ordinary folder.
 @constant _WKWebExtensionBookmarkFolderTypeBookmarksBar Indicates the folder is the bookmarks bar.
 @constant _WKWebExtensionBookmarkFolderTypeManaged Indicates the folder holds bookmarks managed by an administrator.
 @constant _WKWebExtensionBookmarkFolderTypeOther Indicates the folder is the default folder for new bookmarks.
 */
typedef NS_ENUM(NSInteger, _WKWebExtensionBookmarkFolderType) {
    _WKWebExtensionBookmarkFolderTypeNone,
    _WKWebExtensionBookmarkFolderTypeBookmarksBar,
    _WKWebExtensionBookmarkFolderTypeManaged,
    _WKWebExtensionBookmarkFolderTypeOther,
} NS_SWIFT_NAME(_WKWebExtension.BookmarkFolderType) WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA), visionos(WK_XROS_TBA));

/*! @abstract A class conforming to the ``_WKWebExtensionBookmark`` protocol represents a bookmark or folder to web extensions. */
WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA), visionos(WK_XROS_TBA)) NS_SWIFT_UI_ACTOR
@protocol _WKWebExtensionBookmark <NSObject>
@optional

/*!
 @abstract Called when the unique identifier for the bookmark node is needed.
 @param context The context in which the web extension is running.
 @return An opaque string that uniquely identifies the bookmark or folder.
 @discussion The identifier must stay the same across calls and relaunches, because extensions can store it, and it must not
 be `0`, which identifies the root.
 */
- (NSString *)identifierForWebExtensionContext:(WKWebExtensionContext *)context NS_SWIFT_NAME(identifier(for:));

/*!
 @abstract Called when the type of the node is needed.
 @param context The context in which the web extension is running.
 @return Whether the node is a bookmark or a folder.
 @discussion Defaults to ``_WKWebExtensionBookmarkTypeFolder`` if the node has no URL, otherwise
 ``_WKWebExtensionBookmarkTypeBookmark``, if not implemented.
 */
- (_WKWebExtensionBookmarkType)bookmarkTypeForWebExtensionContext:(WKWebExtensionContext *)context NS_SWIFT_NAME(bookmarkType(for:));

/*!
 @abstract Called when the kind of built-in folder is needed.
 @param context The context in which the web extension is running.
 @return The kind of built-in folder, or ``_WKWebExtensionBookmarkFolderTypeNone`` for a bookmark or an ordinary folder.
 @discussion Defaults to ``_WKWebExtensionBookmarkFolderTypeNone`` if not implemented.
 */
- (_WKWebExtensionBookmarkFolderType)folderTypeForWebExtensionContext:(WKWebExtensionContext *)context NS_SWIFT_NAME(folderType(for:));

/*!
 @abstract Called when the title of the bookmark or folder is needed.
 @param context The context in which the web extension is running.
 @return The title of the bookmark or folder.
 @discussion Defaults to an empty string if not implemented.
 @seealso setTitle:forWebExtensionContext:completionHandler:
 */
- (nullable NSString *)titleForWebExtensionContext:(WKWebExtensionContext *)context NS_SWIFT_NAME(title(for:));

/*!
 @abstract Called to set the title of the bookmark or folder.
 @param title The title that should be set.
 @param context The context in which the web extension is running.
 @param completionHandler A block that must be called upon completion. It takes a single error argument,
 which should be provided if any errors occurred.
 @discussion No action is performed if not implemented.
 @seealso titleForWebExtensionContext:
 */
- (void)setTitle:(NSString *)title forWebExtensionContext:(WKWebExtensionContext *)context completionHandler:(void (^)(NSError * _Nullable error))completionHandler NS_SWIFT_NAME(setTitle(_:for:completionHandler:));

/*!
 @abstract Called when the URL of the bookmark is needed.
 @param context The context in which the web extension is running.
 @return The URL of the bookmark, or `nil` for a folder.
 @discussion Defaults to `nil` if not implemented.
 @seealso setURL:forWebExtensionContext:completionHandler:
 */
- (nullable NSURL *)urlForWebExtensionContext:(WKWebExtensionContext *)context NS_SWIFT_NAME(url(for:));

/*!
 @abstract Called to set the URL of the bookmark.
 @param url The URL that should be set.
 @param context The context in which the web extension is running.
 @param completionHandler A block that must be called upon completion. It takes a single error argument,
 which should be provided if any errors occurred.
 @discussion No action is performed if not implemented. This is never called for a folder.
 @seealso urlForWebExtensionContext:
 */
- (void)setURL:(NSURL *)url forWebExtensionContext:(WKWebExtensionContext *)context completionHandler:(void (^)(NSError * _Nullable error))completionHandler NS_SWIFT_NAME(setURL(_:for:completionHandler:));

/*!
 @abstract Called when the date the bookmark or folder was added is needed.
 @param context The context in which the web extension is running.
 @return The date it was added, or `nil` if it is not known.
 @discussion Defaults to `nil` if not implemented, which reports a date added of `0` to the extension.
 */
- (nullable NSDate *)dateAddedForWebExtensionContext:(WKWebExtensionContext *)context NS_SWIFT_NAME(dateAdded(for:));

/*!
 @abstract Called when the managed state of the bookmark or folder is needed.
 @param context The context in which the web extension is running.
 @return `YES` if an administrator manages the bookmark or folder, `NO` otherwise.
 @discussion Defaults to `NO` if not implemented. Extensions cannot change managed bookmarks and folders.
 */
- (BOOL)isManagedForWebExtensionContext:(WKWebExtensionContext *)context NS_SWIFT_NAME(isManaged(for:));

/*!
 @abstract Called when the synced state of the bookmark or folder is needed.
 @param context The context in which the web extension is running.
 @return `YES` if the bookmark or folder syncs to the user's account, `NO` otherwise.
 @discussion Defaults to `NO` if not implemented. Managed bookmarks and folders always report `NO`.
 */
- (BOOL)isSyncedForWebExtensionContext:(WKWebExtensionContext *)context NS_SWIFT_NAME(isSynced(for:));

/*!
 @abstract Called when the children of the folder are needed.
 @param context The context in which the web extension is running.
 @return The bookmarks and folders in the folder, in order, or `nil` for a bookmark.
 @discussion Defaults to `nil` if not implemented.
 */
- (nullable NSArray<id<_WKWebExtensionBookmark>> *)childrenForWebExtensionContext:(WKWebExtensionContext *)context NS_SWIFT_NAME(children(for:));

/*!
 @abstract Called to create a bookmark or folder in the folder.
 @param configuration The configuration for the new bookmark or folder.
 @param context The context in which the web extension is running.
 @param completionHandler A block that must be called upon completion. It takes the new bookmark or folder and an error
 argument, which should be provided if any errors occurred.
 @discussion No action is performed if not implemented.
 */
- (void)createChildUsingConfiguration:(_WKWebExtensionBookmarkConfiguration *)configuration forWebExtensionContext:(WKWebExtensionContext *)context completionHandler:(void (^)(id<_WKWebExtensionBookmark> _Nullable newBookmark, NSError * _Nullable error))completionHandler NS_SWIFT_NAME(createChild(using:for:completionHandler:));

/*!
 @abstract Called to move the bookmark or folder into a folder.
 @param parent The folder that should become the parent.
 @param index The index in the new parent where the bookmark or folder should be inserted. When the new parent is the
 current parent, the index counts the children before the move.
 @param context The context in which the web extension is running.
 @param completionHandler A block that must be called upon completion. It takes a single error argument,
 which should be provided if any errors occurred.
 @discussion No action is performed if not implemented. The parent is never the bookmark or folder itself or one of its
 descendants, and the index is never greater than the number of children.
 */
- (void)moveToParent:(id<_WKWebExtensionBookmark>)parent atIndex:(NSUInteger)index forWebExtensionContext:(WKWebExtensionContext *)context completionHandler:(void (^)(NSError * _Nullable error))completionHandler NS_SWIFT_NAME(move(toParent:index:for:completionHandler:));

/*!
 @abstract Called to remove the bookmark or folder.
 @param context The context in which the web extension is running.
 @param completionHandler A block that must be called upon completion. It takes a single error argument,
 which should be provided if any errors occurred.
 @discussion If the node is a folder, the bookmarks and folders in it are removed too. No action is performed if not
 implemented.
 */
- (void)removeForWebExtensionContext:(WKWebExtensionContext *)context completionHandler:(void (^)(NSError * _Nullable error))completionHandler NS_SWIFT_NAME(remove(for:completionHandler:));

@end

NS_HEADER_AUDIT_END(nullability, sendability)
