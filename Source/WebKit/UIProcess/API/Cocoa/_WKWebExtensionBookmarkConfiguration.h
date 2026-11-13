/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#import <WebKit/_WKWebExtensionBookmarks.h>

NS_HEADER_AUDIT_BEGIN(nullability, sendability)

/*!
 @abstract A ``_WKWebExtensionBookmarkConfiguration`` object encapsulates configuration options for a new bookmark or folder in an extension.
 @discussion This class holds various options that influence the initial state of a bookmark or folder.
 The app retains the discretion to disregard any or all of these options, or even opt not to create a bookmark or folder.
 */
WK_CLASS_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA), visionos(WK_XROS_TBA))
NS_SWIFT_UI_ACTOR NS_SWIFT_NAME(_WKWebExtension.BookmarkConfiguration)
@interface _WKWebExtensionBookmarkConfiguration : NSObject

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

/*! @abstract Indicates whether a bookmark or a folder should be created. */
@property (nonatomic, readonly) _WKWebExtensionBookmarkType type;

/*! @abstract Indicates the title for the bookmark or folder. */
@property (nonatomic, readonly, copy) NSString *title;

/*!
 @abstract Indicates the URL for the bookmark.
 @discussion If ``type`` is ``_WKWebExtensionBookmarkTypeFolder``, this property is `nil`.
 */
@property (nonatomic, nullable, readonly, copy) NSURL *url;

/*!
 @abstract Indicates the position in the parent folder where the bookmark or folder should be inserted.
 @discussion The index is never greater than the number of children in the parent folder.
 */
@property (nonatomic, readonly) NSUInteger index;

@end

NS_HEADER_AUDIT_END(nullability, sendability)
