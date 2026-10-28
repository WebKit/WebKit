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
#import "Helpers/Test.h"
#import "Helpers/Utilities.h"
#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/cocoa/TestNSBundleExtras.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKFeature.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/RetainPtr.h>

#if ENABLE(IPC_TESTING_API)

// TALOS-2026-2510 / rdar://183347241.
//
// WebSharedWorkerServerConnection::requestSharedWorker only validates the registrable domain of
// the renderer-supplied top origin, but WebSharedWorker::topSite() later builds a WebCore::Site
// from the whole origin and the UI process picks a web content process by comparing full Sites
// (scheme + domain). A compromised renderer that changes only the scheme of its recorded top
// origin therefore names a Site that matches no eligible process, and
// WebProcessPool::establishRemoteWorkerContextConnectionToNetworkProcess creates a fresh
// page-less process committed to the forged Site.
//
// These tests forge the message the renderer already sends, which is what native code execution
// in a web content process buys an attacker. The forged requests are built by parsing a real
// outgoing RequestSharedWorker message and re-serializing it, so they encode no assumption about
// the wire layout of SharedWorkerKey.

static constexpr auto attackerBytes = R"TESTRESOURCE(
<!DOCTYPE html>
<script src="/coreipc.js"></script>
<script>
const requestSharedWorkerMessageName = window.IPC ? IPC.messages.WebSharedWorkerServerConnection_RequestSharedWorker.name : 0;
const capturedRequests = [];
const workerMessages = [];

// Keeps every harvested SharedWorker object alive. SharedWorker::fromIdentifier() only resolves
// while the object is, and the network process needs it to resolve to run the script fetch a
// forged request triggers.
const harvestedWorkers = [];

if (window.IPC) {
    IPC.addOutgoingMessageListener('Networking', message => {
        if (message.name !== requestSharedWorkerMessageName)
            return;
        const definition = CoreIPC.messageByName[requestSharedWorkerMessageName];
        const [, , parsed] = ArgumentParser.parseArguments(new DataView(message.buffer), 16, definition.arguments);
        capturedRequests.push(parsed);
    });
}

// The identifiers in a parsed message are BigInts, which JSON cannot round-trip.
function cloneParsedMessage(value)
{
    if (Array.isArray(value))
        return value.map(cloneParsedMessage);
    if (value !== null && typeof value === 'object') {
        const copy = { };
        for (const key of Object.keys(value))
            copy[key] = cloneParsedMessage(value[key]);
        return copy;
    }
    return value;
}

// A shared worker whose script 404s leaves the network process with no context connection for
// this domain, so a later forged request is the first one that needs a process. It still leaves
// behind a live SharedWorkerObjectIdentifier and an unentangled message port pair for that
// request to reuse.
function harvestFailedSharedWorker(index)
{
    return new Promise((resolve, reject) => {
        const countBefore = capturedRequests.length;
        const worker = new SharedWorker('missing-worker.js', { name: 'harvest' + index });
        harvestedWorkers.push(worker);
        worker.port.onmessage = event => workerMessages.push(event.data);
        worker.onerror = () => {
            const waitForCapture = () => {
                if (capturedRequests.length > countBefore)
                    resolve(capturedRequests[capturedRequests.length - 1]);
                else
                    setTimeout(waitForCapture, 10);
            };
            waitForCapture();
        };
        setTimeout(() => reject(new Error('harvest' + index + ' never failed to load')), 10000);
    });
}

async function harvest(count)
{
    if (!window.IPC)
        throw new Error('the IPC testing API is not available');
    const requests = [];
    for (let index = 0; index < count; ++index)
        requests.push(await harvestFailedSharedWorker(index));
    window.harvestedRequests = requests;
    return requests.length;
}

// ArgumentParser.untypeResult() tests typeof value == 'object' before Array.isArray(), so its array
// branch never runs and a std::pair is parsed back as { 0: ..., 1: ... }. ArgumentSerializer.serializePair()
// requires a real array, so rebuild the one pair this message carries: TransferredMessagePort.
function restoreTransferredMessagePort(request)
{
    request.port.alias = [request.port.alias[0], request.port.alias[1]];
    return request;
}

