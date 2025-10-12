/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKFindConfiguration.h>
#import <WebKit/WKFindResult.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/_WKFindDelegate.h>
#import <wtf/RetainPtr.h>

#if ENABLE(IMAGE_ANALYSIS)
#import "ImageAnalysisTestingUtilities.h"
#import "InstanceMethodSwizzler.h"
#import <WebKit/_WKFeature.h>
#import <pal/cocoa/VisionKitCoreSoftLink.h>
#endif

TEST(WKWebView, FindAPIForwardsNoMatch)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadHTMLString:@"word word" baseURL:nil];

    // The default find configuration is "forwards, case insensitive, wrapping"
    auto configuration = adoptNS([[WKFindConfiguration alloc] init]);

    EXPECT_TRUE([webView selectionRangeHasStartOffset:0 endOffset:0]);

    static bool done;
    [webView findString:@"foobar" withConfiguration:configuration.get() completionHandler:^(WKFindResult *result) {
        EXPECT_FALSE(result.matchFound);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:0 endOffset:0]);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(WKWebView, FindAPIForwardsWrap)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadHTMLString:@"word word" baseURL:nil];

    // The default find configuration is "forwards, case insensitive, wrapping"
    auto configuration = adoptNS([[WKFindConfiguration alloc] init]);

    static bool done;
    [webView findString:@"word" withConfiguration:configuration.get() completionHandler:^(WKFindResult *result) {
        EXPECT_TRUE(result.matchFound);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:0 endOffset:4]);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView findString:@"word" withConfiguration:configuration.get() completionHandler:^(WKFindResult *result) {
        EXPECT_TRUE(result.matchFound);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:5 endOffset:9]);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView findString:@"word" withConfiguration:configuration.get() completionHandler:^(WKFindResult *result) {
        EXPECT_TRUE(result.matchFound);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:0 endOffset:4]);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(WKWebView, FindAPIForwardsNoWrap)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadHTMLString:@"word word" baseURL:nil];

    // The default find configuration is "forwards, case insensitive, wrapping"
    auto configuration = adoptNS([[WKFindConfiguration alloc] init]);
    configuration.get().wraps = NO;

    static bool done;
    [webView findString:@"word" withConfiguration:configuration.get() completionHandler:^(WKFindResult *result) {
        EXPECT_TRUE(result.matchFound);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:0 endOffset:4]);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView findString:@"word" withConfiguration:configuration.get() completionHandler:^(WKFindResult *result) {
        EXPECT_TRUE(result.matchFound);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:5 endOffset:9]);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView findString:@"word" withConfiguration:configuration.get() completionHandler:^(WKFindResult *result) {
        EXPECT_FALSE(result.matchFound);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:0 endOffset:0]);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(WKWebView, FindAPIBackwardsWrap)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadHTMLString:@"word word" baseURL:nil];

    // The default find configuration is "forwards, case insensitive, wrapping"
    auto configuration = adoptNS([[WKFindConfiguration alloc] init]);
    configuration.get().backwards = YES;

    EXPECT_TRUE([webView selectionRangeHasStartOffset:0 endOffset:0]);

    static bool done;
    [webView findString:@"word" withConfiguration:configuration.get() completionHandler:^(WKFindResult *result) {
        EXPECT_TRUE(result.matchFound);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:5 endOffset:9]);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView findString:@"word" withConfiguration:configuration.get() completionHandler:^(WKFindResult *result) {
        EXPECT_TRUE(result.matchFound);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:0 endOffset:4]);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView findString:@"word" withConfiguration:configuration.get() completionHandler:^(WKFindResult *result) {
        EXPECT_TRUE(result.matchFound);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:5 endOffset:9]);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(WKWebView, FindAPIBackwardsNoWrap)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadHTMLString:@"word word" baseURL:nil];

    // The default find configuration is "forwards, case insensitive, wrapping"
    auto configuration = adoptNS([[WKFindConfiguration alloc] init]);
    configuration.get().backwards = YES;
    configuration.get().wraps = NO;

    EXPECT_TRUE([webView selectionRangeHasStartOffset:0 endOffset:0]);

    static bool done;
    [webView findString:@"word" withConfiguration:configuration.get() completionHandler:^(WKFindResult *result) {
        EXPECT_TRUE(result.matchFound);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:5 endOffset:9]);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView findString:@"word" withConfiguration:configuration.get() completionHandler:^(WKFindResult *result) {
        EXPECT_TRUE(result.matchFound);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:0 endOffset:4]);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView findString:@"word" withConfiguration:configuration.get() completionHandler:^(WKFindResult *result) {
        EXPECT_FALSE(result.matchFound);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:0 endOffset:0]);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(WKWebView, FindAPIForwardsCaseSensitive)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadHTMLString:@"word Word word" baseURL:nil];

    // The default find configuration is "forwards, case insensitive, wrapping"
    auto configuration = adoptNS([[WKFindConfiguration alloc] init]);
    configuration.get().caseSensitive = YES;

    static bool done;
    [webView findString:@"word" withConfiguration:configuration.get() completionHandler:^(WKFindResult *result) {
        EXPECT_TRUE(result.matchFound);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:0 endOffset:4]);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView findString:@"word" withConfiguration:configuration.get() completionHandler:^(WKFindResult *result) {
        EXPECT_TRUE(result.matchFound);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:10 endOffset:14]);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView findString:@"word" withConfiguration:configuration.get() completionHandler:^(WKFindResult *result) {
        EXPECT_TRUE(result.matchFound);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:0 endOffset:4]);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView findString:@"Word" withConfiguration:configuration.get() completionHandler:^(WKFindResult *result) {
        EXPECT_TRUE(result.matchFound);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:5 endOffset:9]);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
}
#if PLATFORM(MAC)
#if ENABLE(IMAGE_ANALYSIS)
static unsigned gDidProcessRequestCount = 0;

