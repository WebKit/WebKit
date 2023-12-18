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

#if ENABLE(WK_WEB_EXTENSIONS)

#ifdef __OBJC__

#import <WebKit/WebKit.h>
#import <WebKit/_WKWebExtensionAction.h>
#import <WebKit/_WKWebExtensionContextPrivate.h>
#import <WebKit/_WKWebExtensionControllerConfigurationPrivate.h>
#import <WebKit/_WKWebExtensionControllerDelegatePrivate.h>
#import <WebKit/_WKWebExtensionControllerPrivate.h>
#import <WebKit/_WKWebExtensionMatchPattern.h>
#import <WebKit/_WKWebExtensionMessagePort.h>
#import <WebKit/_WKWebExtensionPermission.h>
#import <WebKit/_WKWebExtensionPrivate.h>
#import <WebKit/_WKWebExtensionTab.h>
#import <WebKit/_WKWebExtensionTabCreationOptions.h>
#import <WebKit/_WKWebExtensionWindow.h>

@interface TestWebExtensionsDelegate : NSObject <_WKWebExtensionControllerDelegate>

@property (nonatomic, copy) NSArray<id <_WKWebExtensionWindow>> *(^openWindows)(_WKWebExtensionContext *);
@property (nonatomic, copy) id <_WKWebExtensionWindow> (^focusedWindow)(_WKWebExtensionContext *);

@property (nonatomic, copy) void (^openNewWindow)(_WKWebExtensionWindowCreationOptions *, _WKWebExtensionContext *, void (^)(id<_WKWebExtensionWindow>, NSError *));
@property (nonatomic, copy) void (^openNewTab)(_WKWebExtensionTabCreationOptions *, _WKWebExtensionContext *, void (^)(id<_WKWebExtensionTab>, NSError *));
@property (nonatomic, copy) void (^openOptionsPage)(_WKWebExtensionContext *, void (^)(NSError *));

@property (nonatomic, copy) void (^promptForPermissions)(id <_WKWebExtensionTab>, NSSet<NSString *> *, void (^)(NSSet<_WKWebExtensionPermission> *));
@property (nonatomic, copy) void (^promptForPermissionMatchPatterns)(id <_WKWebExtensionTab>, NSSet<_WKWebExtensionMatchPattern *> *, void (^)(NSSet<_WKWebExtensionMatchPattern *> *));

@property (nonatomic, copy) void (^sendMessage)(id message, NSString *applicationIdentifier, void (^)(id replyMessage, NSError *));
@property (nonatomic, copy) void (^connectUsingMessagePort)(_WKWebExtensionMessagePort *);

@property (nonatomic, copy) void (^presentPopupForAction)(_WKWebExtensionAction *);

@end

#endif // __OBJC__

#endif // ENABLE(WK_WEB_EXTENSIONS)
