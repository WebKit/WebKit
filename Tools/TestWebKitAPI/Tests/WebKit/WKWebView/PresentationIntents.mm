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

// Only macOS is covered, as it was when these tests used WebKitLegacy; the list marker checks also rely on AppKit's NSTextList.
#if PLATFORM(MAC)

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "Helpers/cocoa/WKWebViewConfigurationExtras.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

enum class ReadsAllPresentationIntents : bool { No, Yes };

// Loads the HTML in a web view whose converter reads every presentation intent kind, or only block quotes as it does by default.
static RetainPtr<TestWKWebView> webViewWithPresentationIntents(NSString *html, ReadsAllPresentationIntents readsAllPresentationIntents = ReadsAllPresentationIntents::Yes)
{
    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
    [webView synchronouslyLoadHTMLString:html];
    RetainPtr script = [NSString stringWithFormat:@"internals.setPresentationIntentHTMLInterchangeEnabled(%s)", readsAllPresentationIntents == ReadsAllPresentationIntents::Yes ? "true" : "false"];
    [webView stringByEvaluatingJavaScript:script];
    return webView;
}

static RetainPtr<NSAttributedString> attributedStringWithPresentationIntents(NSString *html, ReadsAllPresentationIntents readsAllPresentationIntents = ReadsAllPresentationIntents::Yes)
{
    RetainPtr webView = webViewWithPresentationIntents(html, readsAllPresentationIntents);
    __block RetainPtr<NSAttributedString> result;
    __block bool done = false;
    [webView _getContentsAsAttributedStringWithCompletionHandler:^(NSAttributedString *attributedString, NSDictionary<NSAttributedStringDocumentAttributeKey, id> *, NSError *) {
        result = attributedString;
        done = true;
    }];
    Util::run(&done);
    return result;
}

// Selects characters [start, end) of the first text node in the element with the given ID and converts the selection.
static RetainPtr<NSAttributedString> selectedAttributedStringWithPresentationIntents(TestWKWebView *webView, NSString *elementID, unsigned start, unsigned end)
{
    RetainPtr script = [NSString stringWithFormat:@"(() => { const text = document.getElementById('%@').firstChild; getSelection().setBaseAndExtent(text, %u, text, %u); })()", elementID, start, end];
    [webView stringByEvaluatingJavaScript:script];

    __block RetainPtr<NSAttributedString> result;
    __block bool done = false;
    [webView _getSelectionAsAttributedStringForTesting:^(NSAttributedString *attributedString) {
        result = attributedString;
        done = true;
    }];
    Util::run(&done);
    return result;
}

static NSPresentationIntent *intentForSubstring(NSAttributedString *attributedString, NSString *substring)
{
    NSRange range = [attributedString.string rangeOfString:substring];
    if (range.location == NSNotFound)
        return nil;
    return [attributedString attribute:NSPresentationIntentAttributeName atIndex:range.location effectiveRange:nil];
}

static NSInlinePresentationIntent inlineIntentForSubstring(NSAttributedString *attributedString, NSString *substring)
{
    NSRange range = [attributedString.string rangeOfString:substring];
    if (range.location == NSNotFound)
        return 0;
    return [[attributedString attribute:NSInlinePresentationIntentAttributeName atIndex:range.location effectiveRange:nil] unsignedIntegerValue];
}

static RetainPtr<NSString> thematicBreakString()
{
    return [NSString stringWithFormat:@"%C", static_cast<unichar>(0x2E3B)];
}

// Returns the marker text that starts the paragraph containing the substring.
static NSString *listMarkerForSubstring(NSAttributedString *attributedString, NSString *substring)
{
    NSString *string = [attributedString string];
    NSRange range = [string rangeOfString:substring];
    if (range.location == NSNotFound)
        return nil;
    NSRange paragraph = [string paragraphRangeForRange:NSMakeRange(range.location, 0)];
    return [string substringWithRange:NSMakeRange(paragraph.location, range.location - paragraph.location)];
}

static NSString *expectedListMarker(NSAttributedString *attributedString, NSString *substring, NSInteger itemNumber)
{
    NSRange range = [[attributedString string] rangeOfString:substring];
    if (range.location == NSNotFound)
        return nil;
    NSTextList *list = [[attributedString attribute:NSParagraphStyleAttributeName atIndex:range.location effectiveRange:nil] textLists].firstObject;
    return [NSString stringWithFormat:@"\t%@\t", [list markerForItemNumber:itemNumber]];
}

