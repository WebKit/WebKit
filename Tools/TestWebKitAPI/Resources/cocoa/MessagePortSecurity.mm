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

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Utilities.h"
#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestUIDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKFeature.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/RetainPtr.h>

#if ENABLE(IPC_TESTING_API)

static void enableIPCTestingAPI(WKWebViewConfiguration *configuration)
{
    for (_WKFeature *feature in [WKPreferences _features]) {
        if ([feature.key isEqualToString:@"IPCTestingAPIEnabled"]) {
            [[configuration preferences] _setEnabled:YES forFeature:feature];
            break;
        }
    }
}

static constexpr auto sharedWorkerBytes = R"PORTRESOURCE(
var heldPort = null;
self.addEventListener('connect', function(e) {
    var port = e.ports[0];
    port.addEventListener('message', function(event) {
        if (event.data === 'store-port' && event.ports && event.ports.length > 0) {
            heldPort = event.ports[0];
            port.postMessage('port-stored');
        } else if (event.data === 'get-port' && heldPort) {
            port.postMessage('here-is-port', [heldPort]);
            heldPort = null;
        }
    });
    port.start();
});
)PORTRESOURCE"_s;

static constexpr auto senderBytes = R"PORTRESOURCE(
<script>
var port2ProcessId, port2PortId;
IPC.addOutgoingMessageListener('Networking', function(msg) {
    if (msg.description.indexOf('CreateNewMessagePortChannel') !== -1 && !port2ProcessId) {
        var buf1 = msg.arguments[1];
        if (buf1 instanceof ArrayBuffer) {
            var dv = new DataView(buf1);
            port2ProcessId = dv.getBigUint64(0, true);
            port2PortId = dv.getBigUint64(8, true);
        }
    }
});
var channel = new MessageChannel();
var worker = new SharedWorker('/worker.js');
worker.port.addEventListener('message', function(event) {
    if (event.data === 'port-stored')
        alert('port-stored');
});
worker.port.start();
worker.port.postMessage('store-port', [channel.port2]);
</script>
)PORTRESOURCE"_s;

static constexpr auto receiverBytes = R"PORTRESOURCE(
<script>
var worker = new SharedWorker('/worker.js');
worker.port.addEventListener('message', function(event) {
    if (event.data === 'here-is-port' && event.ports && event.ports.length > 0) {
        window.storedPort = event.ports[0];
        alert('port-received');
    }
});
worker.port.start();
worker.port.postMessage('get-port');
</script>
)PORTRESOURCE"_s;

