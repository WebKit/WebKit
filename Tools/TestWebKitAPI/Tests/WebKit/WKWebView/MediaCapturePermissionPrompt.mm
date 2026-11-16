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
#import <WebKit/WKMockMediaDevice.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKRetainPtr.h>
#import <WebKit/WKString.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/_WKFeature.h>
#import <WebKit/_WKMediaCapturePermissionPromptConfiguration.h>
#import <signal.h>
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

let keptStream;
function startKeptCapture(constraints)
{
    return navigator.mediaDevices.getUserMedia(constraints).then(stream => {
        keptStream = stream;
        return 'capturing';
    }, error => error.name);
}
</script>
)HTML";

static void setCapturePreviewInPromptEnabled(WKWebViewConfiguration *configuration, BOOL enabled)
{
    for (_WKFeature *feature in [WKPreferences _features]) {
        if ([feature.key isEqualToString:@"CaptureDevicePreviewInPromptEnabled"]) {
            [[configuration preferences] _setEnabled:enabled forFeature:feature];
            return;
        }
    }
}

static std::pair<RetainPtr<TestWKWebView>, RetainPtr<MediaCapturePermissionPromptUIDelegate>> createWebView(BOOL showsPreview = YES)
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
    setCapturePreviewInPromptEnabled(configuration.get(), showsPreview);

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

static NSString *labelOfDevice(NSArray<NSDictionary *> *devices, NSString *persistentID)
{
    for (NSDictionary *device in devices) {
        if ([device[@"id"] isEqualToString:persistentID])
            return device[@"label"];
    }
    return nil;
}

TEST(MediaCapturePermissionPrompt, CustomizedTextAndButtons)
{
    auto [webView, delegate] = createWebView();

    __block RetainPtr<_WKMediaCapturePermissionPromptForTesting> shownPrompt;
    [webView _setMediaCapturePermissionPromptHandlerForTesting:^(_WKMediaCapturePermissionPromptForTesting *prompt) {
        shownPrompt = prompt;
        [prompt respondWithButtonIndex:prompt.buttonTitles.count - 1 cameraID:nil microphoneID:nil];
    } runsPreview:NO];

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
    EXPECT_TRUE([shownPrompt showsPreview]);

    EXPECT_EQ(_WKMediaCapturePermissionPromptOutcomeDenied, [promptResult outcome]);
    EXPECT_NULL([promptResult chosenAction]);
}

TEST(MediaCapturePermissionPrompt, DefaultTextWithoutCustomization)
{
    auto [webView, delegate] = createWebView();

    __block RetainPtr<_WKMediaCapturePermissionPromptForTesting> shownPrompt;
    [webView _setMediaCapturePermissionPromptHandlerForTesting:^(_WKMediaCapturePermissionPromptForTesting *prompt) {
        shownPrompt = prompt;
        [prompt respondWithButtonIndex:0 cameraID:nil microphoneID:nil];
    } runsPreview:NO];

    __block RetainPtr<_WKMediaCapturePermissionPromptResult> promptResult;
    [delegate setRequestHandler:^(WKWebView *requestingWebView, PermissionDecisionHandler decisionHandler) {
        [requestingWebView _showMediaCapturePermissionPromptWithConfiguration:nil completionHandler:^(_WKMediaCapturePermissionPromptResult *result) {
            promptResult = result;
            decisionHandler(WKPermissionDecisionGrant);
        }];
    }];

    capture(webView.get(), @{ @"audio": @YES });

    EXPECT_EQ(_WKMediaCapturePermissionPromptOutcomeAllowed, [promptResult outcome]);
    EXPECT_NOT_NULL([promptResult microphoneDeviceID]);
    EXPECT_NULL([promptResult cameraDeviceID]);
    EXPECT_TRUE([[shownPrompt title] containsString:@"webkit.org"]);
    EXPECT_NULL([shownPrompt informativeText]);
    EXPECT_EQ(2U, [shownPrompt buttonTitles].count);
    EXPECT_EQ(0U, [shownPrompt cameras].count);
    EXPECT_GT([shownPrompt microphones].count, 0U);
}

TEST(MediaCapturePermissionPrompt, ActionsPastTheCapAreDropped)
{
    auto [webView, delegate] = createWebView();

    __block RetainPtr<NSArray> shownButtonTitles;
    [webView _setMediaCapturePermissionPromptHandlerForTesting:^(_WKMediaCapturePermissionPromptForTesting *prompt) {
        shownButtonTitles = prompt.buttonTitles;
        [prompt dismissWithoutAnswer];
    } runsPreview:NO];

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
        [prompt respondWithButtonIndex:1 cameraID:nil microphoneID:nil];
    } runsPreview:NO];

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
    EXPECT_NULL([promptResult cameraDeviceID]);
}

