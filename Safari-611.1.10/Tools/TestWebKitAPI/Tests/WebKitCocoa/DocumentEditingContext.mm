/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY) && HAVE(UI_WK_DOCUMENT_CONTEXT)

#import "PlatformUtilities.h"
#import "TestCocoa.h"
#import "TestWKWebView.h"
#import "UIKitSPI.h"
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/_WKTextInputContext.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

#define EXPECT_NSSTRING_EQ(expected, actual) \
    EXPECT_TRUE([actual isKindOfClass:[NSString class]]); \
    EXPECT_WK_STREQ(expected, (NSString *)actual);

#define EXPECT_ATTRIBUTED_STRING_EQ(expected, actual) \
    EXPECT_TRUE([actual isKindOfClass:[NSAttributedString class]]); \
    EXPECT_WK_STREQ(expected, [(NSAttributedString *)actual string]);

static UIWKDocumentRequest *makeRequest(UIWKDocumentRequestFlags flags, UITextGranularity granularity, NSInteger granularityCount, CGRect documentRect = CGRectZero, id <NSCopying> inputElementIdentifier = nil)
{
    auto request = adoptNS([[UIWKDocumentRequest alloc] init]);
    [request setFlags:flags];
    [request setSurroundingGranularity:granularity];
    [request setGranularityCount:granularityCount];
    [request setDocumentRect:documentRect];
    [request setInputElementIdentifier:inputElementIdentifier];
    return request.autorelease();
}

@interface UIWKDocumentContext (TestRunner)
@property (nonatomic, readonly) NSArray<NSValue *> *markedTextRects;
@property (nonatomic, readonly) NSArray<NSValue *> *textRects;
@end

@implementation UIWKDocumentContext (TestRunner)

- (NSArray<NSValue *> *)markedTextRects
{
    // This should ideally be equivalent to [self characterRectsForCharacterRange:self.markedTextRange]. However, the implementation
    // of -characterRectsForCharacterRange: in UIKit doesn't guarantee any order to the returned character rects. See: <rdar://57338528>.
    NSRange range = self.markedTextRange;
    NSMutableArray *rects = [NSMutableArray arrayWithCapacity:range.length];
    for (auto location = range.location; location < range.location + range.length; ++location)
        [rects addObject:[self characterRectsForCharacterRange:NSMakeRange(location, 1)].firstObject];
    return rects;
}

// FIXME: These workarounds can be removed once <rdar://problem/57338528> is fixed.
- (NSUInteger)contextBeforeLength
{
    if ([self.contextBefore isKindOfClass:NSString.class])
        return [(NSString *)self.contextBefore length];
    if ([self.contextBefore isKindOfClass:NSAttributedString.class])
        return [(NSAttributedString *)self.contextBefore length];
    return 0;
}

- (NSUInteger)markedTextLength
{
    if ([self.markedText isKindOfClass:NSString.class])
        return [(NSString *)self.markedText length];
    if ([self.markedText isKindOfClass:NSAttributedString.class])
        return [(NSAttributedString *)self.markedText length];
    return 0;
}

- (NSRange)markedTextRange
{
    return NSMakeRange(self.contextBeforeLength - self.selectedRangeInMarkedText.location, self.markedTextLength);
}

- (NSArray<NSValue *> *)textRects
{
    Vector<std::pair<NSRange, CGRect>> rangesAndRects;
    [self enumerateLayoutRects:[&](NSRange characterRange, CGRect layoutRect, BOOL *) {
        rangesAndRects.append(std::make_pair(characterRange, layoutRect));
    }];

    std::sort(rangesAndRects.begin(), rangesAndRects.end(), [](auto& a, auto& b) {
        return a.first.location < b.first.location;
    });

    auto result = adoptNS([[NSMutableArray alloc] initWithCapacity:rangesAndRects.size()]);
    for (auto& rangeAndRect : rangesAndRects)
        [result addObject:[NSValue valueWithCGRect:rangeAndRect.second]];
    return result.autorelease();
}

@end

@implementation TestWKWebView (SynchronousDocumentContext)

- (UIWKDocumentContext *)synchronouslyRequestDocumentContext:(UIWKDocumentRequest *)request
{
    __block bool finished = false;
    __block RetainPtr<UIWKDocumentContext> result;
    [self _requestDocumentContext:request completionHandler:^(UIWKDocumentContext *context) {
        result = context;
        finished = true;
    }];
    TestWebKitAPI::Util::run(&finished);
    return result.autorelease();
}

- (void)synchronouslyAdjustSelectionWithDelta:(NSRange)range
{
    __block bool finished = false;
    [self _adjustSelectionWithDelta:range completionHandler:^() {
        finished = true;
    }];
    TestWebKitAPI::Util::run(&finished);
}

- (NSArray<_WKTextInputContext *> *)synchronouslyRequestTextInputContextsInRect:(CGRect)rect
{
    __block bool finished = false;
    __block RetainPtr<NSArray<_WKTextInputContext *>> result;
    [self _requestTextInputContextsInRect:rect completionHandler:^(NSArray<_WKTextInputContext *> *contexts) {
        result = contexts;
        finished = true;
    }];
    TestWebKitAPI::Util::run(&finished);
    return result.autorelease();
}

- (UITextPlaceholder *)synchronouslyInsertTextPlaceholderWithSize:(CGSize)size
{
    __block bool finished = false;
    __block RetainPtr<UITextPlaceholder> result;
    [self.textInputContentView insertTextPlaceholderWithSize:size completionHandler:^(UITextPlaceholder *placeholder) {
        result = placeholder;
        finished = true;
    }];
    TestWebKitAPI::Util::run(&finished);
    return result.autorelease();
}

- (void)synchronouslyRemoveTextPlaceholder:(UITextPlaceholder *)placeholder willInsertText:(BOOL)willInsertText
{
    __block bool finished = false;
    [self.textInputContentView removeTextPlaceholder:placeholder willInsertText:willInsertText completionHandler:^(void) {
        finished = true;
    }];
    TestWebKitAPI::Util::run(&finished);
}

@end

static NSString *applyStyle(NSString *htmlString)
{
    return [@"<style>body { margin: 0; } </style><meta name='viewport' content='initial-scale=1'>" stringByAppendingString:htmlString];
}

constexpr unsigned glyphWidth { 25 }; // pixels

static NSString *applyAhemStyle(NSString *htmlString)
{
    return [NSString stringWithFormat:@"<style>@font-face { font-family: Ahem; src: url(Ahem.ttf); } body { margin: 0; } * { font: %upx/1 Ahem; -webkit-text-size-adjust: 100%%; }</style><meta name='viewport' content='width=980, initial-scale=1.0'>%@", glyphWidth, htmlString];
}

TEST(DocumentEditingContext, Simple)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    UIWKDocumentContext *context;

    [webView synchronouslyLoadHTMLString:applyStyle(@"<span id='the'>The</span> quick brown fox <span id='jumps'>jumps</span> over the lazy <span id='dog'>dog.</span>")];
    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(jumps, 0, jumps, 1)"];

    context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText, UITextGranularityWord, 1)];
    EXPECT_NOT_NULL(context);
    EXPECT_NSSTRING_EQ("fox ", context.contextBefore);
    EXPECT_NSSTRING_EQ("jumps", context.selectedText);
    EXPECT_NSSTRING_EQ(" over", context.contextAfter);

    context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText, UITextGranularityWord, 2)];
    EXPECT_NOT_NULL(context);
    EXPECT_NSSTRING_EQ("brown fox ", context.contextBefore);
    EXPECT_NSSTRING_EQ("jumps", context.selectedText);
    EXPECT_NSSTRING_EQ(" over the", context.contextAfter);

    context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText, UITextGranularityParagraph, 1)];
    EXPECT_NOT_NULL(context);
    EXPECT_NSSTRING_EQ("The quick brown fox ", context.contextBefore);
    EXPECT_NSSTRING_EQ("jumps", context.selectedText);
    EXPECT_NSSTRING_EQ(" over the lazy dog.", context.contextAfter);

    // Attributed strings.
    // FIXME: Currently we don't test the attributes... because we don't set any.
    context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestAttributed, UITextGranularityWord, 1)];
    EXPECT_NOT_NULL(context);
    EXPECT_ATTRIBUTED_STRING_EQ("fox ", context.contextBefore);
    EXPECT_ATTRIBUTED_STRING_EQ("jumps", context.selectedText);
    EXPECT_ATTRIBUTED_STRING_EQ(" over", context.contextAfter);

    // Extremities of the document.
    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(the, 0, the, 1)"];
    context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText, UITextGranularityWord, 1)];
    EXPECT_NOT_NULL(context);
    EXPECT_NULL(context.contextBefore);
    EXPECT_NSSTRING_EQ("The", context.selectedText);
    EXPECT_NSSTRING_EQ(" quick", context.contextAfter);

    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(dog, 0, dog, 1)"];
    context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText, UITextGranularitySentence, 1)];
    EXPECT_NOT_NULL(context);
    EXPECT_NSSTRING_EQ("The quick brown fox jumps over the lazy ", context.contextBefore);
    EXPECT_NSSTRING_EQ("dog.", context.selectedText);
    EXPECT_NULL(context.contextAfter);

    // Spatial requests.
    context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText | UIWKDocumentRequestSpatial, UITextGranularityWord, 2, CGRectMake(0, 0, 10, 10))];
    EXPECT_NOT_NULL(context);
    EXPECT_NULL(context.contextBefore);
    EXPECT_NULL(context.selectedText);
    EXPECT_NSSTRING_EQ("The quick", context.contextAfter);

    context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText | UIWKDocumentRequestSpatial, UITextGranularityWord, 1, CGRectMake(0, 0, 800, 600))];
    EXPECT_NOT_NULL(context);
    EXPECT_NSSTRING_EQ("The quick brown fox jumps over the lazy ", context.contextBefore);
    EXPECT_NSSTRING_EQ("dog.", context.selectedText);
    EXPECT_NULL(context.contextAfter);

    [webView stringByEvaluatingJavaScript:@"getSelection().removeAllRanges()"];
    context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText | UIWKDocumentRequestSpatial, UITextGranularityWord, 1, CGRectMake(0, 0, 800, 600))];
    EXPECT_NOT_NULL(context);
    EXPECT_NSSTRING_EQ("The quick brown fox ", context.contextBefore);
    EXPECT_NULL(context.selectedText);
    EXPECT_NSSTRING_EQ("jumps over the lazy dog.", context.contextAfter);

    // Selection adjustment.
    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(jumps, 0, jumps, 1)"];
    [webView synchronouslyAdjustSelectionWithDelta:NSMakeRange(6, -1)];
    context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText, UITextGranularityWord, 1)];
    EXPECT_NOT_NULL(context);
    EXPECT_NSSTRING_EQ("over", context.selectedText);

    [webView synchronouslyAdjustSelectionWithDelta:NSMakeRange(-6, 1)];
    context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText, UITextGranularityWord, 1)];
    EXPECT_NOT_NULL(context);
    EXPECT_NSSTRING_EQ("jumps", context.selectedText);

    // Text rects.
    [webView synchronouslyLoadHTMLString:applyAhemStyle(@"<span id='four'>MMMM</span> MMM MM M")];
    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(four, 0, four, 1)"];
    context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText | UIWKDocumentRequestRects, UITextGranularityWord, 0)];
    EXPECT_NOT_NULL(context);
    EXPECT_NULL(context.contextBefore);
    EXPECT_NSSTRING_EQ("MMMM", context.selectedText);
    EXPECT_NULL(context.contextAfter);

    NSArray<NSValue *> *rects = [[context characterRectsForCharacterRange:NSMakeRange(0, 4)] sortedArrayUsingComparator:^(NSValue *a, NSValue *b) {
        return [@(a.CGRectValue.origin.x) compare:@(b.CGRectValue.origin.x)];
    }];
    EXPECT_EQ(4UL, rects.count);