TEST(PresentationIntents, RequireDefault)
{
    RetainPtr attributedString = attributedStringWithPresentationIntents(@"<h1>Title</h1><p>Body <em>soft</em></p><ul><li>item</li></ul><hr><blockquote>quoted</blockquote><pre><code>code</code></pre>", ReadsAllPresentationIntents::No);
    EXPECT_NULL(intentForSubstring(attributedString.get(), @"Title"));
    EXPECT_NULL(intentForSubstring(attributedString.get(), @"Body"));
    EXPECT_NULL(intentForSubstring(attributedString.get(), @"item"));
    EXPECT_NULL(intentForSubstring(attributedString.get(), @"code"));
    EXPECT_EQ(inlineIntentForSubstring(attributedString.get(), @"soft"), 0U);
    EXPECT_EQ([[attributedString string] rangeOfString:thematicBreakString()].location, static_cast<NSUInteger>(NSNotFound));

    // Without the default, block quotes keep the intent they had before.
    NSPresentationIntent *quoted = intentForSubstring(attributedString.get(), @"quoted");
    ASSERT_NOT_NULL(quoted);
    EXPECT_EQ(quoted.intentKind, NSPresentationIntentKindBlockQuote);

    // An <hr> adds an empty paragraph rather than a break character.
    EXPECT_WK_STREQ([attributedStringWithPresentationIntents(@"<p>before</p><hr><p>after</p>", ReadsAllPresentationIntents::No) string], "before\n\nafter\n");
}

TEST(PresentationIntents, HeadingsAndParagraphs)
{
    RetainPtr attributedString = attributedStringWithPresentationIntents(@"<h1>Title</h1><p>Body</p><h2 aria-level='4'>Override</h2>");

    NSPresentationIntent *title = intentForSubstring(attributedString.get(), @"Title");
    ASSERT_NOT_NULL(title);
    EXPECT_EQ(title.intentKind, NSPresentationIntentKindHeader);
    EXPECT_EQ(title.headerLevel, 1);
    EXPECT_NULL(title.parentIntent);

    NSPresentationIntent *body = intentForSubstring(attributedString.get(), @"Body");
    ASSERT_NOT_NULL(body);
    EXPECT_EQ(body.intentKind, NSPresentationIntentKindParagraph);
    EXPECT_NE(body.identity, title.identity);

    NSPresentationIntent *override = intentForSubstring(attributedString.get(), @"Override");
    ASSERT_NOT_NULL(override);
    EXPECT_EQ(override.intentKind, NSPresentationIntentKindHeader);
    EXPECT_EQ(override.headerLevel, 4);
}

TEST(PresentationIntents, Lists)
{
    RetainPtr attributedString = attributedStringWithPresentationIntents(@"<ol start='3'><li>three</li><li value='7'>seven</li><li>eight</li></ol>"
        "<ul><li>outer<ul><li>inner</li></ul></li></ul>");

    NSPresentationIntent *three = intentForSubstring(attributedString.get(), @"three");
    ASSERT_NOT_NULL(three);
    EXPECT_EQ(three.intentKind, NSPresentationIntentKindListItem);
    EXPECT_EQ(three.ordinal, 3);
    EXPECT_EQ(three.parentIntent.intentKind, NSPresentationIntentKindOrderedList);

    NSPresentationIntent *seven = intentForSubstring(attributedString.get(), @"seven");
    ASSERT_NOT_NULL(seven);
    EXPECT_EQ(seven.ordinal, 7);
    EXPECT_EQ(seven.parentIntent.identity, three.parentIntent.identity);
    EXPECT_EQ(intentForSubstring(attributedString.get(), @"eight").ordinal, 8);

    NSPresentationIntent *outer = intentForSubstring(attributedString.get(), @"outer");
    ASSERT_NOT_NULL(outer);
    EXPECT_EQ(outer.intentKind, NSPresentationIntentKindListItem);
    EXPECT_EQ(outer.ordinal, 1);
    EXPECT_EQ(outer.parentIntent.intentKind, NSPresentationIntentKindUnorderedList);

    NSUInteger afterOuter = NSMaxRange([[attributedString string] rangeOfString:@"outer"]);
    ASSERT_LT(afterOuter, [attributedString length]);
    EXPECT_EQ([[attributedString string] characterAtIndex:afterOuter], '\n');
    EXPECT_EQ(static_cast<NSPresentationIntent *>([attributedString attribute:NSPresentationIntentAttributeName atIndex:afterOuter effectiveRange:nil]).identity, outer.identity);

    NSPresentationIntent *inner = intentForSubstring(attributedString.get(), @"inner");
    ASSERT_NOT_NULL(inner);
    EXPECT_EQ(inner.intentKind, NSPresentationIntentKindListItem);
    EXPECT_EQ(inner.parentIntent.intentKind, NSPresentationIntentKindUnorderedList);
    EXPECT_EQ(inner.parentIntent.parentIntent.identity, outer.identity);
}

