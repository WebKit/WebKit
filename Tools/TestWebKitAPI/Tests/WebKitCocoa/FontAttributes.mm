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

#if PLATFORM(COCOA)

#import "NSFontPanelTesting.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebCore/ColorCocoa.h>
#import <WebCore/FontCocoa.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <cmath>
#import <pal/spi/cf/CoreTextSPI.h>
#import <pal/spi/cocoa/NSAttributedStringSPI.h>
#import <wtf/Vector.h>

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPI.h"
#endif

@interface FontTextStyleUIDelegate : NSObject <WKUIDelegatePrivate> {
@public
    bool _willSetFont;
    bool _done;
}
@end

@implementation FontTextStyleUIDelegate

- (void)_webView:(WKWebView *)webView didChangeFontAttributes:(NSDictionary<NSString *, id> *)fontAttributes
{
    NSString *fontTextStyle = [[[fontAttributes objectForKey:@"NSFont"] fontDescriptor] objectForKey:(__bridge NSString *)kCTFontDescriptorTextStyleAttribute];

    if (_willSetFont) {
        EXPECT_WK_STREQ("UICTFontTextStyleTitle1", fontTextStyle);
        _done = YES;
    }
}

@end

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

namespace TestWebKitAPI {

struct ListExpectation {
    NSString *markerFormat { nil };
    int startingItemNumber { 0 };
};

struct ParagraphStyleExpectation {
    NSTextAlignment alignment { NSTextAlignmentNatural };
    Vector<ListExpectation> textLists;
};

struct ColorExpectation {
    CGFloat red { 0 };
    CGFloat green { 0 };
    CGFloat blue { 0 };
    CGFloat alpha { 0 };
};

struct ShadowExpectation {
    CGFloat opacity { 0 };
    CGFloat blurRadius { 0 };
};

struct FontExpectation {
    const char* fontName { nullptr };
    CGFloat fontSize { 0 };
};

static void checkColor(WebCore::CocoaColor *color, std::optional<ColorExpectation> expectation)
{
    if (!expectation) {
        EXPECT_NULL(color);
        return;
    }

    EXPECT_NOT_NULL(color);

    CGFloat observedRed = 0;
    CGFloat observedGreen = 0;
    CGFloat observedBlue = 0;
    CGFloat observedAlpha = 0;
    [color getRed:&observedRed green:&observedGreen blue:&observedBlue alpha:&observedAlpha];
    EXPECT_EQ(expectation->red, std::round(observedRed * 255));
    EXPECT_EQ(expectation->green, std::round(observedGreen * 255));
    EXPECT_EQ(expectation->blue, std::round(observedBlue * 255));
    EXPECT_LT(std::abs(expectation->alpha - observedAlpha), 0.0001);
}

static void checkShadow(NSShadow *shadow, std::optional<ShadowExpectation> expectation)
{
    if (!expectation) {
        EXPECT_NULL(shadow);
        return;
    }

    EXPECT_NOT_NULL(shadow);

    CGFloat observedAlpha = 0;
    [shadow.shadowColor getRed:nullptr green:nullptr blue:nullptr alpha:&observedAlpha];
    EXPECT_LT(std::abs(expectation->opacity - observedAlpha), 0.0001);
    EXPECT_EQ(expectation->blurRadius, shadow.shadowBlurRadius);
}

static void checkFont(WebCore::CocoaFont *font, FontExpectation expectation)
{
    EXPECT_WK_STREQ(expectation.fontName, font.fontName);
    EXPECT_EQ(expectation.fontSize, font.pointSize);
}

static void checkParagraphStyles(NSParagraphStyle *style, ParagraphStyleExpectation&& expectation)
{
    EXPECT_EQ(expectation.alignment, style.alignment);
    EXPECT_EQ(expectation.textLists.size(), style.textLists.count);
    [style.textLists enumerateObjectsUsingBlock:^(NSTextList *textList, NSUInteger index, BOOL *stop) {
        if (index >= expectation.textLists.size()) {
            *stop = YES;
            return;
        }
        auto& expectedList = expectation.textLists[index];
        EXPECT_EQ(expectedList.startingItemNumber, textList.startingItemNumber);
        EXPECT_WK_STREQ(expectedList.markerFormat, textList.markerFormat);
    }];
}

static RetainPtr<TestWKWebView> webViewForTestingFontAttributes(NSString *testPageName)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:testPageName];
    [webView stringByEvaluatingJavaScript:@"document.body.focus()"];