#if PLATFORM(MACCATALYST)
    EXPECT_EQ(CGRectMake(0, -1, glyphWidth, glyphWidth + 1), rects[0].CGRectValue);
    EXPECT_EQ(CGRectMake(glyphWidth, -1, glyphWidth, glyphWidth + 1), rects[1].CGRectValue);
    EXPECT_EQ(CGRectMake(2 * glyphWidth, -1, glyphWidth, glyphWidth + 1), rects[2].CGRectValue);
    EXPECT_EQ(CGRectMake(3 * glyphWidth, -1, glyphWidth, glyphWidth + 1), rects[3].CGRectValue);
#else
    EXPECT_EQ(CGRectMake(0, 0, glyphWidth, glyphWidth), rects[0].CGRectValue);
    EXPECT_EQ(CGRectMake(glyphWidth, 0, glyphWidth, glyphWidth), rects[1].CGRectValue);
    EXPECT_EQ(CGRectMake(2 * glyphWidth, 0, glyphWidth, glyphWidth), rects[2].CGRectValue);
    EXPECT_EQ(CGRectMake(3 * glyphWidth, 0, glyphWidth, glyphWidth), rects[3].CGRectValue);
#endif
    rects = [context characterRectsForCharacterRange:NSMakeRange(5, 1)];
    EXPECT_EQ(0UL, rects.count);

    context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText | UIWKDocumentRequestRects, UITextGranularityWord, 1)];
    EXPECT_NSSTRING_EQ(" MMM", context.contextAfter);
    rects = [context characterRectsForCharacterRange:NSMakeRange(0, 1)];
    EXPECT_EQ(1UL, rects.count);
#if PLATFORM(MACCATALYST)
    EXPECT_EQ(CGRectMake(0, -1, glyphWidth, glyphWidth + 1), rects.firstObject.CGRectValue);
#else
    EXPECT_EQ(CGRectMake(0, 0, glyphWidth, glyphWidth), rects.firstObject.CGRectValue);
#endif
    rects = [context characterRectsForCharacterRange:NSMakeRange(6, 1)];
    EXPECT_EQ(1UL, rects.count);
#if PLATFORM(MACCATALYST)
    EXPECT_EQ(CGRectMake(6 * glyphWidth, -1, glyphWidth, glyphWidth + 1), rects.firstObject.CGRectValue);
#else
    EXPECT_EQ(CGRectMake(6 * glyphWidth, 0, glyphWidth, glyphWidth), rects.firstObject.CGRectValue);
#endif

    // Text Input Context
    [webView synchronouslyLoadHTMLString:applyStyle(@"<input type='text' style='width: 50px; height: 50px;' value='hello, world'>")];
    NSArray<_WKTextInputContext *> *textInputContexts = [webView synchronouslyRequestTextInputContextsInRect:[webView frame]];
    EXPECT_EQ(1UL, textInputContexts.count);
    EXPECT_EQ(CGRectMake(0, 0, 50, 50), textInputContexts[0].boundingRect);

    context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText, UITextGranularityWord, 0, CGRectZero, textInputContexts[0])];
    EXPECT_NSSTRING_EQ("hello,", context.contextBefore);
    EXPECT_NULL(context.selectedText);
    EXPECT_NSSTRING_EQ(" world", context.contextAfter);
}

TEST(DocumentEditingContext, RequestMarkedText)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    auto *contentView = [webView textInputContentView];

    [webView synchronouslyLoadHTMLString:@"<body style='-webkit-text-size-adjust: none;' contenteditable>"];
    [webView evaluateJavaScript:@"document.body.focus()" completionHandler:nil];
    [webView _synchronouslyExecuteEditCommand:@"InsertText" argument:@"Hello world"];

    [contentView selectWordBackward];
    [contentView setMarkedText:@"world" selectedRange:NSMakeRange(0, 5)];
    [webView waitForNextPresentationUpdate];
    {
        UIWKDocumentContext *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText | UIWKDocumentRequestMarkedTextRects, UITextGranularityCharacter, 0)];
        EXPECT_NULL(context.contextBefore);
        EXPECT_NULL(context.contextAfter);
        EXPECT_NSSTRING_EQ("world", context.selectedText);
        EXPECT_NSSTRING_EQ("world", context.markedText);
        EXPECT_EQ(0U, context.markedTextRange.location);
        EXPECT_EQ(5U, context.markedTextRange.length);

        NSArray<NSValue *> *rectValues = context.markedTextRects;
        EXPECT_EQ(5U, rectValues.count);
        if (rectValues.count >= 5) {
            EXPECT_EQ(CGRectMake(47, 8, 13, 19), [rectValues[0] CGRectValue]);
            EXPECT_EQ(CGRectMake(59, 8, 9, 19), [rectValues[1] CGRectValue]);
            EXPECT_EQ(CGRectMake(67, 8, 6, 19), [rectValues[2] CGRectValue]);
            EXPECT_EQ(CGRectMake(72, 8, 5, 19), [rectValues[3] CGRectValue]);
            EXPECT_EQ(CGRectMake(76, 8, 9, 19), [rectValues[4] CGRectValue]);
        }
    }
    [contentView unmarkText];
    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(document.body.childNodes[0], 0, document.body.childNodes[0], 5)"];
    [contentView setMarkedText:@"Hello" selectedRange:NSMakeRange(0, 5)];
    [webView waitForNextPresentationUpdate];
    {
        UIWKDocumentContext *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText | UIWKDocumentRequestMarkedTextRects, UITextGranularityParagraph, 1)];
        EXPECT_NULL(context.contextBefore);
        EXPECT_NSSTRING_EQ(" world", context.contextAfter);
        EXPECT_NSSTRING_EQ("Hello", context.selectedText);
        EXPECT_NSSTRING_EQ("Hello", context.markedText);
        EXPECT_EQ(0U, context.markedTextRange.location);
        EXPECT_EQ(5U, context.markedTextRange.length);

        NSArray<NSValue *> *rectValues = context.markedTextRects;
        EXPECT_EQ(5U, rectValues.count);
        if (rectValues.count >= 5) {
            EXPECT_EQ(CGRectMake(8, 8, 12, 19), [rectValues[0] CGRectValue]);
            EXPECT_EQ(CGRectMake(19, 8, 8, 19), [rectValues[1] CGRectValue]);
            EXPECT_EQ(CGRectMake(26, 8, 6, 19), [rectValues[2] CGRectValue]);
            EXPECT_EQ(CGRectMake(31, 8, 5, 19), [rectValues[3] CGRectValue]);
            EXPECT_EQ(CGRectMake(35, 8, 9, 19), [rectValues[4] CGRectValue]);
        }
    }
    [contentView unmarkText];
    [webView selectAll:nil];
    [contentView setMarkedText:@"foo" selectedRange:NSMakeRange(0, 3)];
    [webView waitForNextPresentationUpdate];
    {
        UIWKDocumentContext *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText | UIWKDocumentRequestMarkedTextRects, UITextGranularitySentence, 1)];
        EXPECT_NULL(context.contextBefore);
        EXPECT_NULL(context.contextAfter);
        EXPECT_NSSTRING_EQ("foo", context.selectedText);
        EXPECT_NSSTRING_EQ("foo", context.markedText);
        EXPECT_EQ(0U, context.markedTextRange.location);
        EXPECT_EQ(3U, context.markedTextRange.length);

        NSArray<NSValue *> *rectValues = context.markedTextRects;
        EXPECT_EQ(3U, rectValues.count);
        if (rectValues.count >= 3) {
            EXPECT_EQ(CGRectMake(8, 8, 6, 19), [rectValues[0] CGRectValue]);
            EXPECT_EQ(CGRectMake(13, 8, 9, 19), [rectValues[1] CGRectValue]);
            EXPECT_EQ(CGRectMake(21, 8, 9, 19), [rectValues[2] CGRectValue]);
        }
    }
    [contentView unmarkText];
    [webView collapseToEnd];
    [contentView setMarkedText:@"bar" selectedRange:NSMakeRange(0, 3)];
    [webView waitForNextPresentationUpdate];
    {
        UIWKDocumentContext *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText | UIWKDocumentRequestMarkedTextRects, UITextGranularityWord, 1)];
        EXPECT_NSSTRING_EQ("foo", context.contextBefore);
        EXPECT_NULL(context.contextAfter);
        EXPECT_NSSTRING_EQ("bar", context.selectedText);
        EXPECT_NSSTRING_EQ("bar", context.markedText);
        EXPECT_EQ(3U, context.markedTextRange.location);
        EXPECT_EQ(3U, context.markedTextRange.length);

        NSArray<NSValue *> *rectValues = context.markedTextRects;
        EXPECT_EQ(3U, rectValues.count);
        if (rectValues.count >= 3) {
            EXPECT_EQ(CGRectMake(29, 8, 9, 19), [rectValues[0] CGRectValue]);
            EXPECT_EQ(CGRectMake(37, 8, 8, 19), [rectValues[1] CGRectValue]);
            EXPECT_EQ(CGRectMake(44, 8, 6, 19), [rectValues[2] CGRectValue]);
        }
    }
}