TEST(MediaCapturePermissionPrompt, AllowingActionCapturesTheChosenCamera)
{
    auto [webView, delegate] = createWebView();

    __block RetainPtr<NSString> chosenCameraLabel;
    [webView _setMediaCapturePermissionPromptHandlerForTesting:^(_WKMediaCapturePermissionPromptForTesting *prompt) {
        NSArray<NSDictionary *> *cameras = prompt.cameras;
        ASSERT_GT(cameras.count, 1U);
        chosenCameraLabel = cameras[1][@"label"];
        [prompt respondWithButtonIndex:1 cameraID:cameras[1][@"id"] microphoneID:nil];
    } runsPreview:NO];

    __block RetainPtr<_WKMediaCapturePermissionPromptResult> promptResult;
    [delegate setRequestHandler:^(WKWebView *requestingWebView, PermissionDecisionHandler decisionHandler) {
        RetainPtr configuration = adoptNS([[_WKMediaCapturePermissionPromptConfiguration alloc] init]);
        [configuration setAdditionalActions:@[ [_WKMediaCapturePermissionAction actionWithTitle:@"Allow Once" decision:_WKMediaCapturePermissionActionDecisionAllow] ]];
        [requestingWebView _showMediaCapturePermissionPromptWithConfiguration:configuration.get() completionHandler:^(_WKMediaCapturePermissionPromptResult *result) {
            promptResult = result;
            decisionHandler(WKPermissionDecisionGrant);
        }];
    }];

    NSString *captured = capture(webView.get(), @{ @"video": @YES });

    EXPECT_EQ(_WKMediaCapturePermissionPromptOutcomeAllowed, [promptResult outcome]);
    EXPECT_WK_STREQ("Allow Once", [[promptResult chosenAction] title]);
    EXPECT_NOT_NULL([promptResult cameraDeviceID]);
    NSString *expected = [NSString stringWithFormat:@"video:%@", chosenCameraLabel.get()];
    EXPECT_WK_STREQ(expected, captured);
}

TEST(MediaCapturePermissionPrompt, ReportedDeviceIDsNameTheCapturedDevices)
{
    auto [webView, delegate] = createWebView();

    __block RetainPtr<_WKMediaCapturePermissionPromptForTesting> shownPrompt;
    [webView _setMediaCapturePermissionPromptHandlerForTesting:^(_WKMediaCapturePermissionPromptForTesting *prompt) {
        shownPrompt = prompt;
        NSArray<NSDictionary *> *cameras = prompt.cameras;
        NSArray<NSDictionary *> *microphones = prompt.microphones;
        [prompt respondWithButtonIndex:0 cameraID:cameras.lastObject[@"id"] microphoneID:microphones.lastObject[@"id"]];
    } runsPreview:NO];

    __block RetainPtr<_WKMediaCapturePermissionPromptResult> promptResult;
    [delegate setRequestHandler:^(WKWebView *requestingWebView, PermissionDecisionHandler decisionHandler) {
        [requestingWebView _showMediaCapturePermissionPromptWithConfiguration:nil completionHandler:^(_WKMediaCapturePermissionPromptResult *result) {
            promptResult = result;
            decisionHandler(WKPermissionDecisionGrant);
        }];
    }];

    NSString *captured = capture(webView.get(), @{ @"audio": @YES, @"video": @YES });

    EXPECT_EQ(_WKMediaCapturePermissionPromptOutcomeAllowed, [promptResult outcome]);
    EXPECT_NULL([promptResult chosenAction]);
    NSString *cameraLabel = labelOfDevice([shownPrompt cameras], [promptResult cameraDeviceID]);
    NSString *microphoneLabel = labelOfDevice([shownPrompt microphones], [promptResult microphoneDeviceID]);
    EXPECT_NOT_NULL(cameraLabel);
    EXPECT_NOT_NULL(microphoneLabel);
    NSString *expected = [NSString stringWithFormat:@"audio:%@,video:%@", microphoneLabel, cameraLabel];
    EXPECT_WK_STREQ(expected, captured);
}

