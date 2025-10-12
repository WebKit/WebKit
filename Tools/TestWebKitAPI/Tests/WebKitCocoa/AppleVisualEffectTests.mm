/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "config.h"

#if HAVE(CORE_MATERIAL)

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

#if HAVE(MATERIAL_HOSTING)

// FIXME: Remove these tests and update LayoutTests/apple-visual-effects/apple-visual-effect-parsing.html.

TEST(AppleVisualEffect, GlassMaterialParsing)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView _setUseSystemAppearance:YES];

    [webView synchronouslyLoadHTMLString:@"<div id='test'></div>"];
    [webView stringByEvaluatingJavaScript:@"document.querySelector('#test').style.setProperty('-apple-visual-effect', '-apple-system-glass-material')"];

    EXPECT_WK_STREQ("-apple-system-glass-material", [webView stringByEvaluatingJavaScript:@"getComputedStyle(document.querySelector('#test')).getPropertyValue('-apple-visual-effect')"]);
}

TEST(AppleVisualEffect, GlassMaterialParsingWithoutUseSystemAppearance)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView _setUseSystemAppearance:NO];

    [webView synchronouslyLoadHTMLString:@"<div id='test'></div>"];
    [webView stringByEvaluatingJavaScript:@"document.querySelector('#test').style.setProperty('-apple-visual-effect', '-apple-system-glass-material')"];

    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"getComputedStyle(document.querySelector('#test')).getPropertyValue('-apple-visual-effect')"]);
}

TEST(AppleVisualEffect, MediaControlsGlassMaterialParsing)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView _setUseSystemAppearance:YES];

    [webView synchronouslyLoadHTMLString:@"<div id='test'></div>"];
    [webView stringByEvaluatingJavaScript:@"document.querySelector('#test').style.setProperty('-apple-visual-effect', '-apple-system-glass-material-media-controls')"];

    EXPECT_WK_STREQ("-apple-system-glass-material-media-controls", [webView stringByEvaluatingJavaScript:@"getComputedStyle(document.querySelector('#test')).getPropertyValue('-apple-visual-effect')"]);
}

TEST(AppleVisualEffect, MediaControlsGlassMaterialParsingWithoutUseSystemAppearance)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView _setUseSystemAppearance:NO];

    [webView synchronouslyLoadHTMLString:@"<div id='test'></div>"];
    [webView stringByEvaluatingJavaScript:@"document.querySelector('#test').style.setProperty('-apple-visual-effect', '-apple-system-glass-material-media-controls')"];

    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"getComputedStyle(document.querySelector('#test')).getPropertyValue('-apple-visual-effect')"]);
}

TEST(AppleVisualEffect, MediaControlsSubduedGlassMaterialParsing)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView _setUseSystemAppearance:YES];

    [webView synchronouslyLoadHTMLString:@"<div id='test'></div>"];
    [webView stringByEvaluatingJavaScript:@"document.querySelector('#test').style.setProperty('-apple-visual-effect', '-apple-system-glass-material-media-controls-subdued')"];

    EXPECT_WK_STREQ("-apple-system-glass-material-media-controls-subdued", [webView stringByEvaluatingJavaScript:@"getComputedStyle(document.querySelector('#test')).getPropertyValue('-apple-visual-effect')"]);
}

TEST(AppleVisualEffect, MediaControlsSubduedGlassMaterialParsingWithoutUseSystemAppearance)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView _setUseSystemAppearance:NO];

    [webView synchronouslyLoadHTMLString:@"<div id='test'></div>"];
    [webView stringByEvaluatingJavaScript:@"document.querySelector('#test').style.setProperty('-apple-visual-effect', '-apple-system-glass-material-media-controls-subdued')"];

    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"getComputedStyle(document.querySelector('#test')).getPropertyValue('-apple-visual-effect')"]);
}

TEST(AppleVisualEffect, SubduedGlassMaterialParsing)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView _setUseSystemAppearance:YES];

    [webView synchronouslyLoadHTMLString:@"<div id='test'></div>"];
    [webView stringByEvaluatingJavaScript:@"document.querySelector('#test').style.setProperty('-apple-visual-effect', '-apple-system-glass-material-subdued')"];

    EXPECT_WK_STREQ("-apple-system-glass-material-subdued", [webView stringByEvaluatingJavaScript:@"getComputedStyle(document.querySelector('#test')).getPropertyValue('-apple-visual-effect')"]);
}

TEST(AppleVisualEffect, SubduedGlassMaterialParsingWithoutUseSystemAppearance)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView _setUseSystemAppearance:NO];

    [webView synchronouslyLoadHTMLString:@"<div id='test'></div>"];
    [webView stringByEvaluatingJavaScript:@"document.querySelector('#test').style.setProperty('-apple-visual-effect', '-apple-system-glass-material-subdued')"];

    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"getComputedStyle(document.querySelector('#test')).getPropertyValue('-apple-visual-effect')"]);
}

TEST(AppleVisualEffect, ClearGlassMaterialParsing)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView _setUseSystemAppearance:YES];

    [webView synchronouslyLoadHTMLString:@"<div id='test'></div>"];
    [webView stringByEvaluatingJavaScript:@"document.querySelector('#test').style.setProperty('-apple-visual-effect', '-apple-system-glass-material-clear')"];

    EXPECT_WK_STREQ("-apple-system-glass-material-clear", [webView stringByEvaluatingJavaScript:@"getComputedStyle(document.querySelector('#test')).getPropertyValue('-apple-visual-effect')"]);
}

TEST(AppleVisualEffect, ClearGlassMaterialParsingWithoutUseSystemAppearance)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView _setUseSystemAppearance:NO];

    [webView synchronouslyLoadHTMLString:@"<div id='test'></div>"];
    [webView stringByEvaluatingJavaScript:@"document.querySelector('#test').style.setProperty('-apple-visual-effect', '-apple-system-glass-material-clear')"];

    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"getComputedStyle(document.querySelector('#test')).getPropertyValue('-apple-visual-effect')"]);
}

TEST(AppleVisualEffect, NoCrashWhenRemovingLayers)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView _setUseSystemAppearance:YES];

    [webView synchronouslyLoadHTMLString:@"<body style='background: red;'><div style='-apple-visual-effect: -apple-system-glass-material; width: 200px; height:  200px; border-radius: 10px;'><p>Test</p></div></body>"];

    [webView waitForNextPresentationUpdate];

    [webView synchronouslyLoadHTMLString:@"<body style='background: blue;'><div style='-apple-visual-effect: -apple-system-glass-material; width: 200px; height:  200px; border-radius: 10px;'><p>Test</p></div></body>"];

    [webView waitForNextPresentationUpdate];
}

#endif // HAVE(MATERIAL_HOSTING)

} // namespace TestWebKitAPI

#endif // HAVE(CORE_MATERIAL)
