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
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <wtf/RetainPtr.h>

@interface PresentationIntentFontAttributesListener : NSObject <WKUIDelegatePrivate> {
@public
    bool _didChangeFontAttributes;
}
@property (nonatomic, readonly) NSDictionary *lastFontAttributes;
@end

@implementation PresentationIntentFontAttributesListener {
    RetainPtr<NSDictionary> _lastFontAttributes;
}

- (void)_webView:(WKWebView *)webView didChangeFontAttributes:(NSDictionary<NSString *, id> *)fontAttributes
{
    _lastFontAttributes = fontAttributes;
    _didChangeFontAttributes = true;
}

- (NSDictionary *)lastFontAttributes
{
    return _lastFontAttributes.get();
}

@end

// Stands in for the AppKit text formatting bar, which sends -changeAttributes: with a converter that adds a style's attributes.
@interface PresentationIntentAttributeConverter : NSObject
- (instancetype)initWithAttributes:(NSDictionary *)attributes;
- (NSDictionary *)convertAttributes:(NSDictionary *)attributes;
@end

@implementation PresentationIntentAttributeConverter {
    RetainPtr<NSDictionary> _attributes;
}

- (instancetype)initWithAttributes:(NSDictionary *)attributes
{
    if (!(self = [super init]))
        return nil;
    _attributes = attributes;
    return self;
}

- (NSDictionary *)convertAttributes:(NSDictionary *)attributes
{
    RetainPtr result = adoptNS([attributes mutableCopy]);
    [result addEntriesFromDictionary:_attributes.get()];
    return result.autorelease();
}

@end

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

static RetainPtr<TestWKWebView> editableWebViewWithPresentationIntents(NSString *bodyContents, PresentationIntentFontAttributesListener *listener, ReadsAllPresentationIntents readsAllPresentationIntents = ReadsAllPresentationIntents::Yes)
{
    RetainPtr webView = webViewWithPresentationIntents([NSString stringWithFormat:@"<body contenteditable>%@</body>", bodyContents], readsAllPresentationIntents);
    [webView setUIDelegate:listener];
    [webView stringByEvaluatingJavaScript:@"document.body.focus()"];
    return webView;
}

// Puts the caret after the first character of the element's first text node and returns the font attributes reported for it.
static NSDictionary *fontAttributesWithCaretInElement(TestWKWebView *webView, PresentationIntentFontAttributesListener *listener, NSString *elementID)
{
    listener->_didChangeFontAttributes = false;
    [webView stringByEvaluatingJavaScript:[NSString stringWithFormat:@"getSelection().setPosition(document.getElementById('%@').firstChild, 1)", elementID]];
    Util::run(&listener->_didChangeFontAttributes);
    [webView waitForNextPresentationUpdate];
    return listener.lastFontAttributes;
}

static NSPresentationIntent *presentationIntentWithCaretInElement(TestWKWebView *webView, PresentationIntentFontAttributesListener *listener, NSString *elementID)
{
    return fontAttributesWithCaretInElement(webView, listener, elementID)[NSPresentationIntentAttributeName];
}

static void placeCaretInElement(TestWKWebView *webView, NSString *elementID)
{
    [webView stringByEvaluatingJavaScript:[NSString stringWithFormat:@"getSelection().setPosition(document.getElementById('%@').firstChild, 1)", elementID]];
}

// The tag names of the editable body's element children, with their element children, such as "H1" or "UL(LI)".
static RetainPtr<NSString> blockStructure(TestWKWebView *webView)
{
    [webView waitForNextPresentationUpdate];
    return [webView stringByEvaluatingJavaScript:@"[...document.body.children].map(element => element.children.length ? `${element.tagName}(${[...element.children].map(child => child.tagName).join(',')})` : element.tagName).join(',')"];
}

// The UI process only sends -changeAttributes: on once it knows the selection is editable.
static void changeAttributes(TestWKWebView *webView, id converter)
{
    [webView waitForNextPresentationUpdate];
    [webView performSelector:@selector(changeAttributes:) withObject:converter];
}

