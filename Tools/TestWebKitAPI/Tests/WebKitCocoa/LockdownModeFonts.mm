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

#import "config.h"

#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"

namespace TestWebKitAPI {

static RetainPtr<NSString> EncodeResourceAsArrayString(NSString* resource, NSString* extension)
{
    NSURL *resourceURL = [NSBundle.test_resourcesBundle URLForResource:resource withExtension:extension];
    NSData *resourceData = [NSData dataWithContentsOfURL:resourceURL];

    NSError *error = nil;
    NSMutableArray<NSNumber *> *array = [NSMutableArray arrayWithCapacity:resourceData.length];
    const auto* resourceBytes = static_cast<const uint8_t*>(resourceData.bytes);
    for (NSUInteger i = 0; i < resourceData.length; ++i)
        [array addObject:[NSNumber numberWithUnsignedChar:resourceBytes[i]]];

    NSData *json = [NSJSONSerialization dataWithJSONObject:array options:0 error:&error];
    return adoptNS([[NSString alloc] initWithData:json encoding:NSUTF8StringEncoding]);
}

static void loadFontFace(WKWebView *webView, NSString *fontFamily, NSString *encodedFontData, NSString *targetElement, NSString *fontFamilyValue)
{
    [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@""
        "{"
        "  window.fontLoadComplete = false;"
        "  let fontData = new Uint8Array(%@);"
        "  let font = new FontFace('%@', fontData);"
        "  void font.load().then(() => {"
        "    document.fonts.add(font);"
        "    %@.style.setProperty('font-family', '%@');"
        "    window.fontLoadComplete = true;"
        "  }).catch(() => {"
        "    %@.style.setProperty('font-family', '%@');"
        "    window.fontLoadComplete = true;"
        "  });"
        "}", encodedFontData, fontFamily, targetElement, fontFamilyValue, targetElement, fontFamilyValue]];
}

static void waitForFontLoad(WKWebView *webView)
{
    Util::waitForConditionWithLogging([&] {
        return [[webView objectByEvaluatingJavaScript:@"window.fontLoadComplete"] boolValue];
    }, 3, @"Expected font to load.");
}

// rdar://136524076
#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 150000
TEST(LockdownMode, DISABLED_SVGFonts)
#else
TEST(LockdownMode, SVGFonts)
#endif
{
    auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    webViewConfiguration.get().defaultWebpagePreferences.lockdownModeEnabled = YES;
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    NSURL *url = [NSBundle.test_resourcesBundle URLForResource:@"SVGFont" withExtension:@"html"];
    [webView loadRequest:[NSURLRequest requestWithURL:url]];
    [webView _test_waitForDidFinishNavigation];

    auto target1Result = static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"document.getElementById('target1').offsetWidth"]).intValue;
    auto target2Result = static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"document.getElementById('target2').offsetWidth"]).intValue;
    auto referenceResult = static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"document.getElementById('reference').offsetWidth"]).intValue;
    EXPECT_EQ(target1Result, referenceResult);
    EXPECT_EQ(target2Result, referenceResult);
}

TEST(LockdownMode, NotAllowedFontLoadingAPI)
{
    @autoreleasepool {
        auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
        webViewConfiguration.get().defaultWebpagePreferences.lockdownModeEnabled = YES;
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
        NSURL *url = [NSBundle.test_resourcesBundle URLForResource:@"ImmediateFont" withExtension:@"html"];
        [webView loadRequest:[NSURLRequest requestWithURL:url]];
        [webView _test_waitForDidFinishNavigation];

        auto encoded = EncodeResourceAsArrayString(@"Ahem", @"ttf");

        [webView objectByEvaluatingJavaScript:@""
            "let target = document.getElementById('target');"
            "let reference = document.getElementById('reference');"];
        auto beforeTargetResult = static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"target.offsetWidth"]).intValue;

        loadFontFace(webView.get(), @"WebFont", encoded.get(), @"target", @"WebFont, Helvetica");
        waitForFontLoad(webView.get());

        auto targetResult = static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"target.offsetWidth"]).intValue;
        auto referenceResult = static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"reference.offsetWidth"]).intValue;

        EXPECT_NE(beforeTargetResult, targetResult);
    // FIXME: (webkit.org/b/290478) We should expose the safe font parser setting to here and make the next assert conditional to it.
