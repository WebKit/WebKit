/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "config.h"

#if WK_API_ENABLED && PLATFORM(IOS_FAMILY)

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>

static void traverseLayerTree(CALayer *layer, void(^block)(CALayer *))
{
    for (CALayer *child in layer.sublayers)
        traverseLayerTree(child, block);
    block(layer);
}

TEST(WKScrollViewTests, PositionFixedLayerAfterScrolling)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"fixed-nav-bar"];

    __block bool done = false;
    [webView _doAfterNextPresentationUpdate:^() {
        done = true;
    }];

    [CATransaction begin];
    [webView scrollView].contentOffset = CGPointMake(0, 5000);
    [CATransaction commit];

    // Purposefully hang the main thread for a short while to give the remote layer tree transaction an
    // opportunity to arrive in the UI process before dispatching the next visible content rect update.
    usleep(USEC_PER_SEC * 0.25);

    TestWebKitAPI::Util::run(&done);

    bool foundLayerForFixedNavigationBar = false;
    traverseLayerTree([webView layer], [&] (CALayer *layer) {
        if (!CGSizeEqualToSize(layer.bounds.size, CGSizeMake(320, 50)))
            return;

        auto boundsInWebViewCoordinates = [layer convertRect:layer.bounds toLayer:[webView layer]];
        EXPECT_EQ(CGRectGetMinX(boundsInWebViewCoordinates), 0);
        EXPECT_EQ(CGRectGetMinY(boundsInWebViewCoordinates), 0);
        foundLayerForFixedNavigationBar = true;
    });
    EXPECT_TRUE(foundLayerForFixedNavigationBar);
}

#endif // WK_API_ENABLED && PLATFORM(IOS_FAMILY)
