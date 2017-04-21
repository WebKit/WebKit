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

#include "config.h"

#if ENABLE(DATA_INTERACTION)

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKDraggableElementInfo.h>

@implementation _WKDraggableElementInfo (PositionInformationTests)

- (void)expectToBeLink:(BOOL)isLink image:(BOOL)isImage atPoint:(CGPoint)point
{
    EXPECT_EQ(isLink, self.isLink);
    EXPECT_EQ(isImage, self.isImage);
    EXPECT_EQ(point.x, self.point.x);
    EXPECT_EQ(point.y, self.point.y);
}

@end

namespace TestWebKitAPI {

TEST(PositionInformationTests, FindDraggableLinkAtPosition)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"link-and-input"];

    _WKDraggableElementInfo *information = [webView draggableElementAtPosition:CGPointMake(100, 50)];
    [information expectToBeLink:YES image:NO atPoint:CGPointMake(100, 50)];
}

TEST(PositionInformationTests, RequestDraggableLinkAtPosition)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"link-and-input"];

    __block bool isDone = false;
    [webView requestDraggableElementAtPosition:CGPointMake(100, 50) completionBlock:^(_WKDraggableElementInfo *information) {
        [information expectToBeLink:YES image:NO atPoint:CGPointMake(100, 50)];
        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);
}

TEST(PositionInformationTests, FindDraggableLinkAtDifferentPositionWithinRequestBlock)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"link-and-input"];

    __block bool isDone = false;
    [webView requestDraggableElementAtPosition:CGPointMake(100, 50) completionBlock:^(_WKDraggableElementInfo *information) {
        _WKDraggableElementInfo *synchronousInformation = [webView draggableElementAtPosition:CGPointMake(100, 300)];
        [synchronousInformation expectToBeLink:NO image:NO atPoint:CGPointMake(100, 300)];
        [information expectToBeLink:YES image:NO atPoint:CGPointMake(100, 50)];
        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);
}

TEST(PositionInformationTests, FindDraggableLinkAtSamePositionWithinRequestBlock)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"link-and-input"];

    __block bool isDone = false;
    [webView requestDraggableElementAtPosition:CGPointMake(100, 50) completionBlock:^(_WKDraggableElementInfo *information) {
        _WKDraggableElementInfo *synchronousInformation = [webView draggableElementAtPosition:CGPointMake(100, 50)];
        [synchronousInformation expectToBeLink:YES image:NO atPoint:CGPointMake(100, 50)];
        [information expectToBeLink:YES image:NO atPoint:CGPointMake(100, 50)];
        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);
}

TEST(PositionInformationTests, RequestDraggableLinkAtSamePositionWithinRequestBlock)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"image-and-contenteditable"];

    __block bool isDoneWithInner = false;
    __block bool isDoneWithOuter = false;

    [webView requestDraggableElementAtPosition:CGPointMake(100, 50) completionBlock:^(_WKDraggableElementInfo *outerInformation) {
        [webView requestDraggableElementAtPosition:CGPointMake(100, 50) completionBlock:^(_WKDraggableElementInfo *innerInformation) {
            [innerInformation expectToBeLink:NO image:YES atPoint:CGPointMake(100, 50)];
            isDoneWithInner = true;
        }];
        [outerInformation expectToBeLink:NO image:YES atPoint:CGPointMake(100, 50)];
        isDoneWithOuter = true;
    }];

    TestWebKitAPI::Util::run(&isDoneWithOuter);
    TestWebKitAPI::Util::run(&isDoneWithInner);
}

TEST(PositionInformationTests, RequestDraggableLinkAtDifferentPositionWithinRequestBlock)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"image-and-contenteditable"];

    __block bool isDoneWithInner = false;
    __block bool isDoneWithOuter = false;

    [webView requestDraggableElementAtPosition:CGPointMake(100, 50) completionBlock:^(_WKDraggableElementInfo *outerInformation) {
        [webView requestDraggableElementAtPosition:CGPointMake(100, 300) completionBlock:^(_WKDraggableElementInfo *innerInformation) {
            [innerInformation expectToBeLink:NO image:NO atPoint:CGPointMake(100, 300)];
            isDoneWithInner = true;
        }];
        [outerInformation expectToBeLink:NO image:YES atPoint:CGPointMake(100, 50)];
        isDoneWithOuter = true;
    }];

    TestWebKitAPI::Util::run(&isDoneWithOuter);
    TestWebKitAPI::Util::run(&isDoneWithInner);
}

} // namespace TestWebKitAPI

#endif