#if HAVE(CTFONTMANAGER_CREATEMEMORYSAFEFONTDESCRIPTORFROMDATA)
        EXPECT_NE(targetResult, referenceResult);
#else
        EXPECT_EQ(targetResult, referenceResult);
#endif
    }
}

TEST(LockdownMode, AllowedFontLoadingAPI)
{
    @autoreleasepool {
        auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
        webViewConfiguration.get().defaultWebpagePreferences.lockdownModeEnabled = YES;
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
        NSURL *url = [NSBundle.test_resourcesBundle URLForResource:@"ImmediateFont" withExtension:@"html"];
        [webView loadRequest:[NSURLRequest requestWithURL:url]];
        [webView _test_waitForDidFinishNavigation];

        auto encoded = EncodeResourceAsArrayString(@"Ahem-10000A", @"ttf");

        [webView objectByEvaluatingJavaScript:@""
            "let target = document.getElementById('target');"
            "let reference = document.getElementById('reference');"];
        auto beforeTargetResult = static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"target.offsetWidth"]).intValue;

        loadFontFace(webView.get(), @"WebFont", encoded.get(), @"target", @"WebFont, Helvetica");
        waitForFontLoad(webView.get());

        auto targetResult = static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"target.offsetWidth"]).intValue;
        auto referenceResult = static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"reference.offsetWidth"]).intValue;

        EXPECT_NE(beforeTargetResult, targetResult);
        EXPECT_NE(targetResult, referenceResult);
    }
}

TEST(LockdownMode, NotSupportedFontLoadingAPI)
{
    @autoreleasepool {
        auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
        webViewConfiguration.get().defaultWebpagePreferences.lockdownModeEnabled = YES;
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
        NSURL *url = [NSBundle.test_resourcesBundle URLForResource:@"ImmediateFont" withExtension:@"html"];
        [webView loadRequest:[NSURLRequest requestWithURL:url]];
        [webView _test_waitForDidFinishNavigation];

        auto encoded = EncodeResourceAsArrayString(@"Ahem-CFF", @"otf");

        [webView objectByEvaluatingJavaScript:@""
            "let target = document.getElementById('target');"
            "let reference = document.getElementById('reference');"];
        auto beforeTargetResult = static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"target.offsetWidth"]).intValue;

        loadFontFace(webView.get(), @"WebFont", encoded.get(), @"target", @"WebFont, Helvetica");
        waitForFontLoad(webView.get());

        auto targetResult = static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"target.offsetWidth"]).intValue;
        auto referenceResult = static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"reference.offsetWidth"]).intValue;

        EXPECT_NE(beforeTargetResult, targetResult);
        EXPECT_EQ(targetResult, referenceResult);
    }
}

TEST(LockdownMode, AllowedFont)
{
    @autoreleasepool {
        auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
        webViewConfiguration.get().defaultWebpagePreferences.lockdownModeEnabled = YES;
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
        NSURL *url = [NSBundle.test_resourcesBundle URLForResource:@"LockdownModeFonts" withExtension:@"html"];
        [webView loadRequest:[NSURLRequest requestWithURL:url]];
        [webView _test_waitForDidFinishNavigation];

        [webView objectByEvaluatingJavaScript:@""
            "let target = document.getElementById('target');"
            "let reference = document.getElementById('reference');"];
        auto targetResult = static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"target.offsetWidth"]).intValue;
        auto referenceResult = static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"reference.offsetWidth"]).intValue;

        EXPECT_NE(targetResult, referenceResult);
    }
}

TEST(LockdownMode, NotAllowedFont)
{
    @autoreleasepool {
        auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
        webViewConfiguration.get().defaultWebpagePreferences.lockdownModeEnabled = YES;
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
        NSURL *url = [NSBundle.test_resourcesBundle URLForResource:@"LockdownModeFonts" withExtension:@"html"];
        [webView loadRequest:[NSURLRequest requestWithURL:url]];
        [webView _test_waitForDidFinishNavigation];

        [webView objectByEvaluatingJavaScript:@""
            "let target = document.getElementById('target-not-allowed');"
            "let reference = document.getElementById('reference');"];
        auto targetResult = static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"target.offsetWidth"]).intValue;
        auto referenceResult = static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"reference.offsetWidth"]).intValue;

    // FIXME: (webkit.org/b/290478) We should expose the safe font parser setting to here and make the next assert conditional to it.