TEST(MediaCapturePermissionPrompt, ApplicationHasTheFinalSay)
{
    auto [webView, delegate] = createWebView();

    [webView _setMediaCapturePermissionPromptHandlerForTesting:^(_WKMediaCapturePermissionPromptForTesting *prompt) {
        [prompt respondWithButtonIndex:prompt.buttonTitles.count - 1 cameraID:nil microphoneID:nil];
    } runsPreview:NO];

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

TEST(MediaCapturePermissionPrompt, PlainPromptHasNoDevices)
{
    auto [webView, delegate] = createWebView(NO);

    __block RetainPtr<_WKMediaCapturePermissionPromptForTesting> shownPrompt;
    [webView _setMediaCapturePermissionPromptHandlerForTesting:^(_WKMediaCapturePermissionPromptForTesting *prompt) {
        shownPrompt = prompt;
        [prompt respondWithButtonIndex:1 cameraID:nil microphoneID:nil];
    } runsPreview:NO];

    __block RetainPtr<_WKMediaCapturePermissionPromptResult> promptResult;
    [delegate setRequestHandler:^(WKWebView *requestingWebView, PermissionDecisionHandler decisionHandler) {
        RetainPtr configuration = adoptNS([[_WKMediaCapturePermissionPromptConfiguration alloc] init]);
        [configuration setInformativeText:@"Shown in the plain prompt too."];
        [configuration setAdditionalActions:@[ [_WKMediaCapturePermissionAction actionWithTitle:@"Allow Once" decision:_WKMediaCapturePermissionActionDecisionAllow] ]];
        [requestingWebView _showMediaCapturePermissionPromptWithConfiguration:configuration.get() completionHandler:^(_WKMediaCapturePermissionPromptResult *result) {
            promptResult = result;
            decisionHandler(WKPermissionDecisionGrant);
        }];
    }];

    NSString *captured = capture(webView.get(), @{ @"video": @YES });

    EXPECT_FALSE([shownPrompt showsPreview]);
    EXPECT_EQ(0U, [shownPrompt cameras].count);
    EXPECT_WK_STREQ("Shown in the plain prompt too.", [shownPrompt informativeText]);
    EXPECT_EQ(_WKMediaCapturePermissionPromptOutcomeAllowed, [promptResult outcome]);
    EXPECT_WK_STREQ("Allow Once", [[promptResult chosenAction] title]);
    EXPECT_NULL([promptResult cameraDeviceID]);
    EXPECT_TRUE([captured hasPrefix:@"video:"]);
}

TEST(MediaCapturePermissionPrompt, NoRequestBeingDecided)
{
    auto [webView, delegate] = createWebView();

    __block bool wasShown = false;
    [webView _setMediaCapturePermissionPromptHandlerForTesting:^(_WKMediaCapturePermissionPromptForTesting *prompt) {
        wasShown = true;
        [prompt respondWithButtonIndex:0 cameraID:nil microphoneID:nil];
    } runsPreview:NO];

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
        [prompt respondWithButtonIndex:prompt.buttonTitles.count - 1 cameraID:nil microphoneID:nil];
    } runsPreview:NO];

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
    } runsPreview:NO];

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

    [pendingPrompt respondWithButtonIndex:0 cameraID:nil microphoneID:nil];
    EXPECT_EQ(_WKMediaCapturePermissionPromptOutcomeDismissed, [promptResult outcome]);
}

TEST(MediaCapturePermissionPrompt, AnsweredDenialIsRemembered)
{
    auto [webView, delegate] = createWebView();

    __block NSUInteger numberOfPromptsShown = 0;
    [webView _setMediaCapturePermissionPromptHandlerForTesting:^(_WKMediaCapturePermissionPromptForTesting *prompt) {
        ++numberOfPromptsShown;
        [prompt respondWithButtonIndex:prompt.buttonTitles.count - 1 cameraID:nil microphoneID:nil];
    } runsPreview:NO];

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
    } runsPreview:NO];

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
    } runsPreview:NO];

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
        [prompt respondWithButtonIndex:0 cameraID:nil microphoneID:nil];
    } runsPreview:NO];

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

static void testChosenMicrophoneForLaterRequests(size_t chosenIndex, bool expectChoiceKept)
{
    auto [webView, delegate] = createWebView();

    __block RetainPtr<NSString> chosenLabel;
    __block RetainPtr<NSString> bestLabel;
    [webView _setMediaCapturePermissionPromptHandlerForTesting:^(_WKMediaCapturePermissionPromptForTesting *prompt) {
        NSArray<NSDictionary *> *microphones = prompt.microphones;
        ASSERT_GT(microphones.count, chosenIndex);
        bestLabel = microphones.firstObject[@"label"];
        chosenLabel = microphones[chosenIndex][@"label"];
        [prompt respondWithButtonIndex:0 cameraID:nil microphoneID:microphones[chosenIndex][@"id"]];
    } runsPreview:NO];

    [delegate setRequestHandler:^(WKWebView *requestingWebView, PermissionDecisionHandler decisionHandler) {
        [requestingWebView _showMediaCapturePermissionPromptWithConfiguration:nil completionHandler:^(_WKMediaCapturePermissionPromptResult *) {
            decisionHandler(WKPermissionDecisionGrant);
        }];
    }];

    NSString *firstCapture = capture(webView.get(), @{ @"audio": @YES });
    NSString *expectedFirst = [NSString stringWithFormat:@"audio:%@", chosenLabel.get()];
    EXPECT_WK_STREQ(expectedFirst, firstCapture);

    NSString *expectedLater = [NSString stringWithFormat:@"audio:%@", expectChoiceKept ? chosenLabel.get() : bestLabel.get()];
    EXPECT_WK_STREQ(expectedLater, capture(webView.get(), @{ @"audio": @YES }));
    EXPECT_EQ(1U, [delegate numberOfRequests]);

    NSString *firstListedMicrophone = [webView objectByCallingAsyncFunction:@"const devices = await navigator.mediaDevices.enumerateDevices(); return devices.find(device => device.kind == 'audioinput').label;" withArguments:nil];
    EXPECT_WK_STREQ(expectChoiceKept ? chosenLabel.get() : bestLabel.get(), firstListedMicrophone);
}

