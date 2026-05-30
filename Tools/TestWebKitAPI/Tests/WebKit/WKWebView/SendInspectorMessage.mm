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

#import "Helpers/Utilities.h"
#import "Helpers/cocoa/TestCocoa.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/_WKAutomationSession.h>
#import <WebKit/_WKAutomationSessionConfiguration.h>
#import <WebKit/_WKAutomationSessionDelegate.h>
#import <WebKit/_WKAutomationSessionPrivateForTesting.h>
#import <wtf/RetainPtr.h>

// A delegate that lets each test decide whether to grant the inspector-testing
// capability. The other delegate methods are no-ops; tests focus on driving
// Automation messages directly.
@interface TestSendInspectorMessageDelegate : NSObject <_WKAutomationSessionDelegate>
@property (nonatomic) BOOL allowInspectorTesting;
@end

@implementation TestSendInspectorMessageDelegate
- (BOOL)_automationSessionShouldEnableInspectorTesting:(_WKAutomationSession *)automationSession
{
    return _allowInspectorTesting;
}
@end

namespace {

// Build the JSON wire string for an Automation.sendInspectorMessage outer
// envelope wrapping an Inspector JSON-RPC inner message.
RetainPtr<NSString> makeSendInspectorMessageEnvelope(NSInteger outerId, NSString *handle, NSInteger innerId, NSString *method, NSDictionary *params)
{
    RetainPtr inner = adoptNS([[NSMutableDictionary alloc] init]);
    [inner setObject:@(innerId) forKey:@"id"];
    [inner setObject:method forKey:@"method"];
    [inner setObject:(params ?: @{ }) forKey:@"params"];

    NSError *innerError = nil;
    NSData *innerData = [NSJSONSerialization dataWithJSONObject:inner.get() options:0 error:&innerError];
    EXPECT_NULL(innerError);
    RetainPtr innerMessage = adoptNS([[NSString alloc] initWithData:innerData encoding:NSUTF8StringEncoding]);

    RetainPtr outer = adoptNS([[NSMutableDictionary alloc] init]);
    [outer setObject:@(outerId) forKey:@"id"];
    [outer setObject:@"Automation.sendInspectorMessage" forKey:@"method"];
    [outer setObject:@{
        @"browsingContextHandle": handle ?: @"",
        @"message": innerMessage.get()
    } forKey:@"params"];

    NSError *outerError = nil;
    NSData *outerData = [NSJSONSerialization dataWithJSONObject:outer.get() options:0 error:&outerError];
    EXPECT_NULL(outerError);
    return adoptNS([[NSString alloc] initWithData:outerData encoding:NSUTF8StringEncoding]);
}

NSDictionary *decode(NSString *jsonString)
{
    NSData *data = [jsonString dataUsingEncoding:NSUTF8StringEncoding];
    NSError *err = nil;
    NSDictionary *obj = [NSJSONSerialization JSONObjectWithData:data options:0 error:&err];
    if (![obj isKindOfClass:[NSDictionary class]])
        return nil;
    return obj;
}

// Fixture: load about:blank in a TestWKWebView and create a paired
// _WKAutomationSession with a captured outbound-message buffer.
struct SessionFixture {
    RetainPtr<TestWKWebView> webView;
    RetainPtr<_WKAutomationSession> session;
    RetainPtr<TestSendInspectorMessageDelegate> delegate;
    RetainPtr<NSString> handle;
    RetainPtr<NSMutableArray<NSString *>> captured;
    bool ready { false };
};

void setUpSessionFixture(SessionFixture& fx, BOOL allowInspectorTesting)
{
    fx.webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 240)]);
    [fx.webView synchronouslyLoadHTMLString:@"<html><head><title>SendInspectorMessage Test</title></head><body></body></html>" baseURL:[NSURL URLWithString:@"about:blank"]];

    RetainPtr config = adoptNS([_WKAutomationSessionConfiguration new]);
    fx.session = adoptNS([[_WKAutomationSession alloc] initWithConfiguration:config.get()]);

    fx.delegate = adoptNS([TestSendInspectorMessageDelegate new]);
    fx.delegate.get().allowInspectorTesting = allowInspectorTesting;
    [fx.session setDelegate:fx.delegate.get()];

    auto processPool = [fx.webView.get().configuration processPool];
    [processPool _setAutomationSession:fx.session.get()];

    fx.handle = [fx.session _registerWebViewForTesting:fx.webView.get()];
    EXPECT_GT([fx.handle.get() length], 0u);

    fx.captured = adoptNS([NSMutableArray new]);
    NSMutableArray<NSString *> *captured = fx.captured.get();
    [fx.session _setMessageToFrontendHandlerForTesting:^(NSString *message) {
        [captured addObject:message];
    }];

    fx.ready = true;
}

NSDictionary *findMessageWithOuterId(NSArray<NSString *> *captured, NSInteger outerId)
{
    for (NSString *raw in captured) {
        NSDictionary *parsed = decode(raw);
        NSNumber *idValue = parsed[@"id"];
        if ([idValue isKindOfClass:[NSNumber class]] && [idValue integerValue] == outerId)
            return parsed;
    }
    return nil;
}

NSDictionary *findEvent(NSArray<NSString *> *captured, NSString *eventName)
{
    for (NSString *raw in captured) {
        NSDictionary *parsed = decode(raw);
        if (parsed[@"id"])
            continue;
        if ([parsed[@"method"] isEqualToString:eventName])
            return parsed;
    }
    return nil;
}

