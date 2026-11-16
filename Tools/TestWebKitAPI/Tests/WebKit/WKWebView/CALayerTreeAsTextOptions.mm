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

#import "Helpers/Test.h"
#import <QuartzCore/QuartzCore.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <wtf/RetainPtr.h>

TEST(WebKit, CALayerTreeAsTextOptions)
{
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 480)]);

    RetainPtr layer = adoptNS([[CATransformLayer alloc] init]);
    [layer setName:@"root"];
    [layer setBounds:CGRectMake(0, 0, 100, 100)];
    [layer setTransform:CATransform3DMakeTranslation(10, 20, 0)];

    RetainPtr sublayer = adoptNS([[CALayer alloc] init]);
    [sublayer setHidden:YES];
    [sublayer setMasksToBounds:YES];
    RetainPtr animation = [CABasicAnimation animationWithKeyPath:@"opacity"];
    [animation setDuration:2];
    [sublayer addAnimation:animation.get() forKey:@"fade"];
    [layer addSublayer:sublayer.get()];

    // Without options, the dump is unchanged.
    RetainPtr defaultTree = [webView _caLayerTreeAsTextForLayer:layer.get()];
    EXPECT_WK_STREQ(defaultTree.get(), [webView _caLayerTreeAsTextForLayer:layer.get() options:_WKCALayerTreeAsTextOptionsNone]);
    EXPECT_FALSE([defaultTree containsString:@"layer class"]);
    EXPECT_FALSE([defaultTree containsString:@"animation"]);

    auto options = _WKCALayerTreeAsTextOptionsIncludeLayerClasses
        | _WKCALayerTreeAsTextOptionsIncludeNames
        | _WKCALayerTreeAsTextOptionsIncludeTransforms
        | _WKCALayerTreeAsTextOptionsIncludeVisibility
        | _WKCALayerTreeAsTextOptionsIncludeAnimations;
    RetainPtr tree = [webView _caLayerTreeAsTextForLayer:layer.get() options:options];
    EXPECT_TRUE([tree containsString:@"(layer class CATransformLayer)"]);
    EXPECT_TRUE([tree containsString:@"(layer name root)"]);
    EXPECT_TRUE([tree containsString:@"(layer transform [1 0 0 0] [0 1 0 0] [0 0 1 0] [10 20 0 1])"]);
    EXPECT_TRUE([tree containsString:@"(layer hidden 1)"]);
    EXPECT_TRUE([tree containsString:@"(layer masks to bounds 1)"]);
    EXPECT_TRUE([tree containsString:@"animation fade"]);
    EXPECT_TRUE([tree containsString:@"(key path opacity)"]);
    EXPECT_TRUE([tree containsString:@"(duration 2)"]);

    // None of these layers is backed by the remote layer tree.
    RetainPtr remoteTree = [webView _caLayerTreeAsTextForLayer:layer.get() options:_WKCALayerTreeAsTextOptionsRemoteLayersOnly];
    EXPECT_FALSE([remoteTree containsString:@"layer bounds"]);
}