TEST(MessagePortSecurity, CrossProcessMessageTheftViaTakeAllMessagesForPort)
{
    TestWebKitAPI::HTTPServer server({
        { "/sender"_s, { senderBytes } },
        { "/receiver"_s, { receiverBytes } },
        { "/worker.js"_s, { { { "Content-Type"_s, "application/javascript"_s } }, sharedWorkerBytes } },
        { "/attacker"_s, { "<html><body>attacker</body></html>"_s } },
    });

    auto dataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]);
    auto config = adoptNS([[WKWebViewConfiguration alloc] init]);
    enableIPCTestingAPI(config.get());
    [config setWebsiteDataStore:dataStore.get()];

    // `webViewA` connects to a SharedWorker and sends a message port to it.
    auto navDelegateA = adoptNS([TestNavigationDelegate new]);
    auto uiDelegateA = adoptNS([TestUIDelegate new]);
    auto webViewA = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:config.get()]);
    [webViewA setNavigationDelegate:navDelegateA.get()];
    [webViewA setUIDelegate:uiDelegateA.get()];

    [webViewA loadRequest:server.request("/sender"_s)];
    [navDelegateA waitForDidFinishNavigation];

    // The sender page connects to the shared worker and sends it a port from a message channel.
    // Wait for all of that to finish.
    EXPECT_WK_STREQ([uiDelegateA waitForAlert], "port-stored");

    // `webViewB` will get the port from the shared worker, formally moving it to a new web content process,
    // forcing the networking process to be an intermediary.
    auto navDelegateB = adoptNS([TestNavigationDelegate new]);
    auto uiDelegateB = adoptNS([TestUIDelegate new]);
    auto webViewB = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:config.get()]);
    [webViewB setNavigationDelegate:navDelegateB.get()];
    [webViewB setUIDelegate:uiDelegateB.get()];

    [webViewB loadRequest:server.request("/receiver"_s)];

    // Wait for `webViewB` to get the port from the shared worker, and verify that
    // it's out of process from the first web view.
    // At that point, we know that messages to the port are going through the networking
    // process as an intermediate.
    EXPECT_WK_STREQ([uiDelegateB waitForAlert], "port-received");
    EXPECT_NE([webViewA _webProcessIdentifier], [webViewB _webProcessIdentifier]);

    // Post some messages from `webViewA`
    // Since `webViewB` acquired the port but did not activate it, these messages
    // will queue up in the Networking process.
    [webViewA evaluateJavaScript:
        @"channel.port1.postMessage('secret-message-1');"
        "channel.port1.postMessage('secret-message-2');"
        "channel.port1.postMessage('secret-message-3');"
        "alert('messages-posted');"
        completionHandler:nil];
    EXPECT_WK_STREQ([uiDelegateA waitForAlert], "messages-posted");

    // Earlier we used CoreIPC JS to grab the process identifier and port ID for the now-remote-MessagePort.
    // Grab them here for the attacker to use.
    NSString *port2ProcessString = [webViewA stringByEvaluatingJavaScript:@"port2ProcessId ? port2ProcessId.toString() : 'undefined'"];
    NSString *port2PortString = [webViewA stringByEvaluatingJavaScript:@"port2PortId ? port2PortId.toString() : 'undefined'"];
    EXPECT_FALSE([port2ProcessString isEqualToString:@"undefined"]);
    EXPECT_FALSE([port2PortString isEqualToString:@"undefined"]);

    // `webViewC` is the attacker.
    auto navDelegateC = adoptNS([TestNavigationDelegate new]);
    auto uiDelegateC = adoptNS([TestUIDelegate new]);
    auto webViewC = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:config.get()]);
    [webViewC setNavigationDelegate:navDelegateC.get()];
    [webViewC setUIDelegate:uiDelegateC.get()];

    [webViewC loadRequest:server.request("/attacker"_s)];
    [navDelegateC waitForDidFinishNavigation];

    EXPECT_NE([webViewA _webProcessIdentifier], [webViewC _webProcessIdentifier]);
    EXPECT_NE([webViewB _webProcessIdentifier], [webViewC _webProcessIdentifier]);

    // It uses CoreIPC JS to craft a message to acquire the messages sent to the port
    // we identified above. Ideally it should *not* be able to get those messages.
    NSString *attackScript = [NSString stringWithFormat:
        @"var net = IPC.connectionForProcessTarget('Networking');"
        "var portId = [{type: 'uint64_t', value: BigInt('%@')}, {type: 'uint64_t', value: BigInt('%@')}];"
        "net.sendWithAsyncReply(0,"
        "    IPC.messages.NetworkConnectionToWebProcess_TakeAllMessagesForPort.name,"
        "    [portId],"
        "    function(reply) {"
        "        try {"
        "            var buf = reply.arguments[0];"
        "            if (buf instanceof ArrayBuffer) {"
        "                var dv = new DataView(buf);"
        "                var count = Number(dv.getBigUint64(0, true));"
        "                alert('stolen:' + count);"
        "            } else {"
        "                alert('stolen:unexpected-type');"
        "            }"
        "        } catch(e) {"
        "            alert('error:' + e.message);"
        "        }"
        "    }"
        ");", port2ProcessString, port2PortString];
    [webViewC evaluateJavaScript:attackScript completionHandler:nil];

    // If the bug exists, the attacker will manage to get all 3 pending messages, resulting in "stolen:3"
    // The MESSAGE_CHECK being added responds with an empty set, resulting in "stolen:0"
    EXPECT_WK_STREQ([uiDelegateC waitForAlert], "stolen:0");
}

#endif // ENABLE(IPC_TESTING_API)

