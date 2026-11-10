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

#if PLATFORM(IOS_FAMILY) && ENABLE(REMOTE_INSPECTOR)

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/CGImagePixelReader.h"
#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/cocoa/SiteIsolationTestUtilities.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebCore/Color.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/_WKAutomationSession.h>
#import <WebKit/_WKAutomationSessionConfiguration.h>
#import <WebKit/_WKAutomationSessionDelegate.h>
#import <WebKit/_WKAutomationSessionPrivateForTesting.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/text/MakeString.h>

@interface InspectorFrameHighlightSessionDelegate : NSObject <_WKAutomationSessionDelegate>
@end

@implementation InspectorFrameHighlightSessionDelegate

- (BOOL)_automationSessionShouldEnableInspectorTesting:(_WKAutomationSession *)automationSession
{
    return YES;
}

@end

namespace TestWebKitAPI {

static RetainPtr<NSDictionary> dictionaryFromJSON(NSString *json)
{
    return dynamic_objc_cast<NSDictionary>([NSJSONSerialization JSONObjectWithData:[json dataUsingEncoding:NSUTF8StringEncoding] options:0 error:nil]);
}

static RetainPtr<NSString> jsonFromDictionary(NSDictionary *dictionary)
{
    return adoptNS([[NSString alloc] initWithData:[NSJSONSerialization dataWithJSONObject:dictionary options:0 error:nil] encoding:NSUTF8StringEncoding]);
}

static NSDictionary *opaqueColor(int red, int green, int blue)
{
    return @{ @"r": @(red), @"g": @(green), @"b": @(blue), @"a": @1 };
}

static bool isNear(const WebCore::Color& color, WebCore::SRGBA<uint8_t> expected)
{
    constexpr int tolerance { 16 };
    auto [red, green, blue, alpha] = color.toColorTypeLossy<WebCore::SRGBA<uint8_t>>().resolved();
    auto [expectedRed, expectedGreen, expectedBlue, expectedAlpha] = expected.resolved();
    return std::abs(red - expectedRed) <= tolerance && std::abs(green - expectedGreen) <= tolerance && std::abs(blue - expectedBlue) <= tolerance;
}

class InspectorFrameHighlight : public testing::TestWithParam<SiteIsolationParams> {
public:
    static constexpr CGPoint targetCenter { 130, 240 };
    static constexpr CGPoint targetCenterInIframeCoordinates { 110, 100 };
    static constexpr CGPoint iframeBackground { 40, 160 };
    static constexpr CGPoint mainFrameTargetCenter { 70, 420 };
    static constexpr CGRect iframeRect { { 20, 140 }, { 280, 220 } };
    static constexpr CGRect gridLineRect { { 157, 415 }, { 6, 30 } };

    static constexpr WebCore::SRGBA<uint8_t> highlightColor { 0, 0, 255 };
    static constexpr WebCore::SRGBA<uint8_t> targetColor { 255, 255, 0 };
    static constexpr WebCore::SRGBA<uint8_t> iframeBackgroundColor { 255, 230, 230 };
    static constexpr WebCore::SRGBA<uint8_t> mainFrameBackgroundColor { 255, 255, 255 };
    static constexpr WebCore::SRGBA<uint8_t> gridColor { 0, 255, 0 };
    static constexpr WebCore::SRGBA<uint8_t> elementTitleColor { 255, 255, 194 };

    class Snapshot {
    public:
        explicit Snapshot(TestWKWebView *webView)
            : m_reader { renderLayerTree(webView).get() }
            , m_scale { m_reader.width() / CGRectGetWidth(webView.bounds) }
        {
        }

        bool hasColorAt(CGPoint point, WebCore::SRGBA<uint8_t> expected) const
        {
            return isNear(m_reader.at(static_cast<unsigned>(point.x * m_scale), static_cast<unsigned>(point.y * m_scale)), expected);
        }

        bool hasColorIn(CGRect rect, WebCore::SRGBA<uint8_t> expected) const
        {
            for (auto x = static_cast<unsigned>(CGRectGetMinX(rect) * m_scale); x < CGRectGetMaxX(rect) * m_scale; ++x) {
                for (auto y = static_cast<unsigned>(CGRectGetMinY(rect) * m_scale); y < CGRectGetMaxY(rect) * m_scale; ++y) {
                    if (isNear(m_reader.at(x, y), expected))
                        return true;
                }
            }
            return false;
        }

