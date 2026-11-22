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

#if ENABLE(REMOTE_INSPECTOR)

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Utilities.h"
#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKFrameInfoPrivate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKAutomationSession.h>
#import <WebKit/_WKAutomationSessionConfiguration.h>
#import <WebKit/_WKAutomationSessionDelegate.h>
#import <WebKit/_WKAutomationSessionPrivateForTesting.h>
#import <WebKit/_WKFeature.h>
#import <WebKit/_WKFrameTreeNode.h>
#import <signal.h>
#import <wtf/RetainPtr.h>

// CC> This file is the cross-process harness for the Web Inspector proxying agents. Under Site
// CC> Isolation, UIProcess-side ProxyingNetworkAgent / ProxyingPageAgent forward several protocol
// CC> commands to a WebContent peer over an async-reply IPC. When that peer dies, IPC synthesizes
// CC> the reply from AsyncReplyError; a bare-errorString reply then reads as an empty errorString,
// CC> which the UIProcess handler misreads as SUCCESS. Converting the reply to Expected<T, String>
// CC> makes the three states distinguishable: has_value() = success, empty error() = connection
// CC> loss, non-empty error() = a real backend failure.
// CC>
// CC> The cases below drive Network.getResponseBody, whose Expected<std::pair<String, bool>, String>
// CC> conversion has ALREADY LANDED. They are a NEGATIVE CONTROL / harness proof, NOT a RED-first
// CC> deliverable: getResponseBody cannot go RED here. Their job is to prove this harness really
// CC> crosses a process boundary and really distinguishes a success reply from a connection-loss
// CC> reply, so that later missions can add RED-first cases for the reply shapes that are not yet
// CC> converted.

@interface TestInspectorAutomationDelegate : NSObject <_WKAutomationSessionDelegate>
@end

@implementation TestInspectorAutomationDelegate

- (BOOL)_automationSessionShouldEnableInspectorTesting:(_WKAutomationSession *)automationSession
{
    return YES;
}

@end

