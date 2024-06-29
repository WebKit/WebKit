/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if ENABLE(UNIFIED_PDF_BY_DEFAULT)

#import "CGImagePixelReader.h"
#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebCore/ColorSerialization.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/_WKFeature.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

#if PLATFORM(MAC)

static constexpr auto defaultSamplingInterval = 100;
static Vector<WebCore::Color> sampleColorsInWebView(TestWKWebView *webView, unsigned interval = defaultSamplingInterval)
{
    [webView waitForNextPresentationUpdate];

    Vector<WebCore::Color> samples;
    CGImagePixelReader reader { [webView snapshotAfterScreenUpdates] };
    for (unsigned x = interval; x < reader.width() - interval; x += interval) {
        for (unsigned y = interval; y < reader.height() - interval; y += interval)
            samples.append(reader.at(x, y));
    }
    return samples;
}

TEST(UnifiedPDF, KeyboardScrollingInSinglePageMode)
{
    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 600, 600) configuration:configuration.get() addToWindow:YES]);
    [webView setForceWindowToBecomeKey:YES];

    RetainPtr request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"multiple-pages" withExtension:@"pdf" subdirectory:@"TestWebKitAPI.resources"]];
    [webView synchronouslyLoadRequest:request.get()];
    [[webView window] makeFirstResponder:webView.get()];
    [[webView window] makeKeyAndOrderFront:nil];
    [[webView window] orderFrontRegardless];
    [webView objectByEvaluatingJavaScript:@"internals.setPDFDisplayModeForTesting(document.querySelector('embed'), 'SinglePageDiscrete')"];
    [webView waitForNextPresentationUpdate];
    [webView setMagnification:2];

    auto colorsBeforeScrolling = sampleColorsInWebView(webView.get());
    Vector<WebCore::Color> colorsAfterScrollingDown;
    while (true) {
        [webView sendKey:@"ArrowDown" code:NSDownArrowFunctionKey isDown:YES modifiers:0];
        Util::runFor(200_ms);
        [webView sendKey:@"ArrowDown" code:NSDownArrowFunctionKey isDown:NO modifiers:0];
        Util::runFor(50_ms);
        colorsAfterScrollingDown = sampleColorsInWebView(webView.get());
        if (colorsBeforeScrolling != colorsAfterScrollingDown)
            break;
    }

    Vector<WebCore::Color> colorsAfterScrollingRight;
    while (true) {
        [webView sendKey:@"ArrowRight" code:NSRightArrowFunctionKey isDown:YES modifiers:0];
        Util::runFor(200_ms);
        [webView sendKey:@"ArrowRight" code:NSRightArrowFunctionKey isDown:NO modifiers:0];
        Util::runFor(50_ms);
        colorsAfterScrollingRight = sampleColorsInWebView(webView.get());
        if (colorsAfterScrollingDown != colorsAfterScrollingRight)
            break;
    }
}

#endif // PLATFORM(MAC)

TEST(UnifiedPDF, CopyEditingCommandOnEmptySelectionShouldNotCrash)
{
    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    for (_WKFeature *feature in [WKPreferences _features]) {
        if ([feature.key isEqualToString:@"UnifiedPDFEnabled"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 600, 600) configuration:configuration.get() addToWindow:YES]);
    [webView setForceWindowToBecomeKey:YES];

    RetainPtr request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"multiple-pages" withExtension:@"pdf" subdirectory:@"TestWebKitAPI.resources"]];
    [webView synchronouslyLoadRequest:request.get()];
    [[webView window] makeFirstResponder:webView.get()];
    [[webView window] makeKeyAndOrderFront:nil];
    [[webView window] orderFrontRegardless];

    [webView sendClickAtPoint:NSMakePoint(200, 200)];
    [webView objectByEvaluatingJavaScript:@"internals.sendEditingCommandToPDFForTesting(document.querySelector('embed'), 'copy')"];
}

} // namespace TestWebKitAPI

#endif // ENABLE(UNIFIED_PDF_BY_DEFAULT)