    private:
        // A render server snapshot of a visible web view comes back empty in the simulator test runner.
        static RetainPtr<CGImageRef> renderLayerTree(TestWKWebView *webView)
        {
            RetainPtr renderer { adoptNS([[UIGraphicsImageRenderer alloc] initWithSize:webView.bounds.size]) };
            return [renderer imageWithActions:^(UIGraphicsImageRendererContext *context) {
                [webView.layer renderInContext:context.CGContext];
            }].CGImage;
        }

        CGImagePixelReader m_reader;
        CGFloat m_scale;
    };

    static std::string testNameGenerator(testing::TestParamInfo<SiteIsolationParams> info)
    {
        return info.param.testName();
    }

    void SetUp() override
    {
        m_server = makeUnique<HTTPServer>(std::initializer_list<std::pair<String, HTTPResponse>> {
            { "/mainframe"_s, { makeString("<meta name='viewport' content='width=device-width, initial-scale=1'><body style='margin:0; background:white'>"
                "<iframe id='iframe' style='position:absolute; left:20px; top:140px; width:280px; height:220px; border:0' src='"_s, GetParam().subframeURL("/iframe"_s), "'></iframe>"
                "<div id='main-target' style='position:absolute; left:20px; top:400px; width:100px; height:40px; background:yellow'></div>"
                "<div id='main-grid' style='position:absolute; left:160px; top:400px; width:140px; height:60px; display:grid; grid-template-columns:1fr 1fr; column-gap:20px'><div></div><div></div></div>"
                "<div style='height:2000px'></div></body>"_s) } },
            { "/iframe"_s, { "<body style='margin:0; background:rgb(255, 230, 230)'><div id='target' style='position:absolute; left:50px; top:60px; width:120px; height:80px; background:yellow'></div>"
                "<div style='height:2000px'></div></body>"_s } },
        }, HTTPServer::Protocol::HttpsProxy);

        m_session = adoptNS([[_WKAutomationSession alloc] initWithConfiguration:adoptNS([_WKAutomationSessionConfiguration new]).get()]);
        m_sessionDelegate = adoptNS([InspectorFrameHighlightSessionDelegate new]);
        [m_session setDelegate:m_sessionDelegate.get()];
        m_processPool = adoptNS([WKProcessPool new]);
        [m_processPool _setAutomationSession:m_session.get()];

        RetainPtr configuration { m_server->httpsProxyConfiguration() };
        [configuration setProcessPool:m_processPool.get()];
        GetParam().applyTo(configuration.get());

        m_navigationDelegate = adoptNS([TestNavigationDelegate new]);
        [m_navigationDelegate allowAnyTLSCertificate];
        m_webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 560) configuration:configuration.get() addToWindow:YES]);
        [m_webView setNavigationDelegate:m_navigationDelegate.get()];
        [m_webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
        [m_navigationDelegate waitForDidFinishNavigation];
        EXPECT_TRUE(Util::waitFor([&] {
            RetainPtr iframe { [m_webView firstChildFrame] };
            return iframe && [[m_webView objectByEvaluatingJavaScript:@"!!document.getElementById('target')" inFrame:iframe.get()] boolValue];
        }));
        EXPECT_TRUE(waitForColor(targetCenter, targetColor));

        m_handle = [m_session _registerWebViewForTesting:m_webView.get()];
        [m_session _setMessageToFrontendHandlerForTesting:^(NSString *message) {
            didReceiveMessage(message);
        }];
        sendCommand(nil, @"Target.setPauseOnStart", @{ @"pauseOnStart": @NO });
        // Like the frontend; the enabled page agent refreshes page highlights on scroll.
        sendCommand(pageTarget(), @"Page.enable");
    }

    void TearDown() override
    {
        [m_session _setMessageToFrontendHandlerForTesting:nil];
        [m_processPool _setAutomationSession:nil];
    }

    RetainPtr<NSDictionary> sendCommand(NSString *targetIdentifier, NSString *method, NSDictionary *parameters = @{ })
    {
        int identifier { ++m_lastCommandIdentifier };
        RetainPtr message { jsonFromDictionary(@{ @"id": @(identifier), @"method": method, @"params": parameters }) };
        if (targetIdentifier)
            message = jsonFromDictionary(@{ @"id": @(++m_lastCommandIdentifier), @"method": @"Target.sendMessageToTarget", @"params": @{ @"targetId": targetIdentifier, @"message": message.get() } });
        [m_session _dispatchMessageFromRemoteForTesting:jsonFromDictionary(@{ @"id": @(++m_lastCommandIdentifier), @"method": @"Automation.sendInspectorMessage", @"params": @{ @"browsingContextHandle": m_handle.get(), @"message": message.get() } }).get()];

        EXPECT_TRUE(Util::waitFor([&] {
            return !![m_replies objectForKey:@(identifier)];
        })) << method.UTF8String;
        RetainPtr<NSDictionary> reply { [m_replies objectForKey:@(identifier)] };
        EXPECT_NULL([reply objectForKey:@"error"]) << method.UTF8String;
        return dynamic_objc_cast<NSDictionary>([reply objectForKey:@"result"]);
    }

    NSString *pageTarget() const
    {
        for (NSDictionary *targetInfo in [m_targets allValues]) {
            if ([targetInfo[@"type"] isEqual:@"page"] && ![targetInfo[@"isProvisional"] boolValue])
                return targetInfo[@"targetId"];
        }
        return nil;
    }

    RetainPtr<NSArray> committedTargets() const
    {
        RetainPtr<NSMutableArray> targets { adoptNS([NSMutableArray new]) };
        for (NSDictionary *targetInfo in [m_targets allValues]) {
            if (![targetInfo[@"isProvisional"] boolValue])
                [targets addObject:targetInfo[@"targetId"]];
        }
        return targets;
    }

    RetainPtr<NSString> frameTarget(NSString *path)
    {
        RetainPtr<NSString> result;
        EXPECT_TRUE(Util::waitFor([&] {
            for (NSDictionary *targetInfo in [m_targets allValues]) {
                if (![targetInfo[@"type"] isEqual:@"frame"] || [targetInfo[@"isProvisional"] boolValue])
                    continue;
                if ([[documentNode(targetInfo[@"targetId"]) objectForKey:@"documentURL"] hasSuffix:path]) {
                    result = targetInfo[@"targetId"];
                    return true;
                }
            }
            return false;
        }));
        return result;
    }

    // Mirrors the frontend, which reaches a node in a cross-site iframe only through that frame's target.
    RetainPtr<NSString> targetForIframeNodes()
    {
        if (GetParam().isCrossOriginWithSiteIsolation())
            return frameTarget(@"/iframe");
        return pageTarget();
    }

    RetainPtr<NSDictionary> documentNode(NSString *targetIdentifier)
    {
        if (NSDictionary *document { [m_documents objectForKey:targetIdentifier] })
            return document;
        RetainPtr<NSDictionary> document { [sendCommand(targetIdentifier, @"DOM.getDocument") objectForKey:@"root"] };
        if (document)
            [m_documents setObject:document.get() forKey:targetIdentifier];
        return document;
    }

    RetainPtr<NSNumber> nodeIdentifier(NSString *targetIdentifier, NSString *selector)
    {
        RetainPtr<NSNumber> documentIdentifier { [documentNode(targetIdentifier) objectForKey:@"nodeId"] };
        if (NSNumber *node { [sendCommand(targetIdentifier, @"DOM.querySelector", @{ @"nodeId": documentIdentifier.get(), @"selector": selector }) objectForKey:@"nodeId"] })
            return node;

        RetainPtr search { sendCommand(targetIdentifier, @"DOM.performSearch", @{ @"query": selector }) };
        RetainPtr results { sendCommand(targetIdentifier, @"DOM.getSearchResults", @{ @"searchId": [search objectForKey:@"searchId"], @"fromIndex": @0, @"toIndex": [search objectForKey:@"resultCount"] }) };
        return [[results objectForKey:@"nodeIds"] firstObject];
    }

    void highlightNode(NSString *targetIdentifier, NSString *selector, bool showInfo = false)
    {
        sendCommand(targetIdentifier, @"DOM.highlightNode", @{
            @"nodeId": nodeIdentifier(targetIdentifier, selector).get(),
            @"highlightConfig": @{ @"showInfo": @(showInfo), @"contentColor": opaqueColor(0, 0, 255) },
        });
    }

    bool waitForColor(CGPoint point, WebCore::SRGBA<uint8_t> expected)
    {
        return Util::waitFor([&] {
            return Snapshot { m_webView.get() }.hasColorAt(point, expected);
        });
    }

    bool waitForColor(CGRect rect, WebCore::SRGBA<uint8_t> expected)
    {
        return Util::waitFor([&] {
            return Snapshot { m_webView.get() }.hasColorIn(rect, expected);
        });
    }

    void scrollIframe(int y)
    {
        [m_webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"scrollTo(0, %d); scrollY", y] inFrame:[m_webView firstChildFrame]];
        [m_webView waitForNextPresentationUpdate];
    }

    std::unique_ptr<HTTPServer> m_server;
    RetainPtr<TestWKWebView> m_webView;

