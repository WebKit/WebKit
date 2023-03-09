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

#if PLATFORM(MAC)

#import "AppKitSPI.h"
#import "NSFontPanelTesting.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestFontOptions.h"
#import "TestInspectorBar.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>

@interface WKWebView (NSTextInputClient_Async) <NSTextInputClient_Async, NSTextInputClient_Async_Staging_44648564, NSInspectorBarClient>
@end

@interface TestWKWebView (FontManagerTests)

@property (nonatomic, readonly) NSString *selectedText;

- (NSDictionary<NSString *, id> *)typingAttributes;
- (void)selectNextWord;

@end

@interface FontManagerTestWKWebView : TestWKWebView
@end

@implementation FontManagerTestWKWebView

- (NSArray<NSString *> *)inspectorBarItemIdentifiers
{
    return [TestInspectorBar standardTextItemIdentifiers];
}

@end

@implementation TestWKWebView (FontManagerTests)

- (NSDictionary<NSString *, id> *)typingAttributes
{
    __block bool done = false;
    __block RetainPtr<NSDictionary> result;
    [self typingAttributesWithCompletionHandler:^(NSDictionary<NSString *, id> *attributes) {
        result = attributes;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    return result.autorelease();
}

- (NSString *)selectedText
{
    return [self stringByEvaluatingJavaScript:@"getSelection().toString()"];
}

- (void)selectNextWord
{
    [self moveRight:nil];
    [self moveRight:nil];
    [self selectWord:nil];
}

@end

static RetainPtr<FontManagerTestWKWebView> webViewForFontManagerTesting(NSFontManager *fontManager, NSString *markup)
{
    auto webView = adoptNS([[FontManagerTestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadHTMLString:markup];
    [webView stringByEvaluatingJavaScript:@"document.body.focus()"];
    [webView _setEditable:YES];
    [[fontManager fontPanel:YES] setIsVisible:YES];
    fontManager.target = webView.get();
    return webView;
}

static RetainPtr<FontManagerTestWKWebView> webViewForFontManagerTesting(NSFontManager *fontManager)
{
    return webViewForFontManagerTesting(fontManager, @"<body oninput='lastInputEvent = event' contenteditable>"
        "<span id='foo'>foo</span> <span id='bar'>bar</span> <span id='baz'>baz</span>"
        "</body>");
}

static RetainPtr<NSMenuItemCell> menuItemCellForFontAction(NSUInteger tag)
{
    auto menuItem = adoptNS([[NSMenuItem alloc] init]);
    auto menuItemCell = adoptNS([[NSMenuItemCell alloc] init]);
    [menuItemCell setMenuItem:menuItem.get()];
    [menuItemCell setTag:tag];
    return menuItemCell;
}

namespace TestWebKitAPI {

TEST(FontManagerTests, ToggleBoldAndItalicWithMenuItems)
{
    NSFontManager *fontManager = NSFontManager.sharedFontManager;
    auto webView = webViewForFontManagerTesting(fontManager);

    [webView selectWord:nil];
    [webView waitForNextPresentationUpdate];
    [fontManager addFontTrait:menuItemCellForFontAction(NSBoldFontMask).get()];
    EXPECT_WK_STREQ("700", [webView stylePropertyAtSelectionStart:@"font-weight"]);
    EXPECT_WK_STREQ("700", [webView stylePropertyAtSelectionEnd:@"font-weight"]);
    EXPECT_WK_STREQ("Times-Bold", [fontManager selectedFont].fontName);

    [fontManager addFontTrait:menuItemCellForFontAction(NSUnboldFontMask).get()];
    EXPECT_WK_STREQ("400", [webView stylePropertyAtSelectionStart:@"font-weight"]);
    EXPECT_WK_STREQ("400", [webView stylePropertyAtSelectionEnd:@"font-weight"]);
    EXPECT_WK_STREQ("Times-Roman", [fontManager selectedFont].fontName);

    [fontManager addFontTrait:menuItemCellForFontAction(NSItalicFontMask).get()];
    EXPECT_WK_STREQ("italic", [webView stylePropertyAtSelectionStart:@"font-style"]);
    EXPECT_WK_STREQ("italic", [webView stylePropertyAtSelectionEnd:@"font-style"]);
    EXPECT_WK_STREQ("Times-Italic", [fontManager selectedFont].fontName);

    [fontManager addFontTrait:menuItemCellForFontAction(NSUnitalicFontMask).get()];
    EXPECT_WK_STREQ("normal", [webView stylePropertyAtSelectionStart:@"font-style"]);
    EXPECT_WK_STREQ("normal", [webView stylePropertyAtSelectionEnd:@"font-style"]);
    EXPECT_WK_STREQ("Times-Roman", [fontManager selectedFont].fontName);
}

TEST(FontManagerTests, ChangeFontSizeWithMenuItems)
{
    NSFontManager *fontManager = NSFontManager.sharedFontManager;
    auto webView = webViewForFontManagerTesting(fontManager);

    auto sizeIncreaseMenuItemCell = menuItemCellForFontAction(NSSizeUpFontAction);
    auto sizeDecreaseMenuItemCell = menuItemCellForFontAction(NSSizeDownFontAction);

    // Select "foo" and increase font size.
    [webView selectWord:nil];
    [webView waitForNextPresentationUpdate];
    [fontManager modifyFont:sizeIncreaseMenuItemCell.get()];
    [fontManager modifyFont:sizeIncreaseMenuItemCell.get()];

    // Now select "baz" and decrease font size.
    [webView moveToEndOfParagraph:nil];
    [webView selectWord:nil];
    [webView waitForNextPresentationUpdate];
    [fontManager modifyFont:sizeDecreaseMenuItemCell.get()];
    [fontManager modifyFont:sizeDecreaseMenuItemCell.get()];

    // Lastly, select just the "r" in "bar" and increase font size.
    [webView evaluateJavaScript:@"getSelection().setBaseAndExtent(bar.childNodes[0], 2, bar.childNodes[0], 3)" completionHandler:nil];
    [fontManager modifyFont:sizeIncreaseMenuItemCell.get()];

    [webView moveToBeginningOfParagraph:nil];
    [webView selectWord:nil];
    [webView waitForNextPresentationUpdate];
    EXPECT_WK_STREQ(@"foo", [webView selectedText]);
    EXPECT_WK_STREQ(@"18px", [webView stylePropertyAtSelectionStart:@"font-size"]);
    EXPECT_WK_STREQ(@"18px", [webView stylePropertyAtSelectionEnd:@"font-size"]);

    [webView selectNextWord];
    EXPECT_WK_STREQ(@"bar", [webView selectedText]);
    EXPECT_WK_STREQ(@"16px", [webView stylePropertyAtSelectionStart:@"font-size"]);
    EXPECT_WK_STREQ(@"17px", [webView stylePropertyAtSelectionEnd:@"font-size"]);

    [webView selectNextWord];
    EXPECT_WK_STREQ(@"baz", [webView selectedText]);
    EXPECT_WK_STREQ(@"14px", [webView stylePropertyAtSelectionStart:@"font-size"]);
    EXPECT_WK_STREQ(@"14px", [webView stylePropertyAtSelectionEnd:@"font-size"]);
}

TEST(FontManagerTests, ChangeFontWithPanel)
{
    NSFontManager *fontManager = NSFontManager.sharedFontManager;
    auto webView = webViewForFontManagerTesting(fontManager);

    NSFontPanel *fontPanel = [fontManager fontPanel:YES];
    [fontPanel setIsVisible:YES];
    [webView waitForNextPresentationUpdate];

    auto expectSameAttributes = [](NSFont *font1, NSFont *font2) {
        auto fontAttributes1 = adoptNS(font1.fontDescriptor.fontAttributes.mutableCopy);
        auto fontAttributes2 = adoptNS(font2.fontDescriptor.fontAttributes.mutableCopy);
        [fontAttributes1 removeObjectForKey:NSFontVariationAttribute];
        [fontAttributes2 removeObjectForKey:NSFontVariationAttribute];
        BOOL attributesAreEqual = [fontAttributes1 isEqualToDictionary:fontAttributes2.get()];
        EXPECT_TRUE(attributesAreEqual);
        if (!attributesAreEqual)
            NSLog(@"Expected %@ to have the same attributes as %@", font1, font2);
    };

    NSFont *largeHelveticaFont = [NSFont fontWithName:@"Helvetica" size:20];
    [fontPanel setPanelFont:largeHelveticaFont isMultiple:NO];
    [webView selectWord:nil];
    [fontManager modifyFontViaPanel:fontPanel];
    EXPECT_WK_STREQ("foo", [webView selectedText]);
    EXPECT_WK_STREQ("<span id=\"foo\" style=\"font-size: 20px;\"><font face=\"Helvetica\">foo</font></span>", [webView stringByEvaluatingJavaScript:@"foo.outerHTML"]);
    EXPECT_WK_STREQ("Helvetica", [webView stringByEvaluatingJavaScript:@"getComputedStyle(foo.firstChild)['font-family']"]);
    EXPECT_WK_STREQ("20px", [webView stringByEvaluatingJavaScript:@"getComputedStyle(foo.firstChild)['font-size']"]);
    EXPECT_WK_STREQ("400", [webView stringByEvaluatingJavaScript:@"getComputedStyle(foo.firstChild)['font-weight']"]);
    expectSameAttributes(largeHelveticaFont, fontManager.selectedFont);

    NSFont *smallBoldTimesFont = [fontManager fontWithFamily:@"Times New Roman" traits:NSBoldFontMask weight:NSFontWeightBold size:10];
    [fontPanel setPanelFont:smallBoldTimesFont isMultiple:NO];
    [webView selectNextWord];
    [fontManager modifyFontViaPanel:fontPanel];
    EXPECT_WK_STREQ("bar", [webView selectedText]);
    EXPECT_WK_STREQ("<span id=\"bar\"><font face=\"Times New Roman\" size=\"1\"><b>bar</b></font></span>", [webView stringByEvaluatingJavaScript:@"bar.outerHTML"]);
    EXPECT_WK_STREQ("\"Times New Roman\"", [webView stringByEvaluatingJavaScript:@"getComputedStyle(bar.firstChild.firstChild)['font-family']"]);
    EXPECT_WK_STREQ("10px", [webView stringByEvaluatingJavaScript:@"getComputedStyle(bar.firstChild.firstChild)['font-size']"]);
    EXPECT_WK_STREQ("700", [webView stringByEvaluatingJavaScript:@"getComputedStyle(bar.firstChild.firstChild)['font-weight']"]);
    expectSameAttributes(smallBoldTimesFont, fontManager.selectedFont);

    NSFont *boldItalicArialFont = [fontManager fontWithFamily:@"Arial" traits:NSBoldFontMask | NSItalicFontMask weight:NSFontWeightBold size:14];
    [fontPanel setPanelFont:boldItalicArialFont isMultiple:NO];
    [webView selectNextWord];
    [fontManager modifyFontViaPanel:fontPanel];
    EXPECT_WK_STREQ("baz", [webView selectedText]);
    EXPECT_WK_STREQ("<span id=\"baz\" style=\"font-size: 14px;\"><font face=\"Arial\"><b><i>baz</i></b></font></span>", [webView stringByEvaluatingJavaScript:@"baz.outerHTML"]);
    EXPECT_WK_STREQ("Arial", [webView stringByEvaluatingJavaScript:@"getComputedStyle(baz.querySelector('i'))['font-family']"]);
    EXPECT_WK_STREQ("14px", [webView stringByEvaluatingJavaScript:@"getComputedStyle(baz.querySelector('i'))['font-size']"]);
    EXPECT_WK_STREQ("700", [webView stringByEvaluatingJavaScript:@"getComputedStyle(baz.querySelector('i'))['font-weight']"]);
    EXPECT_WK_STREQ("italic", [webView stringByEvaluatingJavaScript:@"getComputedStyle(baz.querySelector('i'))['font-style']"]);
    expectSameAttributes(boldItalicArialFont, fontManager.selectedFont);

    NSFont *largeItalicLightAvenirFont = [fontManager fontWithFamily:@"Avenir" traits:NSItalicFontMask weight:NSFontWeightLight size:24];
    [fontPanel setPanelFont:largeItalicLightAvenirFont isMultiple:NO];
    [webView selectAll:nil];
    [fontManager modifyFontViaPanel:fontPanel];
    EXPECT_WK_STREQ("foo bar baz", [webView selectedText]);
    EXPECT_WK_STREQ("<font face=\"Avenir-LightOblique\" size=\"5\"><i><span id=\"foo\"><font>foo</font></span> <span id=\"bar\">bar</span> <span id=\"baz\"><font>baz</font></span></i></font>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
    EXPECT_WK_STREQ("Avenir-LightOblique", [webView stringByEvaluatingJavaScript:@"getComputedStyle(foo)['font-family']"]);
    EXPECT_WK_STREQ("24px", [webView stringByEvaluatingJavaScript:@"getComputedStyle(foo)['font-size']"]);
    EXPECT_WK_STREQ("400", [webView stringByEvaluatingJavaScript:@"getComputedStyle(foo)['font-weight']"]);
    expectSameAttributes(largeItalicLightAvenirFont, fontManager.selectedFont);
}

TEST(FontManagerTests, ChangeAttributesWithFontEffectsBox)
{
    NSFontManager *fontManager = NSFontManager.sharedFontManager;
    auto webView = webViewForFontManagerTesting(fontManager);

    NSFontPanel *fontPanel = [fontManager fontPanel:YES];
    [fontPanel setIsVisible:YES];
    [webView waitForNextPresentationUpdate];

    [webView selectWord:nil];
    [fontPanel chooseUnderlineMenuItemWithTitle:@"single"];
    EXPECT_WK_STREQ("foo", [webView selectedText]);
    EXPECT_WK_STREQ("<span id=\"foo\"><u>foo</u></span>", [webView stringByEvaluatingJavaScript:@"foo.outerHTML"]);

    [fontPanel chooseUnderlineMenuItemWithTitle:@"none"];
    EXPECT_WK_STREQ("<span id=\"foo\">foo</span>", [webView stringByEvaluatingJavaScript:@"foo.outerHTML"]);

    [webView selectNextWord];
    [fontPanel chooseStrikeThroughMenuItemWithTitle:@"single"];
    EXPECT_WK_STREQ("bar", [webView selectedText]);
    EXPECT_WK_STREQ("<span id=\"bar\"><strike>bar</strike></span>", [webView stringByEvaluatingJavaScript:@"bar.outerHTML"]);
    EXPECT_EQ(NSUnderlineStyleSingle, [[webView typingAttributes][NSStrikethroughStyleAttributeName] intValue]);

    [fontPanel chooseStrikeThroughMenuItemWithTitle:@"none"];
    EXPECT_WK_STREQ("<span id=\"bar\">bar</span>", [webView stringByEvaluatingJavaScript:@"bar.outerHTML"]);
    EXPECT_EQ(NSUnderlineStyleNone, [[webView typingAttributes][NSStrikethroughStyleAttributeName] intValue]);

    [webView selectNextWord];
    fontPanel.shadowBlur = 8;
    fontPanel.shadowOpacity = 1;
    fontPanel.shadowLength = 0.25;
    [fontPanel toggleShadow];
    EXPECT_WK_STREQ("baz", [webView selectedText]);
    EXPECT_WK_STREQ("<span id=\"baz\" style=\"text-shadow: rgb(0, 0, 0) 0px 2.5px 8px;\">baz</span>", [webView stringByEvaluatingJavaScript:@"baz.outerHTML"]);
    {
        NSShadow *shadow = [webView typingAttributes][NSShadowAttributeName];
        EXPECT_EQ(shadow.shadowOffset.width, 0);
        EXPECT_EQ(shadow.shadowOffset.height, 2.5);
        EXPECT_EQ(shadow.shadowBlurRadius, 8);
        EXPECT_TRUE([shadow.shadowColor isEqual:[NSColor colorWithRed:0 green:0 blue:0 alpha:1]]);
    }

    [fontPanel toggleShadow];
    EXPECT_WK_STREQ("<span id=\"baz\">baz</span>", [webView stringByEvaluatingJavaScript:@"baz.outerHTML"]);
    EXPECT_NULL([webView typingAttributes][NSShadowAttributeName]);

    // Now combine all three attributes together.
    [webView selectAll:nil];
    fontPanel.shadowBlur = 5;
    fontPanel.shadowOpacity = 0.2;
    fontPanel.shadowLength = 0.5;
    [fontPanel toggleShadow];
    [fontPanel chooseUnderlineMenuItemWithTitle:@"single"];
    [fontPanel chooseStrikeThroughMenuItemWithTitle:@"single"];
    EXPECT_WK_STREQ("foo bar baz", [webView selectedText]);
    EXPECT_WK_STREQ("<u style=\"text-shadow: rgba(0, 0, 0, 0.2) 0px 5px 5px;\"><strike><span id=\"foo\">foo</span> <span id=\"bar\">bar</span> <span id=\"baz\">baz</span></strike></u>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
    {
        NSDictionary *typingAttributes = [webView typingAttributes];
        EXPECT_EQ(NSUnderlineStyleSingle, [typingAttributes[NSUnderlineStyleAttributeName] intValue]);
        EXPECT_EQ(NSUnderlineStyleSingle, [typingAttributes[NSStrikethroughStyleAttributeName] intValue]);

        NSShadow *shadow = typingAttributes[NSShadowAttributeName];
        EXPECT_EQ(shadow.shadowOffset.width, 0);
        EXPECT_EQ(shadow.shadowOffset.height, 5);
        EXPECT_EQ(shadow.shadowBlurRadius, 5);
        EXPECT_TRUE([shadow.shadowColor isEqual:[NSColor colorWithRed:0 green:0 blue:0 alpha:0.2]]);
    }

    [fontPanel toggleShadow];
    [fontPanel chooseUnderlineMenuItemWithTitle:@"none"];
    [fontPanel chooseStrikeThroughMenuItemWithTitle:@"none"];
    EXPECT_WK_STREQ("<span id=\"foo\">foo</span> <span id=\"bar\">bar</span> <span id=\"baz\">baz</span>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
    EXPECT_EQ(NSUnderlineStyleNone, [[webView typingAttributes][NSStrikethroughStyleAttributeName] intValue]);
    EXPECT_NULL([webView typingAttributes][NSShadowAttributeName]);
}

TEST(FontManagerTests, ChangeFontColorWithColorPanel)
{
    NSColorPanel *colorPanel = NSColorPanel.sharedColorPanel;
    colorPanel.showsAlpha = YES;

    auto webView = webViewForFontManagerTesting(NSFontManager.sharedFontManager);
    auto checkFontColorOfInputEvents = [webView] (const char* colorAsString) {
        EXPECT_WK_STREQ("formatFontColor", [webView stringByEvaluatingJavaScript:@"lastInputEvent.inputType"]);
        EXPECT_WK_STREQ(colorAsString, [webView stringByEvaluatingJavaScript:@"lastInputEvent.data"]);
    };

    // 1. Select "foo" and turn it red; verify that the font element is used for fully opaque colors.
    colorPanel.color = [NSColor colorWithRed:1 green:0 blue:0 alpha:1];
    [webView selectWord:nil];
    [webView waitForNextPresentationUpdate];
    [webView changeColor:colorPanel];
    EXPECT_WK_STREQ("foo", [webView selectedText]);
    EXPECT_WK_STREQ("<span id=\"foo\"><font color=\"#ff0000\">foo</font></span>", [webView stringByEvaluatingJavaScript:@"foo.outerHTML"]);
    checkFontColorOfInputEvents("rgb(255, 0, 0)");
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"!!foo.querySelector('font')"] boolValue]);

    // 2. Now select "bar" and try a few different colors, starting with a color with alpha.
    colorPanel.color = [NSColor colorWithWhite:1 alpha:0.2];
    [webView selectNextWord];
    [webView waitForNextPresentationUpdate];
    [webView changeColor:colorPanel];
    EXPECT_WK_STREQ("bar", [webView selectedText]);
    EXPECT_WK_STREQ("<span id=\"bar\" style=\"color: rgba(255, 255, 255, 0.2);\">bar</span>", [webView stringByEvaluatingJavaScript:@"bar.outerHTML"]);
    checkFontColorOfInputEvents("rgba(255, 255, 255, 0.2)");
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"!!bar.querySelector('font')"] boolValue]);

    // 3a. Switch back to a solid color.
    colorPanel.color = [NSColor colorWithRed:0.8 green:0.7 blue:0.2 alpha:1];
    [webView changeColor:colorPanel];
    EXPECT_WK_STREQ("<span id=\"bar\"><font color=\"#ccb333\">bar</font></span>", [webView stringByEvaluatingJavaScript:@"bar.outerHTML"]);
    checkFontColorOfInputEvents("rgb(204, 179, 51)");
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"!!bar.querySelector('font')"] boolValue]);

    // 3b. Switch back again to a color with alpha.
    colorPanel.color = [NSColor colorWithRed:0.8 green:0.7 blue:0.2 alpha:0.2];
    [webView changeColor:colorPanel];
    EXPECT_WK_STREQ("<span id=\"bar\" style=\"color: rgba(204, 179, 51, 0.2);\">bar</span>", [webView stringByEvaluatingJavaScript:@"bar.outerHTML"]);
    checkFontColorOfInputEvents("rgba(204, 179, 51, 0.2)");
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"!!bar.querySelector('font')"] boolValue]);

    // 4a. Now collapse the selection to the end and set the typing style color to green.
    colorPanel.color = [NSColor colorWithRed:0 green:1 blue:0 alpha:1];
    [webView collapseToEnd];
    [webView changeColor:colorPanel];
    EXPECT_WK_STREQ("formatFontColor", [webView stringByEvaluatingJavaScript:@"lastInputEvent.inputType"]);
    EXPECT_WK_STREQ("rgb(0, 255, 0)", [webView stringByEvaluatingJavaScript:@"lastInputEvent.data"]);

    // 4b. This should result in inserting green text.
    [webView insertText:@"green"];
    [webView moveWordBackward:nil];
    [webView selectWord:nil];
    EXPECT_WK_STREQ("bargreen", [webView selectedText]);
    EXPECT_WK_STREQ("<span id=\"foo\"><font color=\"#ff0000\">foo</font></span> <span id=\"bar\" style=\"color: rgba(204, 179, 51, 0.2);\">bar</span><font color=\"#00ff00\">green</font> <span id=\"baz\">baz</span>",
        [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
}

TEST(FontManagerTests, ChangeTypingAttributesWithInspectorBar)
{
    auto webView = webViewForFontManagerTesting(NSFontManager.sharedFontManager);
    auto inspectorBar = adoptNS([[TestInspectorBar alloc] initWithWebView:webView.get()]);
    {
        [webView selectAll:nil];
        [webView waitForNextPresentationUpdate];
        NSFont *originalFont = [webView typingAttributes][NSFontAttributeName];
        EXPECT_WK_STREQ("Times", originalFont.familyName);
        EXPECT_EQ(16, originalFont.pointSize);
    }
    {
        // Change font family.
        [inspectorBar chooseFontFamily:@"Helvetica"];
        [webView waitForNextPresentationUpdate];
        NSFont *fontAfterSpecifyingHelvetica = [webView typingAttributes][NSFontAttributeName];
        EXPECT_WK_STREQ("Helvetica", fontAfterSpecifyingHelvetica.familyName);
        EXPECT_EQ(16, fontAfterSpecifyingHelvetica.pointSize);
    }
    {
        // Change font size.
        [webView collapseToStart];
        [webView selectWord:nil];
        [inspectorBar chooseFontSize:32];
        [webView collapseToEnd];
        [webView waitForNextPresentationUpdate];
        NSFont *fontAfterDoublingFontSize = [webView typingAttributes][NSFontAttributeName];
        EXPECT_WK_STREQ("Helvetica", fontAfterDoublingFontSize.familyName);
        EXPECT_EQ(32, fontAfterDoublingFontSize.pointSize);
    }
    {
        // Bold, italic, and underline.
        [webView selectNextWord];
        [inspectorBar formatBold:YES];
        [inspectorBar formatItalic:YES];
        [inspectorBar formatUnderline:YES];
        [webView waitForNextPresentationUpdate];
        NSDictionary *attributes = [webView typingAttributes];
        EXPECT_WK_STREQ("Helvetica-BoldOblique", [attributes[NSFontAttributeName] fontName]);
        EXPECT_EQ(16, [attributes[NSFontAttributeName] pointSize]);
        EXPECT_EQ(NSUnderlineStyleSingle, [attributes[NSUnderlineStyleAttributeName] integerValue]);
    }
    {
        // Add foreground and background colors.
        [webView selectNextWord];
        NSColor *foregroundColor = [NSColor colorWithRed:1 green:1 blue:1 alpha:0.2];
        NSColor *backgroundColor = [NSColor colorWithRed:0.8 green:0.2 blue:0.6 alpha:1];
        [inspectorBar chooseForegroundColor:foregroundColor];
        [inspectorBar chooseBackgroundColor:backgroundColor];
        [webView waitForNextPresentationUpdate];
        NSDictionary *attributes = [webView typingAttributes];
        EXPECT_TRUE([attributes[NSForegroundColorAttributeName] isEqual:foregroundColor]);
        EXPECT_TRUE([attributes[NSBackgroundColorAttributeName] isEqual:backgroundColor]);
    }
}

TEST(FontManagerTests, TypingAttributesAfterSubscriptAndSuperscript)
{
    auto webView = webViewForFontManagerTesting(NSFontManager.sharedFontManager);

    [webView moveToBeginningOfDocument:nil];
    [webView selectWord:nil];
    [webView superscript:nil];
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ(1, [[webView typingAttributes][NSSuperscriptAttributeName] integerValue]);

    [webView selectNextWord];
    [webView subscript:nil];
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ(-1, [[webView typingAttributes][NSSuperscriptAttributeName] integerValue]);

    [webView selectNextWord];
    [webView subscript:nil];
    [webView unscript:nil];
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ(0, [[webView typingAttributes][NSSuperscriptAttributeName] integerValue]);
}

TEST(FontManagerTests, AddFontShadowUsingFontOptions)
{
    TestFontOptions *options = TestFontOptions.sharedInstance;
    auto webView = webViewForFontManagerTesting(NSFontManager.sharedFontManager);

    [webView selectWord:nil];
    [webView waitForNextPresentationUpdate];
    options.shadowWidth = 3;
    options.shadowHeight = -3;
    options.hasShadow = YES;

    EXPECT_WK_STREQ("rgba(0, 0, 0, 0.333) 3px -3px 0px", [webView stylePropertyAtSelectionStart:@"text-shadow"]);
    [webView waitForNextPresentationUpdate];
    NSShadow *shadow = [webView typingAttributes][NSShadowAttributeName];
    EXPECT_EQ(shadow.shadowOffset.width, 3);
    EXPECT_EQ(shadow.shadowOffset.height, -3);
}

TEST(FontManagerTests, AddAndRemoveColorsUsingFontOptions)
{
    TestFontOptions *options = TestFontOptions.sharedInstance;
    auto webView = webViewForFontManagerTesting(NSFontManager.sharedFontManager, @"<body contenteditable>hello</body>");
    [webView selectWord:nil];
    [webView waitForNextPresentationUpdate];

    options.backgroundColor = [NSColor colorWithRed:1 green:0 blue:0 alpha:0.2];
    options.foregroundColor = [NSColor colorWithRed:0 green:0 blue:1 alpha:1];

    EXPECT_WK_STREQ("rgb(0, 0, 255)", [webView stylePropertyAtSelectionStart:@"color"]);
    EXPECT_WK_STREQ("rgba(255, 0, 0, 0.2)", [webView stylePropertyAtSelectionStart:@"background-color"]);
    NSDictionary *attributes = [webView typingAttributes];
    EXPECT_TRUE([[NSColor colorWithRed:0 green:0 blue:1 alpha:1] isEqual:attributes[NSForegroundColorAttributeName]]);
    EXPECT_TRUE([[NSColor colorWithRed:1 green:0 blue:0 alpha:0.2] isEqual:attributes[NSBackgroundColorAttributeName]]);

    options.backgroundColor = nil;
    options.foregroundColor = nil;

    EXPECT_WK_STREQ("rgb(0, 0, 0)", [webView stylePropertyAtSelectionStart:@"color"]);
    EXPECT_WK_STREQ("rgba(0, 0, 0, 0)", [webView stylePropertyAtSelectionStart:@"background-color"]);
    attributes = [webView typingAttributes];
    EXPECT_NULL(attributes[NSForegroundColorAttributeName]);
    EXPECT_NULL(attributes[NSBackgroundColorAttributeName]);
}

TEST(FontManagerTests, SetSelectedSystemFontAfterTogglingBold)
{
    NSFontManager *fontManager = NSFontManager.sharedFontManager;

    auto webView = webViewForFontManagerTesting(NSFontManager.sharedFontManager, @"<body style='font-family: system-ui;' contenteditable>Foo</body>");
    [webView selectWord:nil];
    [webView waitForNextPresentationUpdate];
    [webView waitForNextPresentationUpdate];
    auto initialSelectedFont = retainPtr([fontManager selectedFont]);

    [webView _synchronouslyExecuteEditCommand:@"ToggleBold" argument:nil];
    [webView waitForNextPresentationUpdate];
    auto selectedFontAfterBolding = retainPtr([fontManager selectedFont]);

    [webView _synchronouslyExecuteEditCommand:@"ToggleBold" argument:nil];
    [webView waitForNextPresentationUpdate];
    auto selectedFontAfterUnbolding = retainPtr([fontManager selectedFont]);

    [webView _synchronouslyExecuteEditCommand:@"ToggleBold" argument:nil];
    [webView waitForNextPresentationUpdate];
    auto selectedFontAfterBoldingAgain = retainPtr([fontManager selectedFont]);

    EXPECT_WK_STREQ([initialSelectedFont fontName], [selectedFontAfterUnbolding fontName]);
    EXPECT_WK_STREQ([selectedFontAfterBolding fontName], [selectedFontAfterBoldingAgain fontName]);
    EXPECT_FALSE([[initialSelectedFont fontName] isEqual:[selectedFontAfterBolding fontName]]);
    EXPECT_EQ([initialSelectedFont pointSize], 16.);
    EXPECT_EQ([selectedFontAfterBolding pointSize], 16.);
    EXPECT_EQ([selectedFontAfterUnbolding pointSize], 16.);
    EXPECT_EQ([selectedFontAfterBoldingAgain pointSize], 16.);
}

TEST(FontManagerTests, ObservingFontPanelShouldNotCrashWhenUnparentingViewTwice)
{
    NSFontManager *fontManager = NSFontManager.sharedFontManager;
    auto webView = webViewForFontManagerTesting(fontManager);

    [webView removeFromSuperview];
    [webView addToTestWindow];
    [webView removeFromSuperview];
    [webView addToTestWindow];

    [webView selectWord:nil];
    [webView waitForNextPresentationUpdate];
    [fontManager addFontTrait:menuItemCellForFontAction(NSBoldFontMask).get()];
    EXPECT_WK_STREQ("700", [webView stylePropertyAtSelectionStart:@"font-weight"]);
    EXPECT_WK_STREQ("700", [webView stylePropertyAtSelectionEnd:@"font-weight"]);
    EXPECT_WK_STREQ("Times-Bold", [fontManager selectedFont].fontName);

}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
