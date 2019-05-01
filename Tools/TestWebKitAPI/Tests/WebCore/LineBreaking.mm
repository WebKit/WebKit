/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#import "TestNavigationDelegate.h"
#import "Utilities.h"
#import <unicode/ubrk.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>
#import <wtf/text/WTFString.h>

NSString *generateJavaScriptForTest(NSString *testContent, NSString *localeString, NSString *lineBreakValue)
{
    return [NSString stringWithFormat:@"runTest('%@', '%@', '%@')", testContent, localeString, lineBreakValue];
}

Vector<unsigned> breakingLocationsFromICU(const Vector<UInt16>& testString, const String& locale, const String lineBreakValue)
{
    constexpr int bufferSize = 100;
    char buffer[bufferSize];
    memset(buffer, 0, bufferSize);
    auto utf8Locale = locale.utf8();
    ASSERT(utf8Locale.length() < bufferSize);
    memcpy(buffer, utf8Locale.data(), utf8Locale.length());
    CString icuValue;
    if (lineBreakValue != "auto")
        icuValue = lineBreakValue.utf8();
    UErrorCode status = U_ZERO_ERROR;
    uloc_setKeywordValue("lb", icuValue.data(), buffer, bufferSize, &status);
    ASSERT(U_SUCCESS(status));

    UBreakIterator* iterator = ubrk_open(UBRK_LINE, buffer, testString.data(), testString.size(), &status);
    ASSERT(U_SUCCESS(status));
    ASSERT(iterator);

    Vector<unsigned> result;
    int32_t position = 0;
    for (; position != UBRK_DONE; position = ubrk_next(iterator)) {
        if (position)
            result.append(position);
    }
    ubrk_close(iterator);
    return result;
}

static void testAFewStrings(Vector<Vector<UInt16>> testStrings)
{
    Vector<String> locales = { "en", "ja", "ko", "zh" };

    Vector<String> lineBreakValues = { "auto", "loose", "normal", "strict" };

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"LineBreaking" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];
    [webView _test_waitForDidFinishNavigation];

    for (auto& testCodePoints : testStrings) {
        NSString *testString = [NSString stringWithCharacters:testCodePoints.data() length:testCodePoints.size()];
        for (auto& locale : locales) {
            for (auto& lineBreakValue : lineBreakValues) {
                __block bool didEvaluateJavaScript = false;
                __block Vector<unsigned> webkitLocations;
                [webView evaluateJavaScript:generateJavaScriptForTest(testString, locale, lineBreakValue) completionHandler:^(id value, NSError *error) {
                    for (NSNumber *v in (NSArray *)value)
                        webkitLocations.append(floor([v floatValue] + 0.5));
                    didEvaluateJavaScript = true;
                }];
                TestWebKitAPI::Util::run(&didEvaluateJavaScript);

                const auto& icuLocations = breakingLocationsFromICU(testCodePoints, locale, lineBreakValue);
                EXPECT_EQ(icuLocations.size(), webkitLocations.size());
                if (icuLocations.size() != webkitLocations.size())
                    break;
                for (size_t i = 0; i < icuLocations.size(); ++i)
                    EXPECT_EQ(icuLocations[i], webkitLocations[i]);
            }
        }
    }
}

// Split up the tests because they take too long otherwise and people start thinking their computer is hung.
TEST(WebKit, LineBreaking1)
{
    Vector<Vector<UInt16>> testStrings = {
        {0x0024, 0x31, 0x32, 0x33},
        {0x00a3, 0x31, 0x32, 0x33},
        {0x00a5, 0x31, 0x32, 0x33},
        {0x20ac, 0x31, 0x32, 0x33},
        {0x2116, 0x31, 0x32, 0x33},
    };
    testAFewStrings(testStrings);
}

TEST(WebKit, LineBreaking2)
{
    Vector<Vector<UInt16>> testStrings = {
        {0x4e00, 0x2025, 0x2025},
        {0x4e00, 0x2025, 0x2026},
        {0x4e00, 0x2026, 0x2025},
        {0x4e00, 0x2026, 0x2026},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x0021},
    };
    testAFewStrings(testStrings);
}

