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

NS_HEADER_AUDIT_BEGIN(nullability, sendability)

NS_SWIFT_UI_ACTOR
@interface TestWebExtensionsDelegate : NSObject <WKWebExtensionControllerDelegate, WKWebExtensionControllerDelegatePrivate>

@property (nonatomic, copy, nullable) NSArray<id <WKWebExtensionWindow>> *(^openWindows)(WKWebExtensionContext *);
@property (nonatomic, copy, nullable) id <WKWebExtensionWindow> _Nullable (^focusedWindow)(WKWebExtensionContext *);

#if PLATFORM(MAC)
@property (nonatomic, copy, nullable) void (^openNewWindow)(WKWebExtensionWindowConfiguration *, WKWebExtensionContext *, void (^)(id<WKWebExtensionWindow> _Nullable, NSError * _Nullable));
#endif

@property (nonatomic, copy, nullable) void (^openNewTab)(WKWebExtensionTabConfiguration *, WKWebExtensionContext *, void (^)(id<WKWebExtensionTab> _Nullable, NSError * _Nullable));
@property (nonatomic, copy, nullable) void (^moveTabs)(NSArray<id <WKWebExtensionTab>> *, NSUInteger index, id <WKWebExtensionWindow> window, WKWebExtensionContext *, void (^)(NSError * _Nullable));
@property (nonatomic, copy, nullable) void (^openOptionsPage)(WKWebExtensionContext *, void (^)(NSError * _Nullable));

@property (nonatomic, copy, nullable) void (^promptForPermissions)(id <WKWebExtensionTab> _Nullable, NSSet<NSString *> *, void (^)(NSSet<WKWebExtensionPermission> *, NSDate * _Nullable));
@property (nonatomic, copy, nullable) void (^promptForPermissionMatchPatterns)(id <WKWebExtensionTab> _Nullable, NSSet<WKWebExtensionMatchPattern *> *, void (^)(NSSet<WKWebExtensionMatchPattern *> *, NSDate * _Nullable));
@property (nonatomic, copy, nullable) void (^promptForPermissionToAccessURLs)(id <WKWebExtensionTab> _Nullable, NSSet<NSURL *> *, void (^)(NSSet<NSURL *> *, NSDate * _Nullable));

@property (nonatomic, copy, nullable) void (^sendMessage)(id message, NSString * _Nullable applicationIdentifier, void (^)(id _Nullable replyMessage, NSError * _Nullable));
@property (nonatomic, copy, nullable) void (^connectUsingMessagePort)(WKWebExtensionMessagePort *);

@property (nonatomic, copy, nullable) void (^didUpdateAction)(WKWebExtensionAction *);
@property (nonatomic, copy, nullable) void (^presentPopupForAction)(WKWebExtensionAction *);

@property (nonatomic, copy, nullable) void (^presentSidebar)(_WKWebExtensionSidebar *);

@property (nonatomic, copy, nullable) void (^closeSidebar)(_WKWebExtensionSidebar *);

@property (nonatomic, copy, nullable) void (^didUpdateSidebar)(_WKWebExtensionSidebar *);

@property (nonatomic, copy, nullable) void (^didInvalidateSidebar)(_WKWebExtensionSidebar *);
@property (nonatomic, copy, nullable) _WKWebExtensionSidebarSide (^sidebarSide)(void);

@property (nonatomic, copy, nullable) void (^presentNotification)(_WKWebExtensionNotification *);

@property (nonatomic, copy, nullable) void (^createBookmarkWithParentIdentifier)(NSString * _Nullable parentId, NSNumber * _Nullable index, NSString * _Nullable url, NSString *title, void (^)(NSObject<_WKWebExtensionBookmark> * _Nullable, NSError * _Nullable));
@property (nonatomic, copy, nullable) void (^bookmarksForExtensionContext)(void (^)(NSArray<NSObject<_WKWebExtensionBookmark> *> * _Nullable, NSError * _Nullable));
@property (nonatomic, copy, nullable) void (^removeBookmarkWithIdentifier)(NSString *bookmarkId, BOOL removeFolderWithChildren, void (^completionHandler)(NSError * _Nullable));
@property (nonatomic, copy, nullable) void (^updateBookmarkWithIdentifier)(NSString *bookmarkId, NSString * _Nullable title, NSString * _Nullable url, void (^)(NSObject<_WKWebExtensionBookmark> * _Nullable, NSError * _Nullable));
@property (nonatomic, copy, nullable) void (^moveBookmarkWithIdentifier)(NSString *bookmarkId, NSString * _Nullable parentId, NSNumber * _Nullable index, void (^)(NSObject<_WKWebExtensionBookmark> * _Nullable, NSError * _Nullable));
@end

NS_HEADER_AUDIT_END(nullability, sendability)

#endif // __OBJC__

#endif // ENABLE(WK_WEB_EXTENSIONS)
