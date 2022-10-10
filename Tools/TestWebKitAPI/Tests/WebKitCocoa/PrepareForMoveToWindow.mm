/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#import <WebKit/WKFoundation.h>

#if PLATFORM(MAC)

#import "DeprecatedGlobalValues.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>

TEST(WKWebView, PrepareForMoveToWindow)
{
    auto webView = adoptNS([[WKWebView alloc] init]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    [webView _test_waitForDidFinishNavigation];

    RetainPtr<NSWindow> window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO]);

    [webView _prepareForMoveToWindow:window.get() completionHandler:[webView, window] {
        [window.get().contentView addSubview:webView.get()];
        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);
}

TEST(WKWebView, PrepareToUnparentView)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadTestPageNamed:@"simple"];

    __block bool done = false;
    [webView _prepareForMoveToWindow:nil completionHandler:^{
        [webView removeFromSuperview];
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);
}

TEST(WKWebView, PrepareForMoveToWindowThenClose)
{
    auto webView = adoptNS([[WKWebView alloc] init]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    [webView _test_waitForDidFinishNavigation];

    RetainPtr<NSWindow> window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO]);

    [webView _prepareForMoveToWindow:window.get() completionHandler:[webView, window] {
        isDone = true;
    }];

    [webView _close];

    TestWebKitAPI::Util::run(&isDone);
}

TEST(WKWebView, PrepareForMoveToWindowThenViewDeallocBeforeMoving)
{
    auto window = adoptNS([[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 200, 200) styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO]);

    @autoreleasepool {
        auto webView = adoptNS([[WKWebView alloc] init]);
        [webView _prepareForMoveToWindow:window.get() completionHandler:^{
            isDone = true;
        }];

        TestWebKitAPI::Util::run(&isDone);
        isDone = false;

        webView = nil;
    }

    dispatch_async(dispatch_get_main_queue(), ^{
        [window setFrame:NSMakeRect(0, 0, 10, 10) display:YES];
        isDone = true;
    });

    TestWebKitAPI::Util::run(&isDone);
}

TEST(WKWebView, PrepareForMoveToWindowThenWindowDeallocBeforeMoving)
{
    auto webView = adoptNS([[WKWebView alloc] init]);
    static WeakObjCPtr<NSWindow> weakWindow;

    @autoreleasepool {
        auto window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO]);
        weakWindow = window.get();
        [webView _prepareForMoveToWindow:window.get() completionHandler:^{
            isDone = true;
        }];

        TestWebKitAPI::Util::run(&isDone);
        isDone = false;

        window = nil;
    }

    // Target window is kept alive by webView before it actually moves to.
    ASSERT(weakWindow);

    @autoreleasepool {
        RetainPtr<NSWindow> window = weakWindow.getAutoreleased();
        [window.get().contentView addSubview:webView.get()];
        
        window = nil;
    }

    ASSERT(!weakWindow);
}

#endif
