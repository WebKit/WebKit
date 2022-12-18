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

#if ENABLE(BADGING)

#import "DeprecatedGlobalValues.h"
#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNotificationProvider.h"
#import "TestWKWebView.h"
#import <WebCore/RegistrationDatabase.h>
#import <WebKit/WKNotificationProvider.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
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
result += checkNavigatorProperty('setClientBadge');
result += checkNavigatorProperty('clearClientBadge');
return result;
)SWRESOURCE"_s;

static constexpr auto exerciseBadgeFunctions = R"SWRESOURCE(
window.navigator.setAppBadge(0);
window.navigator.setAppBadge(1);
window.navigator.setAppBadge(Number.MAX_SAFE_INTEGER);
window.navigator.setAppBadge();

window.navigator.setClientBadge(0);
window.navigator.setClientBadge(1);
window.navigator.setClientBadge(Number.MAX_SAFE_INTEGER);
window.navigator.setClientBadge();

window.navigator.clearAppBadge();
window.navigator.clearClientBadge();


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
window.navigator.setClientBadge(-10).then(() => {
    window.webkit.messageHandlers.sw.postMessage("SUCCEEDED");
}).catch((e) => {
    window.webkit.messageHandlers.sw.postMessage("CAUGHT ERROR");
});
window.navigator.setClientBadge('a').then(() => {
    window.webkit.messageHandlers.sw.postMessage("SUCCEEDED");
}).catch((e) => {
    window.webkit.messageHandlers.sw.postMessage("CAUGHT ERROR");
});


return "DONE";
)SWRESOURCE"_s;

@interface BadgeDelegate : NSObject<WKUIDelegatePrivate, _WKWebsiteDataStoreDelegate>
@property (readwrite) int appBadgeIndex;
@property (readwrite) int clientBadgeIndex;
@property (readwrite, copy) NSArray *expectedAppBadgeSequence;
@property (readwrite, copy) NSArray *expectedClientBadgeSequence;
@end

@implementation BadgeDelegate

- (void)updatedAppBadge:(NSNumber *)badge
{
    id innerBadge = badge;
    if (!innerBadge)
        innerBadge = [NSNull null];
    EXPECT_TRUE([_expectedAppBadgeSequence[_appBadgeIndex++] isEqual:innerBadge]);
}

- (void)_webView:(WKWebView *)webView updatedAppBadge:(NSNumber *)badge
{
    [self updatedAppBadge:badge];
}

- (void)_webView:(WKWebView *)webView updatedClientBadge:(NSNumber *)badge
{
    id innerBadge = badge;
    if (!innerBadge)
        innerBadge = [NSNull null];
    EXPECT_TRUE([_expectedClientBadgeSequence[_clientBadgeIndex++] isEqual :innerBadge]);
}

- (void)websiteDataStore:(WKWebsiteDataStore *)dataStore workerOrigin:(WKSecurityOrigin *)workerOrigin updatedAppBadge:(NSNumber *)badge
{
    [self updatedAppBadge:badge];
}

@end

TEST(Badging, APIWindow)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { simpleMainBytes } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    static bool messagesDone = false;
    static int messageCount = 0;
    static const int expectedMessages = 4;

    auto testMessageHandler = adoptNS([[TestMessageHandler alloc] init]);
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

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);
    auto badgeDelegate = adoptNS([BadgeDelegate new]);
    badgeDelegate.get().expectedAppBadgeSequence = @[@0, @1, @9007199254740991, [NSNull null], @0];
    badgeDelegate.get().expectedClientBadgeSequence = @[@0, @1, @9007199254740991, [NSNull null], @0];

    webView.get().UIDelegate = badgeDelegate.get();
    [webView synchronouslyLoadRequest:server.request("/"_s)];

    NSString *nsCheckForBadgeFunctions = [NSString stringWithUTF8String:checkForBadgeFunctions];
    static bool done = false;
    [webView callAsyncJavaScript:nsCheckForBadgeFunctions arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE([result isEqualToString:@"0 0 0 0 "]);
        EXPECT_NULL(error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    webView.get().configuration.preferences._appBadgeEnabled = YES;
    webView.get().configuration.preferences._clientBadgeEnabled = NO;
    [webView synchronouslyLoadRequest:server.request("/"_s)];

    done = false;
    [webView callAsyncJavaScript:nsCheckForBadgeFunctions arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE([result isEqualToString:@"1 1 0 0 "]);
        EXPECT_NULL(error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    webView.get().configuration.preferences._appBadgeEnabled = NO;
    webView.get().configuration.preferences._clientBadgeEnabled = YES;
    [webView synchronouslyLoadRequest:server.request("/"_s)];

    done = false;
    [webView callAsyncJavaScript:nsCheckForBadgeFunctions arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE([result isEqualToString:@"0 0 1 1 "]);
        EXPECT_NULL(error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    webView.get().configuration.preferences._appBadgeEnabled = YES;
    webView.get().configuration.preferences._clientBadgeEnabled = YES;
    [webView synchronouslyLoadRequest:server.request("/"_s)];

    done = false;
    [webView callAsyncJavaScript:nsCheckForBadgeFunctions arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE([result isEqualToString:@"1 1 1 1 "]);
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
    EXPECT_EQ(badgeDelegate.get().clientBadgeIndex, 5);
}

static constexpr auto workerMainBytes = R"TESTRESOURCE(
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

static constexpr auto workerBytes = R"TESTRESOURCE(
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
        { "/"_s, { workerMainBytes } },
        { "/worker.js"_s, { { { "Content-Type"_s, "text/javascript"_s } }, workerBytes } }
    });

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    static bool workerRunning = false;
    static bool badgingDone = false;
    static bool javascriptDone = false;

    auto testMessageHandler = adoptNS([[TestMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:testMessageHandler.get() name:@"testHandler"];
    [testMessageHandler addMessage:@"RUNNING" withHandler:^{
        workerRunning = true;
    }];
    [testMessageHandler addMessage:@"BADGINGDONE" withHandler:^{
        badgingDone = true;
    }];

    configuration.get().preferences._appBadgeEnabled = YES;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);
    auto badgeDelegate = adoptNS([BadgeDelegate new]);
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
    EXPECT_EQ(badgeDelegate.get().clientBadgeIndex, 0);
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

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    static bool workerRunning = false;
    static bool badgingDone = false;
    static bool javascriptDone = false;

    auto testMessageHandler = adoptNS([[TestMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:testMessageHandler.get() name:@"testHandler"];
    [testMessageHandler addMessage:@"RUNNING" withHandler:^{
        workerRunning = true;
    }];
    [testMessageHandler addMessage:@"BADGINGDONE" withHandler:^{
        badgingDone = true;
    }];

    configuration.get().preferences._appBadgeEnabled = YES;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);
    auto badgeDelegate = adoptNS([BadgeDelegate new]);
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
    EXPECT_EQ(badgeDelegate.get().clientBadgeIndex, 0);
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

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    static bool workerRunning = false;
    static bool badgingDone = false;
    static bool javascriptDone = false;

    auto testMessageHandler = adoptNS([[TestMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:testMessageHandler.get() name:@"testHandler"];
    [testMessageHandler addMessage:@"RUNNING" withHandler:^{
        workerRunning = true;
    }];
    [testMessageHandler addMessage:@"BADGINGDONE" withHandler:^{
        badgingDone = true;
    }];

    configuration.get().preferences._appBadgeEnabled = YES;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);
    auto badgeDelegate = adoptNS([BadgeDelegate new]);
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
    EXPECT_EQ(badgeDelegate.get().clientBadgeIndex, 0);
}
#endif // ENABLE(BADGING)