private:
    void didReceiveMessage(NSString *message)
    {
        RetainPtr outerMessage { dictionaryFromJSON(message) };
        if ([outerMessage objectForKey:@"error"])
            ADD_FAILURE() << message.UTF8String;
        if (![[outerMessage objectForKey:@"method"] isEqual:@"Automation.receiveInspectorMessage"])
            return;

        RetainPtr innerMessage { dictionaryFromJSON([outerMessage objectForKey:@"params"][@"message"]) };
        NSString *method { [innerMessage objectForKey:@"method"] };
        NSDictionary *parameters { [innerMessage objectForKey:@"params"] };
        if ([method isEqual:@"Target.targetCreated"])
            [m_targets setObject:parameters[@"targetInfo"] forKey:parameters[@"targetInfo"][@"targetId"]];
        else if ([method isEqual:@"Target.targetDestroyed"])
            [m_targets removeObjectForKey:parameters[@"targetId"]];
        else if ([method isEqual:@"Target.didCommitProvisionalTarget"]) {
            [m_targets removeObjectForKey:parameters[@"oldTargetId"]];
            if (NSDictionary *targetInfo { [m_targets objectForKey:parameters[@"newTargetId"]] }) {
                RetainPtr<NSMutableDictionary> committedTargetInfo { adoptNS([targetInfo mutableCopy]) };
                [committedTargetInfo removeObjectForKey:@"isProvisional"];
                [m_targets setObject:committedTargetInfo.get() forKey:parameters[@"newTargetId"]];
            }
        } else if ([method isEqual:@"Target.dispatchMessageFromTarget"]) {
            RetainPtr reply { dictionaryFromJSON(parameters[@"message"]) };
            if (NSNumber *identifier { [reply objectForKey:@"id"] })
                [m_replies setObject:reply.get() forKey:identifier];
        } else if (NSNumber *identifier { [innerMessage objectForKey:@"id"] })
            [m_replies setObject:innerMessage.get() forKey:identifier];
    }

    RetainPtr<_WKAutomationSession> m_session;
    RetainPtr<InspectorFrameHighlightSessionDelegate> m_sessionDelegate;
    RetainPtr<WKProcessPool> m_processPool;
    RetainPtr<TestNavigationDelegate> m_navigationDelegate;
    RetainPtr<NSString> m_handle;
    RetainPtr<NSMutableDictionary> m_targets { adoptNS([NSMutableDictionary new]) };
    RetainPtr<NSMutableDictionary> m_documents { adoptNS([NSMutableDictionary new]) };
    RetainPtr<NSMutableDictionary> m_replies { adoptNS([NSMutableDictionary new]) };
    int m_lastCommandIdentifier { 0 };
};