// Replays a captured RequestSharedWorker under a worker name no existing SharedWorker uses, so
// the network process has to find a process for it. Passing null for topOriginProtocol replays
// the real scheme.
function requestSharedWorker(index, topOriginProtocol, name)
{
    const request = restoreTransferredMessagePort(cloneParsedMessage(window.harvestedRequests[index]));
    if (topOriginProtocol !== null)
        request.sharedWorkerKey.origin.topOrigin.data.variant.protocol = topOriginProtocol;
    request.sharedWorkerKey.url = { string: new URL('shared-worker.js', location.href).href };
    request.sharedWorkerKey.name = name;
    request.workerOptions.name = name;
    CoreIPC.Networking.WebSharedWorkerServerConnection.RequestSharedWorker(0, request);
    return 'sent';
}

function messagesFromWorkers()
{
    return workerMessages.join(',');
}
</script>
)TESTRESOURCE"_s;

static constexpr auto sharedWorkerBytes = R"TESTRESOURCE(
onconnect = event => {
    const port = event.ports[0];
    port.start();
    port.postMessage(self.name);
};
)TESTRESOURCE"_s;

namespace TestWebKitAPI {

static NSString *callAttackerFunction(TestWKWebView *webView, NSString *script, NSDictionary *arguments)
{
    __block bool done = false;
    __block RetainPtr<id> callResult;
    __block RetainPtr<NSError> callError;
    [webView callAsyncJavaScript:script arguments:arguments inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *error) {
        callResult = result;
        callError = error;
        done = true;
    }];
    Util::run(&done);

    // A throw here means the forged message never reached the network process, which would make
    // every process-count assertion below vacuously true.
    if (callError) {
        NSString *message = [callError.get() userInfo][@"WKJavaScriptExceptionMessage"] ?: [callError.get() localizedDescription];
        ADD_FAILURE() << "attacker script threw: " << message.UTF8String;
    }
    return (NSString *)callResult.autorelease();
}

// Loads the attacker page and harvests `harvestCount` live SharedWorkerObjectIdentifiers, leaving
// the network process with no shared worker context connection for this domain.
using SharedWorkerPlacementTestBlock = void (^)(TestWKWebView *, WKProcessPool *, WKWebsiteDataStore *);

static void runSharedWorkerPlacementTest(unsigned harvestCount, SharedWorkerPlacementTestBlock testBlock)
{
    NSURL *coreIPCURL = [NSBundle.test_resourcesBundle URLForResource:@"coreipc" withExtension:@"js"];
    ASSERT_TRUE(coreIPCURL);
    NSData *coreIPCData = [NSData dataWithContentsOfURL:coreIPCURL];

    HTTPServer server({
        { "/attacker.html"_s, { attackerBytes } },
        { "/coreipc.js"_s, { { { "Content-Type"_s, "text/javascript"_s } }, coreIPCData } },
        { "/shared-worker.js"_s, { { { "Content-Type"_s, "application/javascript"_s } }, sharedWorkerBytes } },
        { "/missing-worker.js"_s, HTTPResponse(404) },
    });

    RetainPtr dataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    RetainPtr dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]);

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    for (_WKFeature *feature in [WKPreferences _features]) {
        if ([feature.key isEqualToString:@"IPCTestingAPIEnabled"]) {
            [[configuration preferences] _setEnabled:YES forFeature:feature];
            break;
        }
    }
    [configuration setWebsiteDataStore:dataStore.get()];

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:server.request("/attacker.html"_s)];
    [navigationDelegate waitForDidFinishNavigation];

    __block bool harvestDone = false;
    __block RetainPtr<id> harvested;
    __block RetainPtr<NSError> harvestError;
    [webView callAsyncJavaScript:@"return await harvest(count);" arguments:@{ @"count": @(harvestCount) } inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *error) {
        harvested = result;
        harvestError = error;
        harvestDone = true;
    }];
    Util::run(&harvestDone);

    // Without a harvested request there is nothing to forge, so stop rather than report a pass.
    EXPECT_NULL(harvestError.get());
    ASSERT_EQ(harvestCount, [(NSNumber *)harvested.get() unsignedIntValue]);

    testBlock(webView.get(), [configuration processPool], dataStore.get());
}