TEST(DocumentEditingContext, RequestMarkedTextRectsAndTextOnly)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:applyAhemStyle(@"<input />")];
    [webView stringByEvaluatingJavaScript:@"document.querySelector('input').focus()"];
    [webView _synchronouslyExecuteEditCommand:@"InsertText" argument:@"Hello world"];

    auto *contentView = [webView textInputContentView];
    [contentView selectWordBackward];
    [contentView setMarkedText:@"world" selectedRange:NSMakeRange(0, 5)];
    [webView collapseToEnd];

    auto request = adoptNS([[UIWKDocumentRequest alloc] init]);
    [request setFlags:UIWKDocumentRequestMarkedTextRects | UIWKDocumentRequestText];

    auto context = retainPtr([webView synchronouslyRequestDocumentContext:request.get()]);
    auto *rectValues = [context markedTextRects];
#if PLATFORM(MACCATALYST)
    EXPECT_EQ(CGRectMake(165, 8, 26, 26), [rectValues[0] CGRectValue]);
    EXPECT_EQ(CGRectMake(190, 8, 26, 26), [rectValues[1] CGRectValue]);
    EXPECT_EQ(CGRectMake(215, 8, 26, 26), [rectValues[2] CGRectValue]);
    EXPECT_EQ(CGRectMake(240, 8, 26, 26), [rectValues[3] CGRectValue]);
    EXPECT_EQ(CGRectMake(265, 8, 26, 26), [rectValues[4] CGRectValue]);
#else
    EXPECT_EQ(CGRectMake(165, 8, 26, 25), [rectValues[0] CGRectValue]);
    EXPECT_EQ(CGRectMake(190, 8, 26, 25), [rectValues[1] CGRectValue]);
    EXPECT_EQ(CGRectMake(215, 8, 26, 25), [rectValues[2] CGRectValue]);
    EXPECT_EQ(CGRectMake(240, 8, 26, 25), [rectValues[3] CGRectValue]);
    EXPECT_EQ(CGRectMake(265, 8, 26, 25), [rectValues[4] CGRectValue]);
#endif
}

TEST(DocumentEditingContext, SpatialRequestInTextField)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    [webView synchronouslyLoadHTMLString:@"<span style='-webkit-text-size-adjust: none;'>Hello<input type='text' value='foo bar' />world</span>"];
    [webView stringByEvaluatingJavaScript:@"document.querySelector('input').focus()"];

    auto request = retainPtr(makeRequest(UIWKDocumentRequestRects | UIWKDocumentRequestText | UIWKDocumentRequestSpatial, UITextGranularityCharacter, 0, [webView textInputContentView].bounds));
    auto context = retainPtr([webView synchronouslyRequestDocumentContext:request.get()]);
    auto *textRects = [context textRects];
    EXPECT_EQ(10U, textRects.count);
    if (textRects.count >= 10) {
        EXPECT_EQ(CGRectMake(8, 9, 12, 19), textRects[0].CGRectValue);
        EXPECT_EQ(CGRectMake(19, 9, 8, 19), textRects[1].CGRectValue);
        EXPECT_EQ(CGRectMake(26, 9, 6, 19), textRects[2].CGRectValue);
        EXPECT_EQ(CGRectMake(31, 9, 5, 19), textRects[3].CGRectValue);
        EXPECT_EQ(CGRectMake(35, 9, 9, 19), textRects[4].CGRectValue);
        EXPECT_EQ(CGRectMake(184, 9, 13, 19), textRects[5].CGRectValue);
        EXPECT_EQ(CGRectMake(196, 9, 9, 19), textRects[6].CGRectValue);
        EXPECT_EQ(CGRectMake(204, 9, 6, 19), textRects[7].CGRectValue);
        EXPECT_EQ(CGRectMake(209, 9, 6, 19), textRects[8].CGRectValue);
        EXPECT_EQ(CGRectMake(214, 9, 9, 19), textRects[9].CGRectValue);
    }
}

static CGRect CGRectFromJSONEncodedDOMRectJSValue(id jsValue)
{
    if (![jsValue isKindOfClass:NSDictionary.class])
        return CGRectNull;
    NSDictionary *domRect = jsValue;
    return CGRectMake([domRect[@"left"] floatValue], [domRect[@"top"] floatValue], [domRect[@"width"] floatValue], [domRect[@"height"] floatValue]);
}

TEST(DocumentEditingContext, SpatialRequest_RectEncompassingInput)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 980, 600)]);

    // Use "padding: 0" as the default user-agent stylesheet can effect text wrapping.
    [webView synchronouslyLoadHTMLString:applyAhemStyle(@"before <textarea id='test' style='width: 26em; margin: 200px 0 0 200px; padding: 0;'>The quick brown fox jumps over the lazy dog.</textarea> after")]; // Word wraps "over" onto next line

    NSArray<_WKTextInputContext *> *textInputContexts = [webView synchronouslyRequestTextInputContextsInRect:[webView frame]];
    EXPECT_EQ(1UL, textInputContexts.count);

    UIWKDocumentContext *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText | UIWKDocumentRequestSpatial, UITextGranularityWord, 200, [webView frame], textInputContexts[0])];
    EXPECT_NSSTRING_EQ("The quick brown fox ", context.contextBefore);
    EXPECT_NULL(context.selectedText);
    EXPECT_NSSTRING_EQ("jumps over the lazy dog.", context.contextAfter);
}

TEST(DocumentEditingContext, SpatialRequest_RectBeforeInput)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 980, 600)]);

    // Use "padding: 0" as the default user-agent stylesheet can effect text wrapping.
    [webView synchronouslyLoadHTMLString:applyAhemStyle(@"before <textarea id='test' style='width: 26em; margin: 200px 0 0 200px; padding: 0;'>The quick brown fox jumps over the lazy dog.</textarea> after")]; // Word wraps "over" onto next line

    NSArray<_WKTextInputContext *> *textInputContexts = [webView synchronouslyRequestTextInputContextsInRect:[webView frame]];
    EXPECT_EQ(1UL, textInputContexts.count);

    auto documentRect = CGRectFromJSONEncodedDOMRectJSValue([webView objectByEvaluatingJavaScript:@"test.getBoundingClientRect().toJSON()"]);
    documentRect.origin.x -= glyphWidth * 2;
    documentRect.origin.y -= glyphWidth * 2;
    documentRect.size.width = glyphWidth;
    documentRect.size.height = glyphWidth;

    UIWKDocumentContext *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText | UIWKDocumentRequestSpatial, UITextGranularityWord, 200, documentRect, textInputContexts[0])];
    EXPECT_NULL(context.contextBefore);
    EXPECT_NULL(context.selectedText);
    EXPECT_NSSTRING_EQ("The quick brown fox jumps over the lazy dog.", context.contextAfter);
}

TEST(DocumentEditingContext, SpatialRequest_RectInsideInput)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 980, 600)]);

    // Use "padding: 0" as the default user-agent stylesheet can effect text wrapping.
    [webView synchronouslyLoadHTMLString:applyAhemStyle(@"before <textarea id='test' style='width: 26em; margin: 200px 0 0 200px; padding: 0;'>The quick brown fox jumps over the lazy dog.</textarea> after")]; // Word wraps "over" onto next line

    NSArray<_WKTextInputContext *> *textInputContexts = [webView synchronouslyRequestTextInputContextsInRect:[webView frame]];
    EXPECT_EQ(1UL, textInputContexts.count);

    auto documentRect = CGRectFromJSONEncodedDOMRectJSValue([webView objectByEvaluatingJavaScript:@"test.getBoundingClientRect().toJSON()"]);
    documentRect.origin.x += (documentRect.size.width / 2) - (glyphWidth * 2);
    documentRect.origin.y += (documentRect.size.height / 2) - (glyphWidth * 2);
    documentRect.size.width = glyphWidth;
    documentRect.size.height = glyphWidth;

    UIWKDocumentContext *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText | UIWKDocumentRequestSpatial, UITextGranularityWord, 200, documentRect, textInputContexts[0])];
    EXPECT_NSSTRING_EQ("The quick b", context.contextBefore);
    EXPECT_NULL(context.selectedText);
    EXPECT_NSSTRING_EQ("rown fox jumps over the lazy dog.", context.contextAfter);
}