#if PLATFORM(MAC)
    NSFontManager *fontManager = NSFontManager.sharedFontManager;
    [[fontManager fontPanel:YES] setIsVisible:YES];
    fontManager.target = webView.get();
#endif
    return webView;
}

TEST(FontAttributes, FontAttributesAfterChangingSelection)
{
    auto delegate = adoptNS([FontAttributesListener new]);
    auto webView = webViewForTestingFontAttributes(@"rich-text-attributes");
    [webView setUIDelegate:delegate.get()];

    auto expectFontManagerState = [] (FontExpectation expectedFont, ColorExpectation expectedColor, std::optional<ShadowExpectation> expectedShadow, BOOL underline, BOOL strikeThrough, BOOL expectMultipleFonts) {
#if PLATFORM(MAC)
        NSFontManager *fontManager = NSFontManager.sharedFontManager;
        NSFontPanel *fontPanel = NSFontPanel.sharedFontPanel;
        if (expectedShadow) {
            EXPECT_TRUE(fontPanel.hasShadow);
            EXPECT_LT(std::abs(expectedShadow->opacity - fontPanel.shadowOpacity), 0.0001);
            EXPECT_EQ(expectedShadow->blurRadius, fontPanel.shadowBlur);
        } else
            EXPECT_FALSE(fontPanel.hasShadow);
        EXPECT_EQ(underline, fontPanel.hasUnderline);
        EXPECT_EQ(strikeThrough, fontPanel.hasStrikeThrough);
        checkColor([fontPanel.foregroundColor colorUsingColorSpace:NSColorSpace.sRGBColorSpace], { WTFMove(expectedColor) });
        checkFont(fontManager.selectedFont, WTFMove(expectedFont));
        EXPECT_EQ(expectMultipleFonts, fontManager.multiple);
#else
        UNUSED_PARAM(expectedFont);
        UNUSED_PARAM(expectedColor);
        UNUSED_PARAM(expectedShadow);
        UNUSED_PARAM(underline);
        UNUSED_PARAM(strikeThrough);
        UNUSED_PARAM(expectMultipleFonts);
#endif
    };

    {
        [webView selectElementWithIdentifier:@"one"];
        NSDictionary *attributes = [webView fontAttributesAfterNextPresentationUpdate];
        checkColor(attributes[NSForegroundColorAttributeName], { { 227, 36, 0, 1 } });
        checkColor(attributes[NSBackgroundColorAttributeName], { { 255, 199, 119, 1 } });
        checkFont(attributes[NSFontAttributeName], { "Helvetica-Bold", 48 });
        checkShadow(attributes[NSShadowAttributeName], std::nullopt);
        checkParagraphStyles(attributes[NSParagraphStyleAttributeName], { NSTextAlignmentNatural, { } });
        EXPECT_EQ(NSUnderlineStyleSingle, [attributes[NSStrikethroughStyleAttributeName] integerValue]);
        EXPECT_EQ(NSUnderlineStyleNone, [attributes[NSUnderlineStyleAttributeName] integerValue]);
        EXPECT_EQ(0, [attributes[NSSuperscriptAttributeName] integerValue]);
        expectFontManagerState({ "Helvetica-Bold", 48 }, { 227, 36, 0, 1 }, std::nullopt, NO, YES, NO);
    }
    {
        [webView selectElementWithIdentifier:@"two"];
        NSDictionary *attributes = [webView fontAttributesAfterNextPresentationUpdate];
        checkColor(attributes[NSForegroundColorAttributeName], { { 102, 157, 52, 1 } });
        checkColor(attributes[NSBackgroundColorAttributeName], { { 255, 197, 171, 1 } });
        checkFont(attributes[NSFontAttributeName], { "Helvetica-Bold", 48 });
        checkShadow(attributes[NSShadowAttributeName], { { 0.470588, 5 } });
        checkParagraphStyles(attributes[NSParagraphStyleAttributeName], { NSTextAlignmentNatural, { } });
        EXPECT_EQ(NSUnderlineStyleNone, [attributes[NSStrikethroughStyleAttributeName] integerValue]);
        EXPECT_EQ(NSUnderlineStyleSingle, [attributes[NSUnderlineStyleAttributeName] integerValue]);
        EXPECT_EQ(0, [attributes[NSSuperscriptAttributeName] integerValue]);
        expectFontManagerState({ "Helvetica-Bold", 48 }, { 102, 157, 52, 1 }, { { 0.470588, 5 } }, YES, NO, NO);
    }
    {
        [webView selectElementWithIdentifier:@"three"];
        NSDictionary *attributes = [webView fontAttributesAfterNextPresentationUpdate];
        checkColor(attributes[NSForegroundColorAttributeName], { { 255, 106, 0, 1 } });
        checkColor(attributes[NSBackgroundColorAttributeName], std::nullopt);
        checkFont(attributes[NSFontAttributeName], { "Menlo-Italic", 18 });
        checkShadow(attributes[NSShadowAttributeName], { });
        checkParagraphStyles(attributes[NSParagraphStyleAttributeName], { NSTextAlignmentCenter, { } });
        EXPECT_EQ(NSUnderlineStyleNone, [attributes[NSStrikethroughStyleAttributeName] integerValue]);
        EXPECT_EQ(NSUnderlineStyleNone, [attributes[NSUnderlineStyleAttributeName] integerValue]);
        EXPECT_EQ(0, [attributes[NSSuperscriptAttributeName] integerValue]);
        expectFontManagerState({ "Menlo-Italic", 18 }, { 255, 106, 0, 1 }, std::nullopt, NO, NO, NO);
    }
    {
        [webView selectElementWithIdentifier:@"four"];
        NSDictionary *attributes = [webView fontAttributesAfterNextPresentationUpdate];
        checkColor(attributes[NSForegroundColorAttributeName], { { 255, 255, 255, 1 } });
        checkColor(attributes[NSBackgroundColorAttributeName], { { 0, 0, 0, 1 } });
        checkFont(attributes[NSFontAttributeName], { "Avenir-Book", 24 });
        checkShadow(attributes[NSShadowAttributeName], std::nullopt);
        checkParagraphStyles(attributes[NSParagraphStyleAttributeName], { NSTextAlignmentCenter, { } });
        EXPECT_EQ(NSUnderlineStyleNone, [attributes[NSStrikethroughStyleAttributeName] integerValue]);
        EXPECT_EQ(NSUnderlineStyleNone, [attributes[NSUnderlineStyleAttributeName] integerValue]);
        EXPECT_EQ(0, [attributes[NSSuperscriptAttributeName] integerValue]);
        expectFontManagerState({ "Avenir-Book", 24 }, { 255, 255, 255, 1 }, std::nullopt, NO, NO, NO);
    }
    {
        [webView selectElementWithIdentifier:@"five"];
        NSDictionary *attributes = [webView fontAttributesAfterNextPresentationUpdate];
        checkColor(attributes[NSForegroundColorAttributeName], { { 131, 16, 0, 1 } });
        checkColor(attributes[NSBackgroundColorAttributeName], std::nullopt);
        checkFont(attributes[NSFontAttributeName], { "TimesNewRomanPS-BoldMT", 24 });
        checkShadow(attributes[NSShadowAttributeName], std::nullopt);
        checkParagraphStyles(attributes[NSParagraphStyleAttributeName], { NSTextAlignmentCenter, { } });
        EXPECT_EQ(NSUnderlineStyleNone, [attributes[NSStrikethroughStyleAttributeName] integerValue]);
        EXPECT_EQ(NSUnderlineStyleNone, [attributes[NSUnderlineStyleAttributeName] integerValue]);
        EXPECT_EQ(0, [attributes[NSSuperscriptAttributeName] integerValue]);
        expectFontManagerState({ "TimesNewRomanPS-BoldMT", 24 }, { 131, 16, 0, 1 }, std::nullopt, NO, NO, NO);
    }
    {
        [webView selectElementWithIdentifier:@"six"];
        NSDictionary *attributes = [webView fontAttributesAfterNextPresentationUpdate];
        checkColor(attributes[NSForegroundColorAttributeName], { { 255, 64, 19, 1 } });
        checkColor(attributes[NSBackgroundColorAttributeName], std::nullopt);
        checkFont(attributes[NSFontAttributeName], { "Avenir-Black", 18 });
        checkShadow(attributes[NSShadowAttributeName], { });
        checkParagraphStyles(attributes[NSParagraphStyleAttributeName], { NSTextAlignmentLeft, {{ NSTextListMarkerDisc, 1 }} });
        EXPECT_EQ(NSUnderlineStyleNone, [attributes[NSStrikethroughStyleAttributeName] integerValue]);
        EXPECT_EQ(NSUnderlineStyleNone, [attributes[NSUnderlineStyleAttributeName] integerValue]);
        EXPECT_EQ(0, [attributes[NSSuperscriptAttributeName] integerValue]);
        expectFontManagerState({ "Avenir-Black", 18 }, { 255, 64, 19, 1 }, std::nullopt, NO, NO, NO);
    }
    {
        [webView selectElementWithIdentifier:@"seven"];
        NSDictionary *attributes = [webView fontAttributesAfterNextPresentationUpdate];
        checkColor(attributes[NSForegroundColorAttributeName], { { 235, 235, 235, 1 } });
        checkColor(attributes[NSBackgroundColorAttributeName], { { 78, 122, 39, 1 } });
        checkFont(attributes[NSFontAttributeName], { "Avenir-BookOblique", 12 });
        checkShadow(attributes[NSShadowAttributeName], std::nullopt);
        checkParagraphStyles(attributes[NSParagraphStyleAttributeName], { NSTextAlignmentLeft, {{ NSTextListMarkerDisc, 1 }} });
        EXPECT_EQ(NSUnderlineStyleNone, [attributes[NSStrikethroughStyleAttributeName] integerValue]);
        EXPECT_EQ(NSUnderlineStyleNone, [attributes[NSUnderlineStyleAttributeName] integerValue]);
        EXPECT_EQ(-1, [attributes[NSSuperscriptAttributeName] integerValue]);
        expectFontManagerState({ "Avenir-BookOblique", 12 }, { 235, 235, 235, 1 }, std::nullopt, NO, NO, NO);
    }
    {
        [webView selectElementWithIdentifier:@"eight"];
        NSDictionary *attributes = [webView fontAttributesAfterNextPresentationUpdate];
        checkColor(attributes[NSForegroundColorAttributeName], std::nullopt);
        checkColor(attributes[NSBackgroundColorAttributeName], std::nullopt);
        checkFont(attributes[NSFontAttributeName], { "Avenir-Book", 12 });
        checkShadow(attributes[NSShadowAttributeName], std::nullopt);
        checkParagraphStyles(attributes[NSParagraphStyleAttributeName], { NSTextAlignmentLeft, {{ NSTextListMarkerDecimal, 1 }} });
        EXPECT_EQ(NSUnderlineStyleNone, [attributes[NSStrikethroughStyleAttributeName] integerValue]);
        EXPECT_EQ(NSUnderlineStyleNone, [attributes[NSUnderlineStyleAttributeName] integerValue]);
        EXPECT_EQ(1, [attributes[NSSuperscriptAttributeName] integerValue]);
        expectFontManagerState({ "Avenir-Book", 12 }, { 0, 0, 0, 1 }, std::nullopt, NO, NO, NO);
    }
    {
        [webView selectElementWithIdentifier:@"nine"];
        NSDictionary *attributes = [webView fontAttributesAfterNextPresentationUpdate];
        checkColor(attributes[NSForegroundColorAttributeName], std::nullopt);
        checkColor(attributes[NSBackgroundColorAttributeName], std::nullopt);
        checkFont(attributes[NSFontAttributeName], { "Georgia", 36 });
        checkShadow(attributes[NSShadowAttributeName], { { 0.658824, 9 } });
        checkParagraphStyles(attributes[NSParagraphStyleAttributeName], { NSTextAlignmentLeft, {{ NSTextListMarkerDecimal, 1 }} });
        EXPECT_EQ(NSUnderlineStyleNone, [attributes[NSStrikethroughStyleAttributeName] integerValue]);
        EXPECT_EQ(NSUnderlineStyleNone, [attributes[NSUnderlineStyleAttributeName] integerValue]);
        EXPECT_EQ(0, [attributes[NSSuperscriptAttributeName] integerValue]);
        expectFontManagerState({ "Georgia", 36 }, { 0, 0, 0, 1 }, { { 0.658824, 9 } }, NO, NO, NO);
    }
    {
        [webView selectElementWithIdentifier:@"ten"];
        NSDictionary *attributes = [webView fontAttributesAfterNextPresentationUpdate];
        checkColor(attributes[NSForegroundColorAttributeName], std::nullopt);
        checkColor(attributes[NSBackgroundColorAttributeName], std::nullopt);
        checkFont(attributes[NSFontAttributeName], { "Avenir-BookOblique", 18 });
        checkShadow(attributes[NSShadowAttributeName], std::nullopt);
        checkParagraphStyles(attributes[NSParagraphStyleAttributeName], { NSTextAlignmentRight, { } });
        EXPECT_EQ(NSUnderlineStyleNone, [attributes[NSStrikethroughStyleAttributeName] integerValue]);
        EXPECT_EQ(NSUnderlineStyleNone, [attributes[NSUnderlineStyleAttributeName] integerValue]);
        EXPECT_EQ(0, [attributes[NSSuperscriptAttributeName] integerValue]);
        expectFontManagerState({ "Avenir-BookOblique", 18 }, { 0, 0, 0, 1 }, std::nullopt, NO, NO, NO);
    }
    {
        [webView selectElementWithIdentifier:@"eleven"];
        NSDictionary *attributes = [webView fontAttributesAfterNextPresentationUpdate];
        checkColor(attributes[NSForegroundColorAttributeName], std::nullopt);
        checkColor(attributes[NSBackgroundColorAttributeName], std::nullopt);
        checkFont(attributes[NSFontAttributeName], { "Menlo-Regular", 20 });
        checkShadow(attributes[NSShadowAttributeName], std::nullopt);
        checkParagraphStyles(attributes[NSParagraphStyleAttributeName], { NSTextAlignmentRight, { } });
        EXPECT_EQ(NSUnderlineStyleNone, [attributes[NSStrikethroughStyleAttributeName] integerValue]);
        EXPECT_EQ(NSUnderlineStyleNone, [attributes[NSUnderlineStyleAttributeName] integerValue]);
        EXPECT_EQ(0, [attributes[NSSuperscriptAttributeName] integerValue]);
        expectFontManagerState({ "Menlo-Regular", 20 }, { 0, 0, 0, 1 }, std::nullopt, NO, NO, NO);
    }
#if PLATFORM(MAC)
    [webView selectAll:nil];
    [webView waitForNextPresentationUpdate];
    EXPECT_TRUE(NSFontManager.sharedFontManager.multiple);
#endif
}