// Replaying a captured request unmodified, under a fresh worker name, must start a worker in the
// process already committed to this site. This is the control for the tests below: it proves the
// parse/mutate/re-serialize path produces a message the network process acts on, and it
// establishes that a legitimate shared worker request creates no new process in this topology.
// If this test fails, the forged-request tests prove nothing.
TEST(SharedWorkerProcessPlacement, ReplayedRequestStartsWorkerWithoutNewProcess)
{
    runSharedWorkerPlacementTest(1, ^(TestWKWebView *webView, WKProcessPool *processPool, WKWebsiteDataStore *) {
        auto processCountBeforeRequest = [processPool _webProcessCountIgnoringPrewarmedAndCached];

        EXPECT_WK_STREQ("sent", callAttackerFunction(webView, @"return requestSharedWorker(0, null, 'control');", @{ }));

        EXPECT_TRUE(Util::waitFor([&] {
            return [[webView stringByEvaluatingJavaScript:@"messagesFromWorkers()"] containsString:@"control"];
        }, 100));

        EXPECT_EQ(processCountBeforeRequest, [processPool _webProcessCountIgnoringPrewarmedAndCached]);
    });
}

// The bug. Changing only the scheme of the claimed top origin names a Site no process holds, so
// the UI process creates a page-less web content process to run the attacker's worker.
TEST(SharedWorkerProcessPlacement, ForgedTopOriginSchemeDoesNotCreateNewProcess)
{
    runSharedWorkerPlacementTest(1, ^(TestWKWebView *webView, WKProcessPool *processPool, WKWebsiteDataStore *) {
        auto processCountBeforeRequest = [processPool _webProcessCountIgnoringPrewarmedAndCached];

        EXPECT_WK_STREQ("sent", callAttackerFunction(webView, @"return requestSharedWorker(0, 'x-forged', 'forged');", @{ }));

        Util::waitFor([&] {
            return [processPool _webProcessCountIgnoringPrewarmedAndCached] > processCountBeforeRequest;
        }, 30);

        EXPECT_EQ(processCountBeforeRequest, [processPool _webProcessCountIgnoringPrewarmedAndCached]);
        EXPECT_FALSE([[webView stringByEvaluatingJavaScript:@"messagesFromWorkers()"] containsString:@"forged"]);
    });
}

} // namespace TestWebKitAPI

#endif // ENABLE(IPC_TESTING_API)

// A document sandboxed by CSP has an opaque security origin, and AbstractWorker::validateURL allows an
// opaque origin to start a shared worker as long as the script is a data: URL. Neither of these needs the
// IPC testing API: they are ordinary content, and they exist because an opaque origin has no scheme or
// registrable domain, so the top-origin validation in
// WebSharedWorkerServerConnection::requestSharedWorker has nothing to compare such an origin against.

static constexpr auto dataURLSharedWorkerBytes = R"TESTRESOURCE(
<!DOCTYPE html>
<script>
// Assembled rather than written inline so that check-webkit-style does not read the MIME type's
// slash as a C++ operator.
const scriptType = ['application', 'javascript'].join('/');
const scriptBody = 'onconnect = event => event.ports[0].postMessage("connected")';
const worker = new SharedWorker('data:' + scriptType + ',' + scriptBody);
worker.port.onmessage = event => {
    window.workerResult = event.data;
    if (window.parent !== window)
        window.parent.postMessage(event.data, '*');
};
worker.port.start();
</script>
)TESTRESOURCE"_s;

static constexpr auto sandboxedFrameHostBytes = R"TESTRESOURCE(
<!DOCTYPE html>
<script>
window.addEventListener('message', event => {
    window.workerResult = event.data;
});
</script>
<iframe src="/sandboxed-frame.html"></iframe>
)TESTRESOURCE"_s;

@interface SharedWorkerHostSchemeHandler : NSObject <WKURLSchemeHandler>
@end

@implementation SharedWorkerHostSchemeHandler

- (void)webView:(WKWebView *)webView startURLSchemeTask:(id<WKURLSchemeTask>)task
{
    RetainPtr data = adoptNS([[NSData alloc] initWithBytes:dataURLSharedWorkerBytes.characters() length:dataURLSharedWorkerBytes.length()]);
    RetainPtr response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:[data.get() length] textEncodingName:nil]);
    [task didReceiveResponse:response.get()];
    [task didReceiveData:data.get()];
    [task didFinish];
}

- (void)webView:(WKWebView *)webView stopURLSchemeTask:(id<WKURLSchemeTask>)task
{
}

@end

