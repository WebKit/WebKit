/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if TARGET_OS_IPHONE

#import <UIKit/UIKit.h>

@class NSCoder;
@class WKWebView;
@class WKWebViewConfiguration;

NS_ASSUME_NONNULL_BEGIN

/*!
 WKWebViewConfiguration is a subclass of UIViewController that can
 be used to display web content.
 @helperclass @link WKWebViewConfiguration @/link
 Used to configure @link WKWebViewController @/link instances.
 */
WK_CLASS_AVAILABLE(ios(WK_IOS_TBA), visionos(WK_XROS_TBA)) WK_API_UNAVAILABLE(macos)
@interface WKWebViewController : UIViewController

/*! @abstract Returns a view controller for a web view with a
 specified configuration
 @param configuration The configuration for the new web view.
 @result An initialized web view controller.
 @discussion This is a designated initializer. You can use
 @link -init @/link to initialize an instance with the default
 configuration. The initializer copies the specified configuration, so
 mutating the configuration after invoking the initializer has no effect
 on the web view.
 */
- (instancetype)initWithConfiguration:(WKWebViewConfiguration *)configuration NS_DESIGNATED_INITIALIZER;

/*! @abstract The web view created by this view controller.
 */
@property (nonatomic, readonly) WKWebView *webView;
@end

NS_ASSUME_NONNULL_END

#endif
