/*
 * Copyright (C) 2023-2024 Apple Inc. All rights reserved.
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

#if ENABLE(WK_WEB_EXTENSIONS)

#ifdef __OBJC__

#import <WebKit/WKWebExtensionAction.h>
#import <WebKit/WKWebExtensionContextPrivate.h>
#import <WebKit/WKWebExtensionControllerConfigurationPrivate.h>
#import <WebKit/WKWebExtensionControllerDelegatePrivate.h>
#import <WebKit/WKWebExtensionControllerPrivate.h>
#import <WebKit/WKWebExtensionMatchPattern.h>
#import <WebKit/WKWebExtensionMessagePort.h>
#import <WebKit/WKWebExtensionPermission.h>
#import <WebKit/WKWebExtensionPrivate.h>
#import <WebKit/WKWebExtensionTab.h>
#import <WebKit/WKWebExtensionTabConfiguration.h>
#import <WebKit/WKWebExtensionWindow.h>
#import <WebKit/WKWebExtensionWindowConfiguration.h>
#import <WebKit/WebKit.h>

@interface TestWebExtensionsDelegate : NSObject <WKWebExtensionControllerDelegate, WKWebExtensionControllerDelegatePrivate>

@property (nonatomic, copy) NSArray<id <WKWebExtensionWindow>> *(^openWindows)(WKWebExtensionContext *);
@property (nonatomic, copy) id <WKWebExtensionWindow> (^focusedWindow)(WKWebExtensionContext *);

#if PLATFORM(MAC)
@property (nonatomic, copy) void (^openNewWindow)(WKWebExtensionWindowConfiguration *, WKWebExtensionContext *, void (^)(id<WKWebExtensionWindow>, NSError *));
#endif

@property (nonatomic, copy) void (^openNewTab)(WKWebExtensionTabConfiguration *, WKWebExtensionContext *, void (^)(id<WKWebExtensionTab>, NSError *));
@property (nonatomic, copy) void (^openOptionsPage)(WKWebExtensionContext *, void (^)(NSError *));

@property (nonatomic, copy) void (^promptForPermissions)(id <WKWebExtensionTab>, NSSet<NSString *> *, void (^)(NSSet<WKWebExtensionPermission> *, NSDate *));
@property (nonatomic, copy) void (^promptForPermissionMatchPatterns)(id <WKWebExtensionTab>, NSSet<WKWebExtensionMatchPattern *> *, void (^)(NSSet<WKWebExtensionMatchPattern *> *, NSDate *));
@property (nonatomic, copy) void (^promptForPermissionToAccessURLs)(id <WKWebExtensionTab>, NSSet<NSURL *> *, void (^)(NSSet<NSURL *> *, NSDate *));

@property (nonatomic, copy) void (^sendMessage)(id message, NSString *applicationIdentifier, void (^)(id replyMessage, NSError *));
@property (nonatomic, copy) void (^connectUsingMessagePort)(WKWebExtensionMessagePort *);

@property (nonatomic, copy) void (^didUpdateAction)(WKWebExtensionAction *);
@property (nonatomic, copy) void (^presentPopupForAction)(WKWebExtensionAction *);

@property (nonatomic, copy) void (^presentSidebar)(_WKWebExtensionSidebar *);

@property (nonatomic, copy) void (^closeSidebar)(_WKWebExtensionSidebar *);

@property (nonatomic, copy) void (^didUpdateSidebar)(_WKWebExtensionSidebar *);

@property (nonatomic, copy) void (^createBookmarkWithParentIdentifier)(NSString *parentId, NSNumber *index, NSString *url, NSString *title, void (^)(NSObject<_WKWebExtensionBookmark> *, NSError *));
@property (nonatomic, copy) void (^bookmarksForExtensionContext)(void (^)(NSArray<NSObject<_WKWebExtensionBookmark> *> *, NSError *));
@property (nonatomic, copy) void (^removeBookmarkWithIdentifier)(NSString *bookmarkId, BOOL removeFolderWithChildren, void (^completionHandler)(NSError *));
@property (nonatomic, copy) void (^updateBookmarkWithIdentifier)(NSString *bookmarkId, NSString *title, NSString *url, void (^)(NSObject<_WKWebExtensionBookmark> *, NSError *));
@property (nonatomic, copy) void (^moveBookmarkWithIdentifier)(NSString *bookmarkId, NSString *parentId, NSNumber *index, void (^)(NSObject<_WKWebExtensionBookmark> *, NSError *));
@end

#endif // __OBJC__

#endif // ENABLE(WK_WEB_EXTENSIONS)
