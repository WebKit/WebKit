/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#import "Helpers/DeprecatedGlobalValues.h"
#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/TestNotificationProvider.h"
#import "TestURLSchemeHandler.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKNotificationProvider.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKSecurityOriginPrivate.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKFeature.h>
#import <WebKit/_WKNotificationData.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <WebKit/_WKWebsiteDataStoreDelegate.h>
#import <wtf/HexNumber.h>

static constexpr auto simpleMainBytes = R"SWRESOURCE(
Hello World!
)SWRESOURCE"_s;

static constexpr auto checkForBadgeFunctions = R"SWRESOURCE(
var result = "";

function checkNavigatorProperty(property)
{
    if (property in window.navigator) {
        if (typeof(property in window.navigator) == "function")
            return 'f ';
        else
            return '1 ';
    }
    return '0 ';
}

result += checkNavigatorProperty('setAppBadge');
result += checkNavigatorProperty('clearAppBadge');
return result;
)SWRESOURCE"_s;

static constexpr auto exerciseBadgeFunctions = R"SWRESOURCE(
window.navigator.setAppBadge(0);
window.navigator.setAppBadge(1);
window.navigator.setAppBadge(Number.MAX_SAFE_INTEGER);
window.navigator.setAppBadge();

window.navigator.clearAppBadge();

window.navigator.setAppBadge(-10).then(() => {
    window.webkit.messageHandlers.sw.postMessage("SUCCEEDED");
}).catch((e) => {
    window.webkit.messageHandlers.sw.postMessage("CAUGHT ERROR");
});
window.navigator.setAppBadge('a').then(() => {
    window.webkit.messageHandlers.sw.postMessage("SUCCEEDED");
}).catch((e) => {
    window.webkit.messageHandlers.sw.postMessage("CAUGHT ERROR");
});

return "DONE";
)SWRESOURCE"_s;

@interface BadgeDelegate : NSObject<WKUIDelegatePrivate, _WKWebsiteDataStoreDelegate>
@property (readwrite) int appBadgeIndex;
@property (readwrite, copy) NSArray *expectedAppBadgeSequence;
@property (readwrite, copy) NSURL *serverURL;
@property (readwrite) BOOL shouldAllowOriginViolations;
@property (readonly) int originViolationCount;
@end

@implementation BadgeDelegate

- (void)updatedAppBadge:(NSNumber *)badge fromOrigin:(WKSecurityOrigin *)origin
{
    id innerBadge = badge;
    if (!innerBadge)
        innerBadge = [NSNull null];

    if (_expectedAppBadgeSequence)
        EXPECT_TRUE([_expectedAppBadgeSequence[_appBadgeIndex++] isEqual:innerBadge]);
    else {
        // This badging delegate expects no badge updates, so getting any is bad news.
        // But we'll still bump the _appBadgeIndex variable to register that we received
        // a badge update for other parts of the test.
        ++_appBadgeIndex;
        FAIL();
    }

    if (_serverURL) {
        if (!_shouldAllowOriginViolations)
            EXPECT_TRUE([origin isSameSiteAsURL:_serverURL]);
        else
            ++_originViolationCount;
    }
}

- (void)_webView:(WKWebView *)webView updatedAppBadge:(NSNumber *)badge fromSecurityOrigin:(WKSecurityOrigin *)origin
{
    [self updatedAppBadge:badge fromOrigin:origin];
}

- (void)websiteDataStore:(WKWebsiteDataStore *)dataStore workerOrigin:(WKSecurityOrigin *)workerOrigin updatedAppBadge:(NSNumber *)badge
{
    [self updatedAppBadge:badge fromOrigin:workerOrigin];
}

@end