TEST(MediaCapturePermissionPrompt, ChosenMicrophoneIsUsedByLaterRequests)
{
    testChosenMicrophoneForLaterRequests(1, true);
}

TEST(MediaCapturePermissionPrompt, ChosenMicrophoneYieldsToABetterMatch)
{
    testChosenMicrophoneForLaterRequests(3, false);
}

TEST(MediaCapturePermissionPrompt, ChosenMicrophoneIsOfferedByALaterPrompt)
{
    auto [webView, delegate] = createWebView();

    __block RetainPtr<NSString> chosenLabel;
    __block RetainPtr<NSString> firstOfferedLabel;
    __block unsigned promptCount = 0;
    __block bool secondPromptShown = false;
    [webView _setMediaCapturePermissionPromptHandlerForTesting:^(_WKMediaCapturePermissionPromptForTesting *prompt) {
        NSArray<NSDictionary *> *microphones = prompt.microphones;
        ASSERT_GT(microphones.count, 1U);
        if (!promptCount++) {
            chosenLabel = microphones[1][@"label"];
            [prompt respondWithButtonIndex:0 cameraID:nil microphoneID:microphones[1][@"id"]];
            return;
        }
        firstOfferedLabel = microphones.firstObject[@"label"];
        secondPromptShown = true;
        [prompt respondWithButtonIndex:prompt.buttonTitles.count - 1 cameraID:nil microphoneID:nil];
    } runsPreview:NO];

    [delegate setRequestHandler:^(WKWebView *requestingWebView, PermissionDecisionHandler decisionHandler) {
        [requestingWebView _showMediaCapturePermissionPromptWithConfiguration:nil completionHandler:^(_WKMediaCapturePermissionPromptResult *result) {
            decisionHandler(result.outcome == _WKMediaCapturePermissionPromptOutcomeDenied ? WKPermissionDecisionDeny : WKPermissionDecisionGrant);
        }];
    }];

    capture(webView.get(), @{ @"audio": @YES });
    EXPECT_EQ(1U, promptCount);

    startCapture(webView.get(), @"{ audio: true, video: true }");
    Util::run(&secondPromptShown);
    EXPECT_WK_STREQ(chosenLabel.get(), firstOfferedLabel.get());
}

TEST(MediaCapturePermissionPrompt, ChosenMicrophoneIsUsedWhenTheDelegateGrantsALaterRequest)
{
    auto [webView, delegate] = createWebView();

    __block RetainPtr<NSString> chosenLabel;
    [webView _setMediaCapturePermissionPromptHandlerForTesting:^(_WKMediaCapturePermissionPromptForTesting *prompt) {
        NSArray<NSDictionary *> *microphones = prompt.microphones;
        ASSERT_GT(microphones.count, 1U);
        chosenLabel = microphones[1][@"label"];
        [prompt respondWithButtonIndex:0 cameraID:nil microphoneID:microphones[1][@"id"]];
    } runsPreview:NO];

    __block bool showPromptForNextRequest = true;
    [delegate setRequestHandler:^(WKWebView *requestingWebView, PermissionDecisionHandler decisionHandler) {
        if (!showPromptForNextRequest) {
            decisionHandler(WKPermissionDecisionGrant);
            return;
        }
        showPromptForNextRequest = false;
        [requestingWebView _showMediaCapturePermissionPromptWithConfiguration:nil completionHandler:^(_WKMediaCapturePermissionPromptResult *) {
            decisionHandler(WKPermissionDecisionGrant);
        }];
    }];

    capture(webView.get(), @{ @"audio": @YES });

    NSString *result = capture(webView.get(), @{ @"audio": @YES, @"video": @YES });
    NSString *expectedAudio = [NSString stringWithFormat:@"audio:%@", chosenLabel.get()];
    EXPECT_TRUE([result containsString:expectedAudio]);
}