TEST(DocumentEditingContext, SpatialRequest_RectAfterInput)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 980, 600)]);

    // Use "padding: 0" as the default user-agent stylesheet can effect text wrapping.
    [webView synchronouslyLoadHTMLString:applyAhemStyle(@"before <textarea id='test' style='width: 26em; margin: 200px 0 0 200px; padding: 0;'>The quick brown fox jumps over the lazy dog.</textarea> after")]; // Word wraps "over" onto next line

    NSArray<_WKTextInputContext *> *textInputContexts = [webView synchronouslyRequestTextInputContextsInRect:[webView frame]];
    EXPECT_EQ(1UL, textInputContexts.count);

    auto documentRect = CGRectFromJSONEncodedDOMRectJSValue([webView objectByEvaluatingJavaScript:@"test.getBoundingClientRect().toJSON()"]);
    documentRect.origin.x += documentRect.size.width + glyphWidth;
    documentRect.origin.y += documentRect.size.height + glyphWidth;
    documentRect.size.width = glyphWidth;
    documentRect.size.height = glyphWidth;

    UIWKDocumentContext *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText | UIWKDocumentRequestSpatial, UITextGranularityWord, 200, documentRect, textInputContexts[0])];
    EXPECT_NSSTRING_EQ("The quick brown fox jumps over the lazy dog.", context.contextBefore);
    EXPECT_NULL(context.selectedText);
    EXPECT_NULL(context.contextAfter);
}

TEST(DocumentEditingContext, SpatialAndCurrentSelectionRequest_RectBeforeRangeSelection)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 980, 600)]);
    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:applyAhemStyle(@"<span id='spatialBox'>The</span> quick brown fox <span id='jumps'>jumps</span> over the dog.")];
    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(jumps, 0, jumps, 1)"];

    // Hit testing below the last line is treated as if the line was hit. So, use height of 1
    // to ensure we aren't even close to the line height.
    auto spatialBoxRect = CGRectFromJSONEncodedDOMRectJSValue([webView objectByEvaluatingJavaScript:@"spatialBox.getBoundingClientRect().toJSON()"]);
    spatialBoxRect.size.height = 1;
    EXPECT_EQ(CGRectMake(0, 0, 3 * glyphWidth, 1), spatialBoxRect);

    UIWKDocumentContext *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText | UIWKDocumentRequestSpatialAndCurrentSelection, UITextGranularityWord, 2, spatialBoxRect)];
    EXPECT_NOT_NULL(context);
    EXPECT_NSSTRING_EQ("The quick brown fox ", context.contextBefore);
    EXPECT_NSSTRING_EQ("jumps", context.selectedText);
    EXPECT_NSSTRING_EQ(" over the", context.contextAfter);
}

TEST(DocumentEditingContext, SpatialAndCurrentSelectionRequest_RectAfterRangeSelection)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 980, 600)]);
    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:applyAhemStyle(@"The quick brown fox <span id='jumps'>jumps</span> over the dog.<span id='spatialBox'></span>")];
    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(jumps, 0, jumps, 1)"];

    // Hit testing below the last line is treated as if the line was hit. So, use height of 1
    // to ensure we aren't even close to the line height.
    auto spatialBoxRect = CGRectFromJSONEncodedDOMRectJSValue([webView objectByEvaluatingJavaScript:@"spatialBox.getBoundingClientRect().toJSON()"]);
    spatialBoxRect.size.height = 1;
    EXPECT_EQ(CGRectMake(39 * glyphWidth, 0, 0, 1), spatialBoxRect);

    UIWKDocumentContext *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText | UIWKDocumentRequestSpatialAndCurrentSelection, UITextGranularityWord, 2, spatialBoxRect)];
    EXPECT_NOT_NULL(context);
    EXPECT_NSSTRING_EQ("brown fox ", context.contextBefore);
    EXPECT_NSSTRING_EQ("jumps", context.selectedText);
    EXPECT_NSSTRING_EQ(" over the dog.", context.contextAfter);
}

TEST(DocumentEditingContext, SpatialAndCurrentSelectionRequest_RectAroundRangeSelection)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 980, 600)]);
    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:applyAhemStyle(@"The quick brown <span id='spatialBox'>fox <span id='jumps'>jumps</span> </span>over the dog.")];
    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(jumps, 0, jumps, 1)"];

    // Hit testing below the last line is treated as if the line was hit. So, use height of 1
    // to ensure we aren't even close to the line height.
    auto spatialBoxRect = CGRectFromJSONEncodedDOMRectJSValue([webView objectByEvaluatingJavaScript:@"spatialBox.getBoundingClientRect().toJSON()"]);
    spatialBoxRect.size.height = 1;
    EXPECT_EQ(CGRectMake(16 * glyphWidth, 0, 10 * glyphWidth, 1), spatialBoxRect);

    UIWKDocumentContext *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText | UIWKDocumentRequestSpatialAndCurrentSelection, UITextGranularityWord, 2, spatialBoxRect)];
    EXPECT_NOT_NULL(context);
    EXPECT_NSSTRING_EQ("quick brown fox ", context.contextBefore);
    EXPECT_NSSTRING_EQ("jumps", context.selectedText);
    EXPECT_NSSTRING_EQ(" over the", context.contextAfter);
}

TEST(DocumentEditingContext, SpatialAndCurrentSelectionRequest_RectBeforeCaretSelection)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 980, 600)]);
    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:applyAhemStyle(@"<body contenteditable='true'><span id='spatialBox'>The</span> quick brown fox <span id='jumps'>jumps</span> over the dog.</body>")];
    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(jumps, 0, jumps, 0)"];

    // Hit testing below the last line is treated as if the line was hit. So, use height of 1
    // to ensure we aren't even close to the line height.
    auto spatialBoxRect = CGRectFromJSONEncodedDOMRectJSValue([webView objectByEvaluatingJavaScript:@"spatialBox.getBoundingClientRect().toJSON()"]);
    spatialBoxRect.size.height = 1;
    EXPECT_EQ(CGRectMake(0, 0, 3 * glyphWidth, 1), spatialBoxRect);

    UIWKDocumentContext *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText | UIWKDocumentRequestSpatialAndCurrentSelection, UITextGranularityWord, 2, spatialBoxRect)];
    EXPECT_NOT_NULL(context);
    EXPECT_NSSTRING_EQ("The quick brown fox ", context.contextBefore);
    EXPECT_NULL(context.selectedText);
    EXPECT_NSSTRING_EQ("jumps over", context.contextAfter);
}

TEST(DocumentEditingContext, SpatialAndCurrentSelectionRequest_RectAfterCaretSelection)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 980, 600)]);
    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:applyAhemStyle(@"<body contenteditable='true'>The quick brown fox <span id='jumps'>jumps</span> over the dog.<span id='spatialBox'></span></body>")];
    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(jumps, 0, jumps, 0)"];

    // Hit testing below the last line is treated as if the line was hit. So, use height of 1
    // to ensure we aren't even close to the line height.
    auto spatialBoxRect = CGRectFromJSONEncodedDOMRectJSValue([webView objectByEvaluatingJavaScript:@"spatialBox.getBoundingClientRect().toJSON()"]);
    spatialBoxRect.size.height = 1;
    EXPECT_EQ(CGRectMake(39 * glyphWidth, 0, 0, 1), spatialBoxRect);

    UIWKDocumentContext *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText | UIWKDocumentRequestSpatialAndCurrentSelection, UITextGranularityWord, 2, spatialBoxRect)];
    EXPECT_NOT_NULL(context);
    EXPECT_NSSTRING_EQ("brown fox ", context.contextBefore);
    EXPECT_NULL(context.selectedText);
    EXPECT_NSSTRING_EQ("jumps over the dog.", context.contextAfter);
}

TEST(DocumentEditingContext, SpatialAndCurrentSelectionRequest_RectAroundCaretSelection)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 980, 600)]);
    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:applyAhemStyle(@"<body contenteditable='true'>The quick brown <span id='spatialBox'>fox <span id='jumps'>jumps</span> </span>over the dog.</body>")];
    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(jumps, 0, jumps, 0)"];

    // Hit testing below the last line is treated as if the line was hit. So, use height of 1
    // to ensure we aren't even close to the line height.
    auto spatialBoxRect = CGRectFromJSONEncodedDOMRectJSValue([webView objectByEvaluatingJavaScript:@"spatialBox.getBoundingClientRect().toJSON()"]);
    spatialBoxRect.size.height = 1;
    EXPECT_EQ(CGRectMake(16 * glyphWidth, 0, 10 * glyphWidth, 1), spatialBoxRect);

    UIWKDocumentContext *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText | UIWKDocumentRequestSpatialAndCurrentSelection, UITextGranularityWord, 2, spatialBoxRect)];
    EXPECT_NOT_NULL(context);
    EXPECT_NSSTRING_EQ("quick brown fox ", context.contextBefore);
    EXPECT_NULL(context.selectedText);
    EXPECT_NSSTRING_EQ("jumps over the", context.contextAfter);
}

