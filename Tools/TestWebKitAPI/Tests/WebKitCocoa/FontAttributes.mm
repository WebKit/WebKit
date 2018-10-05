/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if PLATFORM(COCOA) && WK_API_ENABLED

#import "NSFontPanelTesting.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import <WebKit/WKUIDelegatePrivate.h>
#import <cmath>
#import <wtf/Optional.h>

@interface FontAttributesListener : NSObject <WKUIDelegatePrivate>
@property (nonatomic, readonly) NSDictionary *lastFontAttributes;
@end

@implementation FontAttributesListener {
    RetainPtr<NSDictionary> _lastFontAttributes;
}

- (void)_webView:(WKWebView *)webView didChangeFontAttributes:(NSDictionary<NSString *, id> *)fontAttributes
{
    _lastFontAttributes = fontAttributes;
}

- (NSDictionary *)lastFontAttributes
{
    return _lastFontAttributes.get();
}

@end

@interface TestWKWebView (FontAttributesTesting)
- (void)selectElementWithIdentifier:(NSString *)identifier;
- (NSDictionary *)fontAttributesAfterNextPresentationUpdate;
@end

@implementation TestWKWebView (FontAttributesTesting)

- (void)selectElementWithIdentifier:(NSString *)identifier
{
    [self objectByEvaluatingJavaScript:[NSString stringWithFormat:
        @"element = document.getElementById('%@');"
        "range = document.createRange();"
        "range.selectNodeContents(element);"
        "getSelection().removeAllRanges();"
        "getSelection().addRange(range)", identifier]];
}

- (NSDictionary *)fontAttributesAfterNextPresentationUpdate
{
    [self waitForNextPresentationUpdate];
    return [(FontAttributesListener *)self.UIDelegate lastFontAttributes];
}

@end

#if PLATFORM(MAC)
#define PlatformColor NSColor
#define PlatformFont NSFont
#else
#define PlatformColor UIColor
#define PlatformFont UIFont
static NSString *const NSSuperscriptAttributeName = @"NSSuperscript";
#endif

namespace TestWebKitAPI {

enum class Nullity : uint8_t { Null, NonNull };

struct ColorExpectation {
    ColorExpectation(CGFloat redValue, CGFloat greenValue, CGFloat blueValue, CGFloat alphaValue)
        : red(redValue)
        , green(greenValue)
        , blue(blueValue)
        , alpha(alphaValue)
        , nullity(Nullity::NonNull)
    {
    }

    ColorExpectation() = default;

    CGFloat red { 0 };
    CGFloat green { 0 };
    CGFloat blue { 0 };
    CGFloat alpha { 0 };
    Nullity nullity { Nullity::Null };
};

struct ShadowExpectation {
    ShadowExpectation(CGFloat opacityValue, CGFloat blurRadiusValue)
        : opacity(opacityValue)
        , blurRadius(blurRadiusValue)
        , nullity(Nullity::NonNull)
    {
    }

    ShadowExpectation() = default;

    CGFloat opacity { 0 };
    CGFloat blurRadius { 0 };
    Nullity nullity { Nullity::Null };
};

struct FontExpectation {
    const char* fontName;
    CGFloat fontSize;
};

static void checkColor(PlatformColor *color, ColorExpectation&& expectation)
{
    if (expectation.nullity == Nullity::Null) {
        EXPECT_NULL(color);
        return;
    }

    EXPECT_NOT_NULL(color);

    CGFloat observedRed = 0;
    CGFloat observedGreen = 0;
    CGFloat observedBlue = 0;
    CGFloat observedAlpha = 0;
    [color getRed:&observedRed green:&observedGreen blue:&observedBlue alpha:&observedAlpha];
    EXPECT_EQ(expectation.red, std::round(observedRed * 255));
    EXPECT_EQ(expectation.green, std::round(observedGreen * 255));
    EXPECT_EQ(expectation.blue, std::round(observedBlue * 255));
    EXPECT_LT(std::abs(expectation.alpha - observedAlpha), 0.0001);
}

static void checkShadow(NSShadow *shadow, ShadowExpectation&& expectation)
{
    if (expectation.nullity == Nullity::Null) {
        EXPECT_NULL(shadow);
        return;
    }

    EXPECT_NOT_NULL(shadow);

    CGFloat observedAlpha = 0;
    [shadow.shadowColor getRed:nullptr green:nullptr blue:nullptr alpha:&observedAlpha];
    EXPECT_LT(std::abs(expectation.opacity - observedAlpha), 0.0001);
    EXPECT_EQ(expectation.blurRadius, shadow.shadowBlurRadius);
}

static void checkFont(PlatformFont *font, FontExpectation&& expectation)
{
    EXPECT_WK_STREQ(expectation.fontName, font.fontName);
    EXPECT_EQ(expectation.fontSize, font.pointSize);
}

static RetainPtr<TestWKWebView> webViewForTestingFontAttributes()
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"rich-text-attributes"];
    [webView stringByEvaluatingJavaScript:@"document.body.focus()"];
    return webView;
}