TEST(MediaCapturePermissionPrompt, SpeechRecognitionWaitsForTheCaptureDecision)
{
    auto [webView, delegate] = createWebView();

    __block RetainPtr<_WKMediaCapturePermissionPromptForTesting> shownPrompt;
    [webView _setMediaCapturePermissionPromptHandlerForTesting:^(_WKMediaCapturePermissionPromptForTesting *prompt) {
        shownPrompt = prompt;
        [prompt respondWithButtonIndex:prompt.buttonTitles.count - 1 cameraID:nil microphoneID:nil];
    } runsPreview:NO];

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
    EXPECT_EQ(0U, [shownPrompt cameras].count);
    EXPECT_EQ(1U, [shownPrompt microphones].count);
    EXPECT_EQ(_WKMediaCapturePermissionPromptOutcomeDenied, [speechPromptResult outcome]);
}

static RetainPtr<_WKMediaCapturePermissionPromptForTesting> showPreviewPrompt(TestWKWebView *webView, MediaCapturePermissionPromptUIDelegate *delegate, NSString *constraints, std::shared_ptr<bool> finished)
{
    __block RetainPtr<_WKMediaCapturePermissionPromptForTesting> shownPrompt;
    [webView _setMediaCapturePermissionPromptHandlerForTesting:^(_WKMediaCapturePermissionPromptForTesting *prompt) {
        shownPrompt = prompt;
    } runsPreview:YES];

    [delegate setRequestHandler:^(WKWebView *requestingWebView, PermissionDecisionHandler decisionHandler) {
        [requestingWebView _showMediaCapturePermissionPromptWithConfiguration:nil completionHandler:^(_WKMediaCapturePermissionPromptResult *) {
            decisionHandler(WKPermissionDecisionDeny);
            *finished = true;
        }];
    }];

    startCapture(webView, constraints);
    auto* shownPromptPointer = &shownPrompt;
    EXPECT_TRUE(Util::waitFor([shownPromptPointer] {
        return !!*shownPromptPointer;
    }));
    return shownPrompt;
}

TEST(MediaCapturePermissionPrompt, PreviewRunsWhilePromptIsUp)
{
    auto [webView, delegate] = createWebView();

    auto finished = std::make_shared<bool>(false);
    RetainPtr prompt = showPreviewPrompt(webView.get(), delegate.get(), @"{ audio: true, video: true }", finished);
    ASSERT_NOT_NULL(prompt.get());

    EXPECT_TRUE(Util::waitFor([&] {

        return [prompt hasPreviewLayer];

    }));
    EXPECT_TRUE(Util::waitFor([&] {
        return [prompt audioLevelUpdateCount] > 0;
    }));
    EXPECT_EQ(1U, [prompt previewLayerCount]);

    [prompt respondWithButtonIndex:[prompt buttonTitles].count - 1 cameraID:nil microphoneID:nil];
    Util::run(finished.get());
    EXPECT_FALSE([prompt hasPreviewLayer]);
}

TEST(MediaCapturePermissionPrompt, ChoosingAnotherCameraRestartsThePreview)
{
    auto [webView, delegate] = createWebView();

    auto finished = std::make_shared<bool>(false);
    RetainPtr prompt = showPreviewPrompt(webView.get(), delegate.get(), @"{ video: true }", finished);
    ASSERT_NOT_NULL(prompt.get());
    ASSERT_GT([prompt cameras].count, 1U);
    EXPECT_TRUE(Util::waitFor([&] {
        return [prompt hasPreviewLayer];
    }));

    [prompt selectCameraAtIndex:1];
    EXPECT_WK_STREQ([prompt cameras][1][@"id"], [prompt selectedCameraID]);
    EXPECT_TRUE(Util::waitFor([&] {
        return [prompt previewLayerCount] == 2 && [prompt hasPreviewLayer];
    }));

    [prompt dismissWithoutAnswer];
    Util::run(finished.get());
}

