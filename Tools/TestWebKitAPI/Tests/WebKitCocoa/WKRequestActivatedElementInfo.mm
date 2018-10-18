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

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebView.h>
#import <WebKit/_WKActivatedElementInfo.h>
#import <wtf/RetainPtr.h>

#if WK_API_ENABLED && PLATFORM(IOS_FAMILY)

namespace TestWebKitAPI {

static void checkElementTypeAndBoundingRect(_WKActivatedElementInfo *elementInfo, _WKActivatedElementType expectedType, CGRect expectedBoundingRect)
{
    auto observedBoundingRect = elementInfo.boundingRect;
    EXPECT_EQ(CGRectGetWidth(expectedBoundingRect), CGRectGetWidth(observedBoundingRect));
    EXPECT_EQ(CGRectGetHeight(expectedBoundingRect), CGRectGetHeight(observedBoundingRect));
    EXPECT_EQ(CGRectGetMinX(expectedBoundingRect), CGRectGetMinX(observedBoundingRect));
    EXPECT_EQ(CGRectGetMinY(expectedBoundingRect), CGRectGetMinY(observedBoundingRect));
    EXPECT_EQ(expectedType, elementInfo.type);
}
    
TEST(WebKit, RequestActivatedElementInfoForLink)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView loadHTMLString:@"<html><head><meta name='viewport' content='initial-scale=1'></head><body style = 'margin: 0px;'><a href='testURL.test' style='display: block; height: 100%;' title='HitTestLinkTitle' id='testID'></a></body></html>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];
    
    __block bool finished = false;
    [webView _requestActivatedElementAtPosition:CGPointMake(50, 50) completionBlock: ^(_WKActivatedElementInfo *elementInfo) {
        
        EXPECT_TRUE(elementInfo.type == _WKActivatedElementTypeLink);
        EXPECT_WK_STREQ(elementInfo.URL.absoluteString, "testURL.test");
        EXPECT_WK_STREQ(elementInfo.title, "HitTestLinkTitle");
        EXPECT_WK_STREQ(elementInfo.ID, @"testID");
        EXPECT_NOT_NULL(elementInfo.image);
        EXPECT_EQ(elementInfo.boundingRect.size.width, 320);
        EXPECT_EQ(elementInfo.boundingRect.size.height, 500);
        EXPECT_EQ(elementInfo.image.size.width, 320);
        EXPECT_EQ(elementInfo.image.size.height, 500);
        
        finished = true;
    }];
    
    TestWebKitAPI::Util::run(&finished);
}
    
TEST(WebKit, RequestActivatedElementInfoForBlank)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView loadHTMLString:@"<html><head><meta name='viewport' content='initial-scale=1'></head><body style = 'margin: 0px;'></body></html>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];
    
    __block bool finished = false;
    [webView _requestActivatedElementAtPosition:CGPointMake(50, 50) completionBlock: ^(_WKActivatedElementInfo *elementInfo) {
        
        EXPECT_TRUE(elementInfo.type == _WKActivatedElementTypeUnspecified);
        EXPECT_EQ(elementInfo.boundingRect.size.width, 320);
        EXPECT_EQ(elementInfo.boundingRect.size.height, 500);
        
        finished = true;
    }];
    
    TestWebKitAPI::Util::run(&finished);
}
    
TEST(WebKit, RequestActivatedElementInfoWithNestedSynchronousUpdates)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='initial-scale=1'><style>body { margin:0 } a { display:block; width:200px; height:200px }</style><a href='https://www.apple.com'>FOO</a>"];

    __block bool finished = false;
    [webView _requestActivatedElementAtPosition:CGPointMake(100, 100) completionBlock:^(_WKActivatedElementInfo *elementInfo) {
        _WKActivatedElementInfo *synchronousElementInfo = [webView activatedElementAtPosition:CGPointMake(300, 300)];
        EXPECT_EQ(_WKActivatedElementTypeUnspecified, synchronousElementInfo.type);
        checkElementTypeAndBoundingRect(elementInfo, _WKActivatedElementTypeLink, CGRectMake(0, 0, 200, 200));
        finished = true;
    }];
    TestWebKitAPI::Util::run(&finished);

    finished = false;
    [webView _requestActivatedElementAtPosition:CGPointMake(100, 100) completionBlock:^(_WKActivatedElementInfo *elementInfo) {
        _WKActivatedElementInfo *synchronousElementInfo = [webView activatedElementAtPosition:CGPointMake(100, 100)];
        checkElementTypeAndBoundingRect(synchronousElementInfo, _WKActivatedElementTypeLink, CGRectMake(0, 0, 200, 200));
        checkElementTypeAndBoundingRect(elementInfo, _WKActivatedElementTypeLink, CGRectMake(0, 0, 200, 200));
        finished = true;
    }];
    TestWebKitAPI::Util::run(&finished);
}

TEST(WebKit, RequestActivatedElementInfoWithNestedRequests)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"image-and-contenteditable"];

    __block bool finishedWithInner = false;
    __block bool finishedWithOuter = false;
    [webView _requestActivatedElementAtPosition:CGPointMake(100, 50) completionBlock:^(_WKActivatedElementInfo *outerElementInfo) {
        [webView _requestActivatedElementAtPosition:CGPointMake(100, 50) completionBlock:^(_WKActivatedElementInfo *innerElementInfo) {
            checkElementTypeAndBoundingRect(innerElementInfo, _WKActivatedElementTypeImage, CGRectMake(0, 0, 200, 200));
            finishedWithInner = true;
        }];
        checkElementTypeAndBoundingRect(outerElementInfo, _WKActivatedElementTypeImage, CGRectMake(0, 0, 200, 200));
        finishedWithOuter = true;
    }];
    TestWebKitAPI::Util::run(&finishedWithOuter);
    TestWebKitAPI::Util::run(&finishedWithInner);

    finishedWithInner = false;
    finishedWithOuter = false;
    [webView _requestActivatedElementAtPosition:CGPointMake(100, 50) completionBlock:^(_WKActivatedElementInfo *outerElementInfo) {
        [webView _requestActivatedElementAtPosition:CGPointMake(300, 300) completionBlock:^(_WKActivatedElementInfo *innerElementInfo) {
            EXPECT_EQ(_WKActivatedElementTypeUnspecified, innerElementInfo.type);
            finishedWithInner = true;
        }];
        checkElementTypeAndBoundingRect(outerElementInfo, _WKActivatedElementTypeImage, CGRectMake(0, 0, 200, 200));
        finishedWithOuter = true;
    }];
    TestWebKitAPI::Util::run(&finishedWithOuter);
    TestWebKitAPI::Util::run(&finishedWithInner);
}

}

#endif
