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

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>

@interface TestWKWebView (FontManagerTests)

@property (nonatomic, readonly) NSString *selectedText;

- (NSString *)stylePropertyAtSelectionStart:(NSString *)propertyName;
- (NSString *)stylePropertyAtSelectionEnd:(NSString *)propertyName;
- (void)selectNextWord;

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
    [webView synchronouslyLoadHTMLString:@"<body contenteditable><span id='foo'>foo</span> <span id='bar'>bar</span> <span id='baz'>baz</span></body>"];
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
    NSFontManager *fontManager = [NSFontManager sharedFontManager];
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
    NSFontManager *fontManager = [NSFontManager sharedFontManager];
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

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC) && WK_API_ENABLED
