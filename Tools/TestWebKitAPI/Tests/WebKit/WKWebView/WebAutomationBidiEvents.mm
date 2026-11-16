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

#if ENABLE(REMOTE_INSPECTOR) && ENABLE(WEBDRIVER_BIDI)

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKAutomationSession.h>
#import <WebKit/_WKAutomationSessionConfiguration.h>
#import <WebKit/_WKAutomationSessionPrivateForTesting.h>
#import <WebKit/_WKFeature.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

static NSString *jsonString(id object)
{
    return adoptNS([[NSString alloc] initWithData:[NSJSONSerialization dataWithJSONObject:object options:0 error:nil] encoding:NSUTF8StringEncoding]).autorelease();
}

static id jsonObject(NSString *string)
{
    return [NSJSONSerialization JSONObjectWithData:[string dataUsingEncoding:NSUTF8StringEncoding] options:0 error:nil];
}

class WebAutomationBidiEvents : public testing::Test {
public:
    void SetUp() override
    {
        RetainPtr sessionConfiguration = adoptNS([_WKAutomationSessionConfiguration new]);
        m_session = adoptNS([[_WKAutomationSession alloc] initWithConfiguration:sessionConfiguration.get()]);
        [m_session setSessionIdentifier:@"BidiEventsTest"];

        RetainPtr processPoolConfiguration = adoptNS([_WKProcessPoolConfiguration new]);
        [processPoolConfiguration setUsesSingleWebProcess:usesSingleWebProcess()];
        m_processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);
        [m_processPool _setAutomationSession:m_session.get()];

        // The session sends BiDi responses and events to the driver as Automation.bidiMessageSent events.
        m_bidiMessages = adoptNS([NSMutableArray new]);
        [m_session _setMessageToFrontendHandlerForTesting:^(NSString *message) {
            NSDictionary *automationMessage = jsonObject(message);
            if (![automationMessage[@"method"] isEqualToString:@"Automation.bidiMessageSent"])
                return;
            [m_bidiMessages addObject:jsonObject(automationMessage[@"params"][@"message"])];
        }];
    }

    void TearDown() override
    {
        [m_session _setMessageToFrontendHandlerForTesting:nil];
        [m_processPool _setAutomationSession:nil];
    }

    // One web process hosts both kinds of pages. It sends its messages to the UI process in order, so once an event for
    // the page that is controlled by automation arrives, the messages that the other page sent before have been handled
    // too.
    virtual bool usesSingleWebProcess() const { return true; }

    RetainPtr<TestWKWebView> createWebView(bool controlledByAutomation, bool enablesIPCTestingAPI = false)
    {
        RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
        [configuration setProcessPool:m_processPool.get()];
        [configuration _setControlledByAutomation:controlledByAutomation];
        if (enablesIPCTestingAPI) {
            for (_WKFeature *feature in [WKPreferences _features]) {
                if ([feature.key isEqualToString:@"IPCTestingAPIEnabled"])
                    [[configuration preferences] _setEnabled:YES forFeature:feature];
            }
        }
        return adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    }

    // Returns the BiDi response, which holds either a "result" or an "error".
    NSDictionary *sendBidiCommand(NSString *method, NSDictionary *parameters)
    {
        NSNumber *commandID = @(++m_lastCommandID);
        NSString *bidiCommand = jsonString(@{ @"id": commandID, @"method": method, @"params": parameters });
        [m_session _dispatchMessageFromRemoteForTesting:jsonString(@{ @"id": commandID, @"method": @"Automation.processBidiMessage", @"params": @{ @"message": bidiCommand } })];

        NSDictionary *response = nil;
        Util::waitFor([&] {
            for (NSDictionary *message in m_bidiMessages.get()) {
                if ([message[@"id"] isEqual:commandID])
                    response = message;
            }
            return !!response;
        });
        return response;
    }

    RetainPtr<NSArray<NSDictionary *>> eventParameters(NSString *eventName)
    {
        RetainPtr<NSMutableArray<NSDictionary *>> parameters = adoptNS([NSMutableArray new]);
        for (NSDictionary *message in m_bidiMessages.get()) {
            if ([message[@"method"] isEqualToString:eventName])
                [parameters addObject:message[@"params"]];
        }
        return parameters;
    }

    RetainPtr<_WKAutomationSession> m_session;
    RetainPtr<WKProcessPool> m_processPool;
    RetainPtr<NSMutableArray<NSDictionary *>> m_bidiMessages;
    unsigned m_lastCommandID { 0 };
};