TEST(DocumentEditingContext, SpatialAndCurrentSelectionRequest_RectEncompassingInputWithSelection)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 980, 600)]);

    // Use "padding: 0" as the default user-agent stylesheet can effect text wrapping.
    [webView synchronouslyLoadHTMLString:applyAhemStyle(@"before <span contenteditable id='test' style='display: inline-block; width: 26em; margin: 200px 0 0 200px;'>The quick brown <span id='fox_jumps_over'>fox jumps over</span> the lazy dog.</span> after")]; // Word wraps "over" onto next line
    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(fox_jumps_over, 0, fox_jumps_over, 1)"];

    NSArray<_WKTextInputContext *> *textInputContexts = [webView synchronouslyRequestTextInputContextsInRect:[webView frame]];
    EXPECT_EQ(1UL, textInputContexts.count);

    UIWKDocumentContext *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText | UIWKDocumentRequestSpatialAndCurrentSelection, UITextGranularityWord, 200, [webView frame], textInputContexts[0])];
    EXPECT_NSSTRING_EQ("The quick brown ", context.contextBefore);
    EXPECT_NSSTRING_EQ("fox jumps over", context.selectedText);
    EXPECT_NSSTRING_EQ(" the lazy dog.", context.contextAfter);
}

TEST(DocumentEditingContext, SpatialAndCurrentSelectionRequest_RectBeforeInputWithSelection)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 980, 600)]);

    // Use "padding: 0" as the default user-agent stylesheet can effect text wrapping.
    [webView synchronouslyLoadHTMLString:applyAhemStyle(@"before <span contenteditable id='test' style='display: inline-block; width: 26em; margin: 200px 0 0 200px;'>The quick brown <span id='fox_jumps_over'>fox jumps over</span> the lazy dog.</span> after")]; // Word wraps "over" onto next line
    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(fox_jumps_over, 0, fox_jumps_over, 1)"];

    NSArray<_WKTextInputContext *> *textInputContexts = [webView synchronouslyRequestTextInputContextsInRect:[webView frame]];
    EXPECT_EQ(1UL, textInputContexts.count);

    auto documentRect = CGRectFromJSONEncodedDOMRectJSValue([webView objectByEvaluatingJavaScript:@"test.getBoundingClientRect().toJSON()"]);
    documentRect.origin.x -= glyphWidth * 2;
    documentRect.origin.y -= glyphWidth * 2;
    documentRect.size.width = glyphWidth;
    documentRect.size.height = glyphWidth;

    UIWKDocumentContext *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText | UIWKDocumentRequestSpatialAndCurrentSelection, UITextGranularityWord, 200, documentRect, textInputContexts[0])];
    EXPECT_NSSTRING_EQ("The quick brown ", context.contextBefore);
    EXPECT_NSSTRING_EQ("fox jumps over", context.selectedText);
    EXPECT_NSSTRING_EQ(" the lazy dog.", context.contextAfter);
}

TEST(DocumentEditingContext, SpatialAndCurrentSelectionRequest_RectBeforeSelectionInInput)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 980, 600)]);

    // Use "padding: 0" as the default user-agent stylesheet can effect text wrapping.
    [webView synchronouslyLoadHTMLString:applyAhemStyle(@"before <span contenteditable id='test' style='display: inline-block; width: 26em; margin: 200px 0 0 200px;'>The quick brown <span id='fox_jumps_over'>fox jumps over</span> the lazy dog.</span> after")]; // Word wraps "over" onto next line
    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(fox_jumps_over, 0, fox_jumps_over, 1)"];

    NSArray<_WKTextInputContext *> *textInputContexts = [webView synchronouslyRequestTextInputContextsInRect:[webView frame]];
    EXPECT_EQ(1UL, textInputContexts.count);

    auto selectionBoxRect = CGRectFromJSONEncodedDOMRectJSValue([webView objectByEvaluatingJavaScript:@"fox_jumps_over.getBoundingClientRect().toJSON()"]);

    auto documentRect = CGRectFromJSONEncodedDOMRectJSValue([webView objectByEvaluatingJavaScript:@"test.getBoundingClientRect().toJSON()"]);
    documentRect.origin.x = ((documentRect.origin.x + selectionBoxRect.origin.x) / 2) - (glyphWidth * 2);
    documentRect.origin.y = ((documentRect.origin.y + selectionBoxRect.origin.y) / 2) - (glyphWidth * 2);
    documentRect.size.width = glyphWidth;
    documentRect.size.height = glyphWidth;

    UIWKDocumentContext *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText | UIWKDocumentRequestSpatialAndCurrentSelection, UITextGranularityWord, 200, documentRect, textInputContexts[0])];
    EXPECT_NSSTRING_EQ("The quick brown ", context.contextBefore);
    EXPECT_NSSTRING_EQ("fox jumps over", context.selectedText);
    EXPECT_NSSTRING_EQ(" the lazy dog.", context.contextAfter);
}

TEST(DocumentEditingContext, SpatialAndCurrentSelectionRequest_RectAfterSelectionInInput)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 980, 600)]);

    // Use "padding: 0" as the default user-agent stylesheet can effect text wrapping.
    [webView synchronouslyLoadHTMLString:applyAhemStyle(@"before <span contenteditable id='test' style='display: inline-block; width: 26em; margin: 200px 0 0 200px;'>The quick brown <span id='fox_jumps_over'>fox jumps over</span> the lazy dog.</span> after")]; // Word wraps "over" onto next line
    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(fox_jumps_over, 0, fox_jumps_over, 1)"];

    NSArray<_WKTextInputContext *> *textInputContexts = [webView synchronouslyRequestTextInputContextsInRect:[webView frame]];
    EXPECT_EQ(1UL, textInputContexts.count);

    auto selectionBoxRect = CGRectFromJSONEncodedDOMRectJSValue([webView objectByEvaluatingJavaScript:@"fox_jumps_over.getBoundingClientRect().toJSON()"]);

    auto documentRect = CGRectFromJSONEncodedDOMRectJSValue([webView objectByEvaluatingJavaScript:@"test.getBoundingClientRect().toJSON()"]);
    documentRect.origin.x = ((documentRect.origin.x + documentRect.size.width + selectionBoxRect.origin.x + selectionBoxRect.size.width) / 2) + glyphWidth;
    documentRect.origin.y = ((documentRect.origin.y + documentRect.size.height + selectionBoxRect.origin.y + selectionBoxRect.size.height) / 2) + glyphWidth;
    documentRect.size.width = glyphWidth;
    documentRect.size.height = glyphWidth;

    UIWKDocumentContext *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText | UIWKDocumentRequestSpatialAndCurrentSelection, UITextGranularityWord, 200, documentRect, textInputContexts[0])];
    EXPECT_NSSTRING_EQ("The quick brown ", context.contextBefore);
    EXPECT_NSSTRING_EQ("fox jumps over", context.selectedText);
    EXPECT_NSSTRING_EQ(" the lazy dog.", context.contextAfter);
}

TEST(DocumentEditingContext, SpatialAndCurrentSelectionRequest_RectAfterInputWithSelection)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 980, 600)]);

    // Use "padding: 0" as the default user-agent stylesheet can effect text wrapping.
    [webView synchronouslyLoadHTMLString:applyAhemStyle(@"before <span contenteditable id='test' style='display: inline-block; width: 26em; margin: 200px 0 0 200px;'>The quick brown <span id='fox_jumps_over'>fox jumps over</span> the lazy dog.</span> after")]; // Word wraps "over" onto next line
    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(fox_jumps_over, 0, fox_jumps_over, 1)"];

    NSArray<_WKTextInputContext *> *textInputContexts = [webView synchronouslyRequestTextInputContextsInRect:[webView frame]];
    EXPECT_EQ(1UL, textInputContexts.count);

    auto documentRect = CGRectFromJSONEncodedDOMRectJSValue([webView objectByEvaluatingJavaScript:@"test.getBoundingClientRect().toJSON()"]);
    documentRect.origin.x += documentRect.size.width + (glyphWidth * 2);
    documentRect.origin.y += documentRect.size.height + (glyphWidth * 2);
    documentRect.size.width = glyphWidth;
    documentRect.size.height = glyphWidth;

    UIWKDocumentContext *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText | UIWKDocumentRequestSpatialAndCurrentSelection, UITextGranularityWord, 200, documentRect, textInputContexts[0])];
    EXPECT_NSSTRING_EQ("The quick brown ", context.contextBefore);
    EXPECT_NSSTRING_EQ("fox jumps over", context.selectedText);
    EXPECT_NSSTRING_EQ(" the lazy dog.", context.contextAfter);
}

TEST(DocumentEditingContext, RequestRectsInTextAreaAcrossWordWrappedLine)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    // Use "padding: 0" as the default user-agent stylesheet can effect text wrapping.
    [webView synchronouslyLoadHTMLString:applyAhemStyle(@"<textarea id='test' style='width: 26em; padding: 0'>The quick brown fox jumps over the lazy dog.</textarea>")]; // Word wraps "over" onto next line
    [webView stringByEvaluatingJavaScript:@"test.focus(); test.setSelectionRange(25, 25)"]; // Place caret after 's' in "jumps".

    auto *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText | UIWKDocumentRequestRects, UITextGranularityWord, 2)];
    EXPECT_NOT_NULL(context);
    EXPECT_NSSTRING_EQ("fox jumps", context.contextBefore);
    EXPECT_NSSTRING_EQ(" over the", context.contextAfter);
    auto *textRects = [context textRects];
    EXPECT_EQ(18U, textRects.count);

#if PLATFORM(MACCATALYST)
    const size_t yPos = 2;
    const size_t height = 26;
#else
    const size_t yPos = 3;
    const size_t height = 25;
