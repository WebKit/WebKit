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
#import <WebKit/_WKMediaCapturePermissionPromptConfiguration.h>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>

using PermissionDecisionHandler = void (^)(WKPermissionDecision);

using DisplayCaptureDecisionHandler = void (^)(WKDisplayCapturePermissionDecision);

@interface MediaCapturePermissionPromptUIDelegate : NSObject <WKUIDelegatePrivate>
@property (nonatomic, copy) void (^requestHandler)(WKWebView *, PermissionDecisionHandler);
@property (nonatomic, copy) void (^displayCaptureRequestHandler)(WKWebView *, DisplayCaptureDecisionHandler);
@property (nonatomic) NSUInteger numberOfRequests;
@end

@implementation MediaCapturePermissionPromptUIDelegate

- (void)dealloc
{
    [_requestHandler release];
    [_displayCaptureRequestHandler release];
    [super dealloc];
}

- (void)webView:(WKWebView *)webView requestMediaCapturePermissionForOrigin:(WKSecurityOrigin *)origin initiatedByFrame:(WKFrameInfo *)frame type:(WKMediaCaptureType)type decisionHandler:(PermissionDecisionHandler)decisionHandler
{
    ++_numberOfRequests;
    _requestHandler(webView, decisionHandler);
}

- (void)_webView:(WKWebView *)webView requestDisplayCapturePermissionForOrigin:(WKSecurityOrigin *)origin initiatedByFrame:(WKFrameInfo *)frame withSystemAudio:(BOOL)withSystemAudio decisionHandler:(DisplayCaptureDecisionHandler)decisionHandler
{
    _displayCaptureRequestHandler(webView, decisionHandler);
}