TEST(PresentationIntents, FontAttributesReportBlockIntents)
{
    RetainPtr listener = adoptNS([[PresentationIntentFontAttributesListener alloc] init]);
    RetainPtr webView = editableWebViewWithPresentationIntents(@"<h2 id='heading'>Heading</h2><p id='paragraph'>Body</p>"
        "<blockquote><p id='quoted'>Quoted</p></blockquote><ol start='3'><li id='item'>Item</li></ol>"
        "<pre><code id='code' class='language-swift'>let x</code></pre><div id='plain'>Plain</div>", listener.get());

    NSPresentationIntent *heading = presentationIntentWithCaretInElement(webView.get(), listener.get(), @"heading");
    ASSERT_NOT_NULL(heading);
    EXPECT_EQ(heading.intentKind, NSPresentationIntentKindHeader);
    EXPECT_EQ(heading.headerLevel, 2);
    EXPECT_NULL(heading.parentIntent);

    NSPresentationIntent *paragraph = presentationIntentWithCaretInElement(webView.get(), listener.get(), @"paragraph");
    ASSERT_NOT_NULL(paragraph);
    EXPECT_EQ(paragraph.intentKind, NSPresentationIntentKindParagraph);

    NSPresentationIntent *quoted = presentationIntentWithCaretInElement(webView.get(), listener.get(), @"quoted");
    ASSERT_NOT_NULL(quoted);
    EXPECT_EQ(quoted.intentKind, NSPresentationIntentKindParagraph);
    EXPECT_EQ(quoted.parentIntent.intentKind, NSPresentationIntentKindBlockQuote);
    EXPECT_NE(quoted.identity, quoted.parentIntent.identity);

    NSPresentationIntent *item = presentationIntentWithCaretInElement(webView.get(), listener.get(), @"item");
    ASSERT_NOT_NULL(item);
    EXPECT_EQ(item.intentKind, NSPresentationIntentKindListItem);
    EXPECT_EQ(item.ordinal, 3);
    EXPECT_EQ(item.parentIntent.intentKind, NSPresentationIntentKindOrderedList);

    NSPresentationIntent *code = presentationIntentWithCaretInElement(webView.get(), listener.get(), @"code");
    ASSERT_NOT_NULL(code);
    EXPECT_EQ(code.intentKind, NSPresentationIntentKindCodeBlock);
    EXPECT_WK_STREQ(code.languageHint, "swift");

    EXPECT_NULL(presentationIntentWithCaretInElement(webView.get(), listener.get(), @"plain"));
}

TEST(PresentationIntents, FontAttributesRequireDefault)
{
    RetainPtr listener = adoptNS([[PresentationIntentFontAttributesListener alloc] init]);
    RetainPtr webView = editableWebViewWithPresentationIntents(@"<h1 id='heading'>Heading</h1>", listener.get(), ReadsAllPresentationIntents::No);
    NSDictionary *attributes = fontAttributesWithCaretInElement(webView.get(), listener.get(), @"heading");
    ASSERT_NOT_NULL(attributes);
    EXPECT_NULL(attributes[NSPresentationIntentAttributeName]);
}

TEST(PresentationIntents, SetBlockPresentationIntent)
{
    RetainPtr listener = adoptNS([[PresentationIntentFontAttributesListener alloc] init]);
    RetainPtr webView = editableWebViewWithPresentationIntents(@"<p id='line'>Line</p>", listener.get());

    placeCaretInElement(webView.get(), @"line");
    [webView _setBlockPresentationIntent:[NSPresentationIntent headerIntentWithIdentity:1 level:1 nestedInsideIntent:nil]];
    EXPECT_WK_STREQ(blockStructure(webView.get()).get(), "H1");

    [webView _setBlockPresentationIntent:[NSPresentationIntent headerIntentWithIdentity:1 level:3 nestedInsideIntent:nil]];
    EXPECT_WK_STREQ(blockStructure(webView.get()).get(), "H3");

    [webView _setBlockPresentationIntent:[NSPresentationIntent paragraphIntentWithIdentity:1 nestedInsideIntent:nil]];
    EXPECT_WK_STREQ(blockStructure(webView.get()).get(), "P");

    [webView _setBlockPresentationIntent:[NSPresentationIntent blockQuoteIntentWithIdentity:1 nestedInsideIntent:nil]];
    EXPECT_WK_STREQ(blockStructure(webView.get()).get(), "BLOCKQUOTE");

    [webView _setBlockPresentationIntent:[NSPresentationIntent paragraphIntentWithIdentity:1 nestedInsideIntent:nil]];
    [webView _setBlockPresentationIntent:[NSPresentationIntent unorderedListIntentWithIdentity:1 nestedInsideIntent:nil]];
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"document.querySelector('ul > li')?.textContent"], "Line");

    // Applying the list kind the selection already has keeps the list.
    [webView _setBlockPresentationIntent:[NSPresentationIntent unorderedListIntentWithIdentity:1 nestedInsideIntent:nil]];
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"document.querySelector('ul > li')?.textContent"], "Line");

    [webView _setBlockPresentationIntent:[NSPresentationIntent orderedListIntentWithIdentity:1 nestedInsideIntent:nil]];
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"document.querySelector('ol > li')?.textContent"], "Line");
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"String(!!document.querySelector('ul'))"], "false");
}