TEST(PresentationIntents, CodeBlocks)
{
    RetainPtr attributedString = attributedStringWithPresentationIntents(@"<pre><code class='language-swift'>let first\nlet second</code></pre><pre>\n  <code>spaced</code>\n</pre>"
        "<pre role='code' class='language-js'>rolecode</pre><p>Use <code>inline</code> code</p>");

    NSPresentationIntent *first = intentForSubstring(attributedString.get(), @"first");
    ASSERT_NOT_NULL(first);
    EXPECT_EQ(first.intentKind, NSPresentationIntentKindCodeBlock);
    EXPECT_WK_STREQ(first.languageHint, "swift");
    EXPECT_EQ(intentForSubstring(attributedString.get(), @"second").identity, first.identity);
    EXPECT_EQ(inlineIntentForSubstring(attributedString.get(), @"first"), 0U);

    // Whitespace around the <code> doesn't keep the <pre> from being a code block.
    NSPresentationIntent *spaced = intentForSubstring(attributedString.get(), @"spaced");
    ASSERT_NOT_NULL(spaced);
    EXPECT_EQ(spaced.intentKind, NSPresentationIntentKindCodeBlock);
    // FIXME: The converter leaves the hint nil, but CoreIPCPresentationIntent turns a nil hint into an empty string.
    EXPECT_EQ(spaced.languageHint.length, 0U);
    EXPECT_EQ(inlineIntentForSubstring(attributedString.get(), @"spaced"), 0U);

    NSPresentationIntent *roleCode = intentForSubstring(attributedString.get(), @"rolecode");
    ASSERT_NOT_NULL(roleCode);
    EXPECT_EQ(roleCode.intentKind, NSPresentationIntentKindCodeBlock);
    EXPECT_WK_STREQ(roleCode.languageHint, "js");
    EXPECT_EQ(inlineIntentForSubstring(attributedString.get(), @"rolecode"), 0U);

    EXPECT_EQ(intentForSubstring(attributedString.get(), @"inline").intentKind, NSPresentationIntentKindParagraph);
    EXPECT_EQ(inlineIntentForSubstring(attributedString.get(), @"inline"), NSInlinePresentationIntentCode);
}

TEST(PresentationIntents, PreformattedTextWithoutCode)
{
    RetainPtr attributedString = attributedStringWithPresentationIntents(@"<pre>plain</pre><pre>see <code>mixed</code> here</pre><pre><code>one</code><code>two</code></pre>"
        "<pre class='language-swift'>hinted</pre><pre><code role='presentation'>presented</code></pre>");

    // A <pre> is only a code block if its only child element is a <code>, so other preformatted text is a paragraph.
    NSPresentationIntent *plain = intentForSubstring(attributedString.get(), @"plain");
    ASSERT_NOT_NULL(plain);
    EXPECT_EQ(plain.intentKind, NSPresentationIntentKindParagraph);

    NSPresentationIntent *see = intentForSubstring(attributedString.get(), @"see");
    ASSERT_NOT_NULL(see);
    EXPECT_EQ(see.intentKind, NSPresentationIntentKindParagraph);
    EXPECT_NE(see.identity, plain.identity);
    EXPECT_EQ(intentForSubstring(attributedString.get(), @"mixed").identity, see.identity);
    EXPECT_EQ(inlineIntentForSubstring(attributedString.get(), @"mixed"), NSInlinePresentationIntentCode);

    EXPECT_EQ(intentForSubstring(attributedString.get(), @"one").intentKind, NSPresentationIntentKindParagraph);
    EXPECT_EQ(inlineIntentForSubstring(attributedString.get(), @"one"), NSInlinePresentationIntentCode);

    // A language class alone doesn't make a code block; the <pre> needs a <code> child or role="code".
    EXPECT_EQ(intentForSubstring(attributedString.get(), @"hinted").intentKind, NSPresentationIntentKindParagraph);

    // A role that replaces the meaning of the <code> means the <pre> doesn't hold code either.
    EXPECT_EQ(intentForSubstring(attributedString.get(), @"presented").intentKind, NSPresentationIntentKindParagraph);
    EXPECT_EQ(inlineIntentForSubstring(attributedString.get(), @"presented"), 0U);
}