// When a WebContent process detaches from its Networking process, then reattaches to a new one,
// then tries to use stale message ports... It triggers a message check in the new Networking process.
// This test verifies that a WebContent process with "stale" message ports clears record of them,
// so that we cannot trigger this message check under normal operation.
TEST(MessagePortSecurity, MessagePortsSurviveNetworkProcessRestart)
{
    TestWebKitAPI::HTTPServer server({
        { "/main"_s, { "<!doctype html><script>"
            "window.channel = new MessageChannel();"
            "window.channel.port2.onmessage = function(e) { alert('received-' + e.data); };"
            "</script>"_s } },
        { "/ping"_s, { "ok"_s } },
    });

    RetainPtr<TestNavigationDelegate> navDelegate = adoptNS([TestNavigationDelegate new]);
    RetainPtr<TestUIDelegate> uiDelegate = adoptNS([TestUIDelegate new]);
    RetainPtr<WKWebViewConfiguration> config = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:config.get()]);
    [webView setNavigationDelegate:navDelegate.get()];
    [webView setUIDelegate:uiDelegate.get()];

    [webView loadRequest:server.request("/main"_s)];
    [navDelegate waitForDidFinishNavigation];

    pid_t initialWebProcessPID = [webView _webProcessIdentifier];
    pid_t initialNetworkProcessPID = [[webView configuration].websiteDataStore _networkProcessIdentifier];
    EXPECT_TRUE(!!initialWebProcessPID);
    EXPECT_TRUE(!!initialNetworkProcessPID);

    [webView evaluateJavaScript:@"channel.port1.postMessage('first');" completionHandler:nil];
    EXPECT_WK_STREQ([uiDelegate waitForAlert], "received-first");

    // Killing the Networking process should cause the WebContent process to handle the disconnect,
    // invalidating its current ports. This will help it avoid being killed by failing a MESSAGE_CHECK
    // in the new Networking process.
    [[webView configuration].websiteDataStore _terminateNetworkProcess];

    // Force a new Networking process to fire up and connect.
    NSString *fetchJS = [NSString stringWithFormat:
        @"fetch('%@/ping').then(r => r.text()).catch(e => 'err')",
        server.origin().createNSString().get()];
    __block bool fetchSettled = false;
    [webView evaluateJavaScript:fetchJS completionHandler:^(id, NSError *) {
        fetchSettled = true;
    }];
    TestWebKitAPI::Util::run(&fetchSettled);

    pid_t reestablishedNetworkProcessPID = [[webView configuration].websiteDataStore _networkProcessIdentifier];
    EXPECT_NE(initialNetworkProcessPID, reestablishedNetworkProcessPID);

    __block bool webContentTerminated = false;
    navDelegate.get().webContentProcessDidTerminate = ^(WKWebView *, _WKProcessTerminationReason) {
        webContentTerminated = true;
    };

    // Posting on a stale leftover port should be an instant no-op - The port has been invalidated.
    __block bool unexpectedAlert = false;
    uiDelegate.get().runJavaScriptAlertPanelWithMessage = ^(WKWebView *, NSString *, WKFrameInfo *, void (^handler)(void)) {
        unexpectedAlert = true;
        handler();
    };
    [webView evaluateJavaScript:@"channel.port1.postMessage('stale');" completionHandler:nil];

    // Let any in-flight tasks settle. If the bug is present (stale TakeAllMessagesForPort over the new
    // connection), webContentTerminated would flip during this window when the MESSAGE_CHECK fails.
    TestWebKitAPI::Util::runFor(0.5_s);

    EXPECT_FALSE(webContentTerminated);
    EXPECT_FALSE(unexpectedAlert);
    EXPECT_EQ(initialWebProcessPID, [webView _webProcessIdentifier]);

    // New MessageChannels should, of course, work again.
    __block bool freshAlertReceived = false;
    __block RetainPtr<NSString> freshAlertText;
    uiDelegate.get().runJavaScriptAlertPanelWithMessage = ^(WKWebView *, NSString *message, WKFrameInfo *, void (^handler)(void)) {
        freshAlertText = message;
        freshAlertReceived = true;
        handler();
    };

    [webView evaluateJavaScript:
        @"window.fresh = new MessageChannel();"
        "window.fresh.port2.onmessage = function(e) { alert('fresh-' + e.data); };"
        "window.fresh.port1.postMessage('hello');"
        completionHandler:nil];

    TestWebKitAPI::Util::runFor(&freshAlertReceived, 5_s);
    EXPECT_TRUE(freshAlertReceived);
    if (freshAlertReceived)
        EXPECT_WK_STREQ(freshAlertText.get(), "fresh-hello");
    EXPECT_FALSE(webContentTerminated);
}