TEST(PresentationIntents, SetBlockPresentationIntentReportsAndUndoes)
{
    RetainPtr listener = adoptNS([[PresentationIntentFontAttributesListener alloc] init]);
    RetainPtr webView = editableWebViewWithPresentationIntents(@"<p id='line'>Line</p>", listener.get());

    placeCaretInElement(webView.get(), @"line");
    listener->_didChangeFontAttributes = false;
    [webView _setBlockPresentationIntent:[NSPresentationIntent headerIntentWithIdentity:1 level:2 nestedInsideIntent:nil]];
    Util::run(&listener->_didChangeFontAttributes);
    [webView waitForNextPresentationUpdate];
    NSPresentationIntent *intent = listener.get().lastFontAttributes[NSPresentationIntentAttributeName];
    EXPECT_EQ(intent.intentKind, NSPresentationIntentKindHeader);
    EXPECT_EQ(intent.headerLevel, 2);

    [webView _executeEditCommand:@"Undo" argument:nil completion:nil];
    EXPECT_WK_STREQ(blockStructure(webView.get()).get(), "P");
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"document.getElementById('line')?.textContent"], "Line");
}

TEST(PresentationIntents, SetBlockPresentationIntentCodeBlock)
{
    RetainPtr listener = adoptNS([[PresentationIntentFontAttributesListener alloc] init]);
    RetainPtr webView = editableWebViewWithPresentationIntents(@"<p id='line'>Line</p>", listener.get());

    placeCaretInElement(webView.get(), @"line");
    [webView _setBlockPresentationIntent:[NSPresentationIntent codeBlockIntentWithIdentity:1 languageHint:nil nestedInsideIntent:nil]];
    EXPECT_WK_STREQ(blockStructure(webView.get()).get(), "PRE(CODE)");
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"document.querySelector('pre > code')?.textContent"], "Line");

    // The caret is still in the block, so the code block is reported.
    listener->_didChangeFontAttributes = false;
    [webView stringByEvaluatingJavaScript:@"getSelection().setPosition(document.querySelector('code').firstChild, 2)"];
    Util::run(&listener->_didChangeFontAttributes);
    [webView waitForNextPresentationUpdate];
    NSPresentationIntent *intent = listener.get().lastFontAttributes[NSPresentationIntentAttributeName];
    EXPECT_EQ(intent.intentKind, NSPresentationIntentKindCodeBlock);

    // Leaving the code block leaves no inline code behind.
    [webView _setBlockPresentationIntent:[NSPresentationIntent paragraphIntentWithIdentity:1 nestedInsideIntent:nil]];
    EXPECT_WK_STREQ(blockStructure(webView.get()).get(), "P");
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"document.body.textContent"], "Line");
}

TEST(PresentationIntents, SetBlockPresentationIntentCodeBlockUndoesInOneStep)
{
    RetainPtr listener = adoptNS([[PresentationIntentFontAttributesListener alloc] init]);
    RetainPtr webView = editableWebViewWithPresentationIntents(@"<p id='line'>Line</p>", listener.get());

    placeCaretInElement(webView.get(), @"line");
    [webView _setBlockPresentationIntent:[NSPresentationIntent codeBlockIntentWithIdentity:1 languageHint:nil nestedInsideIntent:nil]];
    EXPECT_WK_STREQ(blockStructure(webView.get()).get(), "PRE(CODE)");

    [webView _executeEditCommand:@"Undo" argument:nil completion:nil];
    EXPECT_WK_STREQ(blockStructure(webView.get()).get(), "P");
}

TEST(PresentationIntents, SetBlockPresentationIntentLeavesList)
{
    RetainPtr listener = adoptNS([[PresentationIntentFontAttributesListener alloc] init]);
    RetainPtr webView = editableWebViewWithPresentationIntents(@"<ul><li id='item'>Item</li></ul>", listener.get());

    placeCaretInElement(webView.get(), @"item");
    [webView _setBlockPresentationIntent:[NSPresentationIntent headerIntentWithIdentity:1 level:2 nestedInsideIntent:nil]];
    EXPECT_WK_STREQ(blockStructure(webView.get()).get(), "H2");
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"String(!!document.querySelector('ul, li'))"], "false");

    [webView _executeEditCommand:@"Undo" argument:nil completion:nil];
    EXPECT_WK_STREQ(blockStructure(webView.get()).get(), "UL(LI)");
}