TEST(MediaCapturePermissionPrompt, PreviewPausesCaptureInOtherPages)
{
    auto [capturingWebView, capturingDelegate] = createWebView();
    [capturingDelegate setRequestHandler:^(WKWebView *, PermissionDecisionHandler decisionHandler) {
        decisionHandler(WKPermissionDecisionGrant);
    }];
    EXPECT_WK_STREQ("capturing", [capturingWebView objectByCallingAsyncFunction:@"return await startKeptCapture({ audio: true })" withArguments:nil]);
    EXPECT_TRUE(Util::waitFor([&] {
        return [capturingWebView microphoneCaptureState] == WKMediaCaptureStateActive;
    }));

    auto [promptingWebView, promptingDelegate] = createWebView();
    auto finished = std::make_shared<bool>(false);
    RetainPtr prompt = showPreviewPrompt(promptingWebView.get(), promptingDelegate.get(), @"{ audio: true }", finished);
    ASSERT_NOT_NULL(prompt.get());

    EXPECT_TRUE(Util::waitFor([&] {

        return [prompt audioLevelUpdateCount] > 0;

    }));
    EXPECT_TRUE(Util::waitFor([&] {
        return [capturingWebView microphoneCaptureState] == WKMediaCaptureStateMuted;
    }));

    [prompt respondWithButtonIndex:[prompt buttonTitles].count - 1 cameraID:nil microphoneID:nil];
    Util::run(finished.get());
    EXPECT_TRUE(Util::waitFor([&] {
        return [capturingWebView microphoneCaptureState] == WKMediaCaptureStateActive;
    }));
}

TEST(MediaCapturePermissionPrompt, PreviewComesBackAfterGPUProcessExit)
{
    auto [webView, delegate] = createWebView();

    auto finished = std::make_shared<bool>(false);
    RetainPtr prompt = showPreviewPrompt(webView.get(), delegate.get(), @"{ video: true }", finished);
    ASSERT_NOT_NULL(prompt.get());
    EXPECT_TRUE(Util::waitFor([&] {
        return [prompt hasPreviewLayer];
    }));
    EXPECT_EQ(1U, [prompt previewLayerCount]);

    pid_t gpuProcessIdentifier = [[webView configuration].processPool _gpuProcessIdentifier];
    ASSERT_NE(gpuProcessIdentifier, 0);
    kill(gpuProcessIdentifier, SIGKILL);

    EXPECT_TRUE(Util::waitFor([&] {

        return [prompt previewLayerCount] == 2 && [prompt hasPreviewLayer];

    }));

    [prompt dismissWithoutAnswer];
    Util::run(finished.get());
}

TEST(MediaCapturePermissionPrompt, PromptFollowsAnUnpluggedCamera)
{
    auto [webView, delegate] = createWebView();

    auto finished = std::make_shared<bool>(false);
    RetainPtr prompt = showPreviewPrompt(webView.get(), delegate.get(), @"{ video: true }", finished);
    ASSERT_NOT_NULL(prompt.get());
    ASSERT_GT([prompt cameras].count, 1U);
    EXPECT_TRUE(Util::waitFor([&] {
        return [prompt hasPreviewLayer];
    }));

    NSUInteger cameraCount = [prompt cameras].count;
    RetainPtr<NSString> unpluggedCameraID = [prompt selectedCameraID];
    WKRemoveMockMediaDevice((__bridge WKContextRef)[webView configuration].processPool, adoptWK(WKStringCreateWithUTF8CString([unpluggedCameraID UTF8String])).get());

    EXPECT_TRUE(Util::waitFor([&] {

        return [prompt cameras].count == cameraCount - 1;

    }));
    EXPECT_FALSE([[prompt selectedCameraID] isEqualToString:unpluggedCameraID.get()]);
    EXPECT_TRUE(Util::waitFor([&] {
        return [prompt previewLayerCount] == 2 && [prompt hasPreviewLayer];
    }));

    [prompt dismissWithoutAnswer];
    Util::run(finished.get());
    WKResetMockMediaDevices((__bridge WKContextRef)[webView configuration].processPool);
}

TEST(MediaCapturePermissionPrompt, GrantWhilePreviewIsUpCapturesAfterThePreviewStops)
{
    auto [webView, delegate] = createWebView();

    __block RetainPtr<_WKMediaCapturePermissionPromptForTesting> prompt;
    [webView _setMediaCapturePermissionPromptHandlerForTesting:^(_WKMediaCapturePermissionPromptForTesting *shownPrompt) {
        prompt = shownPrompt;
    } runsPreview:YES];

    __block BlockPtr<void(WKPermissionDecision)> pendingDecision;
    __block RetainPtr<_WKMediaCapturePermissionPromptResult> promptResult;
    [delegate setRequestHandler:^(WKWebView *requestingWebView, PermissionDecisionHandler decisionHandler) {
        pendingDecision = makeBlockPtr(decisionHandler);
        [requestingWebView _showMediaCapturePermissionPromptWithConfiguration:nil completionHandler:^(_WKMediaCapturePermissionPromptResult *result) {
            promptResult = result;
        }];
    }];

    __block bool captureFinished = false;
    __block bool promptHadClosedWhenCaptureFinished = false;
    __block RetainPtr<NSString> captured;
    [webView callAsyncJavaScript:@"return await capture(constraints)" arguments:@{ @"constraints": @{ @"video": @YES } } inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id value, NSError *) {
        captured = value;
        promptHadClosedWhenCaptureFinished = !!promptResult;
        captureFinished = true;
    }];

    auto* promptPointer = &prompt;
    EXPECT_TRUE(Util::waitFor([promptPointer] {
        return *promptPointer && [promptPointer->get() hasPreviewLayer];
    }));

    pendingDecision(WKPermissionDecisionGrant);
    Util::run(&captureFinished);

    EXPECT_TRUE([captured hasPrefix:@"video:"]);
    EXPECT_TRUE(promptHadClosedWhenCaptureFinished);
    EXPECT_EQ(_WKMediaCapturePermissionPromptOutcomeDismissed, [promptResult outcome]);
    EXPECT_FALSE([prompt hasPreviewLayer]);
}