TEST(Badging, APIWindow)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { simpleMainBytes } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    static bool messagesDone = false;
    static int messageCount = 0;
    static const int expectedMessages = 2;

    RetainPtr testMessageHandler = adoptNS([[TestMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:testMessageHandler.get() name:@"sw"];
    [testMessageHandler addMessage:@"SUCCEEDED" withHandler:^{
        EXPECT_TRUE(false);
        if (++messageCount == expectedMessages)
            messagesDone = true;
    }];
    [testMessageHandler addMessage:@"CAUGHT ERROR" withHandler:^{
        EXPECT_TRUE(true);
        if (++messageCount == expectedMessages)
            messagesDone = true;
    }];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);
    RetainPtr badgeDelegate = adoptNS([BadgeDelegate new]);
    badgeDelegate.get().serverURL = server.request("/"_s).URL;
    badgeDelegate.get().expectedAppBadgeSequence = @[@0, @1, @9007199254740991, [NSNull null], @0];

    webView.get().UIDelegate = badgeDelegate.get();
    [webView synchronouslyLoadRequest:server.request("/"_s)];

    NSString *nsCheckForBadgeFunctions = [NSString stringWithUTF8String:checkForBadgeFunctions];
    static bool done = false;
    [webView callAsyncJavaScript:nsCheckForBadgeFunctions arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE([result isEqualToString:@"0 0 "]);
        EXPECT_NULL(error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    webView.get().configuration.preferences._appBadgeEnabled = YES;
    [webView synchronouslyLoadRequest:server.request("/"_s)];

    done = false;
    [webView callAsyncJavaScript:nsCheckForBadgeFunctions arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE([result isEqualToString:@"1 1 "]);
        EXPECT_NULL(error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    webView.get().configuration.preferences._appBadgeEnabled = NO;
    [webView synchronouslyLoadRequest:server.request("/"_s)];

    done = false;
    [webView callAsyncJavaScript:nsCheckForBadgeFunctions arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE([result isEqualToString:@"0 0 "]);
        EXPECT_NULL(error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    webView.get().configuration.preferences._appBadgeEnabled = YES;
    [webView synchronouslyLoadRequest:server.request("/"_s)];

    done = false;
    [webView callAsyncJavaScript:nsCheckForBadgeFunctions arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE([result isEqualToString:@"1 1 "]);
        EXPECT_NULL(error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    NSString *nsExerciseBadgeFunctions = [NSString stringWithUTF8String:exerciseBadgeFunctions];
    done = false;
    [webView callAsyncJavaScript:nsExerciseBadgeFunctions arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE([result isEqualToString:@"DONE"]);
        EXPECT_NULL(error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    TestWebKitAPI::Util::run(&messagesDone);

    EXPECT_EQ(badgeDelegate.get().appBadgeIndex, 5);
}

static constexpr auto badgingWorkerMainBytes = R"TESTRESOURCE(
<script>
window.worker = new Worker('worker.js');
worker.onerror = (event) => {
    window.webkit.messageHandlers.testHandler.postMessage(event);
};
worker.onmessage = (event) => {
    window.webkit.messageHandlers.testHandler.postMessage(event.data);
};
</script>
)TESTRESOURCE"_s;

static constexpr auto badingWorkerBytes = R"TESTRESOURCE(
self.onmessage = (event) => {
    if (event.data != 'updateBadge')
        return;

    navigator.setAppBadge(10);
    navigator.clearAppBadge();

    self.postMessage('BADGINGDONE');
};
self.postMessage('RUNNING');

)TESTRESOURCE"_s;

TEST(Badging, DedicatedWorker)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { badgingWorkerMainBytes } },
        { "/worker.js"_s, { { { "Content-Type"_s, "text/javascript"_s } }, badingWorkerBytes } }
    });

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    static bool workerRunning = false;
    static bool badgingDone = false;
    static bool javascriptDone = false;

    RetainPtr testMessageHandler = adoptNS([[TestMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:testMessageHandler.get() name:@"testHandler"];
    [testMessageHandler addMessage:@"RUNNING" withHandler:^{
        workerRunning = true;
    }];
    [testMessageHandler addMessage:@"BADGINGDONE" withHandler:^{
        badgingDone = true;
    }];

    configuration.get().preferences._appBadgeEnabled = YES;
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);
    RetainPtr badgeDelegate = adoptNS([BadgeDelegate new]);
    badgeDelegate.get().serverURL = server.request("/"_s).URL;
    badgeDelegate.get().expectedAppBadgeSequence = @[@10, @0];
    configuration.get().websiteDataStore._delegate = badgeDelegate.get();

    [webView synchronouslyLoadRequest:server.request("/"_s)];

    TestWebKitAPI::Util::run(&workerRunning);

    [webView callAsyncJavaScript:@"window.worker.postMessage('updateBadge');" arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(error);
        javascriptDone = true;
    }];
    TestWebKitAPI::Util::run(&javascriptDone);
    TestWebKitAPI::Util::run(&badgingDone);

    EXPECT_EQ(badgeDelegate.get().appBadgeIndex, 2);
}

static constexpr auto sharedWorkerMainBytes = R"TESTRESOURCE(
<script>
window.sharedWorker = new SharedWorker('sharedworker.js');
sharedWorker.port.onmessage = (event) => {
    window.webkit.messageHandlers.testHandler.postMessage(event.data);
};
</script>
)TESTRESOURCE"_s;

static constexpr auto sharedWorkerBytes = R"TESTRESOURCE(
onconnect = (e) => {
    self.port = e.ports[0];
    self.port.postMessage("RUNNING");

    self.port.onmessage = (event) => {
        if (event.data != 'updateBadge')
            return;

        navigator.setAppBadge(10);
        navigator.clearAppBadge();

        self.port.postMessage('BADGINGDONE');
    };
}

)TESTRESOURCE"_s;

TEST(Badging, SharedWorker)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { sharedWorkerMainBytes } },
        { "/sharedworker.js"_s, { { { "Content-Type"_s, "text/javascript"_s } }, sharedWorkerBytes } }
    });

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    static bool workerRunning = false;
    static bool badgingDone = false;
    static bool javascriptDone = false;

    RetainPtr testMessageHandler = adoptNS([[TestMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:testMessageHandler.get() name:@"testHandler"];
    [testMessageHandler addMessage:@"RUNNING" withHandler:^{
        workerRunning = true;
    }];
    [testMessageHandler addMessage:@"BADGINGDONE" withHandler:^{
        badgingDone = true;
    }];

    configuration.get().preferences._appBadgeEnabled = YES;
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);
    RetainPtr badgeDelegate = adoptNS([BadgeDelegate new]);
    badgeDelegate.get().serverURL = server.request("/"_s).URL;
    badgeDelegate.get().expectedAppBadgeSequence = @[@10, @0];
    configuration.get().websiteDataStore._delegate = badgeDelegate.get();

    [webView synchronouslyLoadRequest:server.request("/"_s)];

    TestWebKitAPI::Util::run(&workerRunning);

    [webView callAsyncJavaScript:@"window.sharedWorker.port.postMessage('updateBadge');" arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(error);
        javascriptDone = true;
    }];
    TestWebKitAPI::Util::run(&javascriptDone);
    TestWebKitAPI::Util::run(&badgingDone);

    EXPECT_EQ(badgeDelegate.get().appBadgeIndex, 2);
}