TEST(PresentationIntents, SetBlockPresentationIntentListFromCodeBlock)
{
    RetainPtr listener = adoptNS([[PresentationIntentFontAttributesListener alloc] init]);
    RetainPtr webView = editableWebViewWithPresentationIntents(@"<pre><code id='code'>Code</code></pre>", listener.get());

    placeCaretInElement(webView.get(), @"code");
    [webView _setBlockPresentationIntent:[NSPresentationIntent orderedListIntentWithIdentity:1 nestedInsideIntent:nil]];
    [webView waitForNextPresentationUpdate];
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"document.querySelector('ol > li')?.textContent"], "Code");
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"String(!!document.querySelector('pre, code'))"], "false");
}

TEST(PresentationIntents, SetBlockPresentationIntentInsertsThematicBreak)
{
    RetainPtr listener = adoptNS([[PresentationIntentFontAttributesListener alloc] init]);
    RetainPtr webView = editableWebViewWithPresentationIntents(@"<p id='line'>Line</p>", listener.get());

    placeCaretInElement(webView.get(), @"line");
    [webView _setBlockPresentationIntent:[NSPresentationIntent thematicBreakIntentWithIdentity:1 nestedInsideIntent:nil]];
    [webView waitForNextPresentationUpdate];
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"String(document.querySelectorAll('hr').length)"], "1");
}

TEST(PresentationIntents, ChangeAttributesAppliesBlockIntent)
{
    RetainPtr listener = adoptNS([[PresentationIntentFontAttributesListener alloc] init]);
    RetainPtr webView = editableWebViewWithPresentationIntents(@"<p id='line'>Line</p>", listener.get());

    placeCaretInElement(webView.get(), @"line");
    RetainPtr converter = adoptNS([[PresentationIntentAttributeConverter alloc] initWithAttributes:@{
        NSPresentationIntentAttributeName : [NSPresentationIntent headerIntentWithIdentity:1 level:1 nestedInsideIntent:nil],
        NSFontAttributeName : [NSFont boldSystemFontOfSize:28],
    }]);
    changeAttributes(webView.get(), converter.get());

    // The block's look comes from the page's style sheet, so the style's font is not applied inline.
    EXPECT_WK_STREQ(blockStructure(webView.get()).get(), "H1");
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"document.body.innerHTML"], "<h1>Line</h1>");
}

TEST(PresentationIntents, ChangeAttributesWithoutBlockIntent)
{
    RetainPtr listener = adoptNS([[PresentationIntentFontAttributesListener alloc] init]);
    RetainPtr webView = editableWebViewWithPresentationIntents(@"<p id='line'>Line</p>", listener.get());

    [webView stringByEvaluatingJavaScript:@"getSelection().selectAllChildren(document.getElementById('line'))"];
    RetainPtr converter = adoptNS([[PresentationIntentAttributeConverter alloc] initWithAttributes:@{
        NSUnderlineStyleAttributeName : @(NSUnderlineStyleSingle),
    }]);
    changeAttributes(webView.get(), converter.get());
    [webView waitForNextPresentationUpdate];
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"document.getElementById('line').tagName"], "P");
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"document.queryCommandState('underline') ? 'true' : 'false'"], "true");
}

// Pasted rich text carries its source's fonts inline; applying a block intent hands the block's look to the style sheet.
TEST(PresentationIntents, SetBlockPresentationIntentOnPastedRichText)
{
    RetainPtr listener = adoptNS([[PresentationIntentFontAttributesListener alloc] init]);
    RetainPtr webView = editableWebViewWithPresentationIntents(@"<div id='line'><br></div>", listener.get());
    [webView stringByEvaluatingJavaScript:@"getSelection().setPosition(document.getElementById('line'), 0)"];

    RetainPtr rtf = [@"{\\rtf1\\ansi\\ansicpg1252\\cocoartf2927\n{\\fonttbl\\f0\\fnil\\fcharset0 Helvetica;}\n"
        "{\\*\\AppleIntentTbl\n{\\AppleIntentKind0}}\n"
        "\\pard\\sa160\\AppleIntent1 \\f0\\fs26 \\cf0 Pasted body\\\n}" dataUsingEncoding:NSASCIIStringEncoding];
    [[NSPasteboard generalPasteboard] declareTypes:@[NSPasteboardTypeRTF] owner:nil];
    [[NSPasteboard generalPasteboard] setData:rtf.get() forType:NSPasteboardTypeRTF];
    [webView paste:nil];
    [webView waitForNextPresentationUpdate];

    RetainPtr<NSString> findText = @"(() => { const walker = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT); let node; while ((node = walker.nextNode())) { if (node.data.includes('Pasted body')) return node; } return null; })()";
    [webView stringByEvaluatingJavaScript:[NSString stringWithFormat:@"getSelection().setPosition(%@, 3)", findText.get()]];
    [webView _setBlockPresentationIntent:[NSPresentationIntent headerIntentWithIdentity:1 level:1 nestedInsideIntent:nil]];
    [webView waitForNextPresentationUpdate];

    RetainPtr enclosingHeading = [NSString stringWithFormat:@"%@.parentElement.closest('h1') ? 'h1' : 'none'", findText.get()];
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:enclosingHeading.get()], "h1");
    // Nothing inline in the heading sets a size, face or margin, so its text is the style sheet's heading size.
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"String([document.querySelector('h1'), ...document.querySelectorAll('h1 *')].some(element => /font|margin|line-height/.test(element.getAttribute('style') ?? '') || element.hasAttribute('size') || element.hasAttribute('face')))"], "false");
    RetainPtr sameSize = [NSString stringWithFormat:@"getComputedStyle(%@.parentElement).fontSize === getComputedStyle(document.querySelector('h1')).fontSize ? 'same' : 'different'", findText.get()];
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:sameSize.get()], "same");
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"getComputedStyle(document.querySelector('h1')).fontSize"], "32px");

    [webView _executeEditCommand:@"Undo" argument:nil completion:nil];
    [webView waitForNextPresentationUpdate];
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"String(!!document.querySelector('h1'))"], "false");
}