TEST(PresentationIntents, MultilinePreformattedText)
{
    RetainPtr attributedString = attributedStringWithPresentationIntents(@"<pre>line one\nline two</pre><pre><code>code one\ncode two</code></pre><pre><span role='code'>role one\nrole two</span></pre>");
    NSString *string = [attributedString string];

    // Preformatted text that isn't code stays a single paragraph, with a line separator where it breaks, as a <br> in a paragraph does.
    RetainPtr expectedLines = [NSString stringWithFormat:@"line one%Cline two\n", static_cast<unichar>(NSLineSeparatorCharacter)];
    EXPECT_TRUE([string hasPrefix:expectedLines]);
    NSPresentationIntent *lineOne = intentForSubstring(attributedString.get(), @"line one");
    ASSERT_NOT_NULL(lineOne);
    EXPECT_EQ(lineOne.intentKind, NSPresentationIntentKindParagraph);
    EXPECT_EQ(intentForSubstring(attributedString.get(), @"line two").identity, lineOne.identity);

    // A code block keeps its newlines and spans them.
    EXPECT_NE([string rangeOfString:@"code one\ncode two"].location, static_cast<NSUInteger>(NSNotFound));
    NSPresentationIntent *codeOne = intentForSubstring(attributedString.get(), @"code one");
    ASSERT_NOT_NULL(codeOne);
    EXPECT_EQ(codeOne.intentKind, NSPresentationIntentKindCodeBlock);
    EXPECT_EQ(intentForSubstring(attributedString.get(), @"code two").identity, codeOne.identity);

    // Any element with role="code" is the code of a code block.
    EXPECT_NE([string rangeOfString:@"role one\nrole two"].location, static_cast<NSUInteger>(NSNotFound));
    NSPresentationIntent *roleOne = intentForSubstring(attributedString.get(), @"role one");
    ASSERT_NOT_NULL(roleOne);
    EXPECT_EQ(roleOne.intentKind, NSPresentationIntentKindCodeBlock);
    EXPECT_EQ(inlineIntentForSubstring(attributedString.get(), @"role one"), 0U);
}

TEST(PresentationIntents, InlineIntents)
{
    RetainPtr attributedString = attributedStringWithPresentationIntents(@"<p><em>soft</em> <strong>loud</strong> <del>gone</del> <strong><em>both</em></strong> plain</p>");

    EXPECT_EQ(inlineIntentForSubstring(attributedString.get(), @"soft"), NSInlinePresentationIntentEmphasized);
    EXPECT_EQ(inlineIntentForSubstring(attributedString.get(), @"loud"), NSInlinePresentationIntentStronglyEmphasized);
    EXPECT_EQ(inlineIntentForSubstring(attributedString.get(), @"gone"), NSInlinePresentationIntentStrikethrough);
    EXPECT_EQ(inlineIntentForSubstring(attributedString.get(), @"both"), NSInlinePresentationIntentEmphasized | NSInlinePresentationIntentStronglyEmphasized);
    EXPECT_EQ(inlineIntentForSubstring(attributedString.get(), @"plain"), 0U);
}

