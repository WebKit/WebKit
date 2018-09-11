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

#if PLATFORM(MAC) && WK_API_ENABLED

#import "AppKitSPI.h"
#import "NSFontPanelTesting.h"
#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>

@interface TestWKWebView (FontManagerTests)

@property (nonatomic, readonly) NSString *selectedText;

- (NSString *)stylePropertyAtSelectionStart:(NSString *)propertyName;
- (NSString *)stylePropertyAtSelectionEnd:(NSString *)propertyName;
- (void)selectNextWord;
- (void)collapseToEnd;

@end

@implementation TestWKWebView (FontManagerTests)

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

- (void)collapseToEnd
{
    [self evaluateJavaScript:@"getSelection().collapseToEnd()" completionHandler:nil];
}

- (NSString *)stylePropertyAtSelectionStart:(NSString *)propertyName
{
    NSString *script = [NSString stringWithFormat:@"getComputedStyle(getSelection().getRangeAt(0).startContainer.parentElement)['%@']", propertyName];
    return [self stringByEvaluatingJavaScript:script];
}

- (NSString *)stylePropertyAtSelectionEnd:(NSString *)propertyName
{
    NSString *script = [NSString stringWithFormat:@"getComputedStyle(getSelection().getRangeAt(0).endContainer.parentElement)['%@']", propertyName];
    return [self stringByEvaluatingJavaScript:script];
}

@end

static RetainPtr<TestWKWebView> webViewForFontManagerTesting(NSFontManager *fontManager)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadHTMLString:@"<body contenteditable>"
        "<span id='foo'>foo</span> <span id='bar'>bar</span> <span id='baz'>baz</span>"
        "</body><script>document.body.addEventListener('input', event => lastInputEvent = event)</script>"];
    [webView stringByEvaluatingJavaScript:@"document.body.focus()"];
    fontManager.target = webView.get();
    return webView;
}

static RetainPtr<NSMenuItemCell> menuItemCellForFontAction(NSFontAction action)
{
    auto menuItem = adoptNS([[NSMenuItem alloc] init]);
    auto menuItemCell = adoptNS([[NSMenuItemCell alloc] init]);
    [menuItemCell setMenuItem:menuItem.get()];
    [menuItemCell setTag:action];
    return menuItemCell;
}