// Selects from the start of the first element's text to the end of the second element's text.
static void selectFromElementToElement(TestWKWebView *webView, NSString *startElementID, NSString *endElementID)
{
    [webView stringByEvaluatingJavaScript:[NSString stringWithFormat:@"(() => { const end = document.getElementById('%@').lastChild; getSelection().setBaseAndExtent(document.getElementById('%@').firstChild, 0, end, end.length); })()", endElementID, startElementID]];
}

TEST(PresentationIntents, SetBlockPresentationIntentInBlockQuote)
{
    RetainPtr listener = adoptNS([[PresentationIntentFontAttributesListener alloc] init]);
    RetainPtr webView = editableWebViewWithPresentationIntents(@"<blockquote><p id='quoted'>Quoted</p></blockquote>", listener.get());

    // Quoted blocks are already quoted.
    placeCaretInElement(webView.get(), @"quoted");
    [webView _setBlockPresentationIntent:[NSPresentationIntent blockQuoteIntentWithIdentity:1 nestedInsideIntent:nil]];
    EXPECT_WK_STREQ(blockStructure(webView.get()).get(), "BLOCKQUOTE(P)");

    // Another kind of block replaces the quote, as it does when the quote has no paragraph inside.
    [webView _setBlockPresentationIntent:[NSPresentationIntent headerIntentWithIdentity:1 level:1 nestedInsideIntent:nil]];
    EXPECT_WK_STREQ(blockStructure(webView.get()).get(), "H1");

    [webView _executeEditCommand:@"Undo" argument:nil completion:nil];
    EXPECT_WK_STREQ(blockStructure(webView.get()).get(), "BLOCKQUOTE(P)");

    [webView _executeEditCommand:@"Redo" argument:nil completion:nil];
    EXPECT_WK_STREQ(blockStructure(webView.get()).get(), "H1");
}

TEST(PresentationIntents, SetBlockPresentationIntentLeavesBlockQuote)
{
    RetainPtr listener = adoptNS([[PresentationIntentFontAttributesListener alloc] init]);
    RetainPtr webView = editableWebViewWithPresentationIntents(@"<blockquote><p id='quoted'>Quoted</p></blockquote>", listener.get());
    placeCaretInElement(webView.get(), @"quoted");
    [webView _setBlockPresentationIntent:[NSPresentationIntent paragraphIntentWithIdentity:1 nestedInsideIntent:nil]];
    EXPECT_WK_STREQ(blockStructure(webView.get()).get(), "P");

    webView = editableWebViewWithPresentationIntents(@"<blockquote id='quoted'>Quoted</blockquote>", listener.get());
    placeCaretInElement(webView.get(), @"quoted");
    [webView _setBlockPresentationIntent:[NSPresentationIntent paragraphIntentWithIdentity:1 nestedInsideIntent:nil]];
    [webView waitForNextPresentationUpdate];
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"String(!!document.querySelector('blockquote'))"], "false");
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"document.querySelector('p')?.textContent"], "Quoted");

    webView = editableWebViewWithPresentationIntents(@"<blockquote><blockquote><p id='quoted'>Quoted</p></blockquote></blockquote>", listener.get());
    placeCaretInElement(webView.get(), @"quoted");
    [webView _setBlockPresentationIntent:[NSPresentationIntent unorderedListIntentWithIdentity:1 nestedInsideIntent:nil]];
    [webView waitForNextPresentationUpdate];
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"[...document.querySelectorAll('ul > li')].map(item => item.textContent).join()"], "Quoted");
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"String(!!document.querySelector('blockquote'))"], "false");
}