TEST(PresentationIntents, Roles)
{
    RetainPtr attributedString = attributedStringWithPresentationIntents(@"<div role='heading' aria-level='3'>rolehead</div><div role='paragraph'>rolepara</div>"
        "<div role='list'><div role='listitem'>roleitem</div></div><div role='blockquote'>rolequote</div><div role='separator'></div>"
        "<p role='presentation'>nothing</p><p><span role='emphasis'>roleem</span> <span role='strong'>rolestrong</span> <span role='code'>rolecode</span> <span role='deletion'>roledel</span></p>");

    NSPresentationIntent *heading = intentForSubstring(attributedString.get(), @"rolehead");
    ASSERT_NOT_NULL(heading);
    EXPECT_EQ(heading.intentKind, NSPresentationIntentKindHeader);
    EXPECT_EQ(heading.headerLevel, 3);

    EXPECT_EQ(intentForSubstring(attributedString.get(), @"rolepara").intentKind, NSPresentationIntentKindParagraph);

    NSPresentationIntent *item = intentForSubstring(attributedString.get(), @"roleitem");
    ASSERT_NOT_NULL(item);
    EXPECT_EQ(item.intentKind, NSPresentationIntentKindListItem);
    EXPECT_EQ(item.ordinal, 1);
    EXPECT_EQ(item.parentIntent.intentKind, NSPresentationIntentKindUnorderedList);

    EXPECT_EQ(intentForSubstring(attributedString.get(), @"rolequote").intentKind, NSPresentationIntentKindBlockQuote);
    EXPECT_EQ(intentForSubstring(attributedString.get(), thematicBreakString().get()).intentKind, NSPresentationIntentKindThematicBreak);
    EXPECT_NULL(intentForSubstring(attributedString.get(), @"nothing"));

    EXPECT_EQ(inlineIntentForSubstring(attributedString.get(), @"roleem"), NSInlinePresentationIntentEmphasized);
    EXPECT_EQ(inlineIntentForSubstring(attributedString.get(), @"rolestrong"), NSInlinePresentationIntentStronglyEmphasized);
    EXPECT_EQ(inlineIntentForSubstring(attributedString.get(), @"rolecode"), NSInlinePresentationIntentCode);
    EXPECT_EQ(inlineIntentForSubstring(attributedString.get(), @"roledel"), NSInlinePresentationIntentStrikethrough);
}

TEST(PresentationIntents, NestedBlockQuotes)
{
    RetainPtr attributedString = attributedStringWithPresentationIntents(@"<blockquote><blockquote>inner</blockquote>outer<p>para</p></blockquote>");

    NSPresentationIntent *inner = intentForSubstring(attributedString.get(), @"inner");
    ASSERT_NOT_NULL(inner);
    EXPECT_EQ(inner.intentKind, NSPresentationIntentKindBlockQuote);
    EXPECT_EQ(inner.parentIntent.intentKind, NSPresentationIntentKindBlockQuote);

    NSPresentationIntent *outer = intentForSubstring(attributedString.get(), @"outer");
    ASSERT_NOT_NULL(outer);
    EXPECT_EQ(outer.intentKind, NSPresentationIntentKindBlockQuote);
    EXPECT_EQ(outer.identity, inner.parentIntent.identity);

    NSPresentationIntent *paragraph = intentForSubstring(attributedString.get(), @"para");
    ASSERT_NOT_NULL(paragraph);
    EXPECT_EQ(paragraph.intentKind, NSPresentationIntentKindParagraph);
    EXPECT_EQ(paragraph.parentIntent.identity, outer.identity);
}

