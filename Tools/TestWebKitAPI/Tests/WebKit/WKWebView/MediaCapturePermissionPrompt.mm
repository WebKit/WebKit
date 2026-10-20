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

#if ENABLE(MEDIA_STREAM)


#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <wtf/RetainPtr.h>

using PermissionDecisionHandler = void (^)(WKPermissionDecision);

@interface MediaCapturePermissionPromptUIDelegate : NSObject <WKUIDelegatePrivate>
@property (nonatomic, copy) void (^requestHandler)(WKWebView *, PermissionDecisionHandler);
@property (nonatomic) NSUInteger numberOfRequests;
@end

@implementation MediaCapturePermissionPromptUIDelegate

- (void)dealloc
{
    [_requestHandler release];
    [super dealloc];
}

- (void)webView:(WKWebView *)webView requestMediaCapturePermissionForOrigin:(WKSecurityOrigin *)origin initiatedByFrame:(WKFrameInfo *)frame type:(WKMediaCaptureType)type decisionHandler:(PermissionDecisionHandler)decisionHandler
{
    ++_numberOfRequests;
    _requestHandler(webView, decisionHandler);
}

@end

namespace TestWebKitAPI {

static constexpr auto capturePageMarkup = R"HTML(
<script>
function capture(constraints)
{
    return navigator.mediaDevices.getUserMedia(constraints).then(stream => {
        const tracks = stream.getTracks();
        tracks.forEach(track => track.stop());
        return tracks.map(track => track.kind + ':' + track.label).sort().join(',');
    }, error => error.name);
}
</script>
)HTML";

static std::pair<RetainPtr<TestWKWebView>, RetainPtr<MediaCapturePermissionPromptUIDelegate>> createWebView()
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get()._mediaCaptureEnabled = YES;
#if PLATFORM(IOS_FAMILY)
    configuration.get().allowsInlineMediaPlayback = YES;
#endif
    RetainPtr preferences = [configuration preferences];
    preferences.get()._mediaCaptureRequiresSecureConnection = NO;
    preferences.get()._mockCaptureDevicesEnabled = YES;
    preferences.get()._getUserMediaRequiresFocus = NO;

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get() addToWindow:YES]);
    RetainPtr delegate = adoptNS([[MediaCapturePermissionPromptUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView synchronouslyLoadHTMLString:@(capturePageMarkup) baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
    return { WTF::move(webView), WTF::move(delegate) };
}

static NSString *capture(TestWKWebView *webView, NSDictionary *constraints)
{
    return [webView objectByCallingAsyncFunction:@"return await capture(constraints)" withArguments:@{ @"constraints": constraints }];
}

// A remembered denial is ignored for a request made in response to a user gesture, and
// -callAsyncJavaScript: always runs as one.
static NSString *captureWithoutUserGesture(TestWKWebView *webView, NSDictionary *constraints)
{
    __block bool done = false;
    __block RetainPtr<NSString> result;
    [webView _callAsyncJavaScript:@"return await capture(constraints)" arguments:@{ @"constraints": constraints } inFrame:nil inContentWorld:WKContentWorld.pageWorld withUserGesture:NO completionHandler:^(id value, NSError *) {
        result = value;
        done = true;
    }];
    Util::run(&done);
    return result.autorelease();
}

TEST(MediaCapturePermissionPrompt, AllowGrantsCapture)
{
    auto [webView, delegate] = createWebView();

    __block RetainPtr<_WKMediaCapturePermissionPromptForTesting> shownPrompt;
    [webView _setMediaCapturePermissionPromptHandlerForTesting:^(_WKMediaCapturePermissionPromptForTesting *prompt) {
        shownPrompt = prompt;
        [prompt respondWithButtonIndex:0];
    }];

    [delegate setRequestHandler:^(WKWebView *, PermissionDecisionHandler decisionHandler) {
        decisionHandler(WKPermissionDecisionPrompt);
    }];

    EXPECT_TRUE([capture(webView.get(), @{ @"audio": @YES }) hasPrefix:@"audio:"]);
    EXPECT_TRUE([[shownPrompt title] containsString:@"webkit.org"]);
    EXPECT_EQ(2U, [shownPrompt buttonTitles].count);
}

TEST(MediaCapturePermissionPrompt, AnsweredDenialIsRemembered)
{
    auto [webView, delegate] = createWebView();

    __block NSUInteger numberOfPromptsShown = 0;
    [webView _setMediaCapturePermissionPromptHandlerForTesting:^(_WKMediaCapturePermissionPromptForTesting *prompt) {
        ++numberOfPromptsShown;
        [prompt respondWithButtonIndex:prompt.buttonTitles.count - 1];
    }];

    [delegate setRequestHandler:^(WKWebView *, PermissionDecisionHandler decisionHandler) {
        decisionHandler(WKPermissionDecisionPrompt);
    }];

    EXPECT_WK_STREQ("NotAllowedError", captureWithoutUserGesture(webView.get(), @{ @"video": @YES }));
    EXPECT_WK_STREQ("NotAllowedError", captureWithoutUserGesture(webView.get(), @{ @"video": @YES }));

    EXPECT_EQ(1U, numberOfPromptsShown);
    EXPECT_EQ(1U, [delegate numberOfRequests]);
}

TEST(MediaCapturePermissionPrompt, UnansweredPromptIsNotRememberedAsDenial)
{
    auto [webView, delegate] = createWebView();

    __block NSUInteger numberOfPromptsShown = 0;
    [webView _setMediaCapturePermissionPromptHandlerForTesting:^(_WKMediaCapturePermissionPromptForTesting *prompt) {
        ++numberOfPromptsShown;
        [prompt dismissWithoutAnswer];
    }];

    [delegate setRequestHandler:^(WKWebView *, PermissionDecisionHandler decisionHandler) {
        decisionHandler(WKPermissionDecisionPrompt);
    }];

    captureWithoutUserGesture(webView.get(), @{ @"video": @YES });
    captureWithoutUserGesture(webView.get(), @{ @"video": @YES });

    EXPECT_EQ(2U, numberOfPromptsShown);
    EXPECT_EQ(2U, [delegate numberOfRequests]);
}

} // namespace TestWebKitAPI

#endif // ENABLE(MEDIA_STREAM)
