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
#import "Helpers/Test.h"
#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/cocoa/SiteIsolationTestUtilities.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKFrameInfoPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKAutomationSession.h>
#import <WebKit/_WKAutomationSessionConfiguration.h>
#import <WebKit/_WKAutomationSessionPrivateForTesting.h>
#import <WebKit/_WKFrameHandle.h>
#import <WebKit/_WKFrameTreeNode.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

static constexpr auto sessionIdentifier = "AccessibilityTest";

static constexpr auto mainFrameHTML = "<body><button id='main' aria-label='Main button'>main</button><button id='toggle' aria-pressed='true'>toggle</button><input type='checkbox' id='checkbox' checked><div id='ariaCheckbox' role='checkbox' aria-checked='mixed' aria-label='ARIA checkbox'></div><div id='hidden' hidden>hidden</div><div id='ariaHidden' aria-hidden='true'>aria hidden</div><div id='presentation' role='presentation'>presentation</div><iframe id='iframe' src='https://webkit.org/iframe'></iframe></body>"_s;
static constexpr auto iframeHTML = "<body><button id='inner' aria-label='Inner button'>inner</button></body>"_s;

struct AccessibilityNodeHandle {
    uint64_t processID { 0 };
    uint64_t frameID { 0 };
    uint64_t axID { 0 };
};

// Accessibility node handles are opaque to clients. The test decodes them only to check which frame and
// process a handle refers to, and to build handles that point at the wrong frame.
static std::optional<AccessibilityNodeHandle> decodeAccessibilityNodeHandle(NSString *handle)
{
    if (![handle hasPrefix:@"axnode-"])
        return std::nullopt;
    NSArray<NSString *> *components = [[handle substringFromIndex:7] componentsSeparatedByString:@"."];
    if (components.count != 3)
        return std::nullopt;
    return AccessibilityNodeHandle { strtoull(components[0].UTF8String, nullptr, 10), strtoull(components[1].UTF8String, nullptr, 10), strtoull(components[2].UTF8String, nullptr, 10) };
}

static NSString *encodeAccessibilityNodeHandle(const AccessibilityNodeHandle& handle)
{
    return [NSString stringWithFormat:@"axnode-%llu.%llu.%llu", handle.processID, handle.frameID, handle.axID];
}

static uint64_t frameIDForFrameInfo(WKFrameInfo *frameInfo)
{
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return frameInfo._handle.frameID;
ALLOW_DEPRECATED_DECLARATIONS_END
}