TEST(PresentationIntents, TrailingPreservedNewlines)
{
    NSString *html = @"<pre>a\nb\n</pre><p>next</p><p style='white-space:pre-wrap'>xray\nyoke\n</p><ul><li style='white-space:pre-wrap'>item one\nitem two</li></ul>";
    RetainPtr attributedString = attributedStringWithPresentationIntents(html);

    // A trailing preserved newline gives way to the paragraph break rather than adding an empty line.
    RetainPtr expectedString = [NSString stringWithFormat:@"a%Cb\nnext\nxray%Cyoke\n\titem one%Citem two\n", static_cast<unichar>(NSLineSeparatorCharacter), static_cast<unichar>(NSLineSeparatorCharacter), static_cast<unichar>(NSLineSeparatorCharacter)];
    NSString *string = [attributedString string];
    NSRange itemMarkerEnd = [string rangeOfString:@"\titem one"];
    ASSERT_NE(itemMarkerEnd.location, static_cast<NSUInteger>(NSNotFound));
    // Compare everything but the list marker, whose text depends on the list style.
    NSRange markerStart = [string rangeOfString:@"\t" options:0 range:NSMakeRange(0, itemMarkerEnd.location)];
    ASSERT_NE(markerStart.location, static_cast<NSUInteger>(NSNotFound));
    RetainPtr withoutMarker = [string stringByReplacingCharactersInRange:NSMakeRange(markerStart.location, itemMarkerEnd.location - markerStart.location) withString:@""];
    EXPECT_WK_STREQ(withoutMarker.get(), expectedString.get());

    EXPECT_EQ(intentForSubstring(attributedString.get(), @"yoke").identity, intentForSubstring(attributedString.get(), @"xray").identity);
    EXPECT_EQ(intentForSubstring(attributedString.get(), @"xray").intentKind, NSPresentationIntentKindParagraph);
    NSPresentationIntent *itemOne = intentForSubstring(attributedString.get(), @"item one");
    ASSERT_NOT_NULL(itemOne);
    EXPECT_EQ(itemOne.intentKind, NSPresentationIntentKindListItem);
    EXPECT_EQ(intentForSubstring(attributedString.get(), @"item two").identity, itemOne.identity);

    // Without the default, preserved newlines are left as they were.
    RetainPtr withoutIntents = attributedStringWithPresentationIntents(@"<pre>a\nb\n</pre><p>next</p><p style='white-space:pre-wrap'>xray\nyoke\n</p>", ReadsAllPresentationIntents::No);
    EXPECT_WK_STREQ([withoutIntents string], "a\nb\nnext\nxray\nyoke\n");
}

TEST(PresentationIntents, ThematicBreaks)
{
    RetainPtr attributedString = attributedStringWithPresentationIntents(@"<p>before</p><hr><p>after</p>");
    RetainPtr thematicBreak = thematicBreakString();

    RetainPtr expectedString = [NSString stringWithFormat:@"before\n%@\nafter\n", thematicBreak.get()];
    EXPECT_WK_STREQ([attributedString string], expectedString.get());
    EXPECT_EQ(intentForSubstring(attributedString.get(), thematicBreak.get()).intentKind, NSPresentationIntentKindThematicBreak);
    EXPECT_EQ(intentForSubstring(attributedString.get(), @"before").intentKind, NSPresentationIntentKindParagraph);
    EXPECT_EQ(intentForSubstring(attributedString.get(), @"after").intentKind, NSPresentationIntentKindParagraph);
}

TEST(PresentationIntents, ParagraphSeparators)
{
    RetainPtr attributedString = attributedStringWithPresentationIntents(@"<p>Body <em>soft</em></p><h2>Next</h2>");
    EXPECT_WK_STREQ([attributedString string], "Body soft\nNext\n");

    NSPresentationIntent *body = intentForSubstring(attributedString.get(), @"Body");
    ASSERT_NOT_NULL(body);
    NSUInteger afterSoft = NSMaxRange([[attributedString string] rangeOfString:@"soft"]);
    EXPECT_EQ(static_cast<NSPresentationIntent *>([attributedString attribute:NSPresentationIntentAttributeName atIndex:afterSoft effectiveRange:nil]).identity, body.identity);
    EXPECT_EQ([[attributedString attribute:NSInlinePresentationIntentAttributeName atIndex:afterSoft effectiveRange:nil] unsignedIntegerValue], NSInlinePresentationIntentEmphasized);

    NSPresentationIntent *next = intentForSubstring(attributedString.get(), @"Next");
    ASSERT_NOT_NULL(next);
    EXPECT_EQ(next.intentKind, NSPresentationIntentKindHeader);
    EXPECT_EQ(static_cast<NSPresentationIntent *>([attributedString attribute:NSPresentationIntentAttributeName atIndex:[attributedString length] - 1 effectiveRange:nil]).identity, next.identity);
}

