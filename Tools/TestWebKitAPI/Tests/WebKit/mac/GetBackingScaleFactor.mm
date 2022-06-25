/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#import "PlatformUtilities.h"
#import "SyntheticBackingScaleFactorWindow.h"
#import "Test.h"
#import <WebKit/WKBrowsingContextController.h>
#import <WebKit/WKViewPrivate.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

static bool messageReceived;
static double backingScaleFactor;

static void didReceiveMessageFromInjectedBundle(WKContextRef context, WKStringRef messageName, WKTypeRef messageBody, const void*)
{
    messageReceived = true;
    EXPECT_WK_STREQ("DidGetBackingScaleFactor", messageName);
    ASSERT_NOT_NULL(messageBody);
    EXPECT_EQ(WKDoubleGetTypeID(), WKGetTypeID(messageBody));
    backingScaleFactor = WKDoubleGetValue(static_cast<WKDoubleRef>(messageBody));
}

static void setInjectedBundleClient(WKContextRef context)
{
    WKContextInjectedBundleClientV0 injectedBundleClient;
    memset(&injectedBundleClient, 0, sizeof(injectedBundleClient));

    injectedBundleClient.base.version = 0;
    injectedBundleClient.didReceiveMessageFromInjectedBundle = didReceiveMessageFromInjectedBundle;

    WKContextSetInjectedBundleClient(context, &injectedBundleClient.base);
}

static RetainPtr<SyntheticBackingScaleFactorWindow> createWindow()
{
    RetainPtr<SyntheticBackingScaleFactorWindow> window = adoptNS([[SyntheticBackingScaleFactorWindow alloc] initWithContentRect:NSMakeRect(0, 0, 800, 600) styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:YES]);
    [window.get() setReleasedWhenClosed:NO];
    return window;
}

TEST(WebKit, GetBackingScaleFactor)
{
    WKRetainPtr<WKContextRef> context = adoptWK(Util::createContextForInjectedBundleTest("GetBackingScaleFactorTest"));
    setInjectedBundleClient(context.get());
    auto pageGroup = adoptWK(WKPageGroupCreateWithIdentifier(Util::toWK("GetBackingScaleFactorPageGroup").get()));
    auto view = adoptNS([[WKView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) contextRef:context.get() pageGroupRef:pageGroup.get()]);
    [[view browsingContextController] loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];

    RetainPtr<SyntheticBackingScaleFactorWindow> window1 = createWindow();
    [window1.get() setBackingScaleFactor:1];

    [[window1.get() contentView] addSubview:view.get()];
    WKContextPostMessageToInjectedBundle(context.get(), Util::toWK("GetBackingScaleFactor").get(), 0);
    Util::run(&messageReceived);
    messageReceived = false;
    EXPECT_EQ(1, backingScaleFactor);

    RetainPtr<SyntheticBackingScaleFactorWindow> window2 = createWindow();
    [window2.get() setBackingScaleFactor:2];

    [[window2.get() contentView] addSubview:view.get()];
    WKContextPostMessageToInjectedBundle(context.get(), Util::toWK("GetBackingScaleFactor").get(), 0);
    Util::run(&messageReceived);
    messageReceived = false;
    EXPECT_EQ(2, backingScaleFactor);

    WKPageSetCustomBackingScaleFactor(view.get().pageRef, 3);
    WKContextPostMessageToInjectedBundle(context.get(), Util::toWK("GetBackingScaleFactor").get(), 0);
    Util::run(&messageReceived);
    messageReceived = false;
    EXPECT_EQ(3, backingScaleFactor);
}

} // namespace TestWebKitAPI
