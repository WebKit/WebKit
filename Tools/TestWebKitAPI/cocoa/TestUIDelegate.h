/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#import <WebKit/WKUIDelegatePrivate.h>

@interface TestUIDelegate : NSObject <WKUIDelegate>

@property (nonatomic, copy) WKWebView* (^createWebViewWithConfiguration)(WKWebViewConfiguration *, WKNavigationAction *, WKWindowFeatures *);
@property (nonatomic, copy) void (^runJavaScriptAlertPanelWithMessage)(WKWebView *, NSString *, WKFrameInfo *, void (^)(void));
@property (nonatomic, copy) void (^runJavaScriptConfirmPanelWithMessage)(WKWebView *, NSString *, WKFrameInfo *, void (^)(BOOL));
@property (nonatomic, copy) void (^runJavaScriptPromptPanelWithMessage)(WKWebView *, NSString *, NSString *, WKFrameInfo *, void (^)(NSString *));
#if PLATFORM(MAC)
@property (nonatomic, copy) void (^getContextMenuFromProposedMenu)(NSMenu *, _WKContextMenuElementInfo *, id <NSSecureCoding>, void (^)(NSMenu *));
@property (nonatomic, copy) void (^getWindowFrameWithCompletionHandler)(WKWebView *, void(^)(CGRect));
#endif
@property (nonatomic, copy) void (^saveDataToFile)(WKWebView *, NSData *, NSString *, NSString *, NSURL *);
@property (nonatomic, copy) void (^focusWebView)(WKWebView *);

- (NSString *)waitForAlert;
- (NSString *)waitForConfirm;
- (NSString *)waitForPromptWithReply:(NSString *)reply;

@end

@interface WKWebView (TestUIDelegateExtras)
- (NSString *)_test_waitForAlert;
- (NSString *)_test_waitForConfirm;
- (NSString *)_test_waitForPromptWithReply:(NSString *)reply;
- (void)_test_waitForInspectorToShow;
@end