TEST(WebKit, LineBreaking3)
{
    Vector<Vector<UInt16>> testStrings = {
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x0025},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x003a},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x003b},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x003f},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x00a2},
    };
    testAFewStrings(testStrings);
}

TEST(WebKit, LineBreaking4)
{
    Vector<Vector<UInt16>> testStrings = {
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x00b0},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x2010},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x2013},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x2030},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x2032},
    };
    testAFewStrings(testStrings);
}

TEST(WebKit, LineBreaking5)
{
    Vector<Vector<UInt16>> testStrings = {
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x2033},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x203c},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x2047},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x2048},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x2049},
    };
    testAFewStrings(testStrings);
}

TEST(WebKit, LineBreaking6)
{
    Vector<Vector<UInt16>> testStrings = {
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x2103},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x3005},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x301c},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x303b},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x3041},
    };
    testAFewStrings(testStrings);
}

TEST(WebKit, LineBreaking7)
{
    Vector<Vector<UInt16>> testStrings = {
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x3043},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x3045},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x3047},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x3049},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x3063},
    };
    testAFewStrings(testStrings);
}

TEST(WebKit, LineBreaking8)
{
    Vector<Vector<UInt16>> testStrings = {
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x3083},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x3085},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x3087},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x308e},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x3095},
    };
    testAFewStrings(testStrings);
}

TEST(WebKit, LineBreaking9)
{
    Vector<Vector<UInt16>> testStrings = {
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x3096},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x309d},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x309e},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x30a0},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x30a1},
    };
    testAFewStrings(testStrings);
}

TEST(WebKit, LineBreaking10)
{
    Vector<Vector<UInt16>> testStrings = {
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x30a3},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x30a5},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x30a7},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x30a9},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x30c3},
    };
    testAFewStrings(testStrings);
}

TEST(WebKit, LineBreaking11)
{
    Vector<Vector<UInt16>> testStrings = {
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x30e3},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x30e5},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x30e7},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x30ee},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x30f5},
    };
    testAFewStrings(testStrings);
}

TEST(WebKit, LineBreaking12)
{
    Vector<Vector<UInt16>> testStrings = {
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x30f6},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x30fb},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x30fc},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x30fd},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x30fe},
    };
    testAFewStrings(testStrings);
}

TEST(WebKit, LineBreaking13)
{
    Vector<Vector<UInt16>> testStrings = {
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x31f0},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x31f1},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x31f2},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x31f3},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x31f4},
    };
    testAFewStrings(testStrings);
}

TEST(WebKit, LineBreaking14)
{
    Vector<Vector<UInt16>> testStrings = {
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x31f5},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x31f6},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x31f7},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x31f8},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x31f9},
    };
    testAFewStrings(testStrings);
}

TEST(WebKit, LineBreaking15)
{
    Vector<Vector<UInt16>> testStrings = {
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x31fa},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x31fb},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x31fc},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x31fd},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x31fe},
    };
    testAFewStrings(testStrings);
}

TEST(WebKit, LineBreaking16)
{
    Vector<Vector<UInt16>> testStrings = {
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x31ff},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0xff01},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0xff05},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0xff1a},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0xff1b},
    };
    testAFewStrings(testStrings);
}

TEST(WebKit, LineBreaking17)
{
    Vector<Vector<UInt16>> testStrings = {
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0xff1f},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0xff65},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0xff67},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0xff68},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0xff69},
    };
    testAFewStrings(testStrings);
}

TEST(WebKit, LineBreaking18)
{
    Vector<Vector<UInt16>> testStrings = {
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0xff6a},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0xff6b},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0xff6c},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0xff6d},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0xff6e},
    };
    testAFewStrings(testStrings);
}

TEST(WebKit, LineBreaking19)
{
    Vector<Vector<UInt16>> testStrings = {
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0xff6f},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0xff70},
        {0x4e00, 0x4e8c, 0x4e09, 0x56db, 0xffe0},
        {0xff04, 0x31, 0x32, 0x33},
        {0xffe1, 0x31, 0x32, 0x33},
        {0xffe5, 0x31, 0x32, 0x33}
    };
    testAFewStrings(testStrings);
}