@interface FindDelegate : NSObject<_WKFindDelegate>
@property (nonatomic, readonly) int numberOfCallsToDidFindMatches;
@end

@implementation FindDelegate

- (void)_webView:(WKWebView *)webView didFindMatches:(NSUInteger)matches forString:(NSString *)string withMatchIndex:(NSInteger)matchIndex
{
    _numberOfCallsToDidFindMatches++;
}

@end

static void processRequestWithResults(id, SEL, VKImageAnalyzerRequest *, void (^)(double progress), void (^completion)(VKImageAnalysis *, NSError *))
{
    gDidProcessRequestCount++;
    completion(TestWebKitAPI::createImageAnalysisWithSimpleFixedResults().get(), nil);
}

static VKImageAnalyzerRequest *makeFakeRequest(id, SEL, CGImageRef image, VKImageOrientation orientation, VKAnalysisTypes requestTypes)
{
    return TestWebKitAPI::createRequest(image, orientation, requestTypes).leakRef();
}

template <typename FunctionType>
std::pair<std::unique_ptr<InstanceMethodSwizzler>, std::unique_ptr<InstanceMethodSwizzler>> makeImageAnalysisRequestSwizzler(FunctionType function)
{
    return std::pair {
        makeUnique<InstanceMethodSwizzler>(PAL::getVKImageAnalyzerClassSingleton(), @selector(processRequest:progressHandler:completionHandler:), reinterpret_cast<IMP>(function)),
        makeUnique<InstanceMethodSwizzler>(PAL::getVKImageAnalyzerRequestClassSingleton(), @selector(initWithCGImage:orientation:requestType:), reinterpret_cast<IMP>(makeFakeRequest))
    };
}

static RetainPtr<TestWKWebView> createWebViewWithImageAnalysisDuringFindInPageEnabled()
{
    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    for (_WKFeature *feature in WKPreferences._features) {
        NSString *key = feature.key;
        if ([key isEqualToString:@"ImageAnalysisDuringFindInPageEnabled"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }

    return adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get()]);
}

TEST(WKWebView, FindAPITextInImage)
{
    auto requestSwizzler = makeImageAnalysisRequestSwizzler(processRequestWithResults);

    auto webView = createWebViewWithImageAnalysisDuringFindInPageEnabled();
    [webView synchronouslyLoadTestPageNamed:@"image-with-text"];

    auto findDelegate = adoptNS([[FindDelegate alloc] init]);
    [webView _setFindDelegate:findDelegate.get()];

    __block bool done;
    [webView findString:@"text" withConfiguration:adoptNS([WKFindConfiguration new]).get() completionHandler:^(WKFindResult *) {
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);
    TestWebKitAPI::Util::waitForConditionWithLogging([&] {
        return gDidProcessRequestCount == 1 && [findDelegate numberOfCallsToDidFindMatches] == 2;
    }, 1, @"Timed out waiting for image analysis to start and for search for string to be performed twice.");
}

#endif // ENABLE(IMAGE_ANALYSIS)

#endif // PLATFORM(MAC)