TEST(MediaCapturePermissionPrompt, SpeechRecognitionPromptOffersTheNewDefaultMicrophone)
{
    auto [webView, delegate] = createWebView();

    __block RetainPtr<_WKMediaCapturePermissionPromptForTesting> prompt;
    [webView _setMediaCapturePermissionPromptHandlerForTesting:^(_WKMediaCapturePermissionPromptForTesting *shownPrompt) {
        prompt = shownPrompt;
    } runsPreview:YES];

    __block bool finished = false;
    [delegate setRequestHandler:^(WKWebView *requestingWebView, PermissionDecisionHandler decisionHandler) {
        [requestingWebView _showMediaCapturePermissionPromptWithConfiguration:nil completionHandler:^(_WKMediaCapturePermissionPromptResult *) {
            decisionHandler(WKPermissionDecisionDeny);
            finished = true;
        }];
    }];

    [webView evaluateJavaScript:@"window.recognition = new webkitSpeechRecognition(); recognition.start();" completionHandler:nil];
    auto* promptPointer = &prompt;
    EXPECT_TRUE(Util::waitFor([promptPointer] {
        return *promptPointer && [promptPointer->get() microphones].count;
    }));
    RetainPtr shownPrompt = prompt;
    EXPECT_EQ(1U, [shownPrompt microphones].count);

    auto persistentID = adoptWK(WKStringCreateWithUTF8CString("SPEECHTESTMICROPHONE"));
    auto label = adoptWK(WKStringCreateWithUTF8CString("Speech Test Microphone"));
    auto type = adoptWK(WKStringCreateWithUTF8CString("microphone"));
    WKAddMockMediaDevice((__bridge WKContextRef)[webView configuration].processPool, persistentID.get(), label.get(), type.get(), nullptr, true);

    EXPECT_TRUE(Util::waitFor([&] {
        return [[shownPrompt selectedMicrophoneID] isEqualToString:@"SPEECHTESTMICROPHONE"];
    }));
    EXPECT_EQ(1U, [shownPrompt microphones].count);

    [shownPrompt dismissWithoutAnswer];
    Util::run(&finished);
    WKResetMockMediaDevices((__bridge WKContextRef)[webView configuration].processPool);
}

static void startCameraAndMicrophoneCapture(TestWKWebView *webView, MediaCapturePermissionPromptUIDelegate *delegate)
{
    [delegate setRequestHandler:^(WKWebView *, PermissionDecisionHandler decisionHandler) {
        decisionHandler(WKPermissionDecisionGrant);
    }];
    EXPECT_WK_STREQ("capturing", [webView objectByCallingAsyncFunction:@"return await startKeptCapture({ audio: true, video: true })" withArguments:nil]);
    EXPECT_TRUE(Util::waitFor([&] {
        return [webView cameraCaptureState] == WKMediaCaptureStateActive && [webView microphoneCaptureState] == WKMediaCaptureStateActive;
    }));
}