TEST(MessagePortSecurity, MessagePortsSurviveNetworkProcessRestartWithSiteIsolation)
{
    TestWebKitAPI::HTTPServer server({
        { "/main"_s, { "<!doctype html><script>"
            "window.channel = new MessageChannel();"
            "</script>"_s } },
        { "/ping"_s, { "ok"_s } },
    });

    RetainPtr<TestNavigationDelegate> navDelegate = adoptNS([TestNavigationDelegate new]);
    RetainPtr<TestUIDelegate> uiDelegate = adoptNS([TestUIDelegate new]);
    RetainPtr<WKWebViewConfiguration> config = adoptNS([[WKWebViewConfiguration alloc] init]);

    for (_WKFeature *feature in [WKPreferences _features]) {
        if ([feature.key isEqualToString:@"SiteIsolationEnabled"]) {
            [[config preferences] _setEnabled:YES forFeature:feature];
            break;
        }
    }

    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:config.get()]);
    [webView setNavigationDelegate:navDelegate.get()];
    [webView setUIDelegate:uiDelegate.get()];

    [webView loadRequest:server.request("/main"_s)];
    [navDelegate waitForDidFinishNavigation];

    pid_t initialWebProcessPID = [webView _webProcessIdentifier];
    pid_t initialNetworkProcessPID = [[webView configuration].websiteDataStore _networkProcessIdentifier];
    EXPECT_TRUE(!!initialWebProcessPID);
    EXPECT_TRUE(!!initialNetworkProcessPID);

    [[webView configuration].websiteDataStore _terminateNetworkProcess];

    NSString *fetchJS = [NSString stringWithFormat:
        @"fetch('%@/ping').then(r => r.text()).catch(e => 'err')",
        server.origin().createNSString().get()];
    __block bool fetchSettled = false;
    [webView evaluateJavaScript:fetchJS completionHandler:^(id, NSError *) {
        fetchSettled = true;
    }];
    TestWebKitAPI::Util::run(&fetchSettled);

    pid_t reestablishedNetworkProcessPID = [[webView configuration].websiteDataStore _networkProcessIdentifier];
    EXPECT_NE(initialNetworkProcessPID, reestablishedNetworkProcessPID);

    __block bool webContentTerminated = false;
    navDelegate.get().webContentProcessDidTerminate = ^(WKWebView *, _WKProcessTerminationReason) {
        webContentTerminated = true;
    };

    [webView evaluateJavaScript:@"channel.port2.start();" completionHandler:nil];

    TestWebKitAPI::Util::runFor(0.5_s);

    EXPECT_FALSE(webContentTerminated);
    EXPECT_EQ(initialWebProcessPID, [webView _webProcessIdentifier]);
}

TEST(MessagePortSecurity, WorkerMessagePortsSurviveNetworkProcessRestart)
{
    TestWebKitAPI::HTTPServer server({
        { "/main"_s, { "<!doctype html><script>"
            "window.worker = new Worker('/worker.js');"
            "window.channel = new MessageChannel();"
            "window.worker.onmessage = function(e) { alert('worker-' + e.data); };"
            "window.worker.postMessage('init', [window.channel.port2]);"
            "</script>"_s } },
        { "/worker.js"_s, { { { "Content-Type"_s, "application/javascript"_s } },
            "self.port = null;"
            "self.onmessage = function(e) {"
            "    if (e.data === 'init') {"
            "        self.port = e.ports[0];"
            // Intentionally don't start the port here. Starting it after the NetworkProcess
            // restart is what exercises the racy TakeAllMessagesForPort path.
            "        self.postMessage('ready');"
            "    } else if (e.data === 'start-stale-port') {"
            "        try {"
            "            self.port.start();"
            "            self.postMessage('start-ok');"
            "        } catch (err) { self.postMessage('start-threw-' + err.message); }"
            "    }"
            "};"_s } },
        { "/ping"_s, { "ok"_s } },
    });

    RetainPtr<TestNavigationDelegate> navDelegate = adoptNS([TestNavigationDelegate new]);
    RetainPtr<TestUIDelegate> uiDelegate = adoptNS([TestUIDelegate new]);
    RetainPtr<WKWebViewConfiguration> config = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:config.get()]);
    [webView setNavigationDelegate:navDelegate.get()];
    [webView setUIDelegate:uiDelegate.get()];

    [webView loadRequest:server.request("/main"_s)];
    [navDelegate waitForDidFinishNavigation];

    EXPECT_WK_STREQ([uiDelegate waitForAlert], "worker-ready");

    pid_t initialWebProcessPID = [webView _webProcessIdentifier];
    pid_t initialNetworkProcessPID = [[webView configuration].websiteDataStore _networkProcessIdentifier];

    [[webView configuration].websiteDataStore _terminateNetworkProcess];

    NSString *fetchJS = [NSString stringWithFormat:
        @"fetch('%@/ping').then(r => r.text()).catch(e => 'err')",
        server.origin().createNSString().get()];
    __block bool fetchSettled = false;
    [webView evaluateJavaScript:fetchJS completionHandler:^(id, NSError *) {
        fetchSettled = true;
    }];
    TestWebKitAPI::Util::run(&fetchSettled);

    pid_t reestablishedNetworkProcessPID = [[webView configuration].websiteDataStore _networkProcessIdentifier];
    EXPECT_NE(initialNetworkProcessPID, reestablishedNetworkProcessPID);

    __block bool webContentTerminated = false;
    navDelegate.get().webContentProcessDidTerminate = ^(WKWebView *, _WKProcessTerminationReason) {
        webContentTerminated = true;
    };

    [webView evaluateJavaScript:@"worker.postMessage('start-stale-port');" completionHandler:nil];
    EXPECT_WK_STREQ([uiDelegate waitForAlert], "worker-start-ok");

    TestWebKitAPI::Util::runFor(0.5_s);

    EXPECT_FALSE(webContentTerminated);
    EXPECT_EQ(initialWebProcessPID, [webView _webProcessIdentifier]);
}