#endif
    
    if (textRects.count >= 18) {
        CGFloat x = 401;
        EXPECT_EQ(CGRectMake(x + 0 * glyphWidth, yPos, 25, height), textRects[0].CGRectValue); // f
        EXPECT_EQ(CGRectMake(x + 1 * glyphWidth, yPos, 25, height), textRects[1].CGRectValue); // o
        EXPECT_EQ(CGRectMake(x + 2 * glyphWidth, yPos, 25, height), textRects[2].CGRectValue); // x
        EXPECT_EQ(CGRectMake(x + 3 * glyphWidth, yPos, 25, height), textRects[3].CGRectValue); //
        EXPECT_EQ(CGRectMake(x + 4 * glyphWidth, yPos, 25, height), textRects[4].CGRectValue); // j
        EXPECT_EQ(CGRectMake(x + 5 * glyphWidth, yPos, 25, height), textRects[5].CGRectValue); // u
        EXPECT_EQ(CGRectMake(x + 6 * glyphWidth, yPos, 25, height), textRects[6].CGRectValue); // m
        EXPECT_EQ(CGRectMake(x + 7 * glyphWidth, yPos, 25, height), textRects[7].CGRectValue); // p
        EXPECT_EQ(CGRectMake(x + 8 * glyphWidth, yPos, 25, height), textRects[8].CGRectValue); // s
        EXPECT_EQ(CGRectMake(x + 9 * glyphWidth, yPos, 23, height), textRects[9].CGRectValue); //

        x = 1;
        EXPECT_EQ(CGRectMake(x + 0 * glyphWidth, 25 + yPos, 25, height), textRects[10].CGRectValue); // o
        EXPECT_EQ(CGRectMake(x + 1 * glyphWidth, 25 + yPos, 25, height), textRects[11].CGRectValue); // v
        EXPECT_EQ(CGRectMake(x + 2 * glyphWidth, 25 + yPos, 25, height), textRects[12].CGRectValue); // e
        EXPECT_EQ(CGRectMake(x + 3 * glyphWidth, 25 + yPos, 25, height), textRects[13].CGRectValue); // r
        EXPECT_EQ(CGRectMake(x + 4 * glyphWidth, 25 + yPos, 25, height), textRects[14].CGRectValue); //
        EXPECT_EQ(CGRectMake(x + 5 * glyphWidth, 25 + yPos, 25, height), textRects[15].CGRectValue); // t
        EXPECT_EQ(CGRectMake(x + 6 * glyphWidth, 25 + yPos, 25, height), textRects[16].CGRectValue); // h
        EXPECT_EQ(CGRectMake(x + 7 * glyphWidth, 25 + yPos, 25, height), textRects[17].CGRectValue); // e
    }
}

TEST(DocumentEditingContext, RequestRectsInTextAreaInsideIFrame)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    // Use "padding: 0" for the <textarea> as the default user-agent stylesheet can effect text wrapping.
    [webView synchronouslyLoadHTMLString:applyAhemStyle([NSString stringWithFormat:@"<iframe srcdoc=\"%@\" style='position: absolute; left: 1em; top: 1em; border: none'></iframe>", applyAhemStyle(@"<textarea id='test' style='padding: 0'>The quick brown fox jumps over the lazy dog.</textarea><script>let textarea = document.getElementById('test'); textarea.focus(); textarea.setSelectionRange(0, 0); /* Place caret at the beginning of the field. */</script>")])];

    auto *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText | UIWKDocumentRequestRects, UITextGranularityWord, 1)];
    EXPECT_NOT_NULL(context);
    EXPECT_NULL(context.contextBefore);
    EXPECT_NSSTRING_EQ("The", context.contextAfter);
    auto *textRects = [context textRects];
    EXPECT_EQ(3U, textRects.count);

#if PLATFORM(MACCATALYST)
    const size_t yPos = 27;
    const size_t height = 26;
#else
    const size_t yPos = 28;
    const size_t height = 25;
#endif

    if (textRects.count >= 3) {
        CGFloat x = 28;
        EXPECT_EQ(CGRectMake(x + 0 * glyphWidth, yPos, 25, height), textRects[0].CGRectValue); // T
        EXPECT_EQ(CGRectMake(x + 1 * glyphWidth, yPos, 25, height), textRects[1].CGRectValue); // h
        EXPECT_EQ(CGRectMake(x + 2 * glyphWidth, yPos, 25, height), textRects[2].CGRectValue); // e
    }
}

TEST(DocumentEditingContext, RequestRectsInTextAreaInsideScrolledIFrame)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    // Use "padding: 0" for the <textarea> as the default user-agent stylesheet can effect text wrapping.
    [webView synchronouslyLoadHTMLString:applyAhemStyle([NSString stringWithFormat:@"<iframe srcdoc=\"%@\" style='position: absolute; left: 1em; top: 1em; border: none' height='200'></iframe>", applyAhemStyle(@"<body style='height: 1000px'><div style='width: 200px; height: 200px'></div><textarea id='test' style='padding: 0'>The quick brown fox jumps over the lazy dog.</textarea><script>let textarea = document.getElementById('test'); textarea.focus(); textarea.setSelectionRange(0, 0); /* Place caret at the beginning of the field. */ window.scrollTo(0, 200); /* Scroll <textarea> to the top. */</script></body>")])];

    auto *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText | UIWKDocumentRequestRects, UITextGranularityWord, 1)];
    EXPECT_NOT_NULL(context);
    EXPECT_NULL(context.contextBefore);
    EXPECT_NSSTRING_EQ("The", context.contextAfter);
    auto *textRects = [context textRects];
    EXPECT_EQ(3U, textRects.count);

#if PLATFORM(MACCATALYST)
    const size_t yPos = 27;
    const size_t height = 26;
#else
    const size_t yPos = 28;
    const size_t height = 25;
#endif

    if (textRects.count >= 3) {
        CGFloat x = 28;
        EXPECT_EQ(CGRectMake(x + 0 * glyphWidth, yPos, 25, height), textRects[0].CGRectValue); // T
        EXPECT_EQ(CGRectMake(x + 1 * glyphWidth, yPos, 25, height), textRects[1].CGRectValue); // h
        EXPECT_EQ(CGRectMake(x + 2 * glyphWidth, yPos, 25, height), textRects[2].CGRectValue); // e
    }
}

// MARK: Tests using word granularity

TEST(DocumentEditingContext, RequestFirstTwoWords)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:applyAhemStyle(@"<p id='text' contenteditable>The quick brown fox jumps over the lazy dog.</p>")];
    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(text.firstChild, 0, text.firstChild, 0)"]; // Will focus <p>.

    auto *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText | UIWKDocumentRequestRects, UITextGranularityWord, 2)];
    EXPECT_NOT_NULL(context);
    EXPECT_NULL(context.contextBefore);
    EXPECT_NULL(context.selectedText);
    EXPECT_NSSTRING_EQ("The quick", context.contextAfter);

    auto *textRects = context.textRects;
    ASSERT_EQ(9U, textRects.count);
#if PLATFORM(MACCATALYST)
    EXPECT_EQ(CGRectMake(0, -1, 25, 26), textRects[0].CGRectValue); // T
    EXPECT_EQ(CGRectMake(25, -1, 25, 26), textRects[1].CGRectValue); // h
    EXPECT_EQ(CGRectMake(50, -1, 25, 26), textRects[2].CGRectValue); // e
    EXPECT_EQ(CGRectMake(75, -1, 25, 26), textRects[3].CGRectValue); //
    EXPECT_EQ(CGRectMake(100, -1, 25, 26), textRects[4].CGRectValue); // q
    EXPECT_EQ(CGRectMake(125, -1, 25, 26), textRects[5].CGRectValue); // u
    EXPECT_EQ(CGRectMake(150, -1, 25, 26), textRects[6].CGRectValue); // i
    EXPECT_EQ(CGRectMake(175, -1, 25, 26), textRects[7].CGRectValue); // c
    EXPECT_EQ(CGRectMake(200, -1, 25, 26), textRects[8].CGRectValue); // k
#else
    EXPECT_EQ(CGRectMake(0, 0, 25, 25), textRects[0].CGRectValue); // T
    EXPECT_EQ(CGRectMake(25, 0, 25, 25), textRects[1].CGRectValue); // h
    EXPECT_EQ(CGRectMake(50, 0, 25, 25), textRects[2].CGRectValue); // e
    EXPECT_EQ(CGRectMake(75, 0, 25, 25), textRects[3].CGRectValue); //
    EXPECT_EQ(CGRectMake(100, 0, 25, 25), textRects[4].CGRectValue); // q
    EXPECT_EQ(CGRectMake(125, 0, 25, 25), textRects[5].CGRectValue); // u
    EXPECT_EQ(CGRectMake(150, 0, 25, 25), textRects[6].CGRectValue); // i
    EXPECT_EQ(CGRectMake(175, 0, 25, 25), textRects[7].CGRectValue); // c
    EXPECT_EQ(CGRectMake(200, 0, 25, 25), textRects[8].CGRectValue); // k
#endif
}

TEST(DocumentEditingContext, RequestFirstTwoWordWithLeadingNonBreakableSpace)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:applyAhemStyle(@"<p id='text' contenteditable>&nbsp;The quick brown fox jumps over the lazy dog.</p>")];
    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(text.firstChild, 0, text.firstChild, 0)"]; // Will focus <p>.

    auto *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText | UIWKDocumentRequestRects, UITextGranularityWord, 1)];
    EXPECT_NOT_NULL(context);
    EXPECT_NULL(context.contextBefore);
    EXPECT_NULL(context.selectedText);
    EXPECT_NSSTRING_EQ(" The", context.contextAfter);

    auto *textRects = context.textRects;
    ASSERT_EQ(4U, textRects.count);