namespace TestWebKitAPI {

static void enableFeature(WKWebViewConfiguration *configuration, NSString *featureName)
{
    WKPreferences *preferences = [configuration preferences];
    for (_WKFeature *feature in [WKPreferences _features]) {
        if ([feature.key isEqualToString:featureName]) {
            [preferences _setEnabled:YES forFeature:feature];
            break;
        }
    }
}

static void enableSiteIsolation(WKWebViewConfiguration *configuration)
{
    enableFeature(configuration, @"SiteIsolationEnabled");
}

static NSString *serializeJSON(NSDictionary *object)
{
    NSData *data = [NSJSONSerialization dataWithJSONObject:object options:0 error:nil];
    if (!data)
        return nil;
    return adoptNS([[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding]).autorelease();
}

static NSDictionary *parseJSON(NSString *string)
{
    if (!string)
        return nil;
    id object = [NSJSONSerialization JSONObjectWithData:[string dataUsingEncoding:NSUTF8StringEncoding] options:0 error:nil];
    if (![object isKindOfClass:NSDictionary.class])
        return nil;
    return object;
}

// The Automation transport wraps an Inspector-protocol command in an
// Automation.sendInspectorMessage envelope; the backend reply comes back out as an
// Automation.receiveInspectorMessage event carrying the inner JSON as a string.
static NSString *sendInspectorMessageEnvelope(NSUInteger outerID, NSString *handle, NSUInteger innerID, NSString *method, NSDictionary *params)
{
    NSString *innerMessage = serializeJSON(@{
        @"id": @(innerID),
        @"method": method,
        @"params": params ?: @{ },
    });
    return serializeJSON(@{
        @"id": @(outerID),
        @"method": @"Automation.sendInspectorMessage",
        @"params": @{
            @"browsingContextHandle": handle,
            @"message": innerMessage,
        },
    });
}

static NSDictionary *innerMessageFromCapturedMessage(NSString *rawMessage)
{
    NSDictionary *parsed = parseJSON(rawMessage);
    if (![parsed[@"method"] isEqualToString:@"Automation.receiveInspectorMessage"])
        return nil;
    NSDictionary *params = parsed[@"params"];
    if (![params isKindOfClass:NSDictionary.class])
        return nil;
    return parseJSON(params[@"message"]);
}

static NSDictionary *findInspectorReply(NSArray<NSString *> *captured, NSUInteger innerID)
{
    for (NSString *rawMessage in captured) {
        NSDictionary *inner = innerMessageFromCapturedMessage(rawMessage);
        NSNumber *identifier = inner[@"id"];
        if ([identifier isKindOfClass:NSNumber.class] && identifier.unsignedIntegerValue == innerID)
            return inner;
    }
    return nil;
}

static NSDictionary *findInspectorEvent(NSArray<NSString *> *captured, NSString *eventName, NSString *requestID)
{
    for (NSString *rawMessage in captured) {
        NSDictionary *inner = innerMessageFromCapturedMessage(rawMessage);
        if (![inner[@"method"] isEqualToString:eventName])
            continue;
        NSDictionary *params = inner[@"params"];
        if (![params isKindOfClass:NSDictionary.class])
            continue;
        if (requestID && ![params[@"requestId"] isEqualToString:requestID])
            continue;
        return inner;
    }
    return nil;
}

// The protocol requestId encodes the WebContent process that served the load, which is how
// ProxyingNetworkAgent routes getResponseBody back to that peer. Matching on the URL is enough to
// pick out the iframe's request, because only the cross-origin iframe fetches this path.
static NSString *findRequestIDForURLSuffix(NSArray<NSString *> *captured, NSString *suffix)
{
    for (NSString *rawMessage in captured) {
        NSDictionary *inner = innerMessageFromCapturedMessage(rawMessage);
        if (![inner[@"method"] isEqualToString:@"Network.requestWillBeSent"])
            continue;
        NSDictionary *params = inner[@"params"];
        if (![params isKindOfClass:NSDictionary.class])
            continue;
        NSDictionary *request = params[@"request"];
        if (![request isKindOfClass:NSDictionary.class])
            continue;
        NSString *url = request[@"url"];
        if ([url isKindOfClass:NSString.class] && [url hasSuffix:suffix])
            return params[@"requestId"];
    }
    return nil;
}

template<typename PredicateType> static bool waitUntil(PredicateType&& predicate)
{
    for (unsigned i = 0; i < 400; ++i) {
        if (predicate())
            return true;
        Util::runFor(0.025_s);
    }
    return predicate();
}

static constexpr auto childSubresourcePath = "/child-subresource.txt"_s;
static constexpr auto childSubresourceBody = "cross-process-response-body"_s;

// Holds the Site-Isolated cross-process web view plus the Automation session wired up to observe
// Inspector backend replies. Everything here is deliberately a real cross-process standup: an
// in-process mock would not reach the proxying agents at all.
struct ProxyingAgentFixture {
    RetainPtr<TestWKWebView> webView;
    RetainPtr<TestNavigationDelegate> navigationDelegate;
    RetainPtr<TestInspectorAutomationDelegate> automationDelegate;
    RetainPtr<_WKAutomationSession> session;
    RetainPtr<NSMutableArray<NSString *>> capturedMessages;
    RetainPtr<NSString> handle;
    pid_t mainFramePid { 0 };
    pid_t childFramePid { 0 };

    void dispatch(NSUInteger outerID, NSUInteger innerID, NSString *method, NSDictionary *params) const
    {
        [session _dispatchMessageFromRemoteForTesting:sendInspectorMessageEnvelope(outerID, handle.get(), innerID, method, params)];
    }

    NSDictionary *waitForReply(NSUInteger innerID) const
    {
        RetainPtr messages = capturedMessages;
        waitUntil([&] {
            return !!findInspectorReply(messages.get(), innerID);
        });
        return findInspectorReply(capturedMessages.get(), innerID);
    }
};

static HTTPServer makeServer()
{
    return HTTPServer({
        { "/example"_s, HTTPResponse("<iframe src='https://webkit.org/iframe'></iframe>"_s) },
        { "/iframe"_s, HTTPResponse("<body>iframe</body>"_s) },
        { childSubresourcePath, HTTPResponse(HashMap<String, String> { { "Content-Type"_s, "text/plain"_s } }, childSubresourceBody) },
    }, HTTPServer::Protocol::HttpsProxy);
}

// Stands up the fixture and asserts the anti-vacuous cross-process precondition: the cross-origin
// iframe must live in a DIFFERENT WebContent process from the main frame. Without that, the
// proxied command never crosses a process boundary and the whole harness would be vacuous.
static ProxyingAgentFixture standUpCrossProcessFixture(const HTTPServer& server)
{
    ProxyingAgentFixture fixture;

    RetainPtr<WKWebViewConfiguration> configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration.get());

    fixture.navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [fixture.navigationDelegate allowAnyTLSCertificate];

    fixture.webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    [fixture.webView setNavigationDelegate:fixture.navigationDelegate.get()];

    [fixture.webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [fixture.navigationDelegate waitForDidFinishNavigation];

    RetainPtr<TestWKWebView> webView = fixture.webView;
    waitUntil([&] {
        return [webView mainFrame].childFrames.count > 0;
    });

    fixture.mainFramePid = [fixture.webView mainFrame].info._processIdentifier;
    fixture.childFramePid = [fixture.webView firstChildFrame]._processIdentifier;
    EXPECT_NE(fixture.mainFramePid, 0);
    EXPECT_NE(fixture.childFramePid, 0);
    EXPECT_NE(fixture.mainFramePid, fixture.childFramePid);

    fixture.automationDelegate = adoptNS([TestInspectorAutomationDelegate new]);
    RetainPtr<_WKAutomationSessionConfiguration> sessionConfiguration = adoptNS([_WKAutomationSessionConfiguration new]);
    fixture.session = adoptNS([[_WKAutomationSession alloc] initWithConfiguration:sessionConfiguration.get()]);
    [fixture.session setDelegate:fixture.automationDelegate.get()];
    [[configuration processPool] _setAutomationSession:fixture.session.get()];

    fixture.capturedMessages = adoptNS([NSMutableArray new]);
    RetainPtr<NSMutableArray<NSString *>> capturedMessages = fixture.capturedMessages;
    [fixture.session _setMessageToFrontendHandlerForTesting:^(NSString *message) {
        [capturedMessages addObject:message];
    }];

    fixture.handle = [fixture.session _registerWebViewForTesting:fixture.webView.get()];
    EXPECT_GT([fixture.handle length], 0u);

    return fixture;
}

// Turns on Network instrumentation in every WebContent process of the page, then makes the
// cross-origin iframe fetch a subresource so a requestId owned by the IFRAME's process exists.
// Returns that requestId, or nil if the load was never observed.
static RetainPtr<NSString> loadSubresourceInChildFrameAndWaitForRequestID(const ProxyingAgentFixture& fixture)
{
    fixture.dispatch(1, 100, @"Network.enable", nil);
    EXPECT_NOT_NULL(fixture.waitForReply(100));

    [fixture.webView evaluateJavaScript:@"fetch('/child-subresource.txt').then((response) => response.text())" inFrame:[fixture.webView firstChildFrame] completionHandler:nil];

    RetainPtr<NSMutableArray<NSString *>> capturedMessages = fixture.capturedMessages;
    RetainPtr<NSString> requestID;
    waitUntil([&] {
        requestID = findRequestIDForURLSuffix(capturedMessages.get(), @"/child-subresource.txt");
        if (!requestID)
            return false;
        // Wait for loadingFinished so the peer's resource-data store has decoded the body;
        // otherwise getResponseBody could legitimately fail with a "missing content" error and the
        // success case would be testing the wrong thing.
        return !!findInspectorEvent(capturedMessages.get(), @"Network.loadingFinished", requestID.get());
    });
    return requestID;
}

// CC> Negative control (harness proof): with the iframe's WebContent peer alive, the
// CC> already-converted Expected<std::pair<String, bool>, String> reply carries a value, so the
// CC> proxied command succeeds and the frontend sees a result with the body -- no error.
TEST(WebInspectorProxyingAgentReply, CrossProcessGetResponseBodySucceeds)
{
    HTTPServer server = makeServer();
    auto fixture = standUpCrossProcessFixture(server);

    RetainPtr<NSString> requestID = loadSubresourceInChildFrameAndWaitForRequestID(fixture);
    EXPECT_NOT_NULL(requestID.get());
    if (!requestID)
        return;

    fixture.dispatch(2, 200, @"Network.getResponseBody", @{ @"requestId": requestID.get() });
    NSDictionary *reply = fixture.waitForReply(200);
    EXPECT_NOT_NULL(reply);
    if (!reply)
        return;

    EXPECT_NULL(reply[@"error"]);
    NSDictionary *result = reply[@"result"];
    EXPECT_TRUE([result isKindOfClass:NSDictionary.class]);
    EXPECT_WK_STREQ("cross-process-response-body", result[@"body"]);
    EXPECT_FALSE([result[@"base64Encoded"] boolValue]);
}

// CC> Negative control (harness proof): kill the iframe's WebContent peer and re-issue the SAME
// CC> proxied command. IPC synthesizes the reply via AsyncReplyError, which for Expected<T, String>
// CC> means an unexpected value carrying an EMPTY error string. ProxyingNetworkAgent must translate
// CC> that into an explicit failure -- if the reply shape were a bare errorString, the empty string
// CC> would read as success and the frontend would be handed an empty body for a dead process.
TEST(WebInspectorProxyingAgentReply, CrossProcessGetResponseBodyReportsConnectionLoss)
{
    HTTPServer server = makeServer();
    auto fixture = standUpCrossProcessFixture(server);

    RetainPtr<NSString> requestID = loadSubresourceInChildFrameAndWaitForRequestID(fixture);
    EXPECT_NOT_NULL(requestID.get());
    if (!requestID)
        return;

    // Kill the iframe's peer and dispatch in the same run loop turn, before the UIProcess has
    // processed the connection teardown. That is what forces the reply down the AsyncReplyError
    // path rather than the "WebProcess not found for requestId" pre-check.
    kill(fixture.childFramePid, SIGKILL);
    fixture.dispatch(3, 300, @"Network.getResponseBody", @{ @"requestId": requestID.get() });

    NSDictionary *reply = fixture.waitForReply(300);
    EXPECT_NOT_NULL(reply);
    if (!reply)
        return;

    EXPECT_NULL(reply[@"result"]);
    NSDictionary *error = reply[@"error"];
    EXPECT_TRUE([error isKindOfClass:NSDictionary.class]);
    EXPECT_WK_STREQ("Target WebProcess for requestId is no longer available", error[@"message"]);
}

} // namespace TestWebKitAPI

#endif // ENABLE(REMOTE_INSPECTOR)