TEST(PresentationIntents, BlocksInListItems)
{
    RetainPtr attributedString = attributedStringWithPresentationIntents(@"<ol><li><p>alpha</p><p>beta</p></li><li>gamma</li></ol>");
    NSString *string = [attributedString string];

    RetainPtr lineSeparator = [NSString stringWithFormat:@"%C", static_cast<unichar>(NSLineSeparatorCharacter)];
    EXPECT_EQ([string rangeOfString:lineSeparator].location, static_cast<NSUInteger>(NSNotFound));

    NSPresentationIntent *alpha = intentForSubstring(attributedString.get(), @"alpha");
    NSPresentationIntent *beta = intentForSubstring(attributedString.get(), @"beta");
    NSPresentationIntent *gamma = intentForSubstring(attributedString.get(), @"gamma");
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
    ASSERT_NOT_NULL([[attributedString attribute:NSParagraphStyleAttributeName atIndex:gammaRange.location effectiveRange:nil] textLists].firstObject);
    EXPECT_WK_STREQ(listMarkerForSubstring(attributedString.get(), @"gamma"), expectedListMarker(attributedString.get(), @"gamma", 2));
}

TEST(PresentationIntents, EmptyParagraphs)
{
    RetainPtr paragraphs = attributedStringWithPresentationIntents(@"<p>a</p><p></p><p>b</p>");
    EXPECT_WK_STREQ([paragraphs string], "a\n\nb\n");
    NSPresentationIntent *a = intentForSubstring(paragraphs.get(), @"a");
    ASSERT_NOT_NULL(a);
    NSPresentationIntent *empty = [paragraphs attribute:NSPresentationIntentAttributeName atIndex:2 effectiveRange:nil];
    ASSERT_NOT_NULL(empty);
    EXPECT_EQ(empty.intentKind, NSPresentationIntentKindParagraph);
    EXPECT_NE(empty.identity, a.identity);
    EXPECT_EQ(static_cast<NSPresentationIntent *>([paragraphs attribute:NSPresentationIntentAttributeName atIndex:1 effectiveRange:nil]).identity, a.identity);

    RetainPtr items = attributedStringWithPresentationIntents(@"<ol><li>one</li><li></li><li>three</li></ol>");
    NSPresentationIntent *three = intentForSubstring(items.get(), @"three");
    ASSERT_NOT_NULL(three);
    EXPECT_EQ(three.ordinal, 3);
    EXPECT_WK_STREQ(listMarkerForSubstring(items.get(), @"three"), expectedListMarker(items.get(), @"three", 3));

    NSRange threeParagraph = [[items string] paragraphRangeForRange:NSMakeRange([[items string] rangeOfString:@"three"].location, 0)];
    ASSERT_GT(threeParagraph.location, 0U);
    NSPresentationIntent *emptyItem = [items attribute:NSPresentationIntentAttributeName atIndex:threeParagraph.location - 1 effectiveRange:nil];
    ASSERT_NOT_NULL(emptyItem);
    EXPECT_EQ(emptyItem.intentKind, NSPresentationIntentKindListItem);
    EXPECT_EQ(emptyItem.ordinal, 2);
}

TEST(PresentationIntents, RoleListInListItem)
{
    RetainPtr attributedString = attributedStringWithPresentationIntents(@"<ol><li>first<div role='list'><div role='listitem'>nested</div></div></li><li>second</li></ol>");

    NSPresentationIntent *nested = intentForSubstring(attributedString.get(), @"nested");
    ASSERT_NOT_NULL(nested);
    EXPECT_EQ(nested.intentKind, NSPresentationIntentKindListItem);
    EXPECT_EQ(nested.parentIntent.parentIntent.identity, intentForSubstring(attributedString.get(), @"first").identity);

    // The role-based list has no NSTextList, so its item continues the outer item without a marker.
    EXPECT_WK_STREQ(listMarkerForSubstring(attributedString.get(), @"nested"), "");
    EXPECT_WK_STREQ(listMarkerForSubstring(attributedString.get(), @"second"), expectedListMarker(attributedString.get(), @"second", 2));
}