TEST(FontAttributes, FontAttributesAfterChangingSelection)
{
    auto delegate = adoptNS([FontAttributesListener new]);
    auto webView = webViewForTestingFontAttributes();
    [webView setUIDelegate:delegate.get()];

    {
        [webView selectElementWithIdentifier:@"one"];
        NSDictionary *attributes = [webView fontAttributesAfterNextPresentationUpdate];
        checkColor(attributes[NSForegroundColorAttributeName], { 227, 36, 0, 1 });
        checkColor(attributes[NSBackgroundColorAttributeName], { 255, 199, 119, 1 });
        checkFont(attributes[NSFontAttributeName], { "Helvetica-Bold", 48 });
        checkShadow(attributes[NSShadowAttributeName], { });
        EXPECT_EQ(NSUnderlineStyleSingle, [attributes[NSStrikethroughStyleAttributeName] integerValue]);
        EXPECT_EQ(NSUnderlineStyleNone, [attributes[NSUnderlineStyleAttributeName] integerValue]);
        EXPECT_EQ(0, [attributes[NSSuperscriptAttributeName] integerValue]);
    }
    {
        [webView selectElementWithIdentifier:@"two"];
        NSDictionary *attributes = [webView fontAttributesAfterNextPresentationUpdate];
        checkColor(attributes[NSForegroundColorAttributeName], { 102, 157, 52, 1 });
        checkColor(attributes[NSBackgroundColorAttributeName], { 255, 197, 171, 1 });
        checkFont(attributes[NSFontAttributeName], { "Helvetica-Bold", 48 });
        checkShadow(attributes[NSShadowAttributeName], { 0.470588, 5 });
        EXPECT_EQ(NSUnderlineStyleNone, [attributes[NSStrikethroughStyleAttributeName] integerValue]);
        EXPECT_EQ(NSUnderlineStyleSingle, [attributes[NSUnderlineStyleAttributeName] integerValue]);
        EXPECT_EQ(0, [attributes[NSSuperscriptAttributeName] integerValue]);
    }
    {
        [webView selectElementWithIdentifier:@"three"];
        NSDictionary *attributes = [webView fontAttributesAfterNextPresentationUpdate];
        checkColor(attributes[NSForegroundColorAttributeName], { 255, 106, 0, 1 });
        checkColor(attributes[NSBackgroundColorAttributeName], { });
        checkFont(attributes[NSFontAttributeName], { "Menlo-Italic", 18 });
        checkShadow(attributes[NSShadowAttributeName], { });
        EXPECT_EQ(NSUnderlineStyleNone, [attributes[NSStrikethroughStyleAttributeName] integerValue]);
        EXPECT_EQ(NSUnderlineStyleNone, [attributes[NSUnderlineStyleAttributeName] integerValue]);
        EXPECT_EQ(0, [attributes[NSSuperscriptAttributeName] integerValue]);
    }
    {
        [webView selectElementWithIdentifier:@"four"];
        NSDictionary *attributes = [webView fontAttributesAfterNextPresentationUpdate];
        checkColor(attributes[NSForegroundColorAttributeName], { 255, 255, 255, 1 });
        checkColor(attributes[NSBackgroundColorAttributeName], { 0, 0, 0, 1 });
        checkFont(attributes[NSFontAttributeName], { "Avenir-Book", 24 });
        checkShadow(attributes[NSShadowAttributeName], { });
        EXPECT_EQ(NSUnderlineStyleNone, [attributes[NSStrikethroughStyleAttributeName] integerValue]);
        EXPECT_EQ(NSUnderlineStyleNone, [attributes[NSUnderlineStyleAttributeName] integerValue]);
        EXPECT_EQ(0, [attributes[NSSuperscriptAttributeName] integerValue]);
    }
    {
        [webView selectElementWithIdentifier:@"five"];
        NSDictionary *attributes = [webView fontAttributesAfterNextPresentationUpdate];
        checkColor(attributes[NSForegroundColorAttributeName], { 131, 16, 0, 1 });
        checkColor(attributes[NSBackgroundColorAttributeName], { });
        checkFont(attributes[NSFontAttributeName], { "TimesNewRomanPS-BoldMT", 24 });
        checkShadow(attributes[NSShadowAttributeName], { });
        EXPECT_EQ(NSUnderlineStyleNone, [attributes[NSStrikethroughStyleAttributeName] integerValue]);
        EXPECT_EQ(NSUnderlineStyleNone, [attributes[NSUnderlineStyleAttributeName] integerValue]);
        EXPECT_EQ(0, [attributes[NSSuperscriptAttributeName] integerValue]);
    }
    {
        [webView selectElementWithIdentifier:@"six"];
        NSDictionary *attributes = [webView fontAttributesAfterNextPresentationUpdate];
        checkColor(attributes[NSForegroundColorAttributeName], { 255, 64, 19, 1 });
        checkColor(attributes[NSBackgroundColorAttributeName], { });
        checkFont(attributes[NSFontAttributeName], { "Avenir-Black", 18 });
        checkShadow(attributes[NSShadowAttributeName], { });
        EXPECT_EQ(NSUnderlineStyleNone, [attributes[NSStrikethroughStyleAttributeName] integerValue]);
        EXPECT_EQ(NSUnderlineStyleNone, [attributes[NSUnderlineStyleAttributeName] integerValue]);
        EXPECT_EQ(0, [attributes[NSSuperscriptAttributeName] integerValue]);
    }
    {
        [webView selectElementWithIdentifier:@"seven"];
        NSDictionary *attributes = [webView fontAttributesAfterNextPresentationUpdate];
        checkColor(attributes[NSForegroundColorAttributeName], { 235, 235, 235, 1 });
        checkColor(attributes[NSBackgroundColorAttributeName], { 78, 122, 39, 1 });
        checkFont(attributes[NSFontAttributeName], { "Avenir-BookOblique", 12 });
        checkShadow(attributes[NSShadowAttributeName], { });
        EXPECT_EQ(NSUnderlineStyleNone, [attributes[NSStrikethroughStyleAttributeName] integerValue]);
        EXPECT_EQ(NSUnderlineStyleNone, [attributes[NSUnderlineStyleAttributeName] integerValue]);
        EXPECT_EQ(-1, [attributes[NSSuperscriptAttributeName] integerValue]);
    }
    {
        [webView selectElementWithIdentifier:@"eight"];
        NSDictionary *attributes = [webView fontAttributesAfterNextPresentationUpdate];
        checkColor(attributes[NSForegroundColorAttributeName], { });
        checkColor(attributes[NSBackgroundColorAttributeName], { });
        checkFont(attributes[NSFontAttributeName], { "Avenir-Book", 12 });
        checkShadow(attributes[NSShadowAttributeName], { });
        EXPECT_EQ(NSUnderlineStyleNone, [attributes[NSStrikethroughStyleAttributeName] integerValue]);
        EXPECT_EQ(NSUnderlineStyleNone, [attributes[NSUnderlineStyleAttributeName] integerValue]);
        EXPECT_EQ(1, [attributes[NSSuperscriptAttributeName] integerValue]);
    }
    {
        [webView selectElementWithIdentifier:@"nine"];
        NSDictionary *attributes = [webView fontAttributesAfterNextPresentationUpdate];
        checkColor(attributes[NSForegroundColorAttributeName], { });
        checkColor(attributes[NSBackgroundColorAttributeName], { });
        checkFont(attributes[NSFontAttributeName], { "Georgia", 36 });
        checkShadow(attributes[NSShadowAttributeName], { 0.658824, 9 });
        EXPECT_EQ(NSUnderlineStyleNone, [attributes[NSStrikethroughStyleAttributeName] integerValue]);
        EXPECT_EQ(NSUnderlineStyleNone, [attributes[NSUnderlineStyleAttributeName] integerValue]);
        EXPECT_EQ(0, [attributes[NSSuperscriptAttributeName] integerValue]);
    }
    {
        [webView selectElementWithIdentifier:@"ten"];
        NSDictionary *attributes = [webView fontAttributesAfterNextPresentationUpdate];
        checkColor(attributes[NSForegroundColorAttributeName], { });
        checkColor(attributes[NSBackgroundColorAttributeName], { });
        checkFont(attributes[NSFontAttributeName], { "Avenir-BookOblique", 18 });
        checkShadow(attributes[NSShadowAttributeName], { });
        EXPECT_EQ(NSUnderlineStyleNone, [attributes[NSStrikethroughStyleAttributeName] integerValue]);
        EXPECT_EQ(NSUnderlineStyleNone, [attributes[NSUnderlineStyleAttributeName] integerValue]);
        EXPECT_EQ(0, [attributes[NSSuperscriptAttributeName] integerValue]);
    }
}

} // namespace TestWebKitAPI

#endif
