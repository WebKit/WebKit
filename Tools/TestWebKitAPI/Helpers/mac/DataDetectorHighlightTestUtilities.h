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

#pragma once

#ifdef __cplusplus

#include <wtf/Platform.h>

#if PLATFORM(MAC) && ENABLE(DATA_DETECTION) && !__has_feature(modules)

#import "Helpers/Test.h"
#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/cocoa/SiteIsolationTestUtilities.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <QuartzCore/QuartzCore.h>
#import <pal/spi/mac/DataDetectorsSPI.h>
#import <wtf/Function.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

namespace TestWebKitAPI {

struct DataDetectorHighlightGeometry {
    CGRect boundingRect;
    CGRect buttonRect;
    CGPoint buttonCenter;
};

enum class DataDetectorHighlightStyle : bool { Bubble, SelectionButton };

RetainPtr<DDHighlightRef> createDataDetectorHighlight(CGRect textRect, CGRect visibleRect, DataDetectorHighlightStyle = DataDetectorHighlightStyle::Bubble);
bool pointIsOnDataDetectorHighlight(DDHighlightRef, CGPoint);
DataDetectorHighlightGeometry dataDetectorHighlightGeometry(CGRect textRect, CGRect visibleRect, DataDetectorHighlightStyle = DataDetectorHighlightStyle::Bubble);
bool rectsAreNear(CGRect, CGRect, CGFloat tolerance);
Vector<std::pair<RetainPtr<CALayer>, CGRect>> visibleLayersAndFrames(CALayer *rootLayer);
RetainPtr<CALayer> visibleLayerNear(CALayer *rootLayer, CGRect expectedFrame, CGFloat tolerance);
testing::AssertionResult hasVisibleLayerNear(CALayer *rootLayer, CGRect expectedFrame, CGFloat tolerance);
testing::AssertionResult hoverUntilHighlightIsShown(TestWKWebView *, CGRect textRect, CGRect highlightBoundingRect, CGFloat tolerance);
testing::AssertionResult waitUntilWindowClipRectWidthIs(TestWKWebView *, WKFrameInfo *, int width);

class DataDetectorInSubframeTest : public testing::TestWithParam<SiteIsolationParams> {
public:
    static constexpr int iframeLeft = 100;
    static constexpr int iframeTop = 120;
    static constexpr int iframeWidth = 400;
    static constexpr int iframeHeight = 300;
    static constexpr int iframeBorder = 3;
    static constexpr int iframePadding = 5;
    static constexpr int iframeBorderAndPadding = iframeBorder + iframePadding;
    static constexpr NSRect iframeContentBox { { iframeLeft + iframeBorderAndPadding, iframeTop + iframeBorderAndPadding }, { iframeWidth, iframeHeight } };
    static constexpr int secondIframeLeft = 520;
    static constexpr NSRect secondIframeContentBox { { secondIframeLeft + iframeBorderAndPadding, iframeTop + iframeBorderAndPadding }, { iframeWidth, iframeHeight } };
    static constexpr int nestedFrameLeft = 30;
    static constexpr int nestedFrameTop = 40;
    static constexpr NSPoint nestedFrameContentOrigin { iframeLeft + iframeBorderAndPadding + nestedFrameLeft, iframeTop + iframeBorderAndPadding + nestedFrameTop };

    static String iframe(int left, const String& url);
    static String mainHTML(const String& iframes);
    static String nestedFrameHTML(const String& url, int width, int height);
    static HTTPResponse html(const String& body);

    virtual void adjustConfiguration(WKWebViewConfiguration *) { }
    void setUpWebView(std::initializer_list<std::pair<String, HTTPResponse>> routes, NOESCAPE const Function<void(WKWebViewConfiguration *)>& configure);
    void load(NSString *path);
    RetainPtr<WKFrameInfo> nestedFrame();
    static CGRect rectInWebView(NSArray *rectInFrame, CGPoint frameContentOrigin);
    static CGPoint pointInSecondFrame(CGPoint pointInFirstFrame);
    static CGPoint center(CGRect);
    CGRect rootVisibleRect(CGRect contentBoxIfRoot);
    void moveMouse(CGPoint pointInWebView);
    bool scrollTo(WKFrameInfo *, int scrollY);

    static std::string testNameGenerator(testing::TestParamInfo<SiteIsolationParams>);

    std::unique_ptr<HTTPServer> server;
    RetainPtr<TestNavigationDelegate> navigationDelegate;
    RetainPtr<TestWKWebView> webView;
};

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC) && ENABLE(DATA_DETECTION) && !__has_feature(modules)

#endif // __cplusplus