#if PLATFORM(MACCATALYST)
    EXPECT_EQ(CGRectMake(0, -1, 25, 26), textRects[0].CGRectValue); //
    EXPECT_EQ(CGRectMake(25, -1, 25, 26), textRects[1].CGRectValue); // T
    EXPECT_EQ(CGRectMake(50, -1, 25, 26), textRects[2].CGRectValue); // h
    EXPECT_EQ(CGRectMake(75, -1, 25, 26), textRects[3].CGRectValue); // e
#else
    EXPECT_EQ(CGRectMake(0, 0, 25, 25), textRects[0].CGRectValue); //
    EXPECT_EQ(CGRectMake(25, 0, 25, 25), textRects[1].CGRectValue); // T
    EXPECT_EQ(CGRectMake(50, 0, 25, 25), textRects[2].CGRectValue); // h
    EXPECT_EQ(CGRectMake(75, 0, 25, 25), textRects[3].CGRectValue); // e
#endif
}

TEST(DocumentEditingContext, RequestLastWord)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:applyAhemStyle(@"<p id='text' contenteditable>The quick brown fox jumps over the lazy dog.</p>")];
    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(text.firstChild, text.firstChild.length, text.firstChild, text.firstChild.length)"]; // Will focus <p>.

    auto *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText | UIWKDocumentRequestRects, UITextGranularityWord, 1)];
    EXPECT_NOT_NULL(context);
    EXPECT_NSSTRING_EQ("dog.", context.contextBefore);
    EXPECT_NULL(context.selectedText);
    EXPECT_NULL(context.contextAfter);

    auto *textRects = context.textRects;
    ASSERT_EQ(4U, textRects.count);
#if PLATFORM(MACCATALYST)
    EXPECT_EQ(CGRectMake(0, 24, 25, 26), textRects[0].CGRectValue); // d
    EXPECT_EQ(CGRectMake(25, 24, 25, 26), textRects[1].CGRectValue); // o
    EXPECT_EQ(CGRectMake(50, 24, 25, 26), textRects[2].CGRectValue); // g
    EXPECT_EQ(CGRectMake(75, 24, 25, 26), textRects[3].CGRectValue); // .
#else
    EXPECT_EQ(CGRectMake(0, 25, 25, 25), textRects[0].CGRectValue); // d
    EXPECT_EQ(CGRectMake(25, 25, 25, 25), textRects[1].CGRectValue); // o
    EXPECT_EQ(CGRectMake(50, 25, 25, 25), textRects[2].CGRectValue); // g
    EXPECT_EQ(CGRectMake(75, 25, 25, 25), textRects[3].CGRectValue); // .
#endif
}

TEST(DocumentEditingContext, RequestLastWordWithTrailingNonBreakableSpace)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:applyAhemStyle(@"<p id='text' contenteditable>The quick brown fox jumps over the lazy dog.&nbsp;</p>")];
    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(text.firstChild, text.firstChild.length, text.firstChild, text.firstChild.length)"]; // Will focus <p>.

    auto *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText | UIWKDocumentRequestRects, UITextGranularityWord, 1)];
    EXPECT_NOT_NULL(context);
    EXPECT_NSSTRING_EQ("dog. ", context.contextBefore);
    EXPECT_NULL(context.selectedText);
    EXPECT_NULL(context.contextAfter);

    auto *textRects = context.textRects;
    ASSERT_EQ(5U, textRects.count);
#if PLATFORM(MACCATALYST)
    EXPECT_EQ(CGRectMake(0, 24, 25, 26), textRects[0].CGRectValue); // d
    EXPECT_EQ(CGRectMake(25, 24, 25, 26), textRects[1].CGRectValue); // o
    EXPECT_EQ(CGRectMake(50, 24, 25, 26), textRects[2].CGRectValue); // g
    EXPECT_EQ(CGRectMake(75, 24, 25, 26), textRects[3].CGRectValue); // .
    EXPECT_EQ(CGRectMake(100, 24, 25, 26), textRects[4].CGRectValue); //
#else
    EXPECT_EQ(CGRectMake(0, 25, 25, 25), textRects[0].CGRectValue); // d
    EXPECT_EQ(CGRectMake(25, 25, 25, 25), textRects[1].CGRectValue); // o
    EXPECT_EQ(CGRectMake(50, 25, 25, 25), textRects[2].CGRectValue); // g
    EXPECT_EQ(CGRectMake(75, 25, 25, 25), textRects[3].CGRectValue); // .
    EXPECT_EQ(CGRectMake(100, 25, 25, 25), textRects[4].CGRectValue); //
#endif
}

TEST(DocumentEditingContext, RequestTwoWordsAroundSelection)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:applyStyle(@"<span id='the'>The</span> quick brown fox <span id='jumps'>jumps</span> over the lazy <span id='dog'>dog.</span>")];
    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(jumps, 0, jumps, 1)"];

    auto *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText, UITextGranularityWord, 2)];
    EXPECT_NOT_NULL(context);
    EXPECT_NSSTRING_EQ("brown fox ", context.contextBefore);
    EXPECT_NSSTRING_EQ("jumps", context.selectedText);
    EXPECT_NSSTRING_EQ(" over the", context.contextAfter);
}

TEST(DocumentEditingContext, RequestThreeWordsAroundSelection)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:applyStyle(@"<span id='the'>The</span> quick brown fox <span id='jumps'>jumps</span> over the lazy <span id='dog'>dog.</span>")];
    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(jumps, 0, jumps, 1)"];

    auto *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText, UITextGranularityWord, 3)];
    EXPECT_NOT_NULL(context);
    EXPECT_NSSTRING_EQ("quick brown fox ", context.contextBefore);
    EXPECT_NSSTRING_EQ("jumps", context.selectedText);
    EXPECT_NSSTRING_EQ(" over the lazy", context.contextAfter);
}

TEST(DocumentEditingContext, RequestBeforeInlinePlaceholder)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:applyStyle(@"<span id='wrapper' contenteditable>hello world</span>")];
    [webView stringByEvaluatingJavaScript:@"getSelection().setPosition(wrapper.firstChild, 5)"]; // Place cursor after "hello".

    auto *placeholder = [webView synchronouslyInsertTextPlaceholderWithSize:CGSizeMake(5, 5)];
    EXPECT_NOT_NULL(placeholder);

    auto *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText, UITextGranularityCharacter, 200)];
    EXPECT_NOT_NULL(context);
    EXPECT_NSSTRING_EQ("hello", context.contextBefore);
    EXPECT_NSSTRING_EQ("\uFFFC world", context.contextAfter);

    [webView synchronouslyRemoveTextPlaceholder:placeholder willInsertText:NO];
}

TEST(DocumentEditingContext, RequestAfterInlinePlaceholder)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:applyStyle(@"<span id='wrapper' contenteditable>hello world</span>")];
    [webView stringByEvaluatingJavaScript:@"getSelection().setPosition(wrapper.firstChild, 6)"]; // Place cursor before "world".

    auto *placeholder = [webView synchronouslyInsertTextPlaceholderWithSize:CGSizeMake(5, 5)];
    EXPECT_NOT_NULL(placeholder);

    auto *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText, UITextGranularityCharacter, 200)];
    EXPECT_NOT_NULL(context);
    EXPECT_NSSTRING_EQ("hello ", context.contextBefore);
    EXPECT_NSSTRING_EQ("\uFFFCworld", context.contextAfter);

    [webView synchronouslyRemoveTextPlaceholder:placeholder willInsertText:NO];
}

TEST(DocumentEditingContext, RequestBeforeBlockPlaceholder)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:applyStyle(@"<span id='wrapper' contenteditable>hello world</span>")];
    [webView stringByEvaluatingJavaScript:@"getSelection().setPosition(wrapper.firstChild, 5)"]; // Place cursor after "hello".

    auto *placeholder = [webView synchronouslyInsertTextPlaceholderWithSize:CGSizeMake(0, 5)];
    EXPECT_NOT_NULL(placeholder);

    auto *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText, UITextGranularityCharacter, 200)];
    EXPECT_NOT_NULL(context);
    EXPECT_NSSTRING_EQ("hello", context.contextBefore);
    EXPECT_NSSTRING_EQ("\uFFFC world", context.contextAfter);

    [webView synchronouslyRemoveTextPlaceholder:placeholder willInsertText:NO];
}

TEST(DocumentEditingContext, RequestAfterBlockPlaceholder)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:applyStyle(@"<span id='wrapper' contenteditable>hello world</span>")];
    [webView stringByEvaluatingJavaScript:@"getSelection().setPosition(wrapper.firstChild, 6)"]; // Place cursor before "world".

    auto *placeholder = [webView synchronouslyInsertTextPlaceholderWithSize:CGSizeMake(0, 5)];
    EXPECT_NOT_NULL(placeholder);

    auto *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText, UITextGranularityCharacter, 200)];
    EXPECT_NOT_NULL(context);
    EXPECT_NSSTRING_EQ("hello", context.contextBefore);
    EXPECT_NSSTRING_EQ("\uFFFCworld", context.contextAfter);

    [webView synchronouslyRemoveTextPlaceholder:placeholder willInsertText:NO];
}

// MARK: Tests using sentence granularity

constexpr NSString * const threeSentencesExample = @"<p id='text' contenteditable>The first sentence. The second sentence. The third sentence.</p>";