// Mail's quotes of other messages stay, along with the quotes around them.
TEST(PresentationIntents, SetBlockPresentationIntentKeepsMailBlockQuote)
{
    RetainPtr listener = adoptNS([[PresentationIntentFontAttributesListener alloc] init]);
    RetainPtr webView = editableWebViewWithPresentationIntents(@"<blockquote><blockquote type='cite'><p id='quoted'>Quoted</p></blockquote></blockquote>", listener.get());

    placeCaretInElement(webView.get(), @"quoted");
    [webView _setBlockPresentationIntent:[NSPresentationIntent headerIntentWithIdentity:1 level:2 nestedInsideIntent:nil]];
    [webView waitForNextPresentationUpdate];
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"document.querySelector('blockquote > blockquote[type=cite] h2')?.textContent"], "Quoted");
}

TEST(PresentationIntents, SetBlockPresentationIntentListItem)
{
    RetainPtr listener = adoptNS([[PresentationIntentFontAttributesListener alloc] init]);
    RetainPtr webView = editableWebViewWithPresentationIntents(@"<p id='line'>Line</p>", listener.get());

    // A list item outside a list is ignored.
    placeCaretInElement(webView.get(), @"line");
    [webView _setBlockPresentationIntent:[NSPresentationIntent listItemIntentWithIdentity:1 ordinal:1 nestedInsideIntent:nil]];
    EXPECT_WK_STREQ(blockStructure(webView.get()).get(), "P");

    RetainPtr orderedList = [NSPresentationIntent orderedListIntentWithIdentity:1 nestedInsideIntent:nil];
    [webView _setBlockPresentationIntent:[NSPresentationIntent listItemIntentWithIdentity:2 ordinal:1 nestedInsideIntent:orderedList.get()]];
    [webView waitForNextPresentationUpdate];
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"[...document.querySelectorAll('ol > li')].map(item => item.textContent).join()"], "Line");

    // Markdown puts a list item's text in a paragraph.
    RetainPtr unorderedList = [NSPresentationIntent unorderedListIntentWithIdentity:1 nestedInsideIntent:nil];
    RetainPtr listItem = [NSPresentationIntent listItemIntentWithIdentity:2 ordinal:1 nestedInsideIntent:unorderedList.get()];
    [webView _setBlockPresentationIntent:[NSPresentationIntent paragraphIntentWithIdentity:3 nestedInsideIntent:listItem.get()]];
    [webView waitForNextPresentationUpdate];
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"[...document.querySelectorAll('ul > li')].map(item => item.textContent).join()"], "Line");
}

TEST(PresentationIntents, SetBlockPresentationIntentIgnoresTables)
{
    RetainPtr listener = adoptNS([[PresentationIntentFontAttributesListener alloc] init]);
    RetainPtr webView = editableWebViewWithPresentationIntents(@"<p id='line'>Line</p>", listener.get());

    placeCaretInElement(webView.get(), @"line");
    [webView _setBlockPresentationIntent:[NSPresentationIntent tableCellIntentWithIdentity:1 column:0 nestedInsideIntent:nil]];
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"document.body.innerHTML"], "<p id=\"line\">Line</p>");
}

TEST(PresentationIntents, ChangeAttributesAppliesListItemIntent)
{
    RetainPtr listener = adoptNS([[PresentationIntentFontAttributesListener alloc] init]);
    RetainPtr webView = editableWebViewWithPresentationIntents(@"<p id='line'>Line</p>", listener.get());

    placeCaretInElement(webView.get(), @"line");
    RetainPtr orderedList = [NSPresentationIntent orderedListIntentWithIdentity:1 nestedInsideIntent:nil];
    RetainPtr converter = adoptNS([[PresentationIntentAttributeConverter alloc] initWithAttributes:@{
        NSPresentationIntentAttributeName : [NSPresentationIntent listItemIntentWithIdentity:2 ordinal:1 nestedInsideIntent:orderedList.get()],
    }]);
    changeAttributes(webView.get(), converter.get());
    [webView waitForNextPresentationUpdate];
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"[...document.querySelectorAll('ol > li')].map(item => item.textContent).join()"], "Line");
}