- (void)_webView:(WKWebView *)webView requestSpeechRecognitionPermissionForOrigin:(WKSecurityOrigin *)origin decisionHandler:(void (^)(BOOL))decisionHandler
{
    decisionHandler(YES);
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
    preferences.get()._screenCaptureEnabled = YES;
    preferences.get()._speechRecognitionEnabled = YES;

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

TEST(MediaCapturePermissionPrompt, CustomizedTextAndButtons)
{
    auto [webView, delegate] = createWebView();

    __block RetainPtr<_WKMediaCapturePermissionPromptForTesting> shownPrompt;
    [webView _setMediaCapturePermissionPromptHandlerForTesting:^(_WKMediaCapturePermissionPromptForTesting *prompt) {
        shownPrompt = prompt;
        [prompt respondWithButtonIndex:prompt.buttonTitles.count - 1];
    }];

    __block RetainPtr<_WKMediaCapturePermissionPromptResult> promptResult;
    [delegate setRequestHandler:^(WKWebView *requestingWebView, PermissionDecisionHandler decisionHandler) {
        RetainPtr configuration = adoptNS([[_WKMediaCapturePermissionPromptConfiguration alloc] init]);
        [configuration setMessageText:@"Use your camera?"];
        [configuration setInformativeText:@"Choose a camera below."];
        [configuration setAllowButtonTitle:@"Share"];
        [configuration setDenyButtonTitle:@"Not Now"];
        [configuration setAdditionalActions:@[
            [_WKMediaCapturePermissionAction actionWithTitle:@"Never for This Website" decision:_WKMediaCapturePermissionActionDecisionDeny],
            [_WKMediaCapturePermissionAction actionWithTitle:@"Allow Once" decision:_WKMediaCapturePermissionActionDecisionAllow],
        ]];
        [requestingWebView _showMediaCapturePermissionPromptWithConfiguration:configuration.get() completionHandler:^(_WKMediaCapturePermissionPromptResult *result) {
            promptResult = result;
            decisionHandler(WKPermissionDecisionDeny);
        }];
    }];

    EXPECT_WK_STREQ("NotAllowedError", capture(webView.get(), @{ @"video": @YES }));

    EXPECT_WK_STREQ("Use your camera?", [shownPrompt title]);
    EXPECT_WK_STREQ("Choose a camera below.", [shownPrompt informativeText]);
    NSArray *expectedTitles = @[ @"Share", @"Never for This Website", @"Allow Once", @"Not Now" ];
    EXPECT_TRUE([[shownPrompt buttonTitles] isEqualToArray:expectedTitles]);

    EXPECT_EQ(_WKMediaCapturePermissionPromptOutcomeDenied, [promptResult outcome]);
    EXPECT_NULL([promptResult chosenAction]);
}

TEST(MediaCapturePermissionPrompt, DefaultTextWithoutCustomization)
{
    auto [webView, delegate] = createWebView();

    __block RetainPtr<_WKMediaCapturePermissionPromptForTesting> shownPrompt;
    [webView _setMediaCapturePermissionPromptHandlerForTesting:^(_WKMediaCapturePermissionPromptForTesting *prompt) {
        shownPrompt = prompt;
        [prompt respondWithButtonIndex:0];
    }];

    __block RetainPtr<_WKMediaCapturePermissionPromptResult> promptResult;
    [delegate setRequestHandler:^(WKWebView *requestingWebView, PermissionDecisionHandler decisionHandler) {
        [requestingWebView _showMediaCapturePermissionPromptWithConfiguration:nil completionHandler:^(_WKMediaCapturePermissionPromptResult *result) {
            promptResult = result;
            decisionHandler(WKPermissionDecisionGrant);
        }];
    }];

    capture(webView.get(), @{ @"audio": @YES });

    EXPECT_EQ(_WKMediaCapturePermissionPromptOutcomeAllowed, [promptResult outcome]);
    EXPECT_TRUE([[shownPrompt title] containsString:@"webkit.org"]);
    EXPECT_NULL([shownPrompt informativeText]);
    EXPECT_EQ(2U, [shownPrompt buttonTitles].count);
}

TEST(MediaCapturePermissionPrompt, ActionsPastTheCapAreDropped)
{
    auto [webView, delegate] = createWebView();

    __block RetainPtr<NSArray> shownButtonTitles;
    [webView _setMediaCapturePermissionPromptHandlerForTesting:^(_WKMediaCapturePermissionPromptForTesting *prompt) {
        shownButtonTitles = prompt.buttonTitles;
        [prompt dismissWithoutAnswer];
    }];

    [delegate setRequestHandler:^(WKWebView *requestingWebView, PermissionDecisionHandler decisionHandler) {
        RetainPtr configuration = adoptNS([[_WKMediaCapturePermissionPromptConfiguration alloc] init]);
        [configuration setAdditionalActions:@[
            [_WKMediaCapturePermissionAction actionWithTitle:@"First" decision:_WKMediaCapturePermissionActionDecisionDeny],
            [_WKMediaCapturePermissionAction actionWithTitle:@"Second" decision:_WKMediaCapturePermissionActionDecisionDeny],
            [_WKMediaCapturePermissionAction actionWithTitle:@"Third" decision:_WKMediaCapturePermissionActionDecisionDeny],
        ]];
        [requestingWebView _showMediaCapturePermissionPromptWithConfiguration:configuration.get() completionHandler:^(_WKMediaCapturePermissionPromptResult *) {
            decisionHandler(WKPermissionDecisionDeny);
        }];
    }];

    capture(webView.get(), @{ @"video": @YES });

    EXPECT_EQ(2U, _WKMediaCapturePermissionPromptMaximumAdditionalActions);
    EXPECT_EQ(_WKMediaCapturePermissionPromptMaximumAdditionalActions + 2, [shownButtonTitles count]);
    EXPECT_FALSE([shownButtonTitles containsObject:@"Third"]);
}

TEST(MediaCapturePermissionPrompt, ChosenActionIsTheApplicationsObject)
{
    auto [webView, delegate] = createWebView();

    [webView _setMediaCapturePermissionPromptHandlerForTesting:^(_WKMediaCapturePermissionPromptForTesting *prompt) {
        [prompt respondWithButtonIndex:1];
    }];

    RetainPtr neverAllow = [_WKMediaCapturePermissionAction actionWithTitle:@"Never for This Website" decision:_WKMediaCapturePermissionActionDecisionDeny];
    __block RetainPtr<_WKMediaCapturePermissionPromptResult> promptResult;
    [delegate setRequestHandler:^(WKWebView *requestingWebView, PermissionDecisionHandler decisionHandler) {
        RetainPtr configuration = adoptNS([[_WKMediaCapturePermissionPromptConfiguration alloc] init]);
        [configuration setAdditionalActions:@[ neverAllow.get() ]];
        [requestingWebView _showMediaCapturePermissionPromptWithConfiguration:configuration.get() completionHandler:^(_WKMediaCapturePermissionPromptResult *result) {
            promptResult = result;
            decisionHandler(WKPermissionDecisionDeny);
        }];
    }];

    EXPECT_WK_STREQ("NotAllowedError", capture(webView.get(), @{ @"video": @YES }));

    EXPECT_EQ(_WKMediaCapturePermissionPromptOutcomeDenied, [promptResult outcome]);
    EXPECT_EQ(neverAllow.get(), [promptResult chosenAction]);
}

TEST(MediaCapturePermissionPrompt, ApplicationHasTheFinalSay)
{
    auto [webView, delegate] = createWebView();

    [webView _setMediaCapturePermissionPromptHandlerForTesting:^(_WKMediaCapturePermissionPromptForTesting *prompt) {
        [prompt respondWithButtonIndex:prompt.buttonTitles.count - 1];
    }];

    __block RetainPtr<_WKMediaCapturePermissionPromptResult> promptResult;
    [delegate setRequestHandler:^(WKWebView *requestingWebView, PermissionDecisionHandler decisionHandler) {
        [requestingWebView _showMediaCapturePermissionPromptWithConfiguration:nil completionHandler:^(_WKMediaCapturePermissionPromptResult *result) {
            promptResult = result;
            decisionHandler(WKPermissionDecisionGrant);
        }];
    }];

    NSString *captured = capture(webView.get(), @{ @"video": @YES });

    EXPECT_EQ(_WKMediaCapturePermissionPromptOutcomeDenied, [promptResult outcome]);
    EXPECT_TRUE([captured hasPrefix:@"video:"]);
}

TEST(MediaCapturePermissionPrompt, NoRequestBeingDecided)
{
    auto [webView, delegate] = createWebView();

    __block bool wasShown = false;
    [webView _setMediaCapturePermissionPromptHandlerForTesting:^(_WKMediaCapturePermissionPromptForTesting *prompt) {
        wasShown = true;
        [prompt respondWithButtonIndex:0];
    }];

    __block RetainPtr<_WKMediaCapturePermissionPromptResult> promptResult;
    [webView _showMediaCapturePermissionPromptWithConfiguration:nil completionHandler:^(_WKMediaCapturePermissionPromptResult *result) {
        promptResult = result;
    }];

    EXPECT_NOT_NULL(promptResult.get());
    EXPECT_EQ(_WKMediaCapturePermissionPromptOutcomeDismissed, [promptResult outcome]);
    EXPECT_FALSE(wasShown);
}

TEST(MediaCapturePermissionPrompt, SecondCallForTheSameRequestIsRefused)
{
    auto [webView, delegate] = createWebView();

    __block NSUInteger numberOfPromptsShown = 0;
    __block RetainPtr<_WKMediaCapturePermissionPromptResult> secondResult;
    __block TestWKWebView *promptingWebView = webView.get();
    [webView _setMediaCapturePermissionPromptHandlerForTesting:^(_WKMediaCapturePermissionPromptForTesting *prompt) {
        ++numberOfPromptsShown;
        [promptingWebView _showMediaCapturePermissionPromptWithConfiguration:nil completionHandler:^(_WKMediaCapturePermissionPromptResult *result) {
            secondResult = result;
        }];
        [prompt respondWithButtonIndex:prompt.buttonTitles.count - 1];
    }];

    [delegate setRequestHandler:^(WKWebView *requestingWebView, PermissionDecisionHandler decisionHandler) {
        [requestingWebView _showMediaCapturePermissionPromptWithConfiguration:nil completionHandler:^(_WKMediaCapturePermissionPromptResult *) {
            decisionHandler(WKPermissionDecisionDeny);
        }];
    }];

    EXPECT_WK_STREQ("NotAllowedError", capture(webView.get(), @{ @"video": @YES }));

    EXPECT_EQ(1U, numberOfPromptsShown);
    EXPECT_EQ(_WKMediaCapturePermissionPromptOutcomeDismissed, [secondResult outcome]);
}

TEST(MediaCapturePermissionPrompt, GrantWhilePromptIsUpReportsDismissed)
{
    auto [webView, delegate] = createWebView();

    __block RetainPtr<_WKMediaCapturePermissionPromptForTesting> pendingPrompt;
    [webView _setMediaCapturePermissionPromptHandlerForTesting:^(_WKMediaCapturePermissionPromptForTesting *prompt) {
        pendingPrompt = prompt;
    }];

    __block RetainPtr<_WKMediaCapturePermissionPromptResult> promptResult;
    [delegate setRequestHandler:^(WKWebView *requestingWebView, PermissionDecisionHandler decisionHandler) {
        [requestingWebView _showMediaCapturePermissionPromptWithConfiguration:nil completionHandler:^(_WKMediaCapturePermissionPromptResult *result) {
            promptResult = result;
        }];
        decisionHandler(WKPermissionDecisionGrant);
    }];

    NSString *captured = capture(webView.get(), @{ @"video": @YES });

    EXPECT_EQ(_WKMediaCapturePermissionPromptOutcomeDismissed, [promptResult outcome]);
    EXPECT_NULL([promptResult chosenAction]);
    EXPECT_TRUE([captured hasPrefix:@"video:"]);

    // The prompt has already closed, so a late answer changes nothing.
    [pendingPrompt respondWithButtonIndex:0];
    EXPECT_EQ(_WKMediaCapturePermissionPromptOutcomeDismissed, [promptResult outcome]);
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

static void startCapture(TestWKWebView *webView, NSString *constraints)
{
    [webView evaluateJavaScript:[NSString stringWithFormat:@"capture(%@)", constraints] completionHandler:nil];
}

static void testPromptClosedByThePage(NOESCAPE const Function<void(TestWKWebView *)>& closePrompt)
{
    auto [webView, delegate] = createWebView();

    __block bool wasShown = false;
    [webView _setMediaCapturePermissionPromptHandlerForTesting:^(_WKMediaCapturePermissionPromptForTesting *prompt) {
        wasShown = true;
    }];

    __block bool done = false;
    __block RetainPtr<_WKMediaCapturePermissionPromptResult> promptResult;
    [delegate setRequestHandler:^(WKWebView *requestingWebView, PermissionDecisionHandler decisionHandler) {
        [requestingWebView _showMediaCapturePermissionPromptWithConfiguration:nil completionHandler:^(_WKMediaCapturePermissionPromptResult *result) {
            promptResult = result;
            decisionHandler(WKPermissionDecisionDeny);
            done = true;
        }];
    }];

    startCapture(webView.get(), @"{ video: true }");
    Util::run(&wasShown);
    closePrompt(webView.get());
    Util::run(&done);

    EXPECT_EQ(_WKMediaCapturePermissionPromptOutcomeDismissed, [promptResult outcome]);
    EXPECT_NULL([promptResult chosenAction]);
}

TEST(MediaCapturePermissionPrompt, NavigationClosesThePrompt)
{
    testPromptClosedByThePage([](TestWKWebView *webView) {
        [webView synchronouslyLoadHTMLString:@"<p>Another document</p>" baseURL:[NSURL URLWithString:@"https://webkit.org/other"]];
    });
}

TEST(MediaCapturePermissionPrompt, ClosingTheWebViewClosesThePrompt)
{
    testPromptClosedByThePage([](TestWKWebView *webView) {
        [webView _close];
    });
}

TEST(MediaCapturePermissionPrompt, ScreenCaptureRequestIsRefused)
{
    auto [webView, delegate] = createWebView();

    __block bool wasShown = false;
    [webView _setMediaCapturePermissionPromptHandlerForTesting:^(_WKMediaCapturePermissionPromptForTesting *prompt) {
        wasShown = true;
        [prompt respondWithButtonIndex:0];
    }];

    __block RetainPtr<_WKMediaCapturePermissionPromptResult> promptResult;
    [delegate setDisplayCaptureRequestHandler:^(WKWebView *requestingWebView, DisplayCaptureDecisionHandler decisionHandler) {
        [requestingWebView _showMediaCapturePermissionPromptWithConfiguration:nil completionHandler:^(_WKMediaCapturePermissionPromptResult *result) {
            promptResult = result;
            decisionHandler(WKDisplayCapturePermissionDecisionDeny);
        }];
    }];

    NSString *captured = [webView objectByCallingAsyncFunction:@"try { await navigator.mediaDevices.getDisplayMedia({ video: true }); return 'granted'; } catch (error) { return error.name; }" withArguments:nil];

    EXPECT_WK_STREQ("NotAllowedError", captured);
    EXPECT_NOT_NULL(promptResult.get());
    EXPECT_EQ(_WKMediaCapturePermissionPromptOutcomeDismissed, [promptResult outcome]);
    EXPECT_FALSE(wasShown);
}

TEST(MediaCapturePermissionPrompt, SpeechRecognitionWaitsForTheCaptureDecision)
{
    auto [webView, delegate] = createWebView();

    __block RetainPtr<_WKMediaCapturePermissionPromptForTesting> shownPrompt;
    [webView _setMediaCapturePermissionPromptHandlerForTesting:^(_WKMediaCapturePermissionPromptForTesting *prompt) {
        shownPrompt = prompt;
        [prompt respondWithButtonIndex:prompt.buttonTitles.count - 1];
    }];

    __block BlockPtr<void(WKPermissionDecision)> pendingCaptureDecision;
    __block bool captureRequested = false;
    __block bool speechPromptDone = false;
    __block RetainPtr<_WKMediaCapturePermissionPromptResult> speechPromptResult;
    [delegate setRequestHandler:^(WKWebView *requestingWebView, PermissionDecisionHandler decisionHandler) {
        if (!pendingCaptureDecision) {
            pendingCaptureDecision = makeBlockPtr(decisionHandler);
            captureRequested = true;
            return;
        }
        [requestingWebView _showMediaCapturePermissionPromptWithConfiguration:nil completionHandler:^(_WKMediaCapturePermissionPromptResult *result) {
            speechPromptResult = result;
            decisionHandler(WKPermissionDecisionDeny);
            speechPromptDone = true;
        }];
    }];

    startCapture(webView.get(), @"{ video: true }");
    Util::run(&captureRequested);

    [webView evaluateJavaScript:@"window.recognition = new webkitSpeechRecognition(); recognition.start();" completionHandler:nil];
    Util::runFor(500_ms);
    EXPECT_EQ(1U, [delegate numberOfRequests]);
    EXPECT_NULL(shownPrompt.get());

    pendingCaptureDecision(WKPermissionDecisionDeny);
    Util::run(&speechPromptDone);

    EXPECT_EQ(2U, [delegate numberOfRequests]);
    EXPECT_TRUE([[shownPrompt title] containsString:@"speech recognition"]);
    EXPECT_EQ(_WKMediaCapturePermissionPromptOutcomeDenied, [speechPromptResult outcome]);
}

} // namespace TestWebKitAPI

#endif // ENABLE(MEDIA_STREAM)