namespace TestWebKitAPI {

TEST(FontManagerTests, ChangeFontSizeWithMenuItems)
{
    NSFontManager *fontManager = NSFontManager.sharedFontManager;
    auto webView = webViewForFontManagerTesting(fontManager);

    auto sizeIncreaseMenuItemCell = menuItemCellForFontAction(NSSizeUpFontAction);
    auto sizeDecreaseMenuItemCell = menuItemCellForFontAction(NSSizeDownFontAction);

    // Select "foo" and increase font size.
    [webView selectWord:nil];
    [fontManager modifyFont:sizeIncreaseMenuItemCell.get()];
    [fontManager modifyFont:sizeIncreaseMenuItemCell.get()];

    // Now select "baz" and decrease font size.
    [webView moveToEndOfParagraph:nil];
    [webView selectWord:nil];
    [fontManager modifyFont:sizeDecreaseMenuItemCell.get()];
    [fontManager modifyFont:sizeDecreaseMenuItemCell.get()];

    // Lastly, select just the "r" in "bar" and increase font size.
    [webView evaluateJavaScript:@"getSelection().setBaseAndExtent(bar.childNodes[0], 2, bar.childNodes[0], 3)" completionHandler:nil];
    [fontManager modifyFont:sizeIncreaseMenuItemCell.get()];

    [webView moveToBeginningOfParagraph:nil];
    [webView selectWord:nil];
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

    NSFont *largeHelveticaFont = [NSFont fontWithName:@"Helvetica" size:20];
    [fontPanel setPanelFont:largeHelveticaFont isMultiple:NO];
    [webView selectWord:nil];
    [fontManager modifyFontViaPanel:fontPanel];
    EXPECT_WK_STREQ("foo", [webView selectedText]);
    EXPECT_WK_STREQ("Helvetica", [webView stylePropertyAtSelectionStart:@"font-family"]);
    EXPECT_WK_STREQ("20px", [webView stylePropertyAtSelectionStart:@"font-size"]);
    EXPECT_WK_STREQ("normal", [webView stylePropertyAtSelectionStart:@"font-weight"]);
    EXPECT_EQ(largeHelveticaFont, fontManager.selectedFont);

    NSFont *smallBoldTimesFont = [fontManager fontWithFamily:@"Times New Roman" traits:NSBoldFontMask weight:NSFontWeightBold size:10];
    [fontPanel setPanelFont:smallBoldTimesFont isMultiple:NO];
    [webView selectNextWord];
    [fontManager modifyFontViaPanel:fontPanel];
    EXPECT_WK_STREQ("bar", [webView selectedText]);
    EXPECT_WK_STREQ("\"Times New Roman\"", [webView stylePropertyAtSelectionStart:@"font-family"]);
    EXPECT_WK_STREQ("10px", [webView stylePropertyAtSelectionStart:@"font-size"]);
    EXPECT_WK_STREQ("bold", [webView stylePropertyAtSelectionStart:@"font-weight"]);
    EXPECT_EQ(smallBoldTimesFont, fontManager.selectedFont);

    NSFont *boldItalicArialFont = [fontManager fontWithFamily:@"Arial" traits:NSBoldFontMask | NSItalicFontMask weight:NSFontWeightBold size:14];
    [fontPanel setPanelFont:boldItalicArialFont isMultiple:NO];
    [webView selectNextWord];
    [fontManager modifyFontViaPanel:fontPanel];
    EXPECT_WK_STREQ("baz", [webView selectedText]);
    EXPECT_WK_STREQ("Arial", [webView stylePropertyAtSelectionStart:@"font-family"]);
    EXPECT_WK_STREQ("14px", [webView stylePropertyAtSelectionStart:@"font-size"]);
    EXPECT_WK_STREQ("bold", [webView stylePropertyAtSelectionStart:@"font-weight"]);
    EXPECT_EQ(boldItalicArialFont, fontManager.selectedFont);

    NSFont *largeItalicLightAvenirFont = [fontManager fontWithFamily:@"Avenir" traits:NSItalicFontMask weight:NSFontWeightLight size:24];
    [fontPanel setPanelFont:largeItalicLightAvenirFont isMultiple:NO];
    [webView selectAll:nil];
    [fontManager modifyFontViaPanel:fontPanel];
    EXPECT_WK_STREQ("foo bar baz", [webView selectedText]);
    EXPECT_WK_STREQ("Avenir-LightOblique", [webView stylePropertyAtSelectionStart:@"font-family"]);
    EXPECT_WK_STREQ("24px", [webView stylePropertyAtSelectionStart:@"font-size"]);
    EXPECT_WK_STREQ("normal", [webView stylePropertyAtSelectionStart:@"font-weight"]);
    EXPECT_EQ(largeItalicLightAvenirFont, fontManager.selectedFont);
}

TEST(FontManagerTests, ChangeAttributesWithFontEffectsBox)
{
    NSFontManager *fontManager = NSFontManager.sharedFontManager;
    auto webView = webViewForFontManagerTesting(fontManager);

    NSFontPanel *fontPanel = [fontManager fontPanel:YES];
    [fontPanel setIsVisible:YES];

    auto textDecorationsAroundSelection = [webView] {
        NSString *decorationsAtStart = [webView stylePropertyAtSelectionStart:@"-webkit-text-decorations-in-effect"];
        NSString *decorationsAtEnd = [webView stylePropertyAtSelectionEnd:@"-webkit-text-decorations-in-effect"];
        if ([decorationsAtStart isEqualToString:decorationsAtEnd] || decorationsAtStart == decorationsAtEnd)
            return decorationsAtStart;
        return [NSString stringWithFormat:@"(%@, %@)", decorationsAtStart, decorationsAtEnd];
    };

    auto textShadowAroundSelection = [webView] {
        NSString *shadowAtStart = [webView stylePropertyAtSelectionStart:@"text-shadow"];
        NSString *shadowAtEnd = [webView stylePropertyAtSelectionEnd:@"text-shadow"];
        if ([shadowAtStart isEqualToString:shadowAtEnd] || shadowAtStart == shadowAtEnd)
            return shadowAtStart;
        return [NSString stringWithFormat:@"(%@, %@)", shadowAtStart, shadowAtEnd];
    };

    [webView selectWord:nil];
    [fontPanel chooseUnderlineMenuItemWithTitle:@"single"];
    EXPECT_WK_STREQ("foo", [webView selectedText]);
    EXPECT_WK_STREQ("underline", textDecorationsAroundSelection());

    [fontPanel chooseUnderlineMenuItemWithTitle:@"none"];
    EXPECT_WK_STREQ("none", textDecorationsAroundSelection());

    [webView selectNextWord];
    [fontPanel chooseStrikeThroughMenuItemWithTitle:@"single"];
    EXPECT_WK_STREQ("bar", [webView selectedText]);
    EXPECT_WK_STREQ("line-through", textDecorationsAroundSelection());

    [fontPanel chooseStrikeThroughMenuItemWithTitle:@"none"];
    EXPECT_WK_STREQ("none", textDecorationsAroundSelection());

    [webView selectNextWord];
    fontPanel.shadowBlur = 8;
    fontPanel.shadowOpacity = 1;
    [fontPanel toggleShadow];
    EXPECT_WK_STREQ("baz", [webView selectedText]);
    EXPECT_WK_STREQ("rgb(0, 0, 0) 0px 1px 8px", textShadowAroundSelection());

    [fontPanel toggleShadow];
    EXPECT_WK_STREQ("none", textShadowAroundSelection());

    // Now combine all three attributes together.
    [webView selectAll:nil];
    fontPanel.shadowBlur = 5;
    fontPanel.shadowOpacity = 0.2;
    [fontPanel toggleShadow];
    [fontPanel chooseUnderlineMenuItemWithTitle:@"single"];
    [fontPanel chooseStrikeThroughMenuItemWithTitle:@"single"];
    EXPECT_WK_STREQ("foo bar baz", [webView selectedText]);
    EXPECT_WK_STREQ("rgba(0, 0, 0, 0.2) 0px 1px 5px", textShadowAroundSelection());
    EXPECT_WK_STREQ("underline line-through", textDecorationsAroundSelection());

    [fontPanel toggleShadow];
    [fontPanel chooseUnderlineMenuItemWithTitle:@"none"];
    [fontPanel chooseStrikeThroughMenuItemWithTitle:@"none"];
    EXPECT_WK_STREQ("none", textShadowAroundSelection());
    EXPECT_WK_STREQ("none", textDecorationsAroundSelection());
}

TEST(FontManagerTests, ChangeFontColorWithColorPanel)
{
    NSColorPanel *colorPanel = NSColorPanel.sharedColorPanel;
    colorPanel.showsAlpha = YES;

    auto webView = webViewForFontManagerTesting(NSFontManager.sharedFontManager);
    auto checkFontColorAtStartAndEndWithInputEvents = [webView] (const char* colorAsString) {
        EXPECT_WK_STREQ(colorAsString, [webView stylePropertyAtSelectionStart:@"color"]);
        EXPECT_WK_STREQ(colorAsString, [webView stylePropertyAtSelectionEnd:@"color"]);
        EXPECT_WK_STREQ("formatFontColor", [webView stringByEvaluatingJavaScript:@"lastInputEvent.inputType"]);
        EXPECT_WK_STREQ(colorAsString, [webView stringByEvaluatingJavaScript:@"lastInputEvent.data"]);
    };

    // 1. Select "foo" and turn it red; verify that the font element is used for fully opaque colors.
    colorPanel.color = NSColor.redColor;
    [webView selectWord:nil];
    [webView changeColor:colorPanel];
    checkFontColorAtStartAndEndWithInputEvents("rgb(255, 0, 0)");
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"!!foo.querySelector('font')"] boolValue]);

    // 2. Now select "bar" and try a few different colors, starting with a color with alpha.
    colorPanel.color = [NSColor colorWithWhite:1 alpha:0.2];
    [webView selectNextWord];
    [webView changeColor:colorPanel];
    checkFontColorAtStartAndEndWithInputEvents("rgba(255, 255, 255, 0.2)");
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"!!bar.querySelector('font')"] boolValue]);

    // 3a. Switch back to a solid color.
    colorPanel.color = [NSColor colorWithRed:0.8 green:0.7 blue:0.2 alpha:1];
    [webView changeColor:colorPanel];
    checkFontColorAtStartAndEndWithInputEvents("rgb(204, 179, 51)");
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"!!bar.querySelector('font')"] boolValue]);

    // 3b. Switch back again to a color with alpha.
    colorPanel.color = [NSColor colorWithRed:0.8 green:0.7 blue:0.2 alpha:0.2];
    [webView changeColor:colorPanel];
    checkFontColorAtStartAndEndWithInputEvents("rgba(204, 179, 51, 0.2)");
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"!!bar.querySelector('font')"] boolValue]);

    // 4a. Now collapse the selection to the end and set the typing style color to green.
    colorPanel.color = NSColor.greenColor;
    [webView collapseToEnd];
    [webView changeColor:colorPanel];
    EXPECT_WK_STREQ("formatFontColor", [webView stringByEvaluatingJavaScript:@"lastInputEvent.inputType"]);
    EXPECT_WK_STREQ("rgb(0, 255, 0)", [webView stringByEvaluatingJavaScript:@"lastInputEvent.data"]);

    // 4b. This should result in inserting green text.
    [webView insertText:@"green"];
    [webView moveWordBackward:nil];
    [webView selectWord:nil];
    EXPECT_WK_STREQ("rgba(204, 179, 51, 0.2)", [webView stylePropertyAtSelectionStart:@"color"]);
    EXPECT_WK_STREQ("rgb(0, 255, 0)", [webView stylePropertyAtSelectionEnd:@"color"]);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC) && WK_API_ENABLED