class WebAutomationAccessibility {
public:
    explicit WebAutomationAccessibility(bool siteIsolationEnabled)
        : m_server({
            { "/mainframe"_s, { mainFrameHTML } },
            { "/iframe"_s, { iframeHTML } }
        }, HTTPServer::Protocol::HttpsProxy)
    {
        RetainPtr sessionConfiguration = adoptNS([_WKAutomationSessionConfiguration new]);
        m_session = adoptNS([[_WKAutomationSession alloc] initWithConfiguration:sessionConfiguration.get()]);
        [m_session setSessionIdentifier:@(sessionIdentifier)];

        m_processPool = adoptNS([WKProcessPool new]);
        [m_processPool _setAutomationSession:m_session.get()];

        RetainPtr configuration = m_server.httpsProxyConfiguration();
        [configuration setProcessPool:m_processPool.get()];
        std::tie(m_webView, m_navigationDelegate) = siteIsolatedViewAndDelegate(configuration, CGRectMake(0, 0, 800, 600), siteIsolationEnabled);

        [m_webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
        [m_navigationDelegate waitForDidFinishNavigation];

        m_browsingContextHandle = [m_session _registerWebViewForTesting:m_webView.get()];
        EXPECT_NOT_NULL(m_browsingContextHandle.get());

        RetainPtr mainFrame = [m_webView mainFrame];
        EXPECT_EQ(mainFrame.get().childFrames.count, 1u);
        m_mainFrameID = frameIDForFrameInfo(mainFrame.get().info);
        m_childFrameID = frameIDForFrameInfo(mainFrame.get().childFrames.firstObject.info);
        m_mainFramePID = mainFrame.get().info._processIdentifier;
        m_childFramePID = mainFrame.get().childFrames.firstObject.info._processIdentifier;
    }

    ~WebAutomationAccessibility()
    {
        [m_processPool _setAutomationSession:nil];
    }

    // Sends a command and returns the parsed reply, which holds either a "result" or an "error".
    RetainPtr<NSDictionary> sendCommand(NSString *method, NSDictionary *parameters)
    {
        unsigned commandID = ++m_lastCommandID;
        NSData *commandData = [NSJSONSerialization dataWithJSONObject:@{ @"id": @(commandID), @"method": method, @"params": parameters } options:0 error:nil];
        RetainPtr command = adoptNS([[NSString alloc] initWithData:commandData encoding:NSUTF8StringEncoding]);

        __block bool gotReply = false;
        __block RetainPtr<NSDictionary> reply;
        [m_session _setMessageToFrontendHandlerForTesting:^(NSString *message) {
            id json = [NSJSONSerialization JSONObjectWithData:[message dataUsingEncoding:NSUTF8StringEncoding] options:0 error:nil];
            if (![json isKindOfClass:NSDictionary.class] || [json[@"id"] unsignedIntValue] != commandID)
                return;
            reply = json;
            gotReply = true;
        }];
        [m_session _dispatchMessageFromRemoteForTesting:command.get()];
        Util::run(&gotReply);
        [m_session _setMessageToFrontendHandlerForTesting:nil];
        return reply;
    }

    RetainPtr<NSString> frameHandleForChildFrame()
    {
        RetainPtr reply = sendCommand(@"Automation.resolveChildFrameHandle", @{ @"browsingContextHandle": m_browsingContextHandle.get(), @"ordinal": @0 });
        RetainPtr<NSString> frameHandle = reply.get()[@"result"][@"result"];
        EXPECT_TRUE([frameHandle length]) << [reply description].UTF8String;
        return frameHandle;
    }

    RetainPtr<NSString> nodeHandleForElement(NSString *frameHandle, NSString *elementID)
    {
        NSString *function = [NSString stringWithFormat:@"function() { return document.getElementById('%@'); }", elementID];
        RetainPtr reply = sendCommand(@"Automation.evaluateJavaScriptFunction", @{ @"browsingContextHandle": m_browsingContextHandle.get(), @"frameHandle": frameHandle, @"function": function, @"arguments": @[ ] });
        NSString *result = reply.get()[@"result"][@"result"];
        id element = result ? [NSJSONSerialization JSONObjectWithData:[result dataUsingEncoding:NSUTF8StringEncoding] options:0 error:nil] : nil;
        RetainPtr<NSString> nodeHandle = [element isKindOfClass:NSDictionary.class] ? element[[NSString stringWithFormat:@"session-node-%s", sessionIdentifier]] : nil;
        EXPECT_TRUE([nodeHandle length]) << [reply description].UTF8String;
        return nodeHandle;
    }

    // A nil frame handle omits the parameter, which selects the main frame.
    RetainPtr<NSDictionary> propertiesForElement(NSString *frameHandle, NSString *nodeHandle)
    {
        RetainPtr<NSMutableDictionary> parameters = adoptNS([@{ @"browsingContextHandle": m_browsingContextHandle.get(), @"nodeHandle": nodeHandle } mutableCopy]);
        if (frameHandle)
            [parameters setObject:frameHandle forKey:@"frameHandle"];
        return sendCommand(@"Automation.getAccessibilityPropertiesForElement", parameters.get());
    }

    RetainPtr<NSDictionary> propertiesForAccessibilityNode(NSString *accessibilityNodeID)
    {
        return sendCommand(@"Automation.getAccessibilityPropertiesForAccessibilityNode", @{ @"browsingContextHandle": m_browsingContextHandle.get(), @"accessibilityNodeId": accessibilityNodeID });
    }

    void removeIframe()
    {
        [m_webView objectByEvaluatingJavaScript:@"document.getElementById('iframe').remove(); true"];
        while ([m_webView mainFrame].childFrames.count)
            Util::spinRunLoop();
    }

    void setAttribute(NSString *elementID, NSString *name, NSString *value)
    {
        [m_webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"document.getElementById('%@').setAttribute('%@', '%@'); true", elementID, name, value]];
    }