static constexpr auto serviceWorkerMainBytes = R"SWRESOURCE(
<script>
const channel = new MessageChannel();
channel.port1.onmessage = (event) => {
    window.webkit.messageHandlers.testHandler.postMessage(event.data);
};

navigator.serviceWorker.register('/sw.js').then((registration) => {
    if (registration.active) {
        registration.active.postMessage({port: channel.port2}, [channel.port2]);
        return;
    }
    worker = registration.installing;
    worker.addEventListener('statechange', function() {
        if (worker.state == 'activated')
            worker.postMessage({port: channel.port2}, [channel.port2]);
    });
}).catch(function(error) {
    log("Registration failed with: " + error);
});
</script>
)SWRESOURCE"_s;

//updateBadge

static constexpr auto serviceWorkerScriptBytes = R"SWRESOURCE(
let port;
self.addEventListener("message", (event) => {
    port = event.data.port;
    port.onmessage = (event) => {
        if (event.data != 'updateBadge')
            return;

        navigator.setAppBadge(10);
        navigator.clearAppBadge();

         port.postMessage('BADGINGDONE');
    };
    port.postMessage("RUNNING");
});
)SWRESOURCE"_s;

TEST(Badging, ServiceWorker)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { serviceWorkerMainBytes } },
        { "/sw.js"_s, { { { "Content-Type"_s, "text/javascript"_s } }, serviceWorkerScriptBytes } }
    });

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    static bool workerRunning = false;
    static bool badgingDone = false;
    static bool javascriptDone = false;

    RetainPtr testMessageHandler = adoptNS([[TestMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:testMessageHandler.get() name:@"testHandler"];
    [testMessageHandler addMessage:@"RUNNING" withHandler:^{
        workerRunning = true;
    }];
    [testMessageHandler addMessage:@"BADGINGDONE" withHandler:^{
        badgingDone = true;
    }];

    configuration.get().preferences._appBadgeEnabled = YES;
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);
    RetainPtr badgeDelegate = adoptNS([BadgeDelegate new]);
    badgeDelegate.get().serverURL = server.request("/"_s).URL;
    badgeDelegate.get().expectedAppBadgeSequence = @[@10, @0];
    configuration.get().websiteDataStore._delegate = badgeDelegate.get();

    [webView synchronouslyLoadRequest:server.request("/"_s)];

    TestWebKitAPI::Util::run(&workerRunning);

    [webView callAsyncJavaScript:@"channel.port1.postMessage('updateBadge');" arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(error);
        javascriptDone = true;
    }];
    TestWebKitAPI::Util::run(&javascriptDone);
    TestWebKitAPI::Util::run(&badgingDone);

    EXPECT_EQ(badgeDelegate.get().appBadgeIndex, 2);
}

