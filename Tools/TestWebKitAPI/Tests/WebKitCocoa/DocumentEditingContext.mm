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
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import "UIKitSPI.h"
#import <WebKit/WKPreferencesRefPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKTextInputContext.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

#define EXPECT_NSSTRING_EQ(expected, actual) \
    EXPECT_TRUE([actual isKindOfClass:[NSString class]]); \
    EXPECT_WK_STREQ(expected, (NSString *)actual);

#define EXPECT_ATTRIBUTED_STRING_EQ(expected, actual) \
    EXPECT_TRUE([actual isKindOfClass:[NSAttributedString class]]); \
    EXPECT_WK_STREQ(expected, [(NSAttributedString *)actual string]);

@interface WKContentView ()
- (void)requestDocumentContext:(UIWKDocumentRequest *)request completionHandler:(void (^)(UIWKDocumentContext *))completionHandler;
- (void)adjustSelectionWithDelta:(NSRange)deltaRange completionHandler:(void (^)(void))completionHandler;
@end

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
    [[self wkContentView] requestDocumentContext:request completionHandler:^(UIWKDocumentContext *context) {
        result = context;
        finished = true;
    }];
    TestWebKitAPI::Util::run(&finished);
    return result.autorelease();
}

- (void)synchronouslyAdjustSelectionWithDelta:(NSRange)range
{
    __block bool finished = false;
    [[self wkContentView] adjustSelectionWithDelta:range completionHandler:^() {
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

@end

static NSString *applyStyle(NSString *htmlString)
{
    return [@"<style>body { margin: 0; } </style><meta name='viewport' content='initial-scale=1'>" stringByAppendingString:htmlString];
}

constexpr unsigned glyphWidth { 25 }; // pixels

static NSString *applyAhemStyle(NSString *htmlString)
{
    return [NSString stringWithFormat:@"<style>@font-face { font-family: Ahem; src: url(Ahem.ttf); } body { margin: 0; font: %upx/1 Ahem; -webkit-text-size-adjust: 100%%; }</style><meta name='viewport' content='width=980, initial-scale=1.0'>%@", glyphWidth, htmlString];
}

TEST(WebKit, DocumentEditingContext)
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
    context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText | UIWKDocumentRequestSpatial, UITextGranularityWord, 1, CGRectMake(0, 0, 10, 10))];
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
    EXPECT_EQ(CGRectMake(0, 0, glyphWidth, glyphWidth), rects[0].CGRectValue);
    EXPECT_EQ(CGRectMake(glyphWidth, 0, glyphWidth, glyphWidth), rects[1].CGRectValue);
    EXPECT_EQ(CGRectMake(2 * glyphWidth, 0, glyphWidth, glyphWidth), rects[2].CGRectValue);
    EXPECT_EQ(CGRectMake(3 * glyphWidth, 0, glyphWidth, glyphWidth), rects[3].CGRectValue);
    rects = [context characterRectsForCharacterRange:NSMakeRange(5, 1)];
    EXPECT_EQ(0UL, rects.count);

    context = [webView synchronouslyRequestDocumentContext:makeRequest(UIWKDocumentRequestText | UIWKDocumentRequestRects, UITextGranularityWord, 1)];
    EXPECT_NSSTRING_EQ(" MMM", context.contextAfter);
    rects = [context characterRectsForCharacterRange:NSMakeRange(0, 1)];
    EXPECT_EQ(1UL, rects.count);
    EXPECT_EQ(CGRectMake(0, 0, glyphWidth, glyphWidth), rects.firstObject.CGRectValue);
    rects = [context characterRectsForCharacterRange:NSMakeRange(6, 1)];
    EXPECT_EQ(1UL, rects.count);
    EXPECT_EQ(CGRectMake(6 * glyphWidth, 0, glyphWidth, glyphWidth), rects.firstObject.CGRectValue);

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

TEST(WebKit, DocumentEditingContextWithMarkedText)
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

TEST(WebKit, DocumentEditingContextSpatialRequestInTextField)
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
        EXPECT_EQ(CGRectMake(182, 9, 13, 19), textRects[5].CGRectValue);
        EXPECT_EQ(CGRectMake(194, 9, 9, 19), textRects[6].CGRectValue);
        EXPECT_EQ(CGRectMake(202, 9, 6, 19), textRects[7].CGRectValue);
        EXPECT_EQ(CGRectMake(207, 9, 6, 19), textRects[8].CGRectValue);
        EXPECT_EQ(CGRectMake(212, 9, 9, 19), textRects[9].CGRectValue);
    }
}

#endif // PLATFORM(IOS_FAMILY) && HAVE(UI_WK_DOCUMENT_CONTEXT)