// Automation is installed on a process pool, and every web process in the pool reports realms and console messages,
// including the ones of pages that aren't controlled by automation, such as pages that the browser's user opened.
// Those must not reach the automation client, neither when they happen, nor when session.subscribe reports the realms
// that already exist.
TEST_F(WebAutomationBidiEvents, ReportsOnlyPagesControlledByAutomation)
{
    RetainPtr controlledWebView = createWebView(true);
    RetainPtr<NSString> controlledContext = [m_session _registerWebViewForTesting:controlledWebView.get()];
    RetainPtr uncontrolledWebView = createWebView(false);

    [uncontrolledWebView synchronouslyLoadHTMLString:@"<script>console.log('not controlled, before subscribing')</script>"];
    [controlledWebView synchronouslyLoadHTMLString:@"<script>console.log('controlled, before subscribing')</script>"];

    NSDictionary *response = sendBidiCommand(@"session.subscribe", @{ @"events": @[ @"log.entryAdded", @"script.realmCreated" ] });
    ASSERT_NOT_NULL(response);
    EXPECT_WK_STREQ("success", (NSString *)response[@"type"]);

    // session.subscribe reported the realm of the page that is controlled by automation.
    EXPECT_EQ(1u, [eventParameters(@"script.realmCreated") count]);

    [uncontrolledWebView synchronouslyLoadHTMLString:@"<script>console.log('not controlled')</script>"];
    [controlledWebView synchronouslyLoadHTMLString:@"<script>console.log('controlled')</script>"];

    RetainPtr logEntries = eventParameters(@"log.entryAdded");
    EXPECT_EQ(1u, [logEntries count]);
    for (NSDictionary *logEntry in logEntries.get())
        EXPECT_WK_STREQ("controlled", (NSString *)logEntry[@"text"]);

    RetainPtr realms = eventParameters(@"script.realmCreated");
    EXPECT_GE([realms count], 2u);
    for (NSDictionary *realm in realms.get())
        EXPECT_WK_STREQ(controlledContext.get(), (NSString *)realm[@"context"]);
}

#if ENABLE(IPC_TESTING_API)
// Each kind of page has a web process of its own, and the IPC testing API lets the other page send any message.
class WebAutomationBidiEventsInSeparateProcesses : public WebAutomationBidiEvents {
    bool usesSingleWebProcess() const final { return false; }
};

// A compromised web process that hosts no frame of a page that is controlled by automation can send console messages that
// name a frame of that page, since frame identifiers are easy to guess. Those messages must not reach the automation
// client. Realm notifications are checked the same way.
TEST_F(WebAutomationBidiEventsInSeparateProcesses, IgnoresMessagesAboutFramesOfOtherProcesses)
{
    RetainPtr controlledWebView = createWebView(true, true);
    RetainPtr uncontrolledWebView = createWebView(false, true);

    [controlledWebView synchronouslyLoadHTMLString:@"" baseURL:[NSURL URLWithString:@"https://controlled.example/"]];
    [uncontrolledWebView synchronouslyLoadHTMLString:@"" baseURL:[NSURL URLWithString:@"https://uncontrolled.example/"]];
    ASSERT_NE([controlledWebView _webProcessIdentifier], [uncontrolledWebView _webProcessIdentifier]);

    NSDictionary *response = sendBidiCommand(@"session.subscribe", @{ @"events": @[ @"log.entryAdded" ] });
    ASSERT_NOT_NULL(response);
    EXPECT_WK_STREQ("success", (NSString *)response[@"type"]);

    // The other page's process names the frame of the page that is controlled by automation. It sends the reply to this
    // script evaluation after the message, so the UI process has handled the message when the evaluation ends.
    NSString *frameID = [controlledWebView stringByEvaluatingJavaScript:@"String(IPC.frameID)"];
    [uncontrolledWebView stringByEvaluatingJavaScript:[NSString stringWithFormat:@
        "IPC.sendMessage('UI', 0, IPC.messages.WebAutomationSession_LogEntryAdded.name, [{ type: 'FrameID', value: [BigInt('%@')] },"
        "    { type: 'uint8_t', value: 3 }, { type: 'uint8_t', value: 0 }, { type: 'String', value: 'made up' }, { type: 'uint8_t', value: 0 },"
        "    { type: 'double', value: 0 }]);"
        "'sent'", frameID]];

    [controlledWebView stringByEvaluatingJavaScript:@"console.log('controlled'); 'logged'"];

    RetainPtr logEntries = eventParameters(@"log.entryAdded");
    EXPECT_EQ(1u, [logEntries count]);
    for (NSDictionary *logEntry in logEntries.get())
        EXPECT_WK_STREQ("controlled", (NSString *)logEntry[@"text"]);
}
#endif // ENABLE(IPC_TESTING_API)

} // namespace TestWebKitAPI

#endif // ENABLE(REMOTE_INSPECTOR) && ENABLE(WEBDRIVER_BIDI)
