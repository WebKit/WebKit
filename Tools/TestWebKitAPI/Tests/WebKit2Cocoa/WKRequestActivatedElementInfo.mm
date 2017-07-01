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

#if WK_API_ENABLED && PLATFORM(IOS)

namespace TestWebKitAPI {
    
TEST(WebKit2, ReqestActivatedElementInfoForLink)
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
    
TEST(WebKit2, ReqestActivatedElementInfoForBlank)
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
    
}

#endif
