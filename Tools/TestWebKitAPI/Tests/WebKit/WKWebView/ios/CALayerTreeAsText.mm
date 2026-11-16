/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#import "Helpers/Test.h"
#import <QuartzCore/QuartzCore.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <wtf/RetainPtr.h>

TEST(WebKit, CALayerTreeAsTextDumpsYCoordinates)
{
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 480)]);

    RetainPtr layer = adoptNS([[CALayer alloc] init]);
    [layer setBounds:CGRectMake(11, 22, 33, 44)];
    [layer setPosition:CGPointMake(123, 456)];
    [layer setAnchorPoint:CGPointMake(0.25, 0.75)];

    RetainPtr sublayer = adoptNS([[CALayer alloc] init]);
    [sublayer setBounds:CGRectMake(0, 0, 10, 20)];
    [sublayer setPosition:CGPointMake(30, 40)];
    [layer addSublayer:sublayer];

    RetainPtr treeText = [webView _caLayerTreeAsTextForLayer:layer];

    EXPECT_TRUE([treeText containsString:@"[x: 11 y: 22 width: 33 height: 44]"]);
    EXPECT_FALSE([treeText containsString:@"[x: 11 y: 11 width: 33 height: 44]"]);

    EXPECT_TRUE([treeText containsString:@"[x: 123 y: 456]"]);
    EXPECT_FALSE([treeText containsString:@"[x: 123 y: 123]"]);

    EXPECT_TRUE([treeText containsString:@"[x: 0.25 y: 0.75]"]);
    EXPECT_FALSE([treeText containsString:@"[x: 0.25 y: 0.25]"]);

    EXPECT_TRUE([treeText containsString:@"[x: 30 y: 40]"]);
    EXPECT_FALSE([treeText containsString:@"[x: 30 y: 30]"]);
}

#endif // PLATFORM(IOS_FAMILY)
