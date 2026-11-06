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
#import "Helpers/cocoa/TestWKWebView.h"
#import "Helpers/cocoa/WKWebViewConfigurationExtras.h"
#import <WebKit/WebKit.h>
#import <wtf/RetainPtr.h>

static RetainPtr<TestWKWebView> createWebViewWithInternalsAndQuirks()
{
    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [configuration preferences].siteSpecificQuirksModeEnabled = YES;
    return adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
}

TEST(QuirksURLChange, TopDocumentPushStateReresolvesSubframeQuirks)
{
    RetainPtr webView = createWebViewWithInternalsAndQuirks();
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><iframe></iframe>" baseURL:[NSURL URLWithString:@"https://www.google.com/"]];

    auto subframeHasAnchorQuirk = [&] {
        return [[webView objectByEvaluatingJavaScript:@"frames[0].internals.activeQuirkNames().includes('NeedsAnchorToBeMouseFocusableQuirk')"] boolValue];
    };

    EXPECT_FALSE(subframeHasAnchorQuirk());

    [webView objectByEvaluatingJavaScript:@"history.pushState({ }, '', '/search')"];
    EXPECT_TRUE(subframeHasAnchorQuirk());

    [webView objectByEvaluatingJavaScript:@"history.replaceState({ }, '', '/')"];
    EXPECT_FALSE(subframeHasAnchorQuirk());
}

#if PLATFORM(IOS_FAMILY)

static NSString * const openRiceAdPopupMarkup = @"<!DOCTYPE html><meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<style>body { margin: 0; } .popup-ad-subview { position: fixed; top: 0; bottom: 0; left: 0; right: 0; display: flex; align-items: center; justify-content: center; }</style>"
    "<div class='popup-ad-subview'><div class='popup-ad-subview-container' id='container' style='height: 700px; width: 100%;'><div id='closeButton' style='height: 51px; width: 51px;'></div></div></div>";

static RetainPtr<TestWKWebView> createWebViewForOpenRiceAdPopup(NSString *baseURL, CGSize size)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration preferences].siteSpecificQuirksModeEnabled = YES;
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, size.width, size.height) configuration:configuration.get()]);
    [webView synchronouslyLoadHTMLString:openRiceAdPopupMarkup baseURL:[NSURL URLWithString:baseURL]];
    [webView waitForNextPresentationUpdate];
    return webView;
}

static double evaluateNumber(TestWKWebView *webView, NSString *script)
{
    return [[webView objectByEvaluatingJavaScript:script] doubleValue];
}

TEST(SiteSpecificQuirks, OpenRiceAdPopupContainerIsClampedToTheViewportHeight)
{
    RetainPtr webView = createWebViewForOpenRiceAdPopup(@"https://www.openrice.com/", CGSizeMake(382, 580));

    double viewportHeight = evaluateNumber(webView.get(), @"innerHeight");
    EXPECT_EQ(viewportHeight, evaluateNumber(webView.get(), @"container.getBoundingClientRect().height"));
    EXPECT_EQ(0, evaluateNumber(webView.get(), @"container.getBoundingClientRect().top"));
    EXPECT_EQ(0, evaluateNumber(webView.get(), @"closeButton.getBoundingClientRect().top"));
}

TEST(SiteSpecificQuirks, OpenRiceAdPopupContainerFollowsViewportHeightChanges)
{
    RetainPtr webView = createWebViewForOpenRiceAdPopup(@"https://www.openrice.com/", CGSizeMake(382, 580));

    [webView setFrame:CGRectMake(0, 0, 382, 500)];
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ(evaluateNumber(webView.get(), @"innerHeight"), evaluateNumber(webView.get(), @"container.getBoundingClientRect().height"));

    [webView setFrame:CGRectMake(0, 0, 382, 800)];
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ(700, evaluateNumber(webView.get(), @"container.getBoundingClientRect().height"));
}

TEST(SiteSpecificQuirks, OpenRiceAdPopupContainerIsClampedWhenTheViewportShrinksBelowItsHeight)
{
    RetainPtr webView = createWebViewForOpenRiceAdPopup(@"https://www.openrice.com/", CGSizeMake(382, 800));
    EXPECT_EQ(700, evaluateNumber(webView.get(), @"container.getBoundingClientRect().height"));

    [webView setFrame:CGRectMake(0, 0, 382, 500)];
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ(evaluateNumber(webView.get(), @"innerHeight"), evaluateNumber(webView.get(), @"container.getBoundingClientRect().height"));
    EXPECT_EQ(0, evaluateNumber(webView.get(), @"closeButton.getBoundingClientRect().top"));

    [webView setFrame:CGRectMake(0, 0, 382, 800)];
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ(700, evaluateNumber(webView.get(), @"container.getBoundingClientRect().height"));

    [webView setFrame:CGRectMake(0, 0, 382, 500)];
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ(evaluateNumber(webView.get(), @"innerHeight"), evaluateNumber(webView.get(), @"container.getBoundingClientRect().height"));
}

TEST(SiteSpecificQuirks, OpenRiceAdPopupContainerIsNotClampedOnOtherSites)
{
    RetainPtr webView = createWebViewForOpenRiceAdPopup(@"https://www.example.com/", CGSizeMake(382, 580));

    EXPECT_EQ(700, evaluateNumber(webView.get(), @"container.getBoundingClientRect().height"));
    EXPECT_LT(evaluateNumber(webView.get(), @"closeButton.getBoundingClientRect().top"), 0);
}

#endif