// A style whose intent cannot be applied still applies its other attributes.
TEST(PresentationIntents, ChangeAttributesWithIgnoredIntent)
{
    RetainPtr listener = adoptNS([[PresentationIntentFontAttributesListener alloc] init]);
    RetainPtr<NSArray<NSPresentationIntent *>> intents = @[
        [NSPresentationIntent listItemIntentWithIdentity:1 ordinal:1 nestedInsideIntent:nil],
        [NSPresentationIntent tableCellIntentWithIdentity:1 column:0 nestedInsideIntent:nil],
    ];
    for (NSPresentationIntent *intent in intents.get()) {
        RetainPtr webView = editableWebViewWithPresentationIntents(@"<p id='line'>Line</p>", listener.get());
        [webView stringByEvaluatingJavaScript:@"getSelection().selectAllChildren(document.getElementById('line'))"];
        RetainPtr converter = adoptNS([[PresentationIntentAttributeConverter alloc] initWithAttributes:@{
            NSPresentationIntentAttributeName : intent,
            NSUnderlineStyleAttributeName : @(NSUnderlineStyleSingle),
        }]);
        changeAttributes(webView.get(), converter.get());
        [webView waitForNextPresentationUpdate];
        EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"String(!!document.querySelector('ol, ul, li'))"], "false");
        EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"document.queryCommandState('underline') ? 'true' : 'false'"], "true");
    }
}

TEST(PresentationIntents, SetBlockPresentationIntentCodeBlockLanguageHint)
{
    RetainPtr listener = adoptNS([[PresentationIntentFontAttributesListener alloc] init]);
    RetainPtr webView = editableWebViewWithPresentationIntents(@"<p id='line'>Line</p><pre><code id='code' class='language-c'>Code</code></pre>", listener.get());

    placeCaretInElement(webView.get(), @"line");
    [webView _setBlockPresentationIntent:[NSPresentationIntent codeBlockIntentWithIdentity:1 languageHint:@"swift" nestedInsideIntent:nil]];
    EXPECT_WK_STREQ(blockStructure(webView.get()).get(), "PRE(CODE),PRE(CODE)");
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"document.querySelector('pre > code').className"], "language-swift");

    listener->_didChangeFontAttributes = false;
    [webView stringByEvaluatingJavaScript:@"getSelection().setPosition(document.querySelector('code').firstChild, 2)"];
    Util::run(&listener->_didChangeFontAttributes);
    [webView waitForNextPresentationUpdate];
    NSPresentationIntent *intent = listener.get().lastFontAttributes[NSPresentationIntentAttributeName];
    EXPECT_EQ(intent.intentKind, NSPresentationIntentKindCodeBlock);
    EXPECT_WK_STREQ(intent.languageHint, "swift");

    // The hint replaces the language of a code block that has one.
    placeCaretInElement(webView.get(), @"code");
    [webView _setBlockPresentationIntent:[NSPresentationIntent codeBlockIntentWithIdentity:1 languageHint:@"swift" nestedInsideIntent:nil]];
    [webView waitForNextPresentationUpdate];
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"document.getElementById('code').className"], "language-swift");
}

TEST(PresentationIntents, SetBlockPresentationIntentOnSeveralBlocks)
{
    RetainPtr listener = adoptNS([[PresentationIntentFontAttributesListener alloc] init]);
    RetainPtr webView = editableWebViewWithPresentationIntents(@"<p id='first'>First</p><p id='second'>Second</p>", listener.get());

    selectFromElementToElement(webView.get(), @"first", @"second");
    [webView _setBlockPresentationIntent:[NSPresentationIntent unorderedListIntentWithIdentity:1 nestedInsideIntent:nil]];
    [webView waitForNextPresentationUpdate];
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"[...document.querySelectorAll('ul > li')].map(item => item.textContent).join()"], "First,Second");

    // A selection that starts in a list of the kind still puts the blocks after it in the list.
    webView = editableWebViewWithPresentationIntents(@"<ul><li id='item'>Item</li></ul><p id='after'>After</p>", listener.get());
    selectFromElementToElement(webView.get(), @"item", @"after");
    [webView _setBlockPresentationIntent:[NSPresentationIntent unorderedListIntentWithIdentity:1 nestedInsideIntent:nil]];
    [webView waitForNextPresentationUpdate];
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"[...document.querySelectorAll('ul > li')].map(item => item.textContent).join()"], "Item,After");
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"String(document.querySelectorAll('ul').length)"], "1");

    // Leaving a list works the same way.
    webView = editableWebViewWithPresentationIntents(@"<ul><li id='item'>Item</li></ul><p id='after'>After</p>", listener.get());
    selectFromElementToElement(webView.get(), @"item", @"after");
    [webView _setBlockPresentationIntent:[NSPresentationIntent headerIntentWithIdentity:1 level:2 nestedInsideIntent:nil]];
    [webView waitForNextPresentationUpdate];
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"String(!!document.querySelector('ul, li'))"], "false");
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"String([...document.body.children].every(element => element.tagName === 'H2'))"], "true");

    // And one that starts outside a list still takes the blocks after it out of their list.
    webView = editableWebViewWithPresentationIntents(@"<p id='before'>Before</p><ol><li id='item'>Item</li></ol>", listener.get());
    selectFromElementToElement(webView.get(), @"before", @"item");
    [webView _setBlockPresentationIntent:[NSPresentationIntent headerIntentWithIdentity:1 level:2 nestedInsideIntent:nil]];
    [webView waitForNextPresentationUpdate];
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"String(!!document.querySelector('ol, li'))"], "false");
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"String([...document.body.children].every(element => element.tagName === 'H2'))"], "true");
}