    uint64_t mainFrameID() const { return m_mainFrameID; }
    uint64_t childFrameID() const { return m_childFrameID; }
    pid_t mainFramePID() const { return m_mainFramePID; }
    pid_t childFramePID() const { return m_childFramePID; }

private:
    HTTPServer m_server;
    RetainPtr<_WKAutomationSession> m_session;
    RetainPtr<WKProcessPool> m_processPool;
    RetainPtr<TestWKWebView> m_webView;
    RetainPtr<TestNavigationDelegate> m_navigationDelegate;
    RetainPtr<NSString> m_browsingContextHandle;
    uint64_t m_mainFrameID { 0 };
    uint64_t m_childFrameID { 0 };
    pid_t m_mainFramePID { 0 };
    pid_t m_childFramePID { 0 };
    unsigned m_lastCommandID { 0 };
};

static NSDictionary *properties(const RetainPtr<NSDictionary>& reply)
{
    return reply.get()[@"result"][@"properties"];
}

static NSString *errorMessage(const RetainPtr<NSDictionary>& reply)
{
    return reply.get()[@"error"][@"message"];
}

static void expectError(const RetainPtr<NSDictionary>& reply, NSString *expectedError)
{
    EXPECT_TRUE([errorMessage(reply) hasPrefix:expectedError]) << "expected " << expectedError.UTF8String << ", got " << [reply description].UTF8String;
}

struct AccessibilityTreeWalk {
    RetainPtr<NSMutableSet> labels;
    bool reachedMainFrame { false };
    bool reachedChildFrame { false };
};

// Walks the accessibility tree from the given handles through every parent and child handle the commands return.
// Every handle must resolve to itself and name one of the page's frames, and every child must name its parent.
static AccessibilityTreeWalk walkAccessibilityTree(WebAutomationAccessibility& test, NSArray<NSString *> *startHandles)
{
    AccessibilityTreeWalk walk { adoptNS([NSMutableSet new]) };
    RetainPtr<NSMutableArray> pending = adoptNS([startHandles mutableCopy]);
    RetainPtr<NSMutableSet> visited = adoptNS([NSMutableSet new]);
    while ([pending count] && [visited count] < 200) {
        RetainPtr<NSString> handle = [pending lastObject];
        [pending removeLastObject];
        if ([visited containsObject:handle.get()])
            continue;
        [visited addObject:handle.get()];

        RetainPtr reply = test.propertiesForAccessibilityNode(handle.get());
        NSDictionary *nodeProperties = properties(reply);
        EXPECT_WK_STREQ(nodeProperties[@"accessibilityNodeId"], handle.get()) << [reply description].UTF8String;
        auto decodedHandle = decodeAccessibilityNodeHandle(handle.get());
        EXPECT_TRUE(decodedHandle) << [handle UTF8String];
        if (!decodedHandle)
            continue;
        EXPECT_TRUE(decodedHandle->frameID == test.mainFrameID() || decodedHandle->frameID == test.childFrameID()) << [handle UTF8String];
        walk.reachedMainFrame |= decodedHandle->frameID == test.mainFrameID();
        walk.reachedChildFrame |= decodedHandle->frameID == test.childFrameID();
        if (NSString *label = nodeProperties[@"label"])
            [walk.labels addObject:label];

        if (NSString *parent = nodeProperties[@"parent"])
            [pending addObject:parent];
        for (NSString *child in nodeProperties[@"children"]) {
            RetainPtr childReply = test.propertiesForAccessibilityNode(child);
            EXPECT_WK_STREQ(properties(childReply)[@"parent"], handle.get()) << [childReply description].UTF8String;
            [pending addObject:child];
        }
    }
    EXPECT_LT([visited count], 200u);
    return walk;
}