#if HAVE(CTFONTMANAGER_CREATEMEMORYSAFEFONTDESCRIPTORFROMDATA)
        EXPECT_NE(targetResult, referenceResult);
#else
        EXPECT_EQ(targetResult, referenceResult);
#endif
    }
}

#if HAVE(CTFONTMANAGER_CREATEMEMORYSAFEFONTDESCRIPTORFROMDATA)
TEST(LockdownMode, ImmediateParsedViaSafeFontParser)
#else
TEST(LockdownMode, DISABLED_ImmediateParsedViaSafeFontParser)
#endif
{
    @autoreleasepool {
        auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
        webViewConfiguration.get().defaultWebpagePreferences.lockdownModeEnabled = YES;
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
        NSURL *url = [NSBundle.test_resourcesBundle URLForResource:@"ImmediateFont" withExtension:@"html"];
        [webView loadRequest:[NSURLRequest requestWithURL:url]];
        [webView _test_waitForDidFinishNavigation];

        // Check a valid font can be loaded as an immediate source first.
        auto encoded = EncodeResourceAsArrayString(@"Ahem", @"ttf");

        [webView objectByEvaluatingJavaScript:@""
            "let target = document.getElementById('target');"
            "let reference = document.getElementById('reference');"];

        auto beforeTargetResult = static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"target.offsetWidth"]).intValue;

        loadFontFace(webView.get(), @"WebFont", encoded.get(), @"target", @"WebFont, Helvetica");
        waitForFontLoad(webView.get());

        auto targetResult = static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"target.offsetWidth"]).intValue;
        auto referenceResult = static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"reference.offsetWidth"]).intValue;

        EXPECT_NE(beforeTargetResult, targetResult);
        EXPECT_NE(targetResult, referenceResult);

        // Switch both target and reference to use the new WebFont, which uses a distinct offsetWidth value to the
        // canary font (if it loads successfully).
        [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@""
            "target.style.setProperty('font-family', 'WebFont');"
            "reference.style.setProperty('font-family', 'WebFont');"]];

        targetResult = static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"target.offsetWidth"]).intValue;
        referenceResult = static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"reference.offsetWidth"]).intValue;

        EXPECT_EQ(targetResult, referenceResult);

        // Now, attempt to load the 'canary' font in the same way. This fails to load in SafeFontParser, but succeeds
        // in FontParser, giving us some signal of which was used.
        auto canaryEncoded = EncodeResourceAsArrayString(@"SafeFontParser-Invalid", @"ttf");

        loadFontFace(webView.get(), @"CanaryFont", canaryEncoded.get(), @"target", @"CanaryFont, WebFont");
        waitForFontLoad(webView.get());

        targetResult = static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"target.offsetWidth"]).intValue;

        // If the canary font loads successfully then these will not be equal. If this does start to fail either
        // (1) we have regressed and are parsing via system font parser, or (2) SafeFontParser has been updated and
        // can handle this font file correctly now.
        EXPECT_EQ(targetResult, referenceResult);
    }
}

#if HAVE(CTFONTMANAGER_CREATEMEMORYSAFEFONTDESCRIPTORFROMDATA)
TEST(LockdownMode, CanaryFontSucceedsInFontParser)
#else
TEST(LockdownMode, DISABLED_CanaryFontSucceedsInFontParser)
#endif
{
    @autoreleasepool {
        auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
        NSURL *url = [NSBundle.test_resourcesBundle URLForResource:@"ImmediateFont" withExtension:@"html"];
        [webView loadRequest:[NSURLRequest requestWithURL:url]];
        [webView _test_waitForDidFinishNavigation];

        // This font file was custom crafted to contain a minimal header which FontParser will consider valid, but
        // SafeFontParser will not, giving us a parser difference to be able to test against. This is used in other
        // tests, but we should ensure that FontParser changes do not cause this font to suddenly fail. If they did,
        // our other tests would not be able to distinguish FontParser to SafeFontParser correctly.
        auto encoded = EncodeResourceAsArrayString(@"SafeFontParser-Invalid", @"ttf");

        [webView objectByEvaluatingJavaScript:@""
            "let target = document.getElementById('target');"
            "let reference = document.getElementById('reference');"];

        auto beforeTargetResult = static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"target.offsetWidth"]).intValue;

        loadFontFace(webView.get(), @"WebFont", encoded.get(), @"target", @"WebFont, Helvetica");
        waitForFontLoad(webView.get());

        auto targetResult = static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"target.offsetWidth"]).intValue;
        auto referenceResult = static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"reference.offsetWidth"]).intValue;

        EXPECT_NE(beforeTargetResult, targetResult);
        EXPECT_NE(targetResult, referenceResult);
    }
}

