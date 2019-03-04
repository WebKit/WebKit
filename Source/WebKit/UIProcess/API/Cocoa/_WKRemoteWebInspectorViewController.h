/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#if !TARGET_OS_IPHONE

@class WKWebView;
@protocol _WKRemoteWebInspectorViewControllerDelegate;

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, WKRemoteWebInspectorDebuggableType) {
    WKRemoteWebInspectorDebuggableTypeJavaScript,
    WKRemoteWebInspectorDebuggableTypeServiceWorker WK_API_AVAILABLE(macosx(10.13.4), ios(11.3)),
    WKRemoteWebInspectorDebuggableTypeWeb,
} WK_API_AVAILABLE(macosx(10.12.3), ios(10.3));

WK_CLASS_AVAILABLE(macosx(10.12.3), ios(10.3))
@interface _WKRemoteWebInspectorViewController : NSObject

@property (nonatomic, assign) id <_WKRemoteWebInspectorViewControllerDelegate> delegate;

@property (nonatomic, readonly, retain) NSWindow *window;
@property (nonatomic, readonly, retain) WKWebView *webView;

- (void)loadForDebuggableType:(WKRemoteWebInspectorDebuggableType)debuggableType backendCommandsURL:(NSURL *)backendCommandsURL;
- (void)close;
- (void)show;

- (void)sendMessageToFrontend:(NSString *)message;

@end

@protocol _WKRemoteWebInspectorViewControllerDelegate <NSObject>
@optional
- (void)inspectorViewController:(_WKRemoteWebInspectorViewController *)controller sendMessageToBackend:(NSString *)message;
- (void)inspectorViewControllerInspectorDidClose:(_WKRemoteWebInspectorViewController *)controller;
@end

NS_ASSUME_NONNULL_END

#endif // !TARGET_OS_IPHONE
