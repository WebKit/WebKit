/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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
#import <WebKit/_WKInspectorExtensionHost.h>
#import <WebKit/_WKInspectorIBActions.h>

#if !TARGET_OS_IPHONE

@class WKWebView;
@class _WKInspectorConfiguration;
@class _WKInspectorDebuggableInfo;

@protocol _WKRemoteWebInspectorViewControllerDelegate;

NS_ASSUME_NONNULL_BEGIN

WK_CLASS_AVAILABLE(macos(10.12.3))
@interface _WKRemoteWebInspectorViewController : NSObject <_WKInspectorExtensionHost, _WKInspectorIBActions>

@property (nonatomic, weak) id <_WKRemoteWebInspectorViewControllerDelegate> delegate;

@property (nonatomic, readonly, retain) NSWindow *window;
@property (nonatomic, readonly, retain) WKWebView *webView;
@property (nonatomic, readonly, copy) _WKInspectorConfiguration *configuration WK_API_AVAILABLE(macos(12.0));

- (instancetype)initWithConfiguration:(_WKInspectorConfiguration *)configuration WK_API_AVAILABLE(macos(12.0));
- (void)loadForDebuggable:(_WKInspectorDebuggableInfo *)debuggableInfo backendCommandsURL:(NSURL *)backendCommandsURL WK_API_AVAILABLE(macos(12.0));

- (void)sendMessageToFrontend:(NSString *)message;

@end

@protocol _WKRemoteWebInspectorViewControllerDelegate <NSObject>
@optional
- (void)inspectorViewController:(_WKRemoteWebInspectorViewController *)controller sendMessageToBackend:(NSString *)message;
- (void)inspectorViewControllerInspectorDidClose:(_WKRemoteWebInspectorViewController *)controller;
@end

NS_ASSUME_NONNULL_END

#endif // !TARGET_OS_IPHONE
