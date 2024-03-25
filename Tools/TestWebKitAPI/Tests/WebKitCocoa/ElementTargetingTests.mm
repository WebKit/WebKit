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

#import "config.h"

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebKit/WKFrameInfoPrivate.h>
#import <WebKit/_WKFrameTreeNode.h>
#import <WebKit/_WKTargetedElementInfo.h>
#import <WebKit/_WKTargetedElementRequest.h>

@interface WKWebView (ElementTargeting)
- (NSArray<_WKTargetedElementInfo *> *)targetedElementInfoAt:(CGPoint)point;
@end

@implementation WKWebView (ElementTargeting)

- (NSArray<_WKTargetedElementInfo *> *)targetedElementInfoAt:(CGPoint)point
{
    __block RetainPtr<NSArray<_WKTargetedElementInfo *>> result;
    auto request = adoptNS([_WKTargetedElementRequest new]);
    [request setPoint:point];
    __block bool done = false;
    [self _requestTargetedElementInfo:request.get() completionHandler:^(NSArray<_WKTargetedElementInfo *> *elements) {
        result = elements;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    return result.autorelease();
}

@end

@interface _WKTargetedElementInfo (TestingAdditions)
@property (nonatomic, readonly) NSArray<_WKFrameTreeNode *> *childFrames;
@end

@implementation _WKTargetedElementInfo (TestingAdditions)

- (NSArray<_WKFrameTreeNode *> *)childFrames
{
    __block RetainPtr<NSArray<_WKFrameTreeNode *>> result;
    __block bool done = false;
    [self getChildFrames:^(NSArray<_WKFrameTreeNode *> *frames) {
        result = frames;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    return result.autorelease();
}

@end

namespace TestWebKitAPI {

TEST(ElementTargeting, BasicElementTargeting)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [webView synchronouslyLoadTestPageNamed:@"element-targeting-1"];

    Util::waitForConditionWithLogging([&] {
        return [[webView objectByEvaluatingJavaScript:@"window.subframeLoaded"] boolValue];
    }, 5, @"Timed out waiting for subframes to finish loading.");

    RetainPtr elements = [webView targetedElementInfoAt:CGPointMake(150, 150)];
    EXPECT_EQ([elements count], 3U);
    {
        auto element = [elements objectAtIndex:0];
        EXPECT_EQ(element.positionType, _WKTargetedElementPositionFixed);
        EXPECT_WK_STREQ(".fixed.container", element.selectors.firstObject);
        EXPECT_TRUE([element.renderedText containsString:@"The round pegs"]);
        EXPECT_EQ(element.renderedText.length, 70U);
        EXPECT_EQ(element.offsetEdges, _WKRectEdgeLeft | _WKRectEdgeTop);

        RetainPtr childFrames = [element childFrames];
        EXPECT_EQ([childFrames count], 1U);

        auto childFrame = [childFrames firstObject];
        EXPECT_FALSE(childFrame.info.mainFrame);
        EXPECT_WK_STREQ(childFrame.info.request.URL.lastPathComponent, "nested-frames.html");
        EXPECT_WK_STREQ(childFrame.info._title, "Outer Subframe");
        EXPECT_EQ(childFrame.childFrames.count, 1U);

        auto nestedChildFrame = childFrame.childFrames.firstObject;
        EXPECT_FALSE(nestedChildFrame.info.mainFrame);
        EXPECT_FALSE(nestedChildFrame.info.mainFrame);
        EXPECT_WK_STREQ(nestedChildFrame.info.request.URL.scheme, "about");
        EXPECT_WK_STREQ(nestedChildFrame.info._title, "Inner Subframe");
        EXPECT_EQ(nestedChildFrame.childFrames.count, 0U);
    }
    {
        auto element = [elements objectAtIndex:1];
        EXPECT_EQ(element.positionType, _WKTargetedElementPositionAbsolute);
        EXPECT_WK_STREQ("#absolute", element.selectors.firstObject);
        EXPECT_TRUE([element.renderedText containsString:@"the crazy ones"]);
        EXPECT_EQ(element.renderedText.length, 64U);
        EXPECT_EQ(element.offsetEdges, _WKRectEdgeRight | _WKRectEdgeBottom);
        EXPECT_EQ(element.childFrames.count, 0U);
    }
    {
        auto element = [elements objectAtIndex:2];
        EXPECT_EQ(element.positionType, _WKTargetedElementPositionStatic);
        EXPECT_WK_STREQ("MAIN > SECTION:first-of-type", element.selectors.firstObject);
        EXPECT_TRUE([element.renderedText containsString:@"Lorem ipsum"]);
        EXPECT_EQ(element.renderedText.length, 896U);
        EXPECT_EQ(element.offsetEdges, _WKRectEdgeNone);
        EXPECT_EQ(element.childFrames.count, 0U);
    }
}

} // namespace TestWebKitAPI