TEST_P(InspectorFrameHighlight, HighlightsNodeInIframe)
{
    EXPECT_TRUE(Snapshot { m_webView.get() }.hasColorAt(iframeBackground, iframeBackgroundColor));

    RetainPtr target { targetForIframeNodes() };
    highlightNode(target.get(), @"#target");
    EXPECT_TRUE(waitForColor(targetCenter, highlightColor));
    {
        Snapshot snapshot { m_webView.get() };
        EXPECT_TRUE(snapshot.hasColorAt(targetCenterInIframeCoordinates, mainFrameBackgroundColor));
        EXPECT_TRUE(snapshot.hasColorAt(iframeBackground, iframeBackgroundColor));
    }

    sendCommand(target.get(), @"DOM.hideHighlight");
    EXPECT_TRUE(waitForColor(targetCenter, targetColor));
}

TEST_P(InspectorFrameHighlight, HighlightsMainFrameNodeThroughMainFrameTarget)
{
    RetainPtr<NSString> target { GetParam().siteIsolationEnabled ? frameTarget(@"/mainframe") : RetainPtr<NSString> { pageTarget() } };
    highlightNode(target.get(), @"#main-target");
    EXPECT_TRUE(waitForColor(mainFrameTargetCenter, highlightColor));
}

TEST_P(InspectorFrameHighlight, MainFrameGridOverlaySurvivesIframeHighlight)
{
    RetainPtr page { pageTarget() };
    sendCommand(page.get(), @"DOM.showGridOverlay", @{
        @"nodeId": nodeIdentifier(page.get(), @"#main-grid").get(),
        @"gridOverlayConfig": @{ @"gridColor": opaqueColor(0, 255, 0) },
    });
    EXPECT_TRUE(waitForColor(gridLineRect, gridColor));

    RetainPtr target { targetForIframeNodes() };
    RetainPtr otherTargets { committedTargets() };
    for (NSString *otherTarget in otherTargets.get()) {
        if (![otherTarget isEqual:target.get()])
            sendCommand(otherTarget, @"DOM.hideHighlight");
    }
    highlightNode(target.get(), @"#target");
    EXPECT_TRUE(waitForColor(targetCenter, highlightColor));
    EXPECT_TRUE(Snapshot { m_webView.get() }.hasColorIn(gridLineRect, gridColor));

    sendCommand(target.get(), @"DOM.hideHighlight");
    EXPECT_TRUE(waitForColor(targetCenter, targetColor));
    EXPECT_TRUE(Snapshot { m_webView.get() }.hasColorIn(gridLineRect, gridColor));
}