static constexpr auto originMainFrameBytes = R"SWRESOURCE(
<iframe src="origintest2://main.com/iframe.html"></iframe>
<script>
window.onmessage = (event) => {
    window.webkit.messageHandlers.testHandler.postMessage(event.data);
}
</script>
)SWRESOURCE"_s;

static constexpr auto originIFrameBytes = R"SWRESOURCE(
<script type="module">
try {
    await window.navigator.setAppBadge(10);
    window.parent.postMessage("BadgeSucceeded", "*");
} catch (e) {
    window.parent.postMessage("BadgeFailed: " + e, "*");
}
</script>
)SWRESOURCE"_s;

static void runOriginTest(NSString *mainURL, NSString *expectedMessage)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    RetainPtr handler = adoptNS([[TestURLSchemeHandler alloc] init]);
    handler.get().startURLSchemeTaskHandler = ^(WKWebView *, id<WKURLSchemeTask> task) {
        if ([task.request.URL.path hasSuffix:@"index.html"])
            return respond(task, originMainFrameBytes);
        if ([task.request.URL.path hasSuffix:@"iframe.html"])
            return respond(task, originIFrameBytes);

        ASSERT_NOT_REACHED();
    };
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"origintest1"];
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"origintest2"];
    configuration.get().preferences._appBadgeEnabled = YES;

    RetainPtr testMessageHandler = adoptNS([[TestMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:testMessageHandler.get() name:@"testHandler"];

    static bool gotMessage = false;
    static RetainPtr<NSString> storedMessage;
    testMessageHandler.get().didReceiveScriptMessage = ^(NSString *message) {
        storedMessage = message;
        gotMessage = true;
    };

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);
    RetainPtr badgeDelegate = adoptNS([BadgeDelegate new]);

    NSURL *url = [NSURL URLWithString:mainURL];
    badgeDelegate.get().serverURL = url;
    badgeDelegate.get().shouldAllowOriginViolations = YES;
    badgeDelegate.get().expectedAppBadgeSequence = @[@10];
    webView.get().UIDelegate = badgeDelegate.get();

    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:url]];
    TestWebKitAPI::Util::run(&gotMessage);

    // For the purposes of this test suite, no origin violations should ever
    // escape the web content process and reach the UI process delegate.
    EXPECT_EQ(badgeDelegate.get().originViolationCount, 0);
    EXPECT_TRUE([storedMessage.get() isEqualToString:expectedMessage]);
}

TEST(Badging, CrossOrigin)
{
    runOriginTest(@"origintest1://webkit.org/index.html", @"BadgeFailed: SecurityError: The operation is insecure.");
}

TEST(Badging, SameOrigin)
{
    runOriginTest(@"origintest2://main.com/index.html", @"BadgeSucceeded");
}