TEST(FontAttributes, NestedTextListsWithHorizontalAlignment)
{
    auto delegate = adoptNS([FontAttributesListener new]);
    auto webView = webViewForTestingFontAttributes(@"nested-lists");
    [webView setUIDelegate:delegate.get()];

    [webView selectElementWithIdentifier:@"one"];
    checkParagraphStyles([webView fontAttributesAfterNextPresentationUpdate][NSParagraphStyleAttributeName], {
        NSTextAlignmentNatural,
        {{ NSTextListMarkerDecimal, 1 }}
    });

    [webView selectElementWithIdentifier:@"two"];
    checkParagraphStyles([webView fontAttributesAfterNextPresentationUpdate][NSParagraphStyleAttributeName], {
        NSTextAlignmentLeft,
        {{ NSTextListMarkerDecimal, 1 }, { NSTextListMarkerCircle, 1 }}
    });

    [webView selectElementWithIdentifier:@"three"];
    checkParagraphStyles([webView fontAttributesAfterNextPresentationUpdate][NSParagraphStyleAttributeName], {
        NSTextAlignmentCenter,
        {{ NSTextListMarkerDecimal, 1 }, { NSTextListMarkerCircle, 1 }, { NSTextListMarkerUppercaseRoman, 50 }}
    });

    [webView selectElementWithIdentifier:@"four"];
    checkParagraphStyles([webView fontAttributesAfterNextPresentationUpdate][NSParagraphStyleAttributeName], {
        NSTextAlignmentLeft,
        {{ NSTextListMarkerDecimal, 1 }, { NSTextListMarkerCircle, 1 }}
    });

    [webView selectElementWithIdentifier:@"five"];
    checkParagraphStyles([webView fontAttributesAfterNextPresentationUpdate][NSParagraphStyleAttributeName], {
        NSTextAlignmentRight,
        {{ NSTextListMarkerDecimal, 1 }, { NSTextListMarkerCircle, 1 }, { NSTextListMarkerLowercaseLatin, 0 }}
    });

    [webView selectElementWithIdentifier:@"six"];
    checkParagraphStyles([webView fontAttributesAfterNextPresentationUpdate][NSParagraphStyleAttributeName], {
        NSTextAlignmentLeft,
        {{ NSTextListMarkerDecimal, 1 }}
    });

    [webView selectElementWithIdentifier:@"seven"];
    checkParagraphStyles([webView fontAttributesAfterNextPresentationUpdate][NSParagraphStyleAttributeName], {
        NSTextAlignmentNatural,
        { }
    });
}