TEST(PresentationIntents, SeparatorRoleOnList)
{
    RetainPtr attributedString = attributedStringWithPresentationIntents(@"<ol><li>one<ul role='separator'><li>nested</li></ul></li><li>two</li></ol>");

    EXPECT_EQ([[attributedString string] rangeOfString:thematicBreakString()].location, static_cast<NSUInteger>(NSNotFound));

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

TEST(PresentationIntents, CodeBlockLanguageCase)
{
    // Without a doctype the document is in quirks mode, where class names are matched without case.
    RetainPtr attributedString = attributedStringWithPresentationIntents(@"<pre role='code' class='language-ObjC'>objc</pre><pre><code class='language-CPlusPlus'>cpp</code></pre>");

    EXPECT_WK_STREQ(intentForSubstring(attributedString.get(), @"objc").languageHint, "ObjC");
    EXPECT_WK_STREQ(intentForSubstring(attributedString.get(), @"cpp").languageHint, "CPlusPlus");
}

TEST(PresentationIntents, ListItemsWithoutListItemDisplay)
{
    RetainPtr attributedString = attributedStringWithPresentationIntents(@"<ol start='5'><li style='display:block'>five</li><li style='display:block' value='9'>nine</li><li style='display:block'>ten</li></ol>"
        "<ol reversed><li style='display:block'>third</li><li style='display:block'>second</li><li style='display:block'>first</li></ol>"
        "<ol reversed start='20'><li style='display:block'>twenty</li><li style='display:block'>nineteen</li></ol>"
        "<ol start='3'><div role='listitem'>rolethree</div><p>skipped</p><div role='listitem'>rolefour</div></ol>");

    EXPECT_EQ(intentForSubstring(attributedString.get(), @"five").ordinal, 5);
    EXPECT_EQ(intentForSubstring(attributedString.get(), @"nine").ordinal, 9);
    EXPECT_EQ(intentForSubstring(attributedString.get(), @"ten").ordinal, 10);

    EXPECT_EQ(intentForSubstring(attributedString.get(), @"third").ordinal, 3);
    EXPECT_EQ(intentForSubstring(attributedString.get(), @"second").ordinal, 2);
    EXPECT_EQ(intentForSubstring(attributedString.get(), @"first").ordinal, 1);

    EXPECT_EQ(intentForSubstring(attributedString.get(), @"twenty").ordinal, 20);
    EXPECT_EQ(intentForSubstring(attributedString.get(), @"nineteen").ordinal, 19);

    NSPresentationIntent *roleThree = intentForSubstring(attributedString.get(), @"rolethree");
    ASSERT_NOT_NULL(roleThree);
    EXPECT_EQ(roleThree.intentKind, NSPresentationIntentKindListItem);
    EXPECT_EQ(roleThree.ordinal, 3);
    EXPECT_EQ(intentForSubstring(attributedString.get(), @"rolefour").ordinal, 4);
}

TEST(PresentationIntents, InlineIntentsForPresentationRole)
{
    RetainPtr attributedString = attributedStringWithPresentationIntents(@"<p><em role='presentation'>plain</em> <strong><em role='none'>loud</em></strong></p>");

    EXPECT_EQ(inlineIntentForSubstring(attributedString.get(), @"plain"), 0U);
    EXPECT_EQ(inlineIntentForSubstring(attributedString.get(), @"loud"), NSInlinePresentationIntentStronglyEmphasized);
}

TEST(PresentationIntents, PartialRanges)
{
    RetainPtr webView = webViewWithPresentationIntents(@"<ul><li id='item'>first item</li><li>second</li></ul><blockquote><p id='quoted'>quoted text</p></blockquote>");

    RetainPtr item = selectedAttributedStringWithPresentationIntents(webView.get(), @"item", 2, 7);
    EXPECT_WK_STREQ([item string], "rst i");
    NSPresentationIntent *itemIntent = intentForSubstring(item.get(), @"rst");
    ASSERT_NOT_NULL(itemIntent);
    EXPECT_EQ(itemIntent.intentKind, NSPresentationIntentKindListItem);
    EXPECT_EQ(itemIntent.ordinal, 1);
    EXPECT_EQ(itemIntent.parentIntent.intentKind, NSPresentationIntentKindUnorderedList);

    RetainPtr quoted = selectedAttributedStringWithPresentationIntents(webView.get(), @"quoted", 0, 6);
    EXPECT_WK_STREQ([quoted string], "quoted");
    NSPresentationIntent *quotedIntent = intentForSubstring(quoted.get(), @"quoted");
    ASSERT_NOT_NULL(quotedIntent);
    EXPECT_EQ(quotedIntent.intentKind, NSPresentationIntentKindParagraph);
    EXPECT_EQ(quotedIntent.parentIntent.intentKind, NSPresentationIntentKindBlockQuote);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