// Gets the properties of an element in each frame, then walks the accessibility tree through the returned
// handles and checks that every handle resolves to a node in the frame it names.
static void runRoundTripTest(bool siteIsolationEnabled)
{
    WebAutomationAccessibility test(siteIsolationEnabled);
    EXPECT_NE(test.mainFramePID(), 0);
    EXPECT_NE(test.childFramePID(), 0);
    if (siteIsolationEnabled)
        EXPECT_NE(test.mainFramePID(), test.childFramePID());
    else
        EXPECT_EQ(test.mainFramePID(), test.childFramePID());
    EXPECT_NE(test.mainFrameID(), test.childFrameID());

    RetainPtr childFrameHandle = test.frameHandleForChildFrame();

    RetainPtr mainReply = test.propertiesForElement(@"", test.nodeHandleForElement(@"", @"main").get());
    NSDictionary *mainProperties = properties(mainReply);
    EXPECT_WK_STREQ(mainProperties[@"role"], "button");
    EXPECT_WK_STREQ(mainProperties[@"label"], "Main button");
    auto mainHandle = decodeAccessibilityNodeHandle(mainProperties[@"accessibilityNodeId"]);
    ASSERT_TRUE(mainHandle) << [mainReply description].UTF8String;
    EXPECT_EQ(mainHandle->frameID, test.mainFrameID());

    // The node an element resolves to is part of the tree: it appears in its parent's children.
    ASSERT_TRUE([mainProperties[@"parent"] length]) << [mainReply description].UTF8String;
    RetainPtr mainParentReply = test.propertiesForAccessibilityNode(mainProperties[@"parent"]);
    EXPECT_TRUE([properties(mainParentReply)[@"children"] containsObject:mainProperties[@"accessibilityNodeId"]]) << [mainParentReply description].UTF8String;

    RetainPtr innerReply = test.propertiesForElement(childFrameHandle.get(), test.nodeHandleForElement(childFrameHandle.get(), @"inner").get());
    NSDictionary *innerProperties = properties(innerReply);
    EXPECT_WK_STREQ(innerProperties[@"role"], "button");
    EXPECT_WK_STREQ(innerProperties[@"label"], "Inner button");
    auto innerHandle = decodeAccessibilityNodeHandle(innerProperties[@"accessibilityNodeId"]);
    ASSERT_TRUE(innerHandle) << [innerReply description].UTF8String;
    EXPECT_EQ(innerHandle->frameID, test.childFrameID());
    if (siteIsolationEnabled)
        EXPECT_NE(innerHandle->processID, mainHandle->processID);
    else
        EXPECT_EQ(innerHandle->processID, mainHandle->processID);

    // The handles round-trip without a frame handle, each through its own frame's process.
    for (NSDictionary *expected in @[ mainProperties, innerProperties ]) {
        RetainPtr reply = test.propertiesForAccessibilityNode(expected[@"accessibilityNodeId"]);
        EXPECT_TRUE([properties(reply) isEqualToDictionary:expected]) << [reply description].UTF8String;
    }

    // In one process, the walk crosses the frame boundary in both directions: down from the main frame's button
    // into the iframe's document, and up from the iframe's button into the main frame. With site isolation, the
    // iframe's document is in another process, so walk each frame's tree from its own button.
    RetainPtr<NSArray> walkStarts;
    if (siteIsolationEnabled)
        walkStarts = @[ @[ mainProperties[@"accessibilityNodeId"], innerProperties[@"accessibilityNodeId"] ] ];
    else
        walkStarts = @[ @[ mainProperties[@"accessibilityNodeId"] ], @[ innerProperties[@"accessibilityNodeId"] ] ];
    for (NSArray<NSString *> *startHandles in walkStarts.get()) {
        auto walk = walkAccessibilityTree(test, startHandles);
        EXPECT_TRUE(walk.reachedMainFrame) << [startHandles description].UTF8String;
        EXPECT_TRUE(walk.reachedChildFrame) << [startHandles description].UTF8String;
        EXPECT_TRUE([walk.labels containsObject:@"Main button"]) << [startHandles description].UTF8String;
        EXPECT_TRUE([walk.labels containsObject:@"Inner button"]) << [startHandles description].UTF8String;
    }

    // A node from one frame never resolves through another frame. When both frames share a process, the
    // AXID is known to that process, so the lookup must reject it. When they don't, the AXID may happen to
    // name some other node in the other process; that node must then belong to the frame the handle names.
    auto expectNotResolvedThroughOtherFrame = [&](const AccessibilityNodeHandle& forgedHandle, NSString *labelInOtherFrame) {
        RetainPtr encodedHandle = encodeAccessibilityNodeHandle(forgedHandle);
        RetainPtr reply = test.propertiesForAccessibilityNode(encodedHandle.get());
        EXPECT_FALSE([properties(reply)[@"label"] isEqualToString:labelInOtherFrame]) << [reply description].UTF8String;
        if (!siteIsolationEnabled)
            expectError(reply, @"AccessibilityNodeNotFound");
        else if (!reply.get()[@"error"])
            EXPECT_WK_STREQ(properties(reply)[@"accessibilityNodeId"], encodedHandle.get());
    };
    expectNotResolvedThroughOtherFrame({ mainHandle->processID, mainHandle->frameID, innerHandle->axID }, @"Inner button");
    expectNotResolvedThroughOtherFrame({ innerHandle->processID, innerHandle->frameID, mainHandle->axID }, @"Main button");

    // A handle that names the wrong process for its frame is stale.
    AccessibilityNodeHandle mainNodeInOtherProcess { mainHandle->processID + 1000, mainHandle->frameID, mainHandle->axID };
    expectError(test.propertiesForAccessibilityNode(encodeAccessibilityNodeHandle(mainNodeInOtherProcess)), @"AccessibilityNodeNotFound");

    // A handle for a frame that is gone refers to a node that is gone.
    test.removeIframe();
    expectError(test.propertiesForAccessibilityNode(innerProperties[@"accessibilityNodeId"]), @"AccessibilityNodeNotFound");
    RetainPtr reply = test.propertiesForAccessibilityNode(mainProperties[@"accessibilityNodeId"]);
    EXPECT_WK_STREQ(properties(reply)[@"label"], "Main button") << [reply description].UTF8String;
}