TEST(FontAttributes, FontTextStyle)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    auto uiDelegate = adoptNS([[FontTextStyleUIDelegate alloc] init]);
    [webView setUIDelegate:uiDelegate.get()];
    [webView synchronouslyLoadHTMLString:@"<body contenteditable>"
        "<span id='foo'>foo</span> <span id='bar'>bar</span> <span id='baz'>baz</span>"
        "</body>"];
    [webView stringByEvaluatingJavaScript:@"document.body.focus()"];
    [webView _setEditable:YES];

    [webView _synchronouslyExecuteEditCommand:@"SelectWord" argument:nil];
    [webView waitForNextPresentationUpdate];

    uiDelegate->_willSetFont = YES;

#if PLATFORM(MAC)
    [webView _setFont:[NSFont preferredFontForTextStyle:NSFontTextStyleTitle1 options:@{ }] sender:nil];
    EXPECT_WK_STREQ("22px", [webView stylePropertyAtSelectionStart:@"font-size"]);
#else
    [webView _setFont:[UIFont preferredFontForTextStyle:UIFontTextStyleTitle1] sender:nil];
    EXPECT_WK_STREQ("28px", [webView stylePropertyAtSelectionStart:@"font-size"]);
#endif

    EXPECT_WK_STREQ("UICTFontTextStyleTitle1", [webView stylePropertyAtSelectionStart:@"font-family"]);

    TestWebKitAPI::Util::run(&uiDelegate->_done);
}

} // namespace TestWebKitAPI

#endif
