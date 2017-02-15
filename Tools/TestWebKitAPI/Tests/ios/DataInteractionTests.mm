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

#import "DataInteractionSimulator.h"
#import "PlatformUtilities.h"
#import "TestWKWebView.h"

namespace TestWebKitAPI {

static void runTestsExpectingToObserveEvents(TestWKWebView *webView, NSArray *eventNamesToObserve, dispatch_block_t test)
{
    NSMutableSet *observedEvents = [NSMutableSet set];
    for (NSString *eventName in eventNamesToObserve) {
        [webView performAfterReceivingMessage:eventName action:^() {
            [observedEvents addObject:eventName];
        }];
    }

    test();

    for (NSString *eventName in eventNamesToObserve)
        EXPECT_TRUE([observedEvents containsObject:eventName]);
}

TEST(DataInteractionTests, ImageToContentEditable)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    runTestsExpectingToObserveEvents(webView.get(), @[ DataInteractionEnterEventName, DataInteractionOverEventName, DataInteractionPerformOperationEventName ], ^() {
        [webView synchronouslyLoadTestPageNamed:@"image-and-contenteditable"];

        RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get() startLocation:CGPointMake(100, 100) endLocation:CGPointMake(100, 300)]);
        [dataInteractionSimulator run];

        EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"!!editor.querySelector('img')"].boolValue);
    });
}

TEST(DataInteractionTests, ImageToTextarea)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    runTestsExpectingToObserveEvents(webView.get(), @[ DataInteractionEnterEventName, DataInteractionOverEventName, DataInteractionPerformOperationEventName ], ^() {
        [webView synchronouslyLoadTestPageNamed:@"image-and-textarea"];

        RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get() startLocation:CGPointMake(100, 100) endLocation:CGPointMake(100, 300)]);
        [dataInteractionSimulator run];

        NSURL *imageURL = [NSURL fileURLWithPath:[webView stringByEvaluatingJavaScript:@"editor.value"]];
        EXPECT_WK_STREQ("icon.png", imageURL.lastPathComponent);
    });
}

TEST(DataInteractionTests, ContentEditableToContentEditable)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    runTestsExpectingToObserveEvents(webView.get(), @[ DataInteractionEnterEventName, DataInteractionOverEventName, DataInteractionPerformOperationEventName ], ^() {
        [webView synchronouslyLoadTestPageNamed:@"autofocus-contenteditable"];

        RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get() startLocation:CGPointMake(100, 100) endLocation:CGPointMake(100, 300)]);
        [dataInteractionSimulator run];

        EXPECT_EQ([webView stringByEvaluatingJavaScript:@"source.textContent"].length, 0UL);
        EXPECT_WK_STREQ("Hello world", [webView stringByEvaluatingJavaScript:@"editor.textContent"].UTF8String);
    });
}

TEST(DataInteractionTests, LinkToInput)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    runTestsExpectingToObserveEvents(webView.get(), @[ DataInteractionEnterEventName, DataInteractionOverEventName, DataInteractionPerformOperationEventName ], ^() {
        [webView synchronouslyLoadTestPageNamed:@"link-and-input"];

        RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get() startLocation:CGPointMake(100, 100) endLocation:CGPointMake(100, 300)]);
        [dataInteractionSimulator run];

        EXPECT_WK_STREQ("https://www.daringfireball.net/", [webView stringByEvaluatingJavaScript:@"editor.value"].UTF8String);
    });
}

TEST(DataInteractionTests, BackgroundImageLinkToInput)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    runTestsExpectingToObserveEvents(webView.get(), @[ DataInteractionEnterEventName, DataInteractionOverEventName, DataInteractionPerformOperationEventName ], ^() {
        [webView synchronouslyLoadTestPageNamed:@"background-image-link-and-input"];

        RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get() startLocation:CGPointMake(100, 100) endLocation:CGPointMake(100, 300)]);
        [dataInteractionSimulator run];

        EXPECT_WK_STREQ("https://www.daringfireball.net/", [webView stringByEvaluatingJavaScript:@"editor.value"].UTF8String);
    });
}

} // namespace TestWebKitAPI

#endif // ENABLE(DATA_INTERACTION)