namespace TestWebKitAPI {

enum class SandboxTopLevelDocument : bool { No, Yes };

static void runDataURLSharedWorkerInSandboxedDocumentTest(SandboxTopLevelDocument sandboxTopLevelDocument)
{
    HTTPServer::ResponseMap responses;
    HTTPResponse sandboxedResponse { { { "Content-Type"_s, "text/html"_s }, { "Content-Security-Policy"_s, "sandbox allow-scripts"_s } }, dataURLSharedWorkerBytes };
    if (sandboxTopLevelDocument == SandboxTopLevelDocument::Yes)
        responses.add("/main.html"_s, WTF::move(sandboxedResponse));
    else {
        responses.add("/main.html"_s, HTTPResponse { sandboxedFrameHostBytes });
        responses.add("/sandboxed-frame.html"_s, WTF::move(sandboxedResponse));
    }
    HTTPServer server(WTF::move(responses));

    RetainPtr dataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    RetainPtr dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]);
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:dataStore.get()];

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    // Written through a pointer rather than declared __block, because the wait below reads it from a C++
    // lambda and those cannot capture __block variables.
    bool webContentProcessTerminated = false;
    auto* terminatedFlag = &webContentProcessTerminated;
    [navigationDelegate setWebContentProcessDidTerminate:^(WKWebView *, _WKProcessTerminationReason) {
        *terminatedFlag = true;
    }];

    [webView loadRequest:server.request("/main.html"_s)];
    [navigationDelegate waitForDidFinishNavigation];

    auto webProcessBeforeWorker = [webView _webProcessIdentifier];
    EXPECT_NE(0, webProcessBeforeWorker);

    EXPECT_TRUE(Util::waitFor([&] {
        return webContentProcessTerminated || [[webView stringByEvaluatingJavaScript:@"window.workerResult || ''"] isEqualToString:@"connected"];
    }, 100));

    // Validating the claimed top origin must not terminate a document that legitimately has an opaque one.
    // Stop here: once the process is gone the two assertions below only restate this one.
    ASSERT_FALSE(webContentProcessTerminated);
    EXPECT_EQ(webProcessBeforeWorker, [webView _webProcessIdentifier]);
    EXPECT_WK_STREQ("connected", [webView stringByEvaluatingJavaScript:@"window.workerResult || ''"]);
}

// The top origin of a sandboxed subframe is still the main frame's real origin, so this exercises an opaque
// client origin with a validatable top origin.
TEST(SharedWorkerProcessPlacement, SandboxedFrameCanStartDataURLSharedWorker)
{
    runDataURLSharedWorkerInSandboxedDocumentTest(SandboxTopLevelDocument::No);
}

// Sandboxing the top-level document makes Document::topOrigin() itself opaque, which is the case a
// site-granular check on the claimed top origin cannot express.
TEST(SharedWorkerProcessPlacement, SandboxedTopLevelDocumentCanStartDataURLSharedWorker)
{
    runDataURLSharedWorkerInSandboxedDocumentTest(SandboxTopLevelDocument::Yes);
}


// A custom scheme handled by a WKURLSchemeHandler is not opaque either, but it reaches that through a
// different branch of SecurityOriginData::shouldTreatAsOpaqueOrigin than file: does, and a URL with no
// authority still leaves the top origin with no registrable domain.
TEST(SharedWorkerProcessPlacement, CustomSchemeDocumentWithoutHostCanStartSharedWorker)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr schemeHandler = adoptNS([SharedWorkerHostSchemeHandler new]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"shared-worker-host"];

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    bool webContentProcessTerminated = false;
    auto* terminatedFlag = &webContentProcessTerminated;
    [navigationDelegate setWebContentProcessDidTerminate:^(WKWebView *, _WKProcessTerminationReason) {
        *terminatedFlag = true;
    }];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"shared-worker-host:///main.html"]]];
    [navigationDelegate waitForDidFinishNavigation];

    auto webProcessBeforeWorker = [webView _webProcessIdentifier];
    EXPECT_NE(0, webProcessBeforeWorker);

    EXPECT_TRUE(Util::waitFor([&] {
        return webContentProcessTerminated || [[webView stringByEvaluatingJavaScript:@"window.workerResult || ''"] isEqualToString:@"connected"];
    }, 100));

    // A top origin with no registrable domain is not a forged one, and must not be treated as one.
    ASSERT_FALSE(webContentProcessTerminated);
    EXPECT_EQ(webProcessBeforeWorker, [webView _webProcessIdentifier]);
    EXPECT_WK_STREQ("connected", [webView stringByEvaluatingJavaScript:@"window.workerResult || ''"]);
}

} // namespace TestWebKitAPI