TEST(DocumentEditingContext, RequestFirstTwoSentences)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:applyStyle(threeSentencesExample)];
    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(text.firstChild, 0, text.firstChild, 0)"]; // Will focus <p>.

    auto *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText, UITextGranularitySentence, 2)];
    EXPECT_NOT_NULL(context);
    EXPECT_NULL(context.contextBefore);
    EXPECT_NULL(context.selectedText);
    EXPECT_NSSTRING_EQ("The first sentence. The second sentence. ", context.contextAfter);
}

TEST(DocumentEditingContext, RequestFirstTwoSentencesNoSpaces)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:applyStyle(@"<p id='text' contenteditable>The first sentence.The second sentence.The third sentence.</p>")];
    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(text.firstChild, 0, text.firstChild, 0)"]; // Will focus <p>.

    auto *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText, UITextGranularitySentence, 2)];
    EXPECT_NOT_NULL(context);
    EXPECT_NULL(context.contextBefore);
    EXPECT_NULL(context.selectedText);
    EXPECT_NSSTRING_EQ("The first sentence.The second sentence.The third sentence.", context.contextAfter);
}

TEST(DocumentEditingContext, RequestLastSentence)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:applyStyle(threeSentencesExample)];
    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(text.firstChild, text.firstChild.length, text.firstChild, text.firstChild.length)"]; // Will focus <p>.

    auto *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText, UITextGranularitySentence, 1)];
    EXPECT_NOT_NULL(context);
    EXPECT_NSSTRING_EQ("The third sentence.", context.contextBefore);
    EXPECT_NULL(context.selectedText);
    EXPECT_NULL(context.contextAfter);
}

TEST(DocumentEditingContext, RequestLastTwoSentences)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:applyStyle(threeSentencesExample)];
    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(text.firstChild, text.firstChild.length, text.firstChild, text.firstChild.length)"]; // Will focus <p>.

    auto *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText, UITextGranularitySentence, 2)];
    EXPECT_NOT_NULL(context);
    EXPECT_NSSTRING_EQ("The second sentence. The third sentence.", context.contextBefore);
    EXPECT_NULL(context.selectedText);
    EXPECT_NULL(context.contextAfter);
}

// MARK: Tests using paragraph granularity

constexpr NSString * const threeParagraphsExample = @"<pre id='text' style='width: 32em' contenteditable>The first sentence in the first paragraph. The second sentence in the first paragraph. The third sentence in the first paragraph.\nThe first sentence in the second paragraph. The second sentence in the second paragraph. The third sentence in the second paragraph.\nThe first sentence in the third paragraph. The second sentence in the third paragraph. The third sentence in the third paragraph.</pre>";

TEST(DocumentEditingContext, RequestFirstParagraph)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:applyStyle(threeParagraphsExample)];
    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(text.firstChild, 0, text.firstChild, 0)"]; // Will focus <p>.

    auto *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText, UITextGranularityParagraph, 1)];
    EXPECT_NOT_NULL(context);
    EXPECT_NULL(context.contextBefore);
    EXPECT_NULL(context.selectedText);
    EXPECT_NSSTRING_EQ("The first sentence in the first paragraph. The second sentence in the first paragraph. The third sentence in the first paragraph.", context.contextAfter);
}

TEST(DocumentEditingContext, RequestFirstTwoParagraphs)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:applyStyle(threeParagraphsExample)];
    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(text.firstChild, 0, text.firstChild, 0)"]; // Will focus <p>.

    auto *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText, UITextGranularityParagraph, 2)];
    EXPECT_NOT_NULL(context);
    EXPECT_NULL(context.contextBefore);
    EXPECT_NULL(context.selectedText);
    EXPECT_NSSTRING_EQ("The first sentence in the first paragraph. The second sentence in the first paragraph. The third sentence in the first paragraph.\nThe first sentence in the second paragraph. The second sentence in the second paragraph. The third sentence in the second paragraph.", context.contextAfter);
}

TEST(DocumentEditingContext, RequestLastParagraph)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:applyStyle(threeParagraphsExample)];
    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(text.firstChild, text.firstChild.length, text.firstChild, text.firstChild.length)"]; // Will focus <p>.

    auto *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText, UITextGranularityParagraph, 1)];
    EXPECT_NOT_NULL(context);
    EXPECT_NSSTRING_EQ("The first sentence in the third paragraph. The second sentence in the third paragraph. The third sentence in the third paragraph.", context.contextBefore);
    EXPECT_NULL(context.selectedText);
    EXPECT_NULL(context.contextAfter);
}

TEST(DocumentEditingContext, RequestLastTwoParagraphs)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:applyStyle(threeParagraphsExample)];
    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(text.firstChild, text.firstChild.length, text.firstChild, text.firstChild.length)"]; // Will focus <p>.

    auto *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText, UITextGranularityParagraph, 2)];
    EXPECT_NOT_NULL(context);
    EXPECT_NSSTRING_EQ("The first sentence in the second paragraph. The second sentence in the second paragraph. The third sentence in the second paragraph.\nThe first sentence in the third paragraph. The second sentence in the third paragraph. The third sentence in the third paragraph.", context.contextBefore);
    EXPECT_NULL(context.selectedText);
    EXPECT_NULL(context.contextAfter);
}

TEST(DocumentEditingContext, RequestLastTwoParagraphsWithSelectiontWithinParagraph)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:applyAhemStyle(@"<span id='the'>The</span> quick brown fox <span id='jumps'>jumps</span> over the lazy <span id='dog'>dog.</span>")];
    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(jumps, 0, jumps, 1)"]; // Will focus <p>.

    auto *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText, UITextGranularityParagraph, 2)];
    EXPECT_NOT_NULL(context);
    EXPECT_NSSTRING_EQ("The quick brown fox ", context.contextBefore);
    EXPECT_NSSTRING_EQ("jumps", context.selectedText);
    EXPECT_NSSTRING_EQ(" over the lazy dog.", context.contextAfter);
}

// MARK: Tests using character granularity
// Note that we always return results with respect to the nearest word boundary in the direction of the selection.

TEST(DocumentEditingContext, RequestFirstCharacter)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:applyStyle(threeParagraphsExample)];
    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(text.firstChild, 0, text.firstChild, 0)"]; // Will focus <p>.

    auto *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText, UITextGranularityCharacter, 1)];
    EXPECT_NOT_NULL(context);
    EXPECT_NULL(context.contextBefore);
    EXPECT_NULL(context.selectedText);
    EXPECT_NSSTRING_EQ("The", context.contextAfter);
}

TEST(DocumentEditingContext, RequestFirstWordUsingCharacterGranularity)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:applyStyle(threeParagraphsExample)];
    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(text.firstChild, 0, text.firstChild, 0)"]; // Will focus <p>.

    auto *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText, UITextGranularityCharacter, 3)];
    EXPECT_NOT_NULL(context);
    EXPECT_NULL(context.contextBefore);
    EXPECT_NULL(context.selectedText);
    EXPECT_NSSTRING_EQ("The", context.contextAfter);
}

TEST(DocumentEditingContext, RequestFirstWordPlusTrailingSpaceUsingCharacterGranularity)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:applyStyle(threeParagraphsExample)];
    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(text.firstChild, 0, text.firstChild, 0)"]; // Will focus <p>.

    auto *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText, UITextGranularityCharacter, 4)];
    EXPECT_NOT_NULL(context);
    EXPECT_NULL(context.contextBefore);
    EXPECT_NULL(context.selectedText);
    EXPECT_NSSTRING_EQ("The first", context.contextAfter);
}

// MARK: Tests using line granularity

TEST(DocumentEditingContext, RequestFirstLine)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:applyAhemStyle(threeParagraphsExample)];
    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(text.firstChild, 0, text.firstChild, 0)"]; // Will focus <p>.

    auto *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText, UITextGranularityLine, 1)];
    EXPECT_NOT_NULL(context);
    EXPECT_NULL(context.contextBefore);
    EXPECT_NULL(context.selectedText);
    EXPECT_NSSTRING_EQ("The first sentence in the first paragraph", context.contextAfter);
}

TEST(DocumentEditingContext, RequestFirstTwoLines)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:applyAhemStyle(threeParagraphsExample)];
    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(text.firstChild, 0, text.firstChild, 0)"]; // Will focus <p>.

    auto *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText, UITextGranularityLine, 2)];
    EXPECT_NOT_NULL(context);
    EXPECT_NULL(context.contextBefore);
    EXPECT_NULL(context.selectedText);
    EXPECT_NSSTRING_EQ("The first sentence in the first paragraph. The second sentence in", context.contextAfter);
}

TEST(DocumentEditingContext, RequestLastLine)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:applyAhemStyle(threeParagraphsExample)];
    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(text.firstChild, text.firstChild.length, text.firstChild, text.firstChild.length)"]; // Will focus <p>.

    auto *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText, UITextGranularityLine, 1)];
    EXPECT_NOT_NULL(context);
    EXPECT_NSSTRING_EQ("third sentence in the third paragraph.", context.contextBefore);
    EXPECT_NULL(context.selectedText);
    EXPECT_NULL(context.contextAfter);
}

TEST(DocumentEditingContext, RequestLastTwoLines)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:applyAhemStyle(threeParagraphsExample)];
    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(text.firstChild, text.firstChild.length, text.firstChild, text.firstChild.length)"]; // Will focus <p>.

    auto *context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText, UITextGranularityLine, 2)];
    EXPECT_NOT_NULL(context);
    EXPECT_NSSTRING_EQ("in the third paragraph. The third sentence in the third paragraph.", context.contextBefore);
    EXPECT_NULL(context.selectedText);
    EXPECT_NULL(context.contextAfter);
}

#endif // PLATFORM(IOS_FAMILY) && HAVE(UI_WK_DOCUMENT_CONTEXT)
