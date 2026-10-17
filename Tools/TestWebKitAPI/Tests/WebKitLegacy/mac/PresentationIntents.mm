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

#import "config.h"

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import <WebKit/WebDocument.h>
#import <WebKit/WebFrameLoadDelegate.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebViewPrivate.h>
#import <wtf/RetainPtr.h>
#import <wtf/Scope.h>

@interface PresentationIntentsLoadDelegate : NSObject <WebFrameLoadDelegate>
@end

@implementation PresentationIntentsLoadDelegate {
    bool _doneLoading;
}

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    _doneLoading = true;
}

- (void)waitForLoadToFinish
{
    TestWebKitAPI::Util::run(&_doneLoading);
}

@end

namespace TestWebKitAPI {

// The converter is tested through WebKitLegacy because it runs in this process, where the default can be scoped to the test.

// Sets the default that makes the converter read every presentation intent kind in this process's
// argument domain, which overrides the global domain without changing it, and restores the domain afterwards.
static auto setPresentationIntentHTMLInterchangeArgument(bool enabled)
{
    RetainPtr defaults = [NSUserDefaults standardUserDefaults];
    RetainPtr existingArguments = [defaults volatileDomainForName:NSArgumentDomain];
    RetainPtr<NSMutableDictionary> newArguments = adoptNS(existingArguments ? [existingArguments mutableCopy] : [[NSMutableDictionary alloc] init]);
    [newArguments setValue:@(enabled) forKey:@"__NSPresentationIntentHTMLInterchange"];
    [defaults setVolatileDomain:newArguments.get() forName:NSArgumentDomain];

    return makeScopeExit([defaults, existingArguments] {
        if (existingArguments)
            [defaults setVolatileDomain:existingArguments.get() forName:NSArgumentDomain];
        else
            [defaults removeVolatileDomainForName:NSArgumentDomain];
    });
}

static RetainPtr<WebView> webViewWithHTML(NSString *html)
{
    RetainPtr webView = adoptNS([[WebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    RetainPtr delegate = adoptNS([[PresentationIntentsLoadDelegate alloc] init]);
    [webView setFrameLoadDelegate:delegate.get()];
    [[webView mainFrame] loadHTMLString:html baseURL:nil];
    [delegate waitForLoadToFinish];
    [webView setFrameLoadDelegate:nil];
    return webView;
}

static NSView<WebDocumentText> *documentView(WebView *webView)
{
    return (NSView<WebDocumentText> *)[[[webView mainFrame] frameView] documentView];
}

static RetainPtr<NSAttributedString> attributedStringFromHTML(NSString *html)
{
    RetainPtr webView = webViewWithHTML(html);
    return [documentView(webView.get()) attributedString];
}

// Selects characters [start, end) of the first text node in the element with the given ID and converts the selection.
static RetainPtr<NSAttributedString> selectedAttributedString(WebView *webView, NSString *elementID, unsigned start, unsigned end)
{
    RetainPtr script = [NSString stringWithFormat:@"(() => { const text = document.getElementById('%@').firstChild; getSelection().setBaseAndExtent(text, %u, text, %u); })()", elementID, start, end];
    [webView stringByEvaluatingJavaScriptFromString:script.get()];
    return [documentView(webView) selectedAttributedString];
}

static NSPresentationIntent *presentationIntentForSubstring(NSAttributedString *attributedString, NSString *substring)
{
    NSRange range = [attributedString.string rangeOfString:substring];
    if (range.location == NSNotFound)
        return nil;
    return [attributedString attribute:NSPresentationIntentAttributeName atIndex:range.location effectiveRange:nil];
}

static NSInlinePresentationIntent inlinePresentationIntentForSubstring(NSAttributedString *attributedString, NSString *substring)
{
    NSRange range = [attributedString.string rangeOfString:substring];
    if (range.location == NSNotFound)
        return 0;
    return [[attributedString attribute:NSInlinePresentationIntentAttributeName atIndex:range.location effectiveRange:nil] unsignedIntegerValue];
}

TEST(WebKitLegacy, PresentationIntentsRequireDefault)
{
    auto restoreArguments = setPresentationIntentHTMLInterchangeArgument(false);

    RetainPtr attributedString = attributedStringFromHTML(@"<h1>Title</h1><p>Body <em>soft</em></p><ul><li>item</li></ul><hr><blockquote>quoted</blockquote><pre>code</pre>");
    EXPECT_NULL(presentationIntentForSubstring(attributedString.get(), @"Title"));
    EXPECT_NULL(presentationIntentForSubstring(attributedString.get(), @"Body"));
    EXPECT_NULL(presentationIntentForSubstring(attributedString.get(), @"item"));
    EXPECT_NULL(presentationIntentForSubstring(attributedString.get(), @"code"));
    EXPECT_EQ(inlinePresentationIntentForSubstring(attributedString.get(), @"soft"), 0U);

    RetainPtr thematicBreak = [NSString stringWithFormat:@"%C", static_cast<unichar>(0x2E3B)];
    EXPECT_EQ([[attributedString string] rangeOfString:thematicBreak.get()].location, static_cast<NSUInteger>(NSNotFound));

    // Without the default, block quotes keep the intent they had before.
    NSPresentationIntent *quoted = presentationIntentForSubstring(attributedString.get(), @"quoted");
    ASSERT_NOT_NULL(quoted);
    EXPECT_EQ(quoted.intentKind, NSPresentationIntentKindBlockQuote);

    // An <hr> adds an empty paragraph rather than a break character.
    EXPECT_WK_STREQ([attributedStringFromHTML(@"<p>before</p><hr><p>after</p>") string], "before\n\nafter\n");
}

TEST(WebKitLegacy, PresentationIntentsForHeadingsAndParagraphs)
{
    auto restoreArguments = setPresentationIntentHTMLInterchangeArgument(true);

    RetainPtr attributedString = attributedStringFromHTML(@"<h1>Title</h1><p>Body</p><h2 aria-level='4'>Override</h2>");

    NSPresentationIntent *title = presentationIntentForSubstring(attributedString.get(), @"Title");
    ASSERT_NOT_NULL(title);
    EXPECT_EQ(title.intentKind, NSPresentationIntentKindHeader);
    EXPECT_EQ(title.headerLevel, 1);
    EXPECT_NULL(title.parentIntent);

    NSPresentationIntent *body = presentationIntentForSubstring(attributedString.get(), @"Body");
    ASSERT_NOT_NULL(body);
    EXPECT_EQ(body.intentKind, NSPresentationIntentKindParagraph);
    EXPECT_NE(body.identity, title.identity);

    NSPresentationIntent *override = presentationIntentForSubstring(attributedString.get(), @"Override");
    ASSERT_NOT_NULL(override);
    EXPECT_EQ(override.intentKind, NSPresentationIntentKindHeader);
    EXPECT_EQ(override.headerLevel, 4);
}

TEST(WebKitLegacy, PresentationIntentsForLists)
{
    auto restoreArguments = setPresentationIntentHTMLInterchangeArgument(true);

    RetainPtr attributedString = attributedStringFromHTML(@"<ol start='3'><li>three</li><li value='7'>seven</li><li>eight</li></ol>"
        "<ul><li>outer<ul><li>inner</li></ul></li></ul>");

    NSPresentationIntent *three = presentationIntentForSubstring(attributedString.get(), @"three");
    ASSERT_NOT_NULL(three);
    EXPECT_EQ(three.intentKind, NSPresentationIntentKindListItem);
    EXPECT_EQ(three.ordinal, 3);
    EXPECT_EQ(three.parentIntent.intentKind, NSPresentationIntentKindOrderedList);

    NSPresentationIntent *seven = presentationIntentForSubstring(attributedString.get(), @"seven");
    ASSERT_NOT_NULL(seven);
    EXPECT_EQ(seven.ordinal, 7);
    EXPECT_EQ(seven.parentIntent.identity, three.parentIntent.identity);
    EXPECT_EQ(presentationIntentForSubstring(attributedString.get(), @"eight").ordinal, 8);

    NSPresentationIntent *outer = presentationIntentForSubstring(attributedString.get(), @"outer");
    ASSERT_NOT_NULL(outer);
    EXPECT_EQ(outer.intentKind, NSPresentationIntentKindListItem);
    EXPECT_EQ(outer.ordinal, 1);
    EXPECT_EQ(outer.parentIntent.intentKind, NSPresentationIntentKindUnorderedList);

    NSUInteger afterOuter = NSMaxRange([[attributedString string] rangeOfString:@"outer"]);
    ASSERT_LT(afterOuter, [attributedString length]);
    EXPECT_EQ([[attributedString string] characterAtIndex:afterOuter], '\n');
    EXPECT_EQ(static_cast<NSPresentationIntent *>([attributedString attribute:NSPresentationIntentAttributeName atIndex:afterOuter effectiveRange:nil]).identity, outer.identity);

    NSPresentationIntent *inner = presentationIntentForSubstring(attributedString.get(), @"inner");
    ASSERT_NOT_NULL(inner);
    EXPECT_EQ(inner.intentKind, NSPresentationIntentKindListItem);
    EXPECT_EQ(inner.parentIntent.intentKind, NSPresentationIntentKindUnorderedList);
    EXPECT_EQ(inner.parentIntent.parentIntent.identity, outer.identity);
}

TEST(WebKitLegacy, PresentationIntentsForCodeBlocks)
{
    auto restoreArguments = setPresentationIntentHTMLInterchangeArgument(true);

    RetainPtr attributedString = attributedStringFromHTML(@"<pre><code class='language-swift'>let first\nlet second</code></pre><pre>plain</pre><p>Use <code>inline</code> code</p>");

    NSPresentationIntent *first = presentationIntentForSubstring(attributedString.get(), @"first");
    ASSERT_NOT_NULL(first);
    EXPECT_EQ(first.intentKind, NSPresentationIntentKindCodeBlock);
    EXPECT_WK_STREQ(first.languageHint, "swift");
    EXPECT_EQ(presentationIntentForSubstring(attributedString.get(), @"second").identity, first.identity);
    EXPECT_EQ(inlinePresentationIntentForSubstring(attributedString.get(), @"first"), 0U);

    NSPresentationIntent *plain = presentationIntentForSubstring(attributedString.get(), @"plain");
    ASSERT_NOT_NULL(plain);
    EXPECT_EQ(plain.intentKind, NSPresentationIntentKindCodeBlock);
    EXPECT_NULL(plain.languageHint);

    EXPECT_EQ(presentationIntentForSubstring(attributedString.get(), @"inline").intentKind, NSPresentationIntentKindParagraph);
    EXPECT_EQ(inlinePresentationIntentForSubstring(attributedString.get(), @"inline"), NSInlinePresentationIntentCode);
}

TEST(WebKitLegacy, InlinePresentationIntents)
{
    auto restoreArguments = setPresentationIntentHTMLInterchangeArgument(true);

    RetainPtr attributedString = attributedStringFromHTML(@"<p><em>soft</em> <strong>loud</strong> <del>gone</del> <strong><em>both</em></strong> plain</p>");

    EXPECT_EQ(inlinePresentationIntentForSubstring(attributedString.get(), @"soft"), NSInlinePresentationIntentEmphasized);
    EXPECT_EQ(inlinePresentationIntentForSubstring(attributedString.get(), @"loud"), NSInlinePresentationIntentStronglyEmphasized);
    EXPECT_EQ(inlinePresentationIntentForSubstring(attributedString.get(), @"gone"), NSInlinePresentationIntentStrikethrough);
    EXPECT_EQ(inlinePresentationIntentForSubstring(attributedString.get(), @"both"), NSInlinePresentationIntentEmphasized | NSInlinePresentationIntentStronglyEmphasized);
    EXPECT_EQ(inlinePresentationIntentForSubstring(attributedString.get(), @"plain"), 0U);
}

TEST(WebKitLegacy, PresentationIntentsForRoles)
{
    auto restoreArguments = setPresentationIntentHTMLInterchangeArgument(true);

    RetainPtr attributedString = attributedStringFromHTML(@"<div role='heading' aria-level='3'>rolehead</div><div role='paragraph'>rolepara</div>"
        "<div role='list'><div role='listitem'>roleitem</div></div><div role='blockquote'>rolequote</div><div role='separator'></div>"
        "<p role='presentation'>nothing</p><p><span role='emphasis'>roleem</span> <span role='strong'>rolestrong</span> <span role='code'>rolecode</span> <span role='deletion'>roledel</span></p>");

    NSPresentationIntent *heading = presentationIntentForSubstring(attributedString.get(), @"rolehead");
    ASSERT_NOT_NULL(heading);
    EXPECT_EQ(heading.intentKind, NSPresentationIntentKindHeader);
    EXPECT_EQ(heading.headerLevel, 3);

    EXPECT_EQ(presentationIntentForSubstring(attributedString.get(), @"rolepara").intentKind, NSPresentationIntentKindParagraph);

    NSPresentationIntent *item = presentationIntentForSubstring(attributedString.get(), @"roleitem");
    ASSERT_NOT_NULL(item);
    EXPECT_EQ(item.intentKind, NSPresentationIntentKindListItem);
    EXPECT_EQ(item.ordinal, 1);
    EXPECT_EQ(item.parentIntent.intentKind, NSPresentationIntentKindUnorderedList);

    EXPECT_EQ(presentationIntentForSubstring(attributedString.get(), @"rolequote").intentKind, NSPresentationIntentKindBlockQuote);
    EXPECT_EQ(presentationIntentForSubstring(attributedString.get(), [NSString stringWithFormat:@"%C", static_cast<unichar>(0x2E3B)]).intentKind, NSPresentationIntentKindThematicBreak);
    EXPECT_NULL(presentationIntentForSubstring(attributedString.get(), @"nothing"));

    EXPECT_EQ(inlinePresentationIntentForSubstring(attributedString.get(), @"roleem"), NSInlinePresentationIntentEmphasized);
    EXPECT_EQ(inlinePresentationIntentForSubstring(attributedString.get(), @"rolestrong"), NSInlinePresentationIntentStronglyEmphasized);
    EXPECT_EQ(inlinePresentationIntentForSubstring(attributedString.get(), @"rolecode"), NSInlinePresentationIntentCode);
    EXPECT_EQ(inlinePresentationIntentForSubstring(attributedString.get(), @"roledel"), NSInlinePresentationIntentStrikethrough);
}

TEST(WebKitLegacy, PresentationIntentsForNestedBlockQuotes)
{
    auto restoreArguments = setPresentationIntentHTMLInterchangeArgument(true);

    RetainPtr attributedString = attributedStringFromHTML(@"<blockquote><blockquote>inner</blockquote>outer<p>para</p></blockquote>");

    NSPresentationIntent *inner = presentationIntentForSubstring(attributedString.get(), @"inner");
    ASSERT_NOT_NULL(inner);
    EXPECT_EQ(inner.intentKind, NSPresentationIntentKindBlockQuote);
    EXPECT_EQ(inner.parentIntent.intentKind, NSPresentationIntentKindBlockQuote);

    NSPresentationIntent *outer = presentationIntentForSubstring(attributedString.get(), @"outer");
    ASSERT_NOT_NULL(outer);
    EXPECT_EQ(outer.intentKind, NSPresentationIntentKindBlockQuote);
    EXPECT_EQ(outer.identity, inner.parentIntent.identity);

    NSPresentationIntent *paragraph = presentationIntentForSubstring(attributedString.get(), @"para");
    ASSERT_NOT_NULL(paragraph);
    EXPECT_EQ(paragraph.intentKind, NSPresentationIntentKindParagraph);
    EXPECT_EQ(paragraph.parentIntent.identity, outer.identity);
}

TEST(WebKitLegacy, PresentationIntentsForThematicBreaks)
{
    auto restoreArguments = setPresentationIntentHTMLInterchangeArgument(true);

    RetainPtr attributedString = attributedStringFromHTML(@"<p>before</p><hr><p>after</p>");
    RetainPtr thematicBreak = [NSString stringWithFormat:@"%C", static_cast<unichar>(0x2E3B)];

    RetainPtr expectedString = [NSString stringWithFormat:@"before\n%@\nafter\n", thematicBreak.get()];
    EXPECT_WK_STREQ([attributedString string], expectedString.get());
    EXPECT_EQ(presentationIntentForSubstring(attributedString.get(), thematicBreak.get()).intentKind, NSPresentationIntentKindThematicBreak);
    EXPECT_EQ(presentationIntentForSubstring(attributedString.get(), @"before").intentKind, NSPresentationIntentKindParagraph);
    EXPECT_EQ(presentationIntentForSubstring(attributedString.get(), @"after").intentKind, NSPresentationIntentKindParagraph);
}

TEST(WebKitLegacy, PresentationIntentsForParagraphSeparators)
{
    auto restoreArguments = setPresentationIntentHTMLInterchangeArgument(true);

    RetainPtr attributedString = attributedStringFromHTML(@"<p>Body <em>soft</em></p><h2>Next</h2>");
    EXPECT_WK_STREQ([attributedString string], "Body soft\nNext\n");

    NSPresentationIntent *body = presentationIntentForSubstring(attributedString.get(), @"Body");
    ASSERT_NOT_NULL(body);
    NSUInteger afterSoft = NSMaxRange([[attributedString string] rangeOfString:@"soft"]);
    EXPECT_EQ(static_cast<NSPresentationIntent *>([attributedString attribute:NSPresentationIntentAttributeName atIndex:afterSoft effectiveRange:nil]).identity, body.identity);
    EXPECT_EQ([[attributedString attribute:NSInlinePresentationIntentAttributeName atIndex:afterSoft effectiveRange:nil] unsignedIntegerValue], NSInlinePresentationIntentEmphasized);

    NSPresentationIntent *next = presentationIntentForSubstring(attributedString.get(), @"Next");
    ASSERT_NOT_NULL(next);
    EXPECT_EQ(next.intentKind, NSPresentationIntentKindHeader);
    EXPECT_EQ(static_cast<NSPresentationIntent *>([attributedString attribute:NSPresentationIntentAttributeName atIndex:[attributedString length] - 1 effectiveRange:nil]).identity, next.identity);
}

TEST(WebKitLegacy, PresentationIntentsForBlocksInListItems)
{
    auto restoreArguments = setPresentationIntentHTMLInterchangeArgument(true);

    RetainPtr attributedString = attributedStringFromHTML(@"<ol><li><p>alpha</p><p>beta</p></li><li>gamma</li></ol>");
    NSString *string = [attributedString string];

    RetainPtr lineSeparator = [NSString stringWithFormat:@"%C", static_cast<unichar>(NSLineSeparatorCharacter)];
    EXPECT_EQ([string rangeOfString:lineSeparator.get()].location, static_cast<NSUInteger>(NSNotFound));

    NSPresentationIntent *alpha = presentationIntentForSubstring(attributedString.get(), @"alpha");
    NSPresentationIntent *beta = presentationIntentForSubstring(attributedString.get(), @"beta");
    NSPresentationIntent *gamma = presentationIntentForSubstring(attributedString.get(), @"gamma");
    ASSERT_NOT_NULL(alpha);
    ASSERT_NOT_NULL(beta);
    ASSERT_NOT_NULL(gamma);
    EXPECT_EQ(alpha.intentKind, NSPresentationIntentKindParagraph);
    EXPECT_EQ(beta.intentKind, NSPresentationIntentKindParagraph);
    EXPECT_NE(alpha.identity, beta.identity);
    EXPECT_EQ(alpha.parentIntent.intentKind, NSPresentationIntentKindListItem);
    EXPECT_EQ(alpha.parentIntent.identity, beta.parentIntent.identity);
    EXPECT_EQ(gamma.intentKind, NSPresentationIntentKindListItem);
    EXPECT_EQ(gamma.ordinal, 2);

    // The second block of an item is its own paragraph without a marker, and the next item is still numbered 2.
    NSRange betaRange = [string rangeOfString:@"beta"];
    ASSERT_GT(betaRange.location, 0U);
    EXPECT_EQ([string characterAtIndex:betaRange.location - 1], '\n');

    NSRange gammaRange = [string rangeOfString:@"gamma"];
    NSRange gammaParagraph = [string paragraphRangeForRange:NSMakeRange(gammaRange.location, 0)];
    EXPECT_EQ(static_cast<NSPresentationIntent *>([attributedString attribute:NSPresentationIntentAttributeName atIndex:gammaParagraph.location effectiveRange:nil]).identity, gamma.identity);
    NSTextList *list = [[attributedString attribute:NSParagraphStyleAttributeName atIndex:gammaRange.location effectiveRange:nil] textLists].firstObject;
    ASSERT_NOT_NULL(list);
    RetainPtr gammaPrefix = [string substringWithRange:NSMakeRange(gammaParagraph.location, gammaRange.location - gammaParagraph.location)];
    RetainPtr expectedPrefix = [NSString stringWithFormat:@"\t%@\t", [list markerForItemNumber:2]];
    EXPECT_WK_STREQ(gammaPrefix.get(), expectedPrefix.get());
}

// Returns the marker text that starts the paragraph containing the substring.
static NSString *markerForSubstring(NSAttributedString *attributedString, NSString *substring)
{
    NSString *string = [attributedString string];
    NSRange range = [string rangeOfString:substring];
    if (range.location == NSNotFound)
        return nil;
    NSRange paragraph = [string paragraphRangeForRange:NSMakeRange(range.location, 0)];
    return [string substringWithRange:NSMakeRange(paragraph.location, range.location - paragraph.location)];
}

static NSString *expectedMarker(NSAttributedString *attributedString, NSString *substring, NSInteger itemNumber)
{
    NSRange range = [[attributedString string] rangeOfString:substring];
    if (range.location == NSNotFound)
        return nil;
    NSTextList *list = [[attributedString attribute:NSParagraphStyleAttributeName atIndex:range.location effectiveRange:nil] textLists].firstObject;
    return [NSString stringWithFormat:@"\t%@\t", [list markerForItemNumber:itemNumber]];
}

TEST(WebKitLegacy, PresentationIntentsForEmptyParagraphs)
{
    auto restoreArguments = setPresentationIntentHTMLInterchangeArgument(true);

    RetainPtr paragraphs = attributedStringFromHTML(@"<p>a</p><p></p><p>b</p>");
    EXPECT_WK_STREQ([paragraphs string], "a\n\nb\n");
    NSPresentationIntent *a = presentationIntentForSubstring(paragraphs.get(), @"a");
    ASSERT_NOT_NULL(a);
    NSPresentationIntent *empty = [paragraphs attribute:NSPresentationIntentAttributeName atIndex:2 effectiveRange:nil];
    ASSERT_NOT_NULL(empty);
    EXPECT_EQ(empty.intentKind, NSPresentationIntentKindParagraph);
    EXPECT_NE(empty.identity, a.identity);
    EXPECT_EQ(static_cast<NSPresentationIntent *>([paragraphs attribute:NSPresentationIntentAttributeName atIndex:1 effectiveRange:nil]).identity, a.identity);

    RetainPtr items = attributedStringFromHTML(@"<ol><li>one</li><li></li><li>three</li></ol>");
    NSPresentationIntent *three = presentationIntentForSubstring(items.get(), @"three");
    ASSERT_NOT_NULL(three);
    EXPECT_EQ(three.ordinal, 3);
    EXPECT_WK_STREQ(markerForSubstring(items.get(), @"three"), expectedMarker(items.get(), @"three", 3));

    NSRange threeParagraph = [[items string] paragraphRangeForRange:NSMakeRange([[items string] rangeOfString:@"three"].location, 0)];
    ASSERT_GT(threeParagraph.location, 0U);
    NSPresentationIntent *emptyItem = [items attribute:NSPresentationIntentAttributeName atIndex:threeParagraph.location - 1 effectiveRange:nil];
    ASSERT_NOT_NULL(emptyItem);
    EXPECT_EQ(emptyItem.intentKind, NSPresentationIntentKindListItem);
    EXPECT_EQ(emptyItem.ordinal, 2);
}

TEST(WebKitLegacy, PresentationIntentsForRoleListInListItem)
{
    auto restoreArguments = setPresentationIntentHTMLInterchangeArgument(true);

    RetainPtr attributedString = attributedStringFromHTML(@"<ol><li>first<div role='list'><div role='listitem'>nested</div></div></li><li>second</li></ol>");

    NSPresentationIntent *nested = presentationIntentForSubstring(attributedString.get(), @"nested");
    ASSERT_NOT_NULL(nested);
    EXPECT_EQ(nested.intentKind, NSPresentationIntentKindListItem);
    EXPECT_EQ(nested.parentIntent.parentIntent.identity, presentationIntentForSubstring(attributedString.get(), @"first").identity);

    // The role-based list has no NSTextList, so its item continues the outer item without a marker.
    EXPECT_WK_STREQ(markerForSubstring(attributedString.get(), @"nested"), "");
    EXPECT_WK_STREQ(markerForSubstring(attributedString.get(), @"second"), expectedMarker(attributedString.get(), @"second", 2));
}

TEST(WebKitLegacy, PresentationIntentsForSeparatorRoleOnList)
{
    auto restoreArguments = setPresentationIntentHTMLInterchangeArgument(true);

    RetainPtr attributedString = attributedStringFromHTML(@"<ol><li>one<ul role='separator'><li>nested</li></ul></li><li>two</li></ol>");
    RetainPtr thematicBreak = [NSString stringWithFormat:@"%C", static_cast<unichar>(0x2E3B)];

    EXPECT_EQ([[attributedString string] rangeOfString:thematicBreak.get()].location, static_cast<NSUInteger>(NSNotFound));

    auto textListsForSubstring = [&](NSString *substring) -> NSArray<NSTextList *> * {
        NSRange range = [[attributedString string] rangeOfString:substring];
        if (range.location == NSNotFound)
            return nil;
        return [[attributedString attribute:NSParagraphStyleAttributeName atIndex:range.location effectiveRange:nil] textLists];
    };
    EXPECT_EQ(textListsForSubstring(@"nested").count, 2U);
    EXPECT_EQ(textListsForSubstring(@"two").count, 1U);
    EXPECT_EQ(textListsForSubstring(@"two").firstObject, textListsForSubstring(@"one").firstObject);
}

TEST(WebKitLegacy, PresentationIntentsForCodeBlockLanguageCase)
{
    auto restoreArguments = setPresentationIntentHTMLInterchangeArgument(true);

    // Without a doctype the document is in quirks mode, where class names are matched without case.
    RetainPtr attributedString = attributedStringFromHTML(@"<pre class='language-ObjC'>objc</pre><pre><code class='language-CPlusPlus'>cpp</code></pre>");

    EXPECT_WK_STREQ(presentationIntentForSubstring(attributedString.get(), @"objc").languageHint, "ObjC");
    EXPECT_WK_STREQ(presentationIntentForSubstring(attributedString.get(), @"cpp").languageHint, "CPlusPlus");
}

TEST(WebKitLegacy, PresentationIntentsForListItemsWithoutListItemDisplay)
{
    auto restoreArguments = setPresentationIntentHTMLInterchangeArgument(true);

    RetainPtr attributedString = attributedStringFromHTML(@"<ol start='5'><li style='display:block'>five</li><li style='display:block' value='9'>nine</li><li style='display:block'>ten</li></ol>"
        "<ol reversed><li style='display:block'>third</li><li style='display:block'>second</li><li style='display:block'>first</li></ol>"
        "<ol reversed start='20'><li style='display:block'>twenty</li><li style='display:block'>nineteen</li></ol>"
        "<ol start='3'><div role='listitem'>rolethree</div><p>skipped</p><div role='listitem'>rolefour</div></ol>");

    EXPECT_EQ(presentationIntentForSubstring(attributedString.get(), @"five").ordinal, 5);
    EXPECT_EQ(presentationIntentForSubstring(attributedString.get(), @"nine").ordinal, 9);
    EXPECT_EQ(presentationIntentForSubstring(attributedString.get(), @"ten").ordinal, 10);

    EXPECT_EQ(presentationIntentForSubstring(attributedString.get(), @"third").ordinal, 3);
    EXPECT_EQ(presentationIntentForSubstring(attributedString.get(), @"second").ordinal, 2);
    EXPECT_EQ(presentationIntentForSubstring(attributedString.get(), @"first").ordinal, 1);

    EXPECT_EQ(presentationIntentForSubstring(attributedString.get(), @"twenty").ordinal, 20);
    EXPECT_EQ(presentationIntentForSubstring(attributedString.get(), @"nineteen").ordinal, 19);

    NSPresentationIntent *roleThree = presentationIntentForSubstring(attributedString.get(), @"rolethree");
    ASSERT_NOT_NULL(roleThree);
    EXPECT_EQ(roleThree.intentKind, NSPresentationIntentKindListItem);
    EXPECT_EQ(roleThree.ordinal, 3);
    EXPECT_EQ(presentationIntentForSubstring(attributedString.get(), @"rolefour").ordinal, 4);
}

TEST(WebKitLegacy, InlinePresentationIntentsForPresentationRole)
{
    auto restoreArguments = setPresentationIntentHTMLInterchangeArgument(true);

    RetainPtr attributedString = attributedStringFromHTML(@"<p><em role='presentation'>plain</em> <strong><em role='none'>loud</em></strong></p>");

    EXPECT_EQ(inlinePresentationIntentForSubstring(attributedString.get(), @"plain"), 0U);
    EXPECT_EQ(inlinePresentationIntentForSubstring(attributedString.get(), @"loud"), NSInlinePresentationIntentStronglyEmphasized);
}

TEST(WebKitLegacy, PresentationIntentsForPartialRanges)
{
    auto restoreArguments = setPresentationIntentHTMLInterchangeArgument(true);

    RetainPtr webView = webViewWithHTML(@"<ul><li id='item'>first item</li><li>second</li></ul><blockquote><p id='quoted'>quoted text</p></blockquote>");

    RetainPtr item = selectedAttributedString(webView.get(), @"item", 2, 7);
    EXPECT_WK_STREQ([item string], "rst i");
    NSPresentationIntent *itemIntent = presentationIntentForSubstring(item.get(), @"rst");
    ASSERT_NOT_NULL(itemIntent);
    EXPECT_EQ(itemIntent.intentKind, NSPresentationIntentKindListItem);
    EXPECT_EQ(itemIntent.ordinal, 1);
    EXPECT_EQ(itemIntent.parentIntent.intentKind, NSPresentationIntentKindUnorderedList);

    RetainPtr quoted = selectedAttributedString(webView.get(), @"quoted", 0, 6);
    EXPECT_WK_STREQ([quoted string], "quoted");
    NSPresentationIntent *quotedIntent = presentationIntentForSubstring(quoted.get(), @"quoted");
    ASSERT_NOT_NULL(quotedIntent);
    EXPECT_EQ(quotedIntent.intentKind, NSPresentationIntentKindParagraph);
    EXPECT_EQ(quotedIntent.parentIntent.intentKind, NSPresentationIntentKindBlockQuote);
}

} // namespace TestWebKitAPI