TEST(WebAutomationAccessibility, CrossOriginIframe)
{
    runRoundTripTest(false);
}

TEST(WebAutomationAccessibility, CrossOriginIframeWithSiteIsolation)
{
    runRoundTripTest(true);
}

TEST(WebAutomationAccessibility, CheckedAndPressedStates)
{
    WebAutomationAccessibility test(false);
    auto propertiesForElementID = [&](NSString *elementID) {
        RetainPtr reply = test.propertiesForElement(@"", test.nodeHandleForElement(@"", elementID).get());
        EXPECT_NOT_NULL(properties(reply)) << [reply description].UTF8String;
        return reply;
    };

    // A toggle button has a pressed state, not a checked state.
    RetainPtr toggle = propertiesForElementID(@"toggle");
    EXPECT_WK_STREQ(properties(toggle)[@"pressed"], "true");
    EXPECT_NULL(properties(toggle)[@"checked"]) << [toggle description].UTF8String;

    RetainPtr checkbox = propertiesForElementID(@"checkbox");
    EXPECT_WK_STREQ(properties(checkbox)[@"checked"], "true");
    EXPECT_NULL(properties(checkbox)[@"pressed"]) << [checkbox description].UTF8String;

    RetainPtr ariaCheckbox = propertiesForElementID(@"ariaCheckbox");
    EXPECT_WK_STREQ(properties(ariaCheckbox)[@"checked"], "mixed");
    EXPECT_NULL(properties(ariaCheckbox)[@"pressed"]) << [ariaCheckbox description].UTF8String;

    RetainPtr button = propertiesForElementID(@"main");
    EXPECT_NULL(properties(button)[@"checked"]) << [button description].UTF8String;
    EXPECT_NULL(properties(button)[@"pressed"]) << [button description].UTF8String;
}