#if HAVE(CTFONTMANAGER_CREATEMEMORYSAFEFONTDESCRIPTORFROMDATA)
TEST(LockdownMode, WorkerFontParsedViaSafeFontParser)
#else
TEST(LockdownMode, DISABLED_WorkerFontParsedViaSafeFontParser)
#endif
{
    @autoreleasepool {
        // The Worker font load request does not succeed with file:// protocols, so we have to use the HTTPServer for this one.
        auto workerTestPageFileURL = [NSBundle.test_resourcesBundle pathForResource:@"SafeFontParser-Worker" ofType:@"html"];
        auto workerTestPage = [NSString stringWithContentsOfFile:workerTestPageFileURL encoding:NSUTF8StringEncoding error:NULL];

        auto ahemFontData = [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"Ahem" withExtension:@"ttf"]];
        auto canaryFontData = [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"SafeFontParser-Invalid" withExtension:@"ttf"]];

        HTTPServer server { { }, HTTPServer::Protocol::Http };
        server.addResponse("/SafeFontParserWorker.html"_s, { workerTestPage });
        server.addResponse("/Ahem.ttf"_s, { ahemFontData });
        server.addResponse("/SafeFontParser-invalid.ttf"_s, { canaryFontData });

        auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
        webViewConfiguration.get().defaultWebpagePreferences.lockdownModeEnabled = YES;

        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);

        auto workerTestPageURL = [NSURL URLWithString:[[NSString alloc] initWithFormat:@"http://localhost:%u/SafeFontParserWorker.html", server.port()]];
        [webView loadRequest:[NSURLRequest requestWithURL:workerTestPageURL]];
        [webView _test_waitForDidFinishNavigation];

        // Ensure the valid font loads correctly first, otherwise failures may be due to some other issue!
        auto fontString = [NSString stringWithFormat:@"http://localhost:%u/Ahem.ttf", server.port()];
        RetainPtr<NSURL> fontURL = [NSURL URLWithString:fontString];

        [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@""
            "fontWorker.postMessage({"
            "  fontFamily: 'Ahem',"
            "  fontUrl: 'url(%@)',"
            "  options: {}"
            "});", fontURL.get().absoluteString]];

        // When the worker completes the load, the result (success, or error message) is stored in fontLoadResult
        NSString* result;
        Util::waitForConditionWithLogging([&] {
            result = [webView stringByEvaluatingJavaScript:@"fontLoadResult"];
            return ![result isEqualToString:@"(null)"];
        }, 3, @"Attempt to load supported font failed to complete.");

        EXPECT_STREQ("success", [result UTF8String]);

        // Reset this as we're about to do another load.
        [webView stringByEvaluatingJavaScript:@"fontLoadResult = undefined;"];

        // Check an invalid font fails to parse
        auto invalidFontString = [NSString stringWithFormat:@"http://localhost:%u/SafeFontParser-invalid.ttf", server.port()];
        RetainPtr<NSURL> invalidFontURL = [NSURL URLWithString:invalidFontString];

        [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@""
            "fontWorker.postMessage({"
            "  fontFamily: 'InvalidFont',"
            "  fontUrl: 'url(%@)',"
            "  options: {}"
            "});", invalidFontURL.get().absoluteString]];

        Util::waitForConditionWithLogging([&] {
            result = [webView stringByEvaluatingJavaScript:@"fontLoadResult"];
            return ![result isEqualToString:@"(null)"];
        }, 3, @"Attempt to load the invalid font failed to complete.");

        // This isn't a terribly helpful error, but it is all we get from a failed font load. By checking a valid
        // font loads above, hopefully if this begins to fail it indicates either (1) we've regressed and are now
        // handling this font file outside of SafeFontParser, or (2) SafeFontParser has updated and now handles this
        // canary font file correctly.
        EXPECT_STREQ(" A network error occurred.", [result UTF8String]);
    }
}
}