// Walk Automation.receiveInspectorMessage events and return the one whose inner
// payload has the matching response id. The backend will deliver unrelated
// Inspector notifications (e.g. Target.targetCreated) on the same channel, so
// matching the inner id is required to pick the response that completes the
// command we sent.
NSDictionary *findReceiveEventWithInnerId(NSArray<NSString *> *captured, NSInteger innerId)
{
    for (NSString *raw in captured) {
        NSDictionary *parsed = decode(raw);
        if (parsed[@"id"])
            continue;
        if (![parsed[@"method"] isEqualToString:@"Automation.receiveInspectorMessage"])
            continue;
        NSString *innerJSON = parsed[@"params"][@"message"];
        if (![innerJSON isKindOfClass:[NSString class]])
            continue;
        NSDictionary *inner = decode(innerJSON);
        NSNumber *idValue = inner[@"id"];
        if ([idValue isKindOfClass:[NSNumber class]] && [idValue integerValue] == innerId)
            return parsed;
    }
    return nil;
}

} // namespace

// Happy path: outer Automation.sendInspectorMessage carrying a Target.* command
// (one of the few commands available on the top-level Inspector dispatcher
// without prior Target attach handshake) should round-trip and produce both a
// success response for the outer command and an Automation.receiveInspectorMessage
// event carrying the inner Target response.
TEST(AutomationSendInspectorMessage, RoundTripTargetSetPauseOnStart)
{
    SessionFixture fx;
    setUpSessionFixture(fx, /*allowInspectorTesting=*/ YES);

    auto envelope = makeSendInspectorMessageEnvelope(1, fx.handle.get(), 99, @"Target.setPauseOnStart", @{ @"pauseOnStart": @NO });
    [fx.session _dispatchMessageFromRemoteForTesting:envelope.get()];

    // Pump the runloop until both an outer response and the inner-carrying
    // event have been captured, or a hard timeout fires.
    __block NSMutableArray<NSString *> *captured = fx.captured.get();
    TestWebKitAPI::Util::runFor(0.05_s);
    NSInteger spin = 0;
    while (spin++ < 200 && (!findMessageWithOuterId(captured, 1) || !findReceiveEventWithInnerId(captured, 99)))
        TestWebKitAPI::Util::runFor(0.05_s);

    NSDictionary *outerResponse = findMessageWithOuterId(captured, 1);
    ASSERT_TRUE(outerResponse != nil);
    EXPECT_NULL(outerResponse[@"error"]);

    NSDictionary *event = findReceiveEventWithInnerId(captured, 99);
    ASSERT_TRUE(event != nil);
    NSDictionary *params = event[@"params"];
    EXPECT_TRUE([params[@"browsingContextHandle"] isEqualToString:fx.handle.get()]);
    NSString *innerMessage = params[@"message"];
    EXPECT_TRUE([innerMessage isKindOfClass:[NSString class]]);

    NSDictionary *inner = decode(innerMessage);
    EXPECT_TRUE([inner[@"id"] isEqualToNumber:@99]);
    EXPECT_NULL(inner[@"error"]);
    NSDictionary *innerResult = inner[@"result"];
    EXPECT_TRUE([innerResult isKindOfClass:[NSDictionary class]]);
}

// Gate closed: outer command should fail with a JSON-RPC error and emit
// no Automation.receiveInspectorMessage event. The InspectorPassthroughChannel
// is never created.
TEST(AutomationSendInspectorMessage, DeniedWhenGateReturnsFalse)
{
    SessionFixture fx;
    setUpSessionFixture(fx, /*allowInspectorTesting=*/ NO);

    auto envelope = makeSendInspectorMessageEnvelope(2, fx.handle.get(), 99, @"Page.getResourceTree", nil);
    [fx.session _dispatchMessageFromRemoteForTesting:envelope.get()];

    __block NSMutableArray<NSString *> *captured = fx.captured.get();
    NSInteger spin = 0;
    while (spin++ < 60 && !findMessageWithOuterId(captured, 2))
        TestWebKitAPI::Util::runFor(0.05_s);

    NSDictionary *outerResponse = findMessageWithOuterId(captured, 2);
    ASSERT_TRUE(outerResponse != nil);
    EXPECT_NOT_NULL(outerResponse[@"error"]);
    EXPECT_NULL(findEvent(captured, @"Automation.receiveInspectorMessage"));
}

// Unknown handle: outer command should fail with WindowNotFound and emit no
// event. Exercises the page-lookup branch of sendInspectorMessage.
TEST(AutomationSendInspectorMessage, WindowNotFoundForUnknownHandle)
{
    SessionFixture fx;
    setUpSessionFixture(fx, /*allowInspectorTesting=*/ YES);

    auto envelope = makeSendInspectorMessageEnvelope(3, @"this-handle-does-not-exist", 99, @"Page.getResourceTree", nil);
    [fx.session _dispatchMessageFromRemoteForTesting:envelope.get()];

    __block NSMutableArray<NSString *> *captured = fx.captured.get();
    NSInteger spin = 0;
    while (spin++ < 60 && !findMessageWithOuterId(captured, 3))
        TestWebKitAPI::Util::runFor(0.05_s);

    NSDictionary *outerResponse = findMessageWithOuterId(captured, 3);
    ASSERT_TRUE(outerResponse != nil);
    EXPECT_NOT_NULL(outerResponse[@"error"]);
    EXPECT_NULL(findEvent(captured, @"Automation.receiveInspectorMessage"));
}

#endif // ENABLE(REMOTE_INSPECTOR)
