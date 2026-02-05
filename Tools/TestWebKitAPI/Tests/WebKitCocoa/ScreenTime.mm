/*
 * Copyright (C) 2024-2026 Apple Inc. All rights reserved.
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

#if ENABLE(SCREEN_TIME)

#import "AppKitSPI.h"
#import "HTTPServer.h"
#import "InstanceMethodSwizzler.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestCocoa.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import "Utilities.h"
#import <ScreenTime/STScreenTimeConfiguration.h>
#import <ScreenTime/STWebHistory.h>
#import <ScreenTime/STWebpageController.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebViewConfiguration.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WKWebsiteDataRecordPrivate.h>
#import <WebKit/WKWebsiteDataStore.h>
#import <WebKit/_WKFeature.h>
#import <pal/cocoa/ScreenTimeSoftLink.h>
#import <wtf/RetainPtr.h>

static void *blockedStateObserverChangeKVOContext = &blockedStateObserverChangeKVOContext;
static bool stateDidChange = false;
static bool receivedLoadMessage = false;
static bool hasVideoInPictureInPictureValue = false;
static bool hasVideoInPictureInPictureCalled = false;

static RetainPtr<TestWKWebView> webViewForScreenTimeTests(WKWebViewConfiguration *configuration = nil, BOOL addToWindow = YES)
{
    if (!configuration)
        configuration = adoptNS([[WKWebViewConfiguration alloc] init]).autorelease();

    auto preferences = [configuration preferences];
    for (_WKFeature *feature in [WKPreferences _features]) {
        if ([feature.key isEqualToString:@"ScreenTimeEnabled"])
            [preferences _setEnabled:YES forFeature:feature];
    }
    return adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 300) configuration:configuration addToWindow:addToWindow]);
}

static std::unique_ptr<InstanceMethodSwizzler> swizzleEnforcesChildRestrictions(bool& done, bool enforces = YES)
{
    return WTF::makeUnique<InstanceMethodSwizzler>(PAL::getSTScreenTimeConfigurationClassSingleton(), @selector(enforcesChildRestrictions), imp_implementationWithBlock(^BOOL {
        done = true;
        return enforces;
    }));
}

static void testSuppressUsageRecordingWithDataStore(RetainPtr<WKWebsiteDataStore>&& websiteDataStore, bool suppressUsageRecordingExpectation)
{
    __block bool done = false;
    __block bool suppressUsageRecording = false;
    __block bool childRestrictionsDone = false;

    InstanceMethodSwizzler swizzler {
        PAL::getSTWebpageControllerClassSingleton(),
        @selector(setSuppressUsageRecording:),
        imp_implementationWithBlock(^(id object, bool value) {
            suppressUsageRecording = value;
            done = true;
        })
    };

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:websiteDataStore.get()];

    RetainPtr webView = webViewForScreenTimeTests(configuration.get());

    RetainPtr request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"http://webkit.org"]];
    auto swizzle = swizzleEnforcesChildRestrictions(childRestrictionsDone);
    [webView synchronouslyLoadSimulatedRequest:request.get() responseHTMLString:@""];
    TestWebKitAPI::Util::run(&childRestrictionsDone);

    TestWebKitAPI::Util::run(&done);

    EXPECT_EQ(suppressUsageRecordingExpectation, suppressUsageRecording);
}

@interface STWebpageController ()
@property (setter=setURLIsBlocked:) BOOL URLIsBlocked;
@end

@interface STWebHistory ()
@property (readonly, copy) STWebHistoryProfileIdentifier profileIdentifier;
@end

@interface WKWebView (Internal)
- (STWebpageController *)_screenTimeWebpageController;
#if PLATFORM(MAC)
- (NSVisualEffectView *) _screenTimeBlurredSnapshot;
#else
- (UIVisualEffectView *) _screenTimeBlurredSnapshot;
#endif
@end

@interface BlockedStateObserver : NSObject
- (instancetype)initWithWebView:(TestWKWebView *)webView;
@end

@implementation BlockedStateObserver {
    RetainPtr<TestWKWebView> _webView;
}

- (instancetype)initWithWebView:(TestWKWebView *)webView
{
    if (!(self = [super init]))
        return nil;

    _webView = webView;
    [_webView addObserver:self forKeyPath:@"isBlockedByScreenTime" options:(NSKeyValueObservingOptionOld | NSKeyValueObservingOptionNew) context:&blockedStateObserverChangeKVOContext];
    return self;
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    if (context == &blockedStateObserverChangeKVOContext) {
        stateDidChange = true;
        return;
    }

    [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
}
@end

static BOOL blurredViewIsPresent(TestWKWebView *webView)
{
#if PLATFORM(IOS_FAMILY)
    for (UIView *subview in [webView subviews]) {
        if ([subview isKindOfClass:[UIVisualEffectView class]])
            return true;
    }
#else
    for (NSView *subview in [webView subviews]) {
        if ([subview isKindOfClass:[NSVisualEffectView class]])
            return true;
    }
#endif
    return false;
}

static BOOL systemScreenTimeBlockingViewIsPresent(TestWKWebView *webView)
{
    RetainPtr controller = [webView _screenTimeWebpageController];
#if PLATFORM(IOS_FAMILY)
    for (UIView *subview in [webView subviews]) {
        if (subview == [controller view])
            return true;
    }
#else
    for (NSView *subview in [webView subviews]) {
        if (subview == [controller view])
            return true;
    }
#endif
    return false;
}

static RetainPtr<TestWKWebView> testShowsSystemScreenTimeBlockingView(bool showsSystemScreenTimeBlockingView)
{
    __block bool childRestrictionsDone = false;

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setShowsSystemScreenTimeBlockingView:showsSystemScreenTimeBlockingView];

    RetainPtr webView = webViewForScreenTimeTests(configuration.get());

    RetainPtr request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"http://webkit.org"]];
    auto swizzle = swizzleEnforcesChildRestrictions(childRestrictionsDone);
    [webView synchronouslyLoadSimulatedRequest:request.get() responseHTMLString:@""];
    TestWebKitAPI::Util::run(&childRestrictionsDone);

    [webView waitForNextPresentationUpdate];

    RetainPtr controller = [webView _screenTimeWebpageController];
    [controller setURLIsBlocked:YES];

    EXPECT_EQ(showsSystemScreenTimeBlockingView, [configuration showsSystemScreenTimeBlockingView]);

    // Check if ScreenTime's blocking view is hidden or not.
    EXPECT_EQ(showsSystemScreenTimeBlockingView, systemScreenTimeBlockingViewIsPresent(webView.get()));

    // Check if WebKit's blurred blocking view is added and in the view hierarchy or not.
    EXPECT_EQ(!showsSystemScreenTimeBlockingView, blurredViewIsPresent(webView.get()));

    return webView;
}

#if PLATFORM(MAC)
static void testWebContentIsNotClickableShowingSystemScreenTimeBlockingView(bool showsSystemScreenTimeBlockingView)
{
    __block bool childRestrictionsDone = false;

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setShowsSystemScreenTimeBlockingView:showsSystemScreenTimeBlockingView];

    RetainPtr webView = webViewForScreenTimeTests(configuration.get());
    RetainPtr observer = adoptNS([[BlockedStateObserver alloc] initWithWebView:webView.get()]);

    auto swizzle = swizzleEnforcesChildRestrictions(childRestrictionsDone);
    [webView synchronouslyLoadHTMLString:
    @"<!DOCTYPE html>"
    "<html>"
    "<head>"
    "<style>"
    "body, html { margin: 0; width: 100%; height: 100%; }"
    "</style>"
    "</head>"
    "<body>"
    "<script>"
    "let mouseDownCounter = 0;"
    "addEventListener('mousedown', function() { mouseDownCounter += 1; });"
    "</script>"
    "</body>"
    "</html>"
    ")" baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    TestWebKitAPI::Util::run(&childRestrictionsDone);

    [webView waitForNextPresentationUpdate];

    RetainPtr screenTimeController = [webView _screenTimeWebpageController];
    [screenTimeController setURLIsBlocked:YES];
    TestWebKitAPI::Util::run(&stateDidChange);

    [webView sendClickAtPoint:NSMakePoint(300, 300)];
    [webView waitForPendingMouseEvents];

    stateDidChange = false;
    [screenTimeController setURLIsBlocked:NO];
    TestWebKitAPI::Util::run(&stateDidChange);

    [webView sendClickAtPoint:NSMakePoint(300, 300)];
    [webView waitForPendingMouseEvents];

    int mouseDownCounter = [[webView objectByEvaluatingJavaScript:@"mouseDownCounter"] intValue];
    EXPECT_EQ(mouseDownCounter, 1);
}
#endif

TEST(ScreenTime, IsBlockedByScreenTimeTrue)
{
    __block bool childRestrictionsDone = false;

    RetainPtr webView = webViewForScreenTimeTests();

    auto swizzle = swizzleEnforcesChildRestrictions(childRestrictionsDone);
    RetainPtr request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"http://webkit.org"]];
    [webView synchronouslyLoadSimulatedRequest:request.get() responseHTMLString:@""];
    TestWebKitAPI::Util::run(&childRestrictionsDone);

    [webView waitForNextPresentationUpdate];

    RetainPtr controller = [webView _screenTimeWebpageController];
    [controller setURLIsBlocked:YES];

    EXPECT_TRUE([webView isBlockedByScreenTime]);
}

TEST(ScreenTime, IsBlockedByScreenTimeFalse)
{
    __block bool childRestrictionsDone = false;

    RetainPtr webView = webViewForScreenTimeTests();

    RetainPtr request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"http://webkit.org"]];
    auto swizzle = swizzleEnforcesChildRestrictions(childRestrictionsDone);
    [webView synchronouslyLoadSimulatedRequest:request.get() responseHTMLString:@""];
    TestWebKitAPI::Util::run(&childRestrictionsDone);

    [webView waitForNextPresentationUpdate];

    RetainPtr controller = [webView _screenTimeWebpageController];
    [controller setURLIsBlocked:NO];

    EXPECT_FALSE([webView isBlockedByScreenTime]);
}

TEST(ScreenTime, IsBlockedByScreenTimeMultiple)
{
    __block bool childRestrictionsDone = false;

    RetainPtr webView = webViewForScreenTimeTests();

    RetainPtr request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"http://webkit.org"]];
    auto swizzle = swizzleEnforcesChildRestrictions(childRestrictionsDone);
    [webView synchronouslyLoadSimulatedRequest:request.get() responseHTMLString:@""];
    TestWebKitAPI::Util::run(&childRestrictionsDone);

    RetainPtr controller = [webView _screenTimeWebpageController];
    [controller setURLIsBlocked:YES];
    [controller setURLIsBlocked:NO];

    [webView waitForNextPresentationUpdate];

    EXPECT_FALSE([webView isBlockedByScreenTime]);
}

TEST(ScreenTime, IsBlockedByScreenTimeKVO)
{
    __block bool childRestrictionsDone = false;

    RetainPtr webView = webViewForScreenTimeTests();
    auto observer = adoptNS([[BlockedStateObserver alloc] initWithWebView:webView.get()]);

    RetainPtr request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"http://webkit.org"]];
    auto swizzle = swizzleEnforcesChildRestrictions(childRestrictionsDone);
    [webView synchronouslyLoadSimulatedRequest:request.get() responseHTMLString:@""];
    TestWebKitAPI::Util::run(&childRestrictionsDone);

    [webView waitForNextPresentationUpdate];

    RetainPtr controller = [webView _screenTimeWebpageController];
    [controller setURLIsBlocked:YES];

    TestWebKitAPI::Util::run(&stateDidChange);

    EXPECT_TRUE([webView isBlockedByScreenTime]);

    stateDidChange = false;

    [controller setURLIsBlocked:NO];

    TestWebKitAPI::Util::run(&stateDidChange);

    EXPECT_FALSE([webView isBlockedByScreenTime]);

    stateDidChange = false;

    [controller setURLIsBlocked:YES];

    TestWebKitAPI::Util::run(&stateDidChange);

    EXPECT_TRUE([webView isBlockedByScreenTime]);
}

TEST(ScreenTime, IdentifierNil)
{
    __block bool done = false;
    __block NSString *identifier = @"testing123";
    __block bool childRestrictionsDone = false;

    InstanceMethodSwizzler swizzler {
        PAL::getSTWebpageControllerClassSingleton(),
        @selector(setProfileIdentifier:),
        imp_implementationWithBlock(^(id object, NSString *profileIdentifier) {
            identifier = profileIdentifier;
            done = true;
        })
    };

    RetainPtr webView = webViewForScreenTimeTests();

    RetainPtr request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"http://webkit.org"]];
    auto swizzle = swizzleEnforcesChildRestrictions(childRestrictionsDone);
    [webView synchronouslyLoadSimulatedRequest:request.get() responseHTMLString:@""];
    TestWebKitAPI::Util::run(&childRestrictionsDone);

    TestWebKitAPI::Util::run(&done);

    EXPECT_NULL(identifier);
}

TEST(ScreenTime, IdentifierString)
{
    __block bool done = false;
    __block RetainPtr identifier = @"";
    __block bool childRestrictionsDone = false;

    InstanceMethodSwizzler swizzler {
        PAL::getSTWebpageControllerClassSingleton(),
        @selector(setProfileIdentifier:),
        imp_implementationWithBlock(^(id object, NSString *profileIdentifier) {
            identifier = profileIdentifier;
            done = true;
        })
    };

    RetainPtr uuid = [NSUUID UUID];
    RetainPtr websiteDataStore = [WKWebsiteDataStore dataStoreForIdentifier:uuid.get()];

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:websiteDataStore.get()];

    RetainPtr webView = webViewForScreenTimeTests(configuration.get());

    RetainPtr request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"http://webkit.org"]];
    auto swizzle = swizzleEnforcesChildRestrictions(childRestrictionsDone);
    [webView synchronouslyLoadSimulatedRequest:request.get() responseHTMLString:@""];
    TestWebKitAPI::Util::run(&childRestrictionsDone);

    TestWebKitAPI::Util::run(&done);

    EXPECT_WK_STREQ(identifier.get(), [uuid UUIDString]);
}

TEST(ScreenTime, IdentifierStringWithRemoveData)
{
    __block bool done = false;
    __block RetainPtr identifier = @"";
    __block bool childRestrictionsDone = false;

    RetainPtr dataTypeScreenTime = adoptNS([[NSSet alloc] initWithArray:@[ WKWebsiteDataTypeScreenTime ]]);

    InstanceMethodSwizzler swizzler {
        PAL::getSTWebHistoryClassSingleton(),
        @selector(deleteHistoryDuringInterval:),
        imp_implementationWithBlock(^(STWebHistory *object, NSDateInterval *) {
            identifier = object.profileIdentifier;
        })
    };

    RetainPtr uuid = [NSUUID UUID];
    RetainPtr websiteDataStore = [WKWebsiteDataStore dataStoreForIdentifier:uuid.get()];
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:websiteDataStore.get()];
    RetainPtr webView = webViewForScreenTimeTests(configuration.get());

    RetainPtr request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"http://icloud.com"]];
    auto swizzle = swizzleEnforcesChildRestrictions(childRestrictionsDone);
    [webView synchronouslyLoadSimulatedRequest:request.get() responseHTMLString:@""];
    TestWebKitAPI::Util::run(&childRestrictionsDone);

    [websiteDataStore removeDataOfTypes:dataTypeScreenTime.get() modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);

    EXPECT_WK_STREQ(identifier.get(), [uuid UUIDString]);
}

TEST(ScreenTime, PersistentSession)
{
    testSuppressUsageRecordingWithDataStore([WKWebsiteDataStore defaultDataStore], false);
}

TEST(ScreenTime, NonPersistentSession)
{
    testSuppressUsageRecordingWithDataStore([WKWebsiteDataStore nonPersistentDataStore], true);
}

TEST(ScreenTime, ShowSystemScreenTimeBlockingTrue)
{
    testShowsSystemScreenTimeBlockingView(true);
}

TEST(ScreenTime, ShowSystemScreenTimeBlockingFalse)
{
    testShowsSystemScreenTimeBlockingView(false);
}

TEST(ScreenTime, ShowSystemScreenTimeBlockingFalseAndRemoved)
{
    RetainPtr webView = testShowsSystemScreenTimeBlockingView(false);
    RetainPtr controller = [webView _screenTimeWebpageController];
    [controller setURLIsBlocked:NO];
    EXPECT_FALSE([[webView configuration] showsSystemScreenTimeBlockingView]);
    // Check if blurred blocking view is removed when URLIsBlocked is false.
    EXPECT_FALSE(blurredViewIsPresent(webView.get()));
}

TEST(ScreenTime, WKWebViewFillsStackView)
{
    CGRect windowRect = CGRectMake(0, 0, 800, 600);

    RetainPtr webView = webViewForScreenTimeTests(nil);
    [webView setTranslatesAutoresizingMaskIntoConstraints:NO];

    RetainPtr request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"http://webkit.org"]];
    [webView synchronouslyLoadSimulatedRequest:request.get() responseHTMLString:@""];

#if PLATFORM(MAC)
    RetainPtr stackView = adoptNS([[NSStackView alloc] init]);
    RetainPtr window = adoptNS([[NSWindow alloc] initWithContentRect:NSRectFromCGRect(windowRect) styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:NO]);
    RetainPtr contentView = [window contentView];
#else
    RetainPtr stackView = adoptNS([[UIStackView alloc] init]);
    RetainPtr hostWindow = adoptNS([[UIWindow alloc] initWithFrame:windowRect]);
    RetainPtr contentView = hostWindow;
#endif

    [contentView addSubview:stackView.get()];
    [stackView addArrangedSubview:webView.get()];

    [stackView setTranslatesAutoresizingMaskIntoConstraints:NO];
    [NSLayoutConstraint activateConstraints:@[
        [[stackView topAnchor] constraintEqualToAnchor:[contentView topAnchor]],
        [[stackView leadingAnchor] constraintEqualToAnchor:[contentView leadingAnchor]],
        [[stackView bottomAnchor] constraintEqualToAnchor:[contentView bottomAnchor]],
        [[stackView trailingAnchor] constraintEqualToAnchor:[contentView trailingAnchor]]
    ]];

#if PLATFORM(MAC)
    [window makeKeyAndOrderFront:nil];
#else
    [hostWindow setHidden:NO];
    [hostWindow setNeedsLayout];
    [hostWindow layoutIfNeeded];
#endif

    EXPECT_TRUE(CGRectEqualToRect([webView frame], windowRect));
}

TEST(ScreenTime, URLIsPlayingVideo)
{
    __block bool childRestrictionsDone = false;

    RetainPtr webView = webViewForScreenTimeTests();

    RetainPtr contentHTML = @"<!DOCTYPE html><html><head></head><body><video src=\"video-with-audio.mp4\" webkit-playsinline></video></body></html>";
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { contentHTML.get() } },
        { "/favicon.ico"_s, { "Actual response is immaterial."_s } },
        { "/video-with-audio.mp4"_s, [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"video-with-audio" withExtension:@"mp4"]] },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto swizzle = swizzleEnforcesChildRestrictions(childRestrictionsDone);
    [webView synchronouslyLoadRequest:server.requestWithLocalhost()];
    TestWebKitAPI::Util::run(&childRestrictionsDone);

    [webView objectByEvaluatingJavaScript:@"function eventToMessage(event){window.webkit.messageHandlers.testHandler.postMessage(event.type);} var video = document.querySelector('video'); video.addEventListener('playing', eventToMessage); video.addEventListener('pause', eventToMessage);"];

    __block bool didBeginPlaying = false;
    [webView performAfterReceivingMessage:@"playing" action:^{ didBeginPlaying = true; }];
    [webView evaluateJavaScript:@"document.querySelector('video').play()" completionHandler:nil];
    TestWebKitAPI::Util::run(&didBeginPlaying);

    EXPECT_TRUE([[webView _screenTimeWebpageController] URLIsPlayingVideo]);

    __block bool didPause = false;
    [webView performAfterReceivingMessage:@"pause" action:^{ didPause = true; }];
    [webView evaluateJavaScript:@"document.querySelector('video').pause()" completionHandler:nil];
    TestWebKitAPI::Util::run(&didPause);

    EXPECT_FALSE([[webView _screenTimeWebpageController] URLIsPlayingVideo]);
}

#if PLATFORM(MAC)

@interface STPictureInPictureUIDelegate : NSObject <WKUIDelegate, WKScriptMessageHandler>
@end

@implementation STPictureInPictureUIDelegate

- (void)_webView:(WKWebView *)webView hasVideoInPictureInPictureDidChange:(BOOL)hasVideoInPictureInPicture
{
    hasVideoInPictureInPictureValue = hasVideoInPictureInPicture;
    hasVideoInPictureInPictureCalled = true;
}

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    NSString *bodyString = (NSString *)[message body];
    if ([bodyString isEqualToString:@"load"])
        receivedLoadMessage = true;
}
@end

TEST(ScreenTime, URLIsPictureInPicture)
{
    __block bool childRestrictionsDone = false;

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration preferences]._allowsPictureInPictureMediaPlayback = YES;

    RetainPtr handler = adoptNS([[STPictureInPictureUIDelegate alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"pictureInPictureChangeHandler"];

    RetainPtr webView = webViewForScreenTimeTests(configuration.get());
    [webView setFrame:NSMakeRect(0, 0, 640, 480)];

    [webView setUIDelegate:handler.get()];

    [webView _forceRequestCandidates];

    RetainPtr window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:NO]);
    [[window contentView] addSubview:webView.get()];
    [window makeKeyAndOrderFront:nil];

    RetainPtr contentHTML = [NSString stringWithContentsOfFile:[NSBundle.test_resourcesBundle pathForResource:@"PictureInPictureDelegate" ofType:@"html"] encoding:NSUTF8StringEncoding error:NULL];
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { contentHTML.get() } },
        { "/favicon.ico"_s, { "Actual response is immaterial."_s } },
        { "/test.mp4"_s, [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"test" withExtension:@"mp4"]] },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    receivedLoadMessage = false;

    auto swizzle = swizzleEnforcesChildRestrictions(childRestrictionsDone);
    [webView loadRequest:server.requestWithLocalhost()];
    TestWebKitAPI::Util::run(&receivedLoadMessage);
    TestWebKitAPI::Util::run(&childRestrictionsDone);

    hasVideoInPictureInPictureValue = false;
    hasVideoInPictureInPictureCalled = false;

    while (![webView _canTogglePictureInPicture])
        [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];

    ASSERT_FALSE([webView _isPictureInPictureActive]);
    [webView _togglePictureInPicture];

    TestWebKitAPI::Util::run(&hasVideoInPictureInPictureCalled);
    EXPECT_TRUE(hasVideoInPictureInPictureValue);
    EXPECT_TRUE([[webView _screenTimeWebpageController] URLIsPictureInPicture]);

    // Wait for PIPAgent to launch, or it won't call -pipDidClose: callback.
    [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:1]];

    ASSERT_TRUE([webView _isPictureInPictureActive]);
    ASSERT_TRUE([webView _canTogglePictureInPicture]);

    hasVideoInPictureInPictureCalled = false;

    [webView _togglePictureInPicture];

    TestWebKitAPI::Util::run(&hasVideoInPictureInPictureCalled);
    EXPECT_FALSE(hasVideoInPictureInPictureValue);
    EXPECT_FALSE([[webView _screenTimeWebpageController] URLIsPictureInPicture]);
}

TEST(ScreenTime, WebContentIsNotClickableBehindSystemScreenTimeBlockingView)
{
    testWebContentIsNotClickableShowingSystemScreenTimeBlockingView(true);
}

TEST(ScreenTime, WebContentIsNotClickableBehindBlurredBlockingView)
{
    testWebContentIsNotClickableShowingSystemScreenTimeBlockingView(false);
}

#endif

TEST(ScreenTime, FetchData)
{
    __block bool childRestrictionsDone = false;

    __block RetainPtr<NSSet<NSURL *>> urls;
    InstanceMethodSwizzler swizzler {
        PAL::getSTWebHistoryClassSingleton(),
        @selector(fetchAllHistoryWithCompletionHandler:),
        imp_implementationWithBlock(^(id object, void (^completionHandler)(NSSet<NSURL *> *urls, NSError *error)) {
            urls = [NSSet setWithArray:@[ adoptNS([[NSURL alloc] initWithString:@"https://www.webkit.org/"]).get() ]];
            completionHandler(urls.get(), nil);
        })
    };

    RetainPtr dataTypeScreenTime = adoptNS([[NSSet alloc] initWithArray:@[ WKWebsiteDataTypeScreenTime ]]);

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    RetainPtr websiteDataStore = [WKWebsiteDataStore defaultDataStore];
    [configuration setWebsiteDataStore:websiteDataStore.get()];

    RetainPtr webView = webViewForScreenTimeTests(configuration.get());

    RetainPtr request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"http://webkit.org"]];
    auto swizzle = swizzleEnforcesChildRestrictions(childRestrictionsDone);
    [webView synchronouslyLoadSimulatedRequest:request.get() responseHTMLString:@""];
    TestWebKitAPI::Util::run(&childRestrictionsDone);

    __block bool done = false;
    [websiteDataStore fetchDataRecordsOfTypes:dataTypeScreenTime.get() completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        EXPECT_WK_STREQ([[dataRecords firstObject] displayName], "webkit.org");
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);
}

TEST(ScreenTime, RemoveDataWithTimeInterval)
{
    __block bool childRestrictionsDone = false;

    __block bool removedHistory = false;
    InstanceMethodSwizzler swizzler {
        PAL::getSTWebHistoryClassSingleton(),
        @selector(deleteHistoryDuringInterval:),
        imp_implementationWithBlock(^(id object, NSDateInterval *interval) {
            removedHistory = true;
        })
    };

    RetainPtr dataTypeScreenTime = adoptNS([[NSSet alloc] initWithArray:@[ WKWebsiteDataTypeScreenTime ]]);

    RetainPtr uuid = [NSUUID UUID];
    RetainPtr websiteDataStore = [WKWebsiteDataStore dataStoreForIdentifier:uuid.get()];

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:websiteDataStore.get()];

    RetainPtr webView = webViewForScreenTimeTests(configuration.get());

    RetainPtr request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"http://webkit.org"]];
    auto swizzle = swizzleEnforcesChildRestrictions(childRestrictionsDone);
    [webView synchronouslyLoadSimulatedRequest:request.get() responseHTMLString:@""];
    TestWebKitAPI::Util::run(&childRestrictionsDone);

    __block bool done = false;
    [websiteDataStore removeDataOfTypes:dataTypeScreenTime.get() modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);

    EXPECT_TRUE(removedHistory);
}

TEST(ScreenTime, RemoveData)
{
    __block bool childRestrictionsDone = false;

    __block RetainPtr<NSSet<NSURL *>> fetchedURLs = adoptNS([[NSSet alloc] initWithArray:@[
        adoptNS([[NSURL alloc] initWithString:@"https://www.github.com/WebKit/WebKit"]).get(),
        adoptNS([[NSURL alloc] initWithString:@"https://www.github.com/APPLE"]).get(),
        adoptNS([[NSURL alloc] initWithString:@"https://fonts.github.com/"]).get(),
        adoptNS([[NSURL alloc] initWithString:@"https://abcdefg.github.com/aPPLe/abc"]).get()
    ]]);

    InstanceMethodSwizzler fetchHistorySwizzler {
        PAL::getSTWebHistoryClassSingleton(),
        @selector(fetchAllHistoryWithCompletionHandler:),
        imp_implementationWithBlock(^(id object, void (^completionHandler)(NSSet<NSURL *> *urls, NSError *error)) {
            completionHandler(fetchedURLs.get(), nil);
        })
    };

    __block RetainPtr<NSMutableSet<NSURL *>> deletedURLs = adoptNS([[NSMutableSet alloc] init]);
    InstanceMethodSwizzler deleteHistorySwizzler {
        PAL::getSTWebHistoryClassSingleton(),
        @selector(deleteHistoryForURL:),
        imp_implementationWithBlock(^(id object, NSURL *url) {
            [deletedURLs addObject:url];
        })
    };

    RetainPtr dataTypeScreenTime = adoptNS([[NSSet alloc] initWithArray:@[ WKWebsiteDataTypeScreenTime ]]);

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    RetainPtr websiteDataStore = [WKWebsiteDataStore defaultDataStore];
    [configuration setWebsiteDataStore:websiteDataStore.get()];

    RetainPtr webView = webViewForScreenTimeTests(configuration.get());

    RetainPtr request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"https://www.github.com/WebKit/WebKit"]];
    auto swizzle = swizzleEnforcesChildRestrictions(childRestrictionsDone);
    [webView synchronouslyLoadSimulatedRequest:request.get() responseHTMLString:@""];
    TestWebKitAPI::Util::run(&childRestrictionsDone);

    __block bool done = false;
    [websiteDataStore fetchDataRecordsOfTypes:dataTypeScreenTime.get() completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        [websiteDataStore removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] forDataRecords:dataRecords completionHandler:^{
            done = true;
        }];
    }];

    TestWebKitAPI::Util::run(&done);

    EXPECT_EQ([deletedURLs count], [fetchedURLs count]);

    for (NSURL *url in fetchedURLs.get())
        EXPECT_TRUE([deletedURLs containsObject:url]);
}

TEST(ScreenTime, OffscreenSystemScreenTimeBlockingView)
{
    __block bool childRestrictionsDone = false;

    RetainPtr webView = webViewForScreenTimeTests();
    RetainPtr request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"http://webkit.org"]];

    auto swizzle = swizzleEnforcesChildRestrictions(childRestrictionsDone);
    [webView synchronouslyLoadSimulatedRequest:request.get() responseHTMLString:@""];
    TestWebKitAPI::Util::run(&childRestrictionsDone);

    [webView waitForNextPresentationUpdate];

    EXPECT_FALSE([[[webView _screenTimeWebpageController] view] isHidden]);

    [webView removeFromTestWindow];

    [webView waitUntilActivityStateUpdateDone];

    EXPECT_TRUE([[[webView _screenTimeWebpageController] view] isHidden]);

    [webView addToTestWindow];

    EXPECT_FALSE([[[webView _screenTimeWebpageController] view] isHidden]);
}

TEST(ScreenTime, OffscreenBlurredScreenTimeBlockingView)
{
    __block bool childRestrictionsDone = false;

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setShowsSystemScreenTimeBlockingView:NO];

    RetainPtr webView = webViewForScreenTimeTests(configuration.get());
    RetainPtr request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"http://webkit.org"]];
    auto swizzle = swizzleEnforcesChildRestrictions(childRestrictionsDone);
    [webView synchronouslyLoadSimulatedRequest:request.get() responseHTMLString:@""];
    TestWebKitAPI::Util::run(&childRestrictionsDone);

    [webView waitForNextPresentationUpdate];

    RetainPtr controller = [webView _screenTimeWebpageController];
    [controller setURLIsBlocked:YES];

    EXPECT_TRUE(blurredViewIsPresent(webView.get()));

    [webView removeFromTestWindow];

    [webView waitUntilActivityStateUpdateDone];
    [webView waitForNextPresentationUpdate];

    EXPECT_TRUE([[webView _screenTimeBlurredSnapshot] isHidden]);

    [webView addToTestWindow];

    [webView waitUntilActivityStateUpdateDone];
    [webView waitForNextPresentationUpdate];

    EXPECT_TRUE(blurredViewIsPresent(webView.get()));

    EXPECT_FALSE([[webView _screenTimeBlurredSnapshot] isHidden]);
}

#if PLATFORM(MAC)

TEST(ScreenTime, DoNotDonateURLsInOccludedWebView)
{
    // FIXME: This test will fail if running on a full screen terminal window. It passes otherwise.
    __block bool suppressUsageRecording = false;
    __block bool done = false;
    __block bool childRestrictionsDone = false;

    InstanceMethodSwizzler swizzler {
        PAL::getSTWebpageControllerClassSingleton(),
        @selector(setSuppressUsageRecording:),
        imp_implementationWithBlock(^(id object, bool value) {
            suppressUsageRecording = value;
            done = true;
        })
    };

    RetainPtr webView = webViewForScreenTimeTests();
    RetainPtr request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"http://webkit.org"]];
    auto swizzle = swizzleEnforcesChildRestrictions(childRestrictionsDone);
    [webView synchronouslyLoadSimulatedRequest:request.get() responseHTMLString:@""];
    TestWebKitAPI::Util::run(&childRestrictionsDone);

    [webView waitForNextPresentationUpdate];

    RetainPtr controller = [webView _screenTimeWebpageController];
    [controller setURLIsBlocked:YES];

    TestWebKitAPI::Util::run(&done);

    EXPECT_FALSE(suppressUsageRecording);

    suppressUsageRecording = false;
    done = false;

    RetainPtr window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:NO]);
    [window setIsVisible:YES];
    [window makeKeyAndOrderFront:nil];

    TestWebKitAPI::Util::run(&done);

    EXPECT_TRUE(suppressUsageRecording);

    suppressUsageRecording = false;
    done = false;

    [window setFrame:CGRectZero display:YES];

    TestWebKitAPI::Util::run(&done);

    EXPECT_FALSE(suppressUsageRecording);
}

TEST(ScreenTime, FullscreenWithSystemBlockingViewInHierarchy)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration preferences].elementFullscreenEnabled = YES;

    RetainPtr webView = webViewForScreenTimeTests(configuration.get());

    __block bool childRestrictionsDone = false;
    auto swizzle = swizzleEnforcesChildRestrictions(childRestrictionsDone);
    RetainPtr request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"http://webkit.org"]];
    [webView synchronouslyLoadSimulatedRequest:request.get() responseHTMLString:@"<div id=\"target\" style=\"width:150px;height:150px;background-color:red\"></div>"];
    TestWebKitAPI::Util::run(&childRestrictionsDone);

    [webView waitForNextPresentationUpdate];

    EXPECT_TRUE(systemScreenTimeBlockingViewIsPresent(webView.get()));

    RetainPtr controller = [webView _screenTimeWebpageController];
    Class remoteViewClass = NSClassFromString(@"NSRemoteView");

    TestWebKitAPI::Util::waitFor([&] {
        return [[[controller view] subviews] indexOfObjectPassingTest:^BOOL(NSView *subview, NSUInteger, BOOL *) {
            return [subview isKindOfClass:remoteViewClass];
        }] != NSNotFound;
    });

    if ([webView _wantsConstraintBasedLayout]) {
        [webView setTranslatesAutoresizingMaskIntoConstraints:NO];

        RetainPtr superview = [webView superview];
        [NSLayoutConstraint activateConstraints:@[
            [[webView topAnchor] constraintEqualToAnchor:[superview topAnchor]],
            [[webView leadingAnchor] constraintEqualToAnchor:[superview leadingAnchor]],
            [[webView bottomAnchor] constraintEqualToAnchor:[superview bottomAnchor]],
            [[webView trailingAnchor] constraintEqualToAnchor:[superview trailingAnchor]]
        ]];
    }

    [webView objectByEvaluatingJavaScript:@"document.querySelector('#target').addEventListener('fullscreenchange', event => { window.webkit.messageHandlers.testHandler.postMessage('fullscreenchange'); });"];

    bool receivedFullscreenChangeMessage = false;
    [webView performAfterReceivingMessage:@"fullscreenchange" action:[&] {
        receivedFullscreenChangeMessage = true;
    }];

    RetainPtr originalWindow = [webView window];

    [webView objectByEvaluatingJavaScriptWithUserGesture:@"document.querySelector('#target').webkitRequestFullscreen()"];

    TestWebKitAPI::Util::run(&receivedFullscreenChangeMessage);
    receivedFullscreenChangeMessage = false;

    TestWebKitAPI::Util::waitFor([&] {
        RetainPtr window = [webView window];
        return [window isVisible] && (window != originalWindow);
    });

    NSSize fullscreenWindowSize = [[webView window] frame].size;
    EXPECT_EQ([webView frame], NSMakeRect(0, 0, fullscreenWindowSize.width, fullscreenWindowSize.height));
}

#endif

TEST(ScreenTime, CreateControllerAfterOffscreenWebViewBecomesInWindow)
{
    __block bool childRestrictionsDone = false;
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    RetainPtr webView = webViewForScreenTimeTests(configuration.get(), NO);

    auto swizzle = swizzleEnforcesChildRestrictions(childRestrictionsDone);
    [webView synchronouslyLoadHTMLString: @"" baseURL:[NSURL URLWithString:@"http://webkit.org"]];

    [webView waitForNextPresentationUpdate];

    EXPECT_FALSE(!![webView _screenTimeWebpageController]);

    [webView addToTestWindow];
    TestWebKitAPI::Util::run(&childRestrictionsDone);
    TestWebKitAPI::Util::waitFor([&] {
        return !![webView _screenTimeWebpageController];
    });

    EXPECT_TRUE(!![webView _screenTimeWebpageController]);
}

TEST(ScreenTime, ScreenTimeControllerSetsURLWhenOffscreenWebViewBecomesInWindow)
{
    __block bool childRestrictionsDone = false;
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    RetainPtr webView = webViewForScreenTimeTests(configuration.get(), NO);

    RetainPtr request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"http://webkit.org"]];
    [webView synchronouslyLoadSimulatedRequest:request.get() responseHTMLString:@""];

    [webView waitForNextPresentationUpdate];

    EXPECT_FALSE(!![webView _screenTimeWebpageController]);

    auto swizzle = swizzleEnforcesChildRestrictions(childRestrictionsDone);
    [webView addToTestWindow];
    TestWebKitAPI::Util::run(&childRestrictionsDone);

    TestWebKitAPI::Util::waitFor([&] {
        return !![webView _screenTimeWebpageController];
    });

    EXPECT_TRUE(!![webView _screenTimeWebpageController]);

    TestWebKitAPI::Util::waitFor([&] {
        return !![[webView _screenTimeWebpageController] URL];
    });

    EXPECT_NOT_NULL([[webView _screenTimeWebpageController] URL]);
}

TEST(ScreenTime, ScreenTimeControllerInstalledAfterRestoreFromSessionState)
{
    __block bool childRestrictionsDone = false;

    RetainPtr webView1 = webViewForScreenTimeTests();

    RetainPtr request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"http://webkit.org"]];
    auto swizzle = swizzleEnforcesChildRestrictions(childRestrictionsDone);
    [webView1 synchronouslyLoadSimulatedRequest:request.get() responseHTMLString:@""];
    TestWebKitAPI::Util::run(&childRestrictionsDone);

    RetainPtr sessionState = [webView1 _sessionState];
    [webView1 _close];

    RetainPtr webView2 = webViewForScreenTimeTests(nil, NO);

    EXPECT_FALSE(!![webView2 _screenTimeWebpageController]);

    [webView2 addToTestWindow];
    [webView2 _restoreSessionState:sessionState.get() andNavigate:YES];
    [webView2 _test_waitForDidFinishNavigation];

    [webView2 waitForNextPresentationUpdate];

    EXPECT_TRUE(!![webView2 _screenTimeWebpageController]);
}

TEST(ScreenTime, ScreenTimeControllerViewOnlyInstalledForHTTPFamily)
{
    __block bool childRestrictionsDone = false;
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setShowsSystemScreenTimeBlockingView:YES];

    RetainPtr webView = webViewForScreenTimeTests(configuration.get());

    RetainPtr request = [NSURLRequest requestWithURL:[NSURL URLWithString:@""]];

    auto swizzle = swizzleEnforcesChildRestrictions(childRestrictionsDone);
    [webView synchronouslyLoadSimulatedRequest:request.get() responseHTMLString:@""];

    [webView waitForNextPresentationUpdate];

    EXPECT_FALSE(systemScreenTimeBlockingViewIsPresent(webView.get()));

    [webView synchronouslyLoadHTMLString:@"<style> body { background-color: red; } </style>"];

    [webView waitForNextPresentationUpdate];

    EXPECT_FALSE(systemScreenTimeBlockingViewIsPresent(webView.get()));

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"http://webkit.org"]];
    [webView synchronouslyLoadSimulatedRequest:request.get() responseHTMLString:@""];
    TestWebKitAPI::Util::run(&childRestrictionsDone);

    [webView waitForNextPresentationUpdate];

    EXPECT_TRUE(systemScreenTimeBlockingViewIsPresent(webView.get()));
}

TEST(ScreenTime, ScreenTimeControllerNotInstalledForNoChildRestrictions)
{
    __block bool childRestrictionsDone = false;

    RetainPtr webView = webViewForScreenTimeTests();
    auto swizzle = swizzleEnforcesChildRestrictions(childRestrictionsDone, NO);
    RetainPtr request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"http://webkit.org"]];
    [webView synchronouslyLoadSimulatedRequest:request.get() responseHTMLString:@""];

    [webView waitForNextPresentationUpdate];

    EXPECT_FALSE(!![webView _screenTimeWebpageController]);
}

#endif