TEST(MediaCapturePermissionPrompt, SecondPagePromptHasNoPreviewWhileAnotherPageHasOne)
{
    auto [capturingWebView, capturingDelegate] = createWebView();
    startCameraAndMicrophoneCapture(capturingWebView.get(), capturingDelegate.get());

    auto [firstWebView, firstDelegate] = createWebView();
    auto firstFinished = std::make_shared<bool>(false);
    RetainPtr firstPrompt = showPreviewPrompt(firstWebView.get(), firstDelegate.get(), @"{ video: true }", firstFinished);
    ASSERT_NOT_NULL(firstPrompt.get());
    EXPECT_TRUE(Util::waitFor([&] {
        return [firstPrompt hasPreviewLayer] && [capturingWebView cameraCaptureState] == WKMediaCaptureStateMuted;
    }));

    auto [secondWebView, secondDelegate] = createWebView();
    auto secondFinished = std::make_shared<bool>(false);
    RetainPtr secondPrompt = showPreviewPrompt(secondWebView.get(), secondDelegate.get(), @"{ video: true }", secondFinished);
    ASSERT_NOT_NULL(secondPrompt.get());

    // The GPU process runs one preview at a time, so a second prompt won't get a preview layer.
    Util::runFor(1_s);
    EXPECT_EQ(0U, [secondPrompt previewLayerCount]);
    EXPECT_FALSE([secondPrompt hasPreviewLayer]);
    EXPECT_TRUE([firstPrompt hasPreviewLayer]);
    EXPECT_EQ(1U, [firstPrompt previewLayerCount]);

    [firstPrompt respondWithButtonIndex:[firstPrompt buttonTitles].count - 1 cameraID:nil microphoneID:nil];
    Util::run(firstFinished.get());
    EXPECT_FALSE([firstPrompt hasPreviewLayer]);

    EXPECT_TRUE(Util::waitFor([&] {
        return [capturingWebView cameraCaptureState] == WKMediaCaptureStateActive;
    }));

    [secondPrompt respondWithButtonIndex:[secondPrompt buttonTitles].count - 1 cameraID:nil microphoneID:nil];
    Util::run(secondFinished.get());
}

static RetainPtr<_WKMediaCapturePermissionPromptForTesting> pauseCaptureWithPreview(TestWKWebView *capturingWebView, TestWKWebView *promptingWebView, MediaCapturePermissionPromptUIDelegate *promptingDelegate, std::shared_ptr<bool> finished)
{
    RetainPtr prompt = showPreviewPrompt(promptingWebView, promptingDelegate, @"{ audio: true, video: true }", finished);
    EXPECT_TRUE(Util::waitFor([&] {
        return [capturingWebView cameraCaptureState] == WKMediaCaptureStateMuted && [capturingWebView microphoneCaptureState] == WKMediaCaptureStateMuted;
    }));
    return prompt;
}

TEST(MediaCapturePermissionPrompt, UnmutingTheCameraDuringAPreviewStillResumesTheMicrophone)
{
    auto [capturingWebView, capturingDelegate] = createWebView();
    startCameraAndMicrophoneCapture(capturingWebView.get(), capturingDelegate.get());

    auto [promptingWebView, promptingDelegate] = createWebView();
    auto finished = std::make_shared<bool>(false);
    RetainPtr prompt = pauseCaptureWithPreview(capturingWebView.get(), promptingWebView.get(), promptingDelegate.get(), finished);
    ASSERT_NOT_NULL(prompt.get());

    __block bool unmuted = false;
    [capturingWebView setCameraCaptureState:WKMediaCaptureStateActive completionHandler:^{
        unmuted = true;
    }];
    Util::run(&unmuted);
    EXPECT_TRUE(Util::waitFor([&] {
        return [capturingWebView cameraCaptureState] == WKMediaCaptureStateActive;
    }));
    EXPECT_EQ(WKMediaCaptureStateMuted, [capturingWebView microphoneCaptureState]);

    [prompt respondWithButtonIndex:[prompt buttonTitles].count - 1 cameraID:nil microphoneID:nil];
    Util::run(finished.get());
    EXPECT_TRUE(Util::waitFor([&] {
        return [capturingWebView microphoneCaptureState] == WKMediaCaptureStateActive;
    }));
    EXPECT_EQ(WKMediaCaptureStateActive, [capturingWebView cameraCaptureState]);
}

TEST(MediaCapturePermissionPrompt, MutingTheCameraDuringAPreviewKeepsItMutedAfterwards)
{
    auto [capturingWebView, capturingDelegate] = createWebView();
    startCameraAndMicrophoneCapture(capturingWebView.get(), capturingDelegate.get());

    auto [promptingWebView, promptingDelegate] = createWebView();
    auto finished = std::make_shared<bool>(false);
    RetainPtr prompt = pauseCaptureWithPreview(capturingWebView.get(), promptingWebView.get(), promptingDelegate.get(), finished);
    ASSERT_NOT_NULL(prompt.get());

    __block bool muted = false;
    [capturingWebView setCameraCaptureState:WKMediaCaptureStateMuted completionHandler:^{
        muted = true;
    }];
    Util::run(&muted);

    [prompt respondWithButtonIndex:[prompt buttonTitles].count - 1 cameraID:nil microphoneID:nil];
    Util::run(finished.get());
    EXPECT_TRUE(Util::waitFor([&] {
        return [capturingWebView microphoneCaptureState] == WKMediaCaptureStateActive;
    }));
    EXPECT_EQ(WKMediaCaptureStateMuted, [capturingWebView cameraCaptureState]);
}

} // namespace TestWebKitAPI

#endif // ENABLE(MEDIA_STREAM)