// Only the <code> of a code block is code block markup; inline code in the other selected blocks stays.
TEST(PresentationIntents, SetBlockPresentationIntentLeavesCodeBlockKeepingInlineCode)
{
    RetainPtr listener = adoptNS([[PresentationIntentFontAttributesListener alloc] init]);
    RetainPtr webView = editableWebViewWithPresentationIntents(@"<pre><code id='code'>Code</code></pre><p id='after'><code>inline</code></p>", listener.get());

    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(document.getElementById('code').firstChild, 0, document.querySelector('#after code').firstChild, 6)"];
    [webView _setBlockPresentationIntent:[NSPresentationIntent paragraphIntentWithIdentity:1 nestedInsideIntent:nil]];
    [webView waitForNextPresentationUpdate];
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"String(!!document.querySelector('pre'))"], "false");
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"[...document.querySelectorAll('code')].map(code => code.textContent).join()"], "inline");
}

TEST(PresentationIntents, SetBlockPresentationIntentRedoes)
{
    RetainPtr listener = adoptNS([[PresentationIntentFontAttributesListener alloc] init]);
    RetainPtr webView = editableWebViewWithPresentationIntents(@"<ul><li id='item'>Item</li></ul>", listener.get());

    placeCaretInElement(webView.get(), @"item");
    [webView _setBlockPresentationIntent:[NSPresentationIntent headerIntentWithIdentity:1 level:2 nestedInsideIntent:nil]];
    EXPECT_WK_STREQ(blockStructure(webView.get()).get(), "H2");
    [webView _executeEditCommand:@"Undo" argument:nil completion:nil];
    EXPECT_WK_STREQ(blockStructure(webView.get()).get(), "UL(LI)");
    [webView _executeEditCommand:@"Redo" argument:nil completion:nil];
    EXPECT_WK_STREQ(blockStructure(webView.get()).get(), "H2");

    webView = editableWebViewWithPresentationIntents(@"<p id='line'>Line</p>", listener.get());
    placeCaretInElement(webView.get(), @"line");
    [webView _setBlockPresentationIntent:[NSPresentationIntent codeBlockIntentWithIdentity:1 languageHint:@"swift" nestedInsideIntent:nil]];
    EXPECT_WK_STREQ(blockStructure(webView.get()).get(), "PRE(CODE)");
    [webView _executeEditCommand:@"Undo" argument:nil completion:nil];
    EXPECT_WK_STREQ(blockStructure(webView.get()).get(), "P");
    [webView _executeEditCommand:@"Redo" argument:nil completion:nil];
    EXPECT_WK_STREQ(blockStructure(webView.get()).get(), "PRE(CODE)");
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"document.querySelector('pre > code').className"], "language-swift");
}

// Content that is not editable keeps its look.
TEST(PresentationIntents, SetBlockPresentationIntentKeepsNonEditableContent)
{
    RetainPtr listener = adoptNS([[PresentationIntentFontAttributesListener alloc] init]);
    RetainPtr webView = editableWebViewWithPresentationIntents(@"<ul><li id='item' style='font-size: 10px'>Item <span contenteditable='false' style='font-size: 30px'><font face='Courier'>fixed</font></span></li></ul>", listener.get());

    placeCaretInElement(webView.get(), @"item");
    [webView _setBlockPresentationIntent:[NSPresentationIntent unorderedListIntentWithIdentity:1 nestedInsideIntent:nil]];
    [webView waitForNextPresentationUpdate];
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"document.getElementById('item').style.fontSize"], "");
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"document.querySelector('[contenteditable=false]').style.fontSize"], "30px");
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"document.querySelector('font')?.getAttribute('face')"], "Courier");
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