TEST_P(InspectorFrameHighlight, ExtendedGridLinesOnScrolledPage)
{
    static constexpr CGRect gridLineBelowGrid { { 157, 460 }, { 6, 80 } };

    [m_webView objectByEvaluatingJavaScript:@"scrollTo(0, 200); scrollY"];
    [m_webView waitForNextPresentationUpdate];

    RetainPtr<NSString> target { GetParam().siteIsolationEnabled ? frameTarget(@"/mainframe") : RetainPtr<NSString> { pageTarget() } };
    sendCommand(target.get(), @"DOM.showGridOverlay", @{
        @"nodeId": nodeIdentifier(target.get(), @"#main-grid").get(),
        @"gridOverlayConfig": @{ @"gridColor": opaqueColor(0, 255, 0), @"showExtendedGridLines": @YES },
    });
    EXPECT_TRUE(waitForColor(gridLineBelowGrid, gridColor));
}

TEST_P(InspectorFrameHighlight, HighlightFollowsIframeScroll)
{
    static constexpr CGPoint upperTargetPoint { 130, 170 };
    static constexpr CGPoint lowerTargetPoint { 130, 270 };

    scrollIframe(40);
    RetainPtr target { targetForIframeNodes() };
    highlightNode(target.get(), @"#target");
    EXPECT_TRUE(waitForColor(upperTargetPoint, highlightColor));
    EXPECT_FALSE(Snapshot { m_webView.get() }.hasColorAt(lowerTargetPoint, highlightColor));

    scrollIframe(0);
    EXPECT_TRUE(waitForColor(lowerTargetPoint, highlightColor));
    EXPECT_FALSE(Snapshot { m_webView.get() }.hasColorAt(upperTargetPoint, highlightColor));
}

TEST_P(InspectorFrameHighlight, HighlightDrawsNoElementTitle)
{
    RetainPtr target { targetForIframeNodes() };
    highlightNode(target.get(), @"#target", true);
    EXPECT_TRUE(waitForColor(targetCenter, highlightColor));
    EXPECT_FALSE(Snapshot { m_webView.get() }.hasColorIn(iframeRect, elementTitleColor));
}

TEST_P(InspectorFrameHighlight, RemovingHighlightedIframe)
{
    RetainPtr target { targetForIframeNodes() };
    highlightNode(target.get(), @"#target");
    [m_webView waitForNextPresentationUpdate];

    [m_webView objectByEvaluatingJavaScript:@"document.getElementById('iframe').remove(); true"];
    RetainPtr page { pageTarget() };
    highlightNode(page.get(), @"#main-target");
    EXPECT_TRUE(waitForColor(mainFrameTargetCenter, highlightColor));
}

INSTANTIATE_TEST_SUITE_P(WebInspector, InspectorFrameHighlight, testing::ValuesIn(siteIsolationParams), &InspectorFrameHighlight::testNameGenerator);

} // namespace TestWebKitAPI

#endif // PLATFORM(IOS_FAMILY) && ENABLE(REMOTE_INSPECTOR)