TEST(Badging, ServiceWorkerOverride)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { serviceWorkerMainBytes } },
        { "/sw.js"_s, { { { "Content-Type"_s, "text/javascript"_s } }, serviceWorkerScriptBytes } }
    });

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    static bool workerRunning = false;
    static bool badgingDone = false;
    static bool javascriptDone = false;

    RetainPtr testMessageHandler = adoptNS([[TestMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:testMessageHandler.get() name:@"testHandler"];
    [testMessageHandler addMessage:@"RUNNING" withHandler:^{
        workerRunning = true;
    }];
    [testMessageHandler addMessage:@"BADGINGDONE" withHandler:^{
        badgingDone = true;
    }];

    // The WKWebView we're about to load and create will have app badge disabled.
    // But the ServiceWorker should have it overidden to enabled.
    configuration.get().preferences._appBadgeEnabled = NO;
    WKPreferences *serviceWorkerOverride = [WKPreferences new];
    serviceWorkerOverride._appBadgeEnabled = YES;
    [configuration.get().websiteDataStore _setServiceWorkerOverridePreferences:serviceWorkerOverride];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);
    RetainPtr badgeDelegate = adoptNS([BadgeDelegate new]);
    badgeDelegate.get().expectedAppBadgeSequence = @[@10, @0];
    configuration.get().websiteDataStore._delegate = badgeDelegate.get();

    [webView synchronouslyLoadRequest:server.request("/"_s)];

    TestWebKitAPI::Util::run(&workerRunning);

    // Confirm that the WKWebView's window object does NOT have the badging functions exposed
    NSString *nsCheckForBadgeFunctions = [NSString stringWithUTF8String:checkForBadgeFunctions];
    static bool done = false;
    [webView callAsyncJavaScript:nsCheckForBadgeFunctions arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE([result isEqualToString:@"0 0 "]);
        EXPECT_NULL(error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    // But then confirm that they DO work as expected in the service worker
    [webView callAsyncJavaScript:@"channel.port1.postMessage('updateBadge');" arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(error);
        javascriptDone = true;
    }];
    TestWebKitAPI::Util::run(&javascriptDone);
    TestWebKitAPI::Util::run(&badgingDone);

    EXPECT_EQ(badgeDelegate.get().appBadgeIndex, 2);
}

#if ENABLE(IPC_TESTING_API)

static constexpr auto badgeAttackFromWorkerBytes = R"TESTRESOURCE(
<script src="coreipc.js"></script>
<script>
async function sendSpoofedBadgeIPC()
{
    const CoreIPC = new CoreIPCClass();

    CoreIPC.UI.WebProcessProxy.SetAppBadgeFromWorker(0, {
        origin: {
            data: {
                variantType: 'WebCore::SecurityOriginData::Tuple',
                variant: {
                    protocol: 'https',
                    host: 'apple.com',
                    port: { }
                }
            }
        },
        badge: {
            optionalValue: 1337
        }
    });
}
</script>
)TESTRESOURCE"_s;

static void runAppBadgeSpoofTest(const String& attackerHTML)
{
    NSURL *coreIPCURL = [NSBundle.test_resourcesBundle URLForResource:@"coreipc" withExtension:@"js"];
    NSData *coreIPCData = [NSData dataWithContentsOfURL:coreIPCURL];

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { attackerHTML } },
        { "/coreipc.js"_s, { { { "Content-Type"_s, "text/javascript"_s } }, coreIPCData } },
    });

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    for (_WKFeature *feature in [WKPreferences _features]) {
        if ([feature.key isEqualToString:@"IPCTestingAPIEnabled"]) {
            [[configuration preferences] _setEnabled:YES forFeature:feature];
            break;
        }
    }
    configuration.get().preferences._appBadgeEnabled = YES;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);

    auto badgeDelegate = adoptNS([BadgeDelegate new]);
    badgeDelegate.get().expectedAppBadgeSequence = nil;

    webView.get().UIDelegate = badgeDelegate.get();
    configuration.get().websiteDataStore._delegate = badgeDelegate.get();

    [webView synchronouslyLoadRequest:server.request("/"_s)];

    static bool javascriptDone = false;
    [webView callAsyncJavaScript:@"sendSpoofedBadgeIPC();" arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *error) {
        javascriptDone = true;
    }];
    TestWebKitAPI::Util::run(&javascriptDone);

    // Give the IPC message time to be processed.
    TestWebKitAPI::Util::spinRunLoop(30);

    EXPECT_EQ(badgeDelegate.get().appBadgeIndex, 0);
}

TEST(Badging, SetAppBadgeFromWorkerOriginSpoof)
{
    runAppBadgeSpoofTest(badgeAttackFromWorkerBytes);
}

static constexpr auto badgeAttackFromFrameBytes = R"TESTRESOURCE(
<script src="coreipc.js"></script>
<script>
async function sendSpoofedBadgeIPC()
{
    const CoreIPC = new CoreIPCClass();

    CoreIPC.UI.WebFrameProxy.SetAppBadge(IPC.frameID[0], {
        origin: {
            data: {
                variantType: 'WebCore::SecurityOriginData::Tuple',
                variant: {
                    protocol: 'https',
                    host: 'apple.com',
                    port: { }
                }
            }
        },
        badge: {
            optionalValue: 1337
        }
    });
}
</script>
)TESTRESOURCE"_s;

TEST(Badging, SetAppBadgeFromFrameOriginSpoof)
{
    runAppBadgeSpoofTest(badgeAttackFromFrameBytes);
}

#endif // ENABLE(IPC_TESTING_API)
