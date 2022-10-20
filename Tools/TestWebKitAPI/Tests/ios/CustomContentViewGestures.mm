/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#import "config.h"

#if PLATFORM(IOS_FAMILY)

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import "UIKitSPI.h"
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

TEST(CustomContentViewGestures, DoNotCrashWhenCheckingGestureDelegateInNewWebView)
{
    auto frame = CGRectMake(0, 0, 320, 600);
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:frame]);
    [webView _close];

    auto hostWindow = adoptNS([[UIWindow alloc] initWithFrame:frame]);
    [hostWindow setHidden:NO];
    [hostWindow addSubview:webView.get()];

    auto contentView = static_cast<UIView<UIGestureRecognizerDelegate> *>([webView textInputContentView]);
    auto addGesture = [&] {
        auto gesture = adoptNS([UITapGestureRecognizer new]);
        [contentView addGestureRecognizer:gesture.get()];
        [gesture setDelegate:contentView];
        return gesture;
    };

    auto gesture1 = addGesture();
    auto gesture2 = addGesture();
    [contentView gestureRecognizer:gesture1.get() shouldRecognizeSimultaneouslyWithGestureRecognizer:gesture2.get()];
}

} // namespace TestWebKitAPI

#endif // PLATFORM(IOS_FAMILY)