TEST(WebAutomationAccessibility, ElementWithoutFrameHandle)
{
    WebAutomationAccessibility test(false);
    RetainPtr nodeHandle = test.nodeHandleForElement(@"", @"main");

    // Omitting the frame handle selects the main frame, the same as passing an empty one.
    RetainPtr withoutFrameHandle = test.propertiesForElement(nil, nodeHandle.get());
    EXPECT_WK_STREQ(properties(withoutFrameHandle)[@"label"], "Main button");
    auto handle = decodeAccessibilityNodeHandle(properties(withoutFrameHandle)[@"accessibilityNodeId"]);
    ASSERT_TRUE(handle) << [withoutFrameHandle description].UTF8String;
    EXPECT_EQ(handle->frameID, test.mainFrameID());

    RetainPtr withEmptyFrameHandle = test.propertiesForElement(@"", nodeHandle.get());
    EXPECT_TRUE([properties(withoutFrameHandle) isEqualToDictionary:properties(withEmptyFrameHandle)]) << [withEmptyFrameHandle description].UTF8String;
}

TEST(WebAutomationAccessibility, IgnoredAndHiddenElements)
{
    WebAutomationAccessibility test(false);

    // Elements with no accessibility node, or whose accessibility node is ignored, aren't part of the tree.
    for (NSString *elementID in @[ @"hidden", @"ariaHidden", @"presentation" ])
        expectError(test.propertiesForElement(@"", test.nodeHandleForElement(@"", elementID).get()), @"AccessibilityNodeNotFound");

    // A handle stops resolving once its node becomes ignored.
    RetainPtr reply = test.propertiesForElement(@"", test.nodeHandleForElement(@"", @"main").get());
    RetainPtr<NSString> handle = properties(reply)[@"accessibilityNodeId"];
    ASSERT_TRUE([handle length]) << [reply description].UTF8String;
    test.setAttribute(@"main", @"aria-hidden", @"true");
    expectError(test.propertiesForAccessibilityNode(handle.get()), @"AccessibilityNodeNotFound");
}

TEST(WebAutomationAccessibility, MalformedAccessibilityNodeHandle)
{
    WebAutomationAccessibility test(true);
    for (NSString *handle in @[ @"", @"garbage", @"axnode-", @"axnode-1", @"axnode-1.2", @"axnode-1.2.", @"axnode-.2.3", @"axnode-a.b.c", @"axnode-1.2.3.4", @"axnode-0.0.0", @"axnode-1.0.1", @"axnode-1.1.0", @"axnode--1.2.3", @"axnode-18446744073709551616.1.1", @"axnode- 1.2.3", @"axnode-1 .2.3", @"axnode-1. 2.3", @"axnode-1.2 .3", @"axnode-1.2. 3", @"axnode-1.2.3 ", @"axnode-\t1.2.3", @"node-1.2.3" ])
        expectError(test.propertiesForAccessibilityNode(handle), @"InvalidNodeIdentifier");
}

} // namespace TestWebKitAPI

#endif // ENABLE(REMOTE_INSPECTOR)
