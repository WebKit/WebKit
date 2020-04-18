/*
 * Copyright (C) 2019-2020 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)

#import "PlatformUtilities.h"
#import "TestCocoa.h"
#import "TestInputDelegate.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKPreferencesRefPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/_WKTextInputContext.h>
#import <wtf/RetainPtr.h>

@implementation TestWKWebView (SynchronousTextInputContext)

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

- (UIResponder<UITextInput> *)synchronouslyFocusTextInputContext:(_WKTextInputContext *)context placeCaretAt:(CGPoint)point
{
    __block bool finished = false;
    __block UIResponder<UITextInput> *responder = nil;
    [self _focusTextInputContext:context placeCaretAt:point completionHandler:^(UIResponder<UITextInput> *textInputResponder) {
        responder = textInputResponder;
        finished = true;
    }];
    TestWebKitAPI::Util::run(&finished);
    return responder;
}

@end

static NSString *applyStyle(NSString *HTMLString)
{
    return [@"<style>body { margin: 0; } iframe { border: none; }</style><meta name='viewport' content='initial-scale=1'>" stringByAppendingString:HTMLString];
}

static NSString *applyIframe(NSString *HTMLString)
{
    return applyStyle([NSString stringWithFormat:@"<iframe src=\"data:text/html,%@\" style='position: absolute; top: 200px;'>", [applyStyle(HTMLString) stringByAddingPercentEncodingWithAllowedCharacters:[NSCharacterSet URLPathAllowedCharacterSet]]]);
}

TEST(RequestTextInputContext, Simple)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    NSArray<_WKTextInputContext *> *contexts;

    // Basic inputs.

    [webView synchronouslyLoadHTMLString:applyStyle(@"<input type='text' style='width: 50px; height: 50px;'>")];
    contexts = [webView synchronouslyRequestTextInputContextsInRect:[webView bounds]];
    EXPECT_EQ(1UL, contexts.count);
    EXPECT_EQ(CGRectMake(0, 0, 50, 50), contexts[0].boundingRect);

    [webView synchronouslyLoadHTMLString:applyStyle(@"<textarea style='width: 100px; height: 100px;'></textarea>")];
    contexts = [webView synchronouslyRequestTextInputContextsInRect:[webView bounds]];
    EXPECT_EQ(1UL, contexts.count);
    EXPECT_EQ(CGRectMake(0, 0, 100, 100), contexts[0].boundingRect);

    [webView synchronouslyLoadHTMLString:applyStyle(@"<div contenteditable style='width: 100px; height: 100px;'></div>")];
    contexts = [webView synchronouslyRequestTextInputContextsInRect:[webView bounds]];
    EXPECT_EQ(1UL, contexts.count);
    EXPECT_EQ(CGRectMake(0, 0, 100, 100), contexts[0].boundingRect);

    // Read only inputs; should not be included.

    [webView synchronouslyLoadHTMLString:applyStyle(@"<input type='text' style='width: 50px; height: 50px;' readonly>")];
    contexts = [webView synchronouslyRequestTextInputContextsInRect:[webView bounds]];
    EXPECT_EQ(0UL, contexts.count);

    [webView synchronouslyLoadHTMLString:applyStyle(@"<textarea style='width: 100px; height: 100px;' readonly>")];
    contexts = [webView synchronouslyRequestTextInputContextsInRect:[webView bounds]];
    EXPECT_EQ(0UL, contexts.count);

    // Inputs outside the requested rect; should not be included.

    [webView synchronouslyLoadHTMLString:applyStyle(@"<input type='text' style='width: 50px; height: 50px;'>")];
    contexts = [webView synchronouslyRequestTextInputContextsInRect:CGRectMake(100, 100, 100, 100)];
    EXPECT_EQ(0UL, contexts.count);

    // Inputs scrolled outside the requested rect; should not be included.

    [webView synchronouslyLoadHTMLString:applyStyle(@"<input type='text' style='width: 50px; height: 50px;'><br><div style='width: 100px; height: 10000px;'></div>")];
    [webView stringByEvaluatingJavaScript:@"window.scrollTo(0, 5000)"];
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ(5000, [[webView objectByEvaluatingJavaScript:@"pageYOffset"] floatValue]);
    contexts = [webView synchronouslyRequestTextInputContextsInRect:[webView bounds]];
    EXPECT_EQ(0UL, contexts.count);

    // Inputs scrolled into the requested rect.

    [webView synchronouslyLoadHTMLString:applyStyle(@"<input type='text' style='width: 50px; height: 50px; position: absolute; top: 5000px;'><br><div style='width: 100px; height: 10000px;'></div>")];
    [webView stringByEvaluatingJavaScript:@"window.scrollTo(0, 5000)"];
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ(5000, [[webView objectByEvaluatingJavaScript:@"pageYOffset"] floatValue]);
    contexts = [webView synchronouslyRequestTextInputContextsInRect:[webView bounds]];
    EXPECT_EQ(1UL, contexts.count);
    EXPECT_EQ(CGRectMake(0, 0, 50, 50), contexts[0].boundingRect);

    // Multiple inputs.

    [webView synchronouslyLoadHTMLString:applyStyle(@"<input type='text' style='width: 50px; height: 50px;'><br/><input type='text' style='width: 50px; height: 50px;'><br/><input type='text' style='width: 50px; height: 50px;'>")];
    contexts = [webView synchronouslyRequestTextInputContextsInRect:[webView bounds]];
    EXPECT_EQ(3UL, contexts.count);
    EXPECT_EQ(CGRectMake(0, 100, 50, 50), contexts[0].boundingRect);
    EXPECT_EQ(CGRectMake(0, 50, 50, 50), contexts[1].boundingRect);
    EXPECT_EQ(CGRectMake(0, 0, 50, 50), contexts[2].boundingRect);

    // Nested <input>-inside-contenteditable.

    [webView synchronouslyLoadHTMLString:applyStyle(@"<div contenteditable style='width: 100px; height: 100px;'><input type='text' style='width: 50px; height: 50px;'></div>")];
    contexts = [webView synchronouslyRequestTextInputContextsInRect:[webView bounds]];
    EXPECT_EQ(2UL, contexts.count);
    EXPECT_EQ(CGRectMake(0, 0, 50, 50), contexts[0].boundingRect);
    EXPECT_EQ(CGRectMake(0, 0, 100, 100), contexts[1].boundingRect);
}

// Consider moving this to TestWKWebView if it could be useful to other tests.
static void webViewLoadHTMLStringAndWaitForAllFramesToPaint(TestWKWebView *webView, NSString *htmlString)
{
    ASSERT(webView); // Make passing a nil web view a more obvious failure than a hang.
    bool didFireDOMLoadEvent = false;
    [webView performAfterLoading:[&] { didFireDOMLoadEvent = true; }];
    [webView loadHTMLString:htmlString baseURL:[NSBundle.mainBundle.bundleURL URLByAppendingPathComponent:@"TestWebKitAPI.resources"]];
    TestWebKitAPI::Util::run(&didFireDOMLoadEvent);
    [webView waitForNextPresentationUpdate];
}

TEST(RequestTextInputContext, Iframe)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    NSArray<_WKTextInputContext *> *contexts;

    webViewLoadHTMLStringAndWaitForAllFramesToPaint(webView.get(), applyIframe(@"<input type='text' style='width: 50px; height: 50px;'>"));
    contexts = [webView synchronouslyRequestTextInputContextsInRect:[webView bounds]];
    EXPECT_EQ(1UL, contexts.count);
    EXPECT_EQ(CGRectMake(0, 200, 50, 50), contexts[0].boundingRect);

    webViewLoadHTMLStringAndWaitForAllFramesToPaint(webView.get(), applyIframe(@"<textarea style='width: 100px; height: 100px;'></textarea>"));
    contexts = [webView synchronouslyRequestTextInputContextsInRect:[webView bounds]];
    EXPECT_EQ(1UL, contexts.count);
    EXPECT_EQ(CGRectMake(0, 200, 100, 100), contexts[0].boundingRect);

    webViewLoadHTMLStringAndWaitForAllFramesToPaint(webView.get(), applyIframe(@"<div contenteditable style='width: 100px; height: 100px;'></div>"));
    contexts = [webView synchronouslyRequestTextInputContextsInRect:[webView bounds]];
    EXPECT_EQ(1UL, contexts.count);
    EXPECT_EQ(CGRectMake(0, 200, 100, 100), contexts[0].boundingRect);
}

TEST(RequestTextInputContext, ViewIsEditable)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView synchronouslyLoadHTMLString:applyStyle(@"<body></body>")];
    [webView _setEditable:YES];
    EXPECT_GE([webView synchronouslyRequestTextInputContextsInRect:[webView bounds]].count, 1UL);
}

static CGRect squareCenteredAtPoint(float x, float y, float length)
{
    return CGRectMake(x - length / 2, y - length / 2, length, length);
}

TEST(RequestTextInputContext, NonCompositedOverlap)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    NSArray<_WKTextInputContext *> *contexts;

    [webView synchronouslyLoadTestPageNamed:@"editable-region-composited-and-non-composited-overlap"];

    // Search rect fully contained in non-composited <div>.
    contexts = [webView synchronouslyRequestTextInputContextsInRect:CGRectMake(270, 150, 10, 10)];
    EXPECT_EQ(0UL, contexts.count);
    contexts = [webView synchronouslyRequestTextInputContextsInRect:CGRectMake(170, 150, 10, 10)];
    EXPECT_EQ(0UL, contexts.count);

    // Search rect overlaps both the non-composited <div> and the editable element.
    contexts = [webView synchronouslyRequestTextInputContextsInRect:squareCenteredAtPoint(270, 150, 200)];
    EXPECT_EQ(1UL, contexts.count);
}

TEST(RequestTextInputContext, CompositedOverlap)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    NSArray<_WKTextInputContext *> *contexts;

    [webView synchronouslyLoadTestPageNamed:@"editable-region-composited-and-non-composited-overlap"];

    // Search rect fully contained in composited <div>.
    contexts = [webView synchronouslyRequestTextInputContextsInRect:CGRectMake(270, 400, 10, 10)];
    EXPECT_EQ(0UL, contexts.count);
    contexts = [webView synchronouslyRequestTextInputContextsInRect:CGRectMake(170, 400, 10, 10)];
    EXPECT_EQ(0UL, contexts.count);

    // Search rect overlaps both the composited <div> and the editable element.
    contexts = [webView synchronouslyRequestTextInputContextsInRect:squareCenteredAtPoint(270, 400, 200)];
    EXPECT_EQ(1UL, contexts.count);
}

TEST(RequestTextInputContext, ReadOnlyField)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView synchronouslyLoadHTMLString:applyStyle(@"<input type='text' value='hello world' style='width: 100px; height: 50px;' readonly>")];
    EXPECT_EQ(0UL, [webView synchronouslyRequestTextInputContextsInRect:[webView bounds]].count);
}

TEST(RequestTextInputContext, DisabledField)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView synchronouslyLoadHTMLString:applyStyle(@"<input type='text' value='hello world' style='width: 100px; height: 50px;' disabled>")];
    EXPECT_EQ(0UL, [webView synchronouslyRequestTextInputContextsInRect:[webView bounds]].count);
}

TEST(RequestTextInputContext, FocusAfterNavigation)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    // 1. Load initial page and save off the text input context for the input element on it.
    [webView synchronouslyLoadHTMLString:applyStyle(@"<input type='text' value='hello world' style='width: 100px; height: 50px;'>")];
    NSArray<_WKTextInputContext *> *contexts = [webView synchronouslyRequestTextInputContextsInRect:[webView bounds]];
    EXPECT_EQ(1UL, contexts.count);
    RetainPtr<_WKTextInputContext> inputElement = contexts[0];

    // 2. Load a new page.
    [webView synchronouslyLoadHTMLString:@"<body></body>"];

    // 3. Focus the input element in the old page.
    EXPECT_NULL([webView synchronouslyFocusTextInputContext:inputElement.get() placeCaretAt:[inputElement boundingRect].origin]);
}

TEST(RequestTextInputContext, CaretShouldNotMoveInAlreadyFocusedField)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    constexpr char exampleText[] = "hello world";
    constexpr size_t exampleTextLength = sizeof(exampleText) - 1;
    [webView synchronouslyLoadHTMLString:applyStyle([NSString stringWithFormat:@"<input type='text' value='%s' style='width: 100px; height: 50px;'>", exampleText])];
    NSArray<_WKTextInputContext *> *contexts = [webView synchronouslyRequestTextInputContextsInRect:[webView bounds]];
    EXPECT_EQ(1UL, contexts.count);

    EXPECT_WK_STREQ("BODY", [webView stringByEvaluatingJavaScript:@"document.activeElement.tagName"]);

    // Place the caret the end of the field; should succeed becausse field is not already focused.
    RetainPtr<_WKTextInputContext> inputElement = contexts[0];
    CGRect boundingRect = [inputElement boundingRect];
    CGPoint endPosition = CGPointMake(boundingRect.origin.x + boundingRect.size.width, boundingRect.origin.y + boundingRect.size.height / 2);
    EXPECT_EQ((UIResponder<UITextInput> *)[webView textInputContentView], [webView synchronouslyFocusTextInputContext:inputElement.get() placeCaretAt:endPosition]);
    EXPECT_WK_STREQ("INPUT", [webView stringByEvaluatingJavaScript:@"document.activeElement.tagName"]);
    EXPECT_EQ(static_cast<int>(exampleTextLength), [[webView objectByEvaluatingJavaScript:@"document.activeElement.selectionStart"] intValue]);
    EXPECT_EQ(static_cast<int>(exampleTextLength), [[webView objectByEvaluatingJavaScript:@"document.activeElement.selectionEnd"] intValue]);

    // Try to place the caret at the start of the field; should fail since field is already focused.
    EXPECT_EQ((UIResponder<UITextInput> *)[webView textInputContentView], [webView synchronouslyFocusTextInputContext:inputElement.get() placeCaretAt:boundingRect.origin]);
    EXPECT_WK_STREQ("INPUT", [webView stringByEvaluatingJavaScript:@"document.activeElement.tagName"]);
    EXPECT_EQ(static_cast<int>(exampleTextLength), [[webView objectByEvaluatingJavaScript:@"document.activeElement.selectionStart"] intValue]);
    EXPECT_EQ(static_cast<int>(exampleTextLength), [[webView objectByEvaluatingJavaScript:@"document.activeElement.selectionEnd"] intValue]);
}

TEST(RequestTextInputContext, CaretShouldNotMoveInAlreadyFocusedField2)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[] (WKWebView *, id <_WKFocusedElementInfo>) { return _WKFocusStartsInputSessionPolicyAllow; }];
    [webView _setInputDelegate:inputDelegate.get()];

    [webView synchronouslyLoadHTMLString:applyStyle(@"<input type='text' id='input' value='hello world' style='width: 100px; height: 50px;'>")];

    // Use JavaScript to place the caret after the 'h' in the field.
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"input.focus()"];
    EXPECT_WK_STREQ("INPUT", [webView stringByEvaluatingJavaScript:@"document.activeElement.tagName"]);
    [webView stringByEvaluatingJavaScript:@"input.setSelectionRange(1, 1)"];
    EXPECT_EQ(1, [[webView objectByEvaluatingJavaScript:@"document.activeElement.selectionStart"] intValue]);
    EXPECT_EQ(1, [[webView objectByEvaluatingJavaScript:@"document.activeElement.selectionEnd"] intValue]);

    // Use -focusTextInputContext: to place the caret at the beginning of the field; no change because the field is already focused.
    NSArray<_WKTextInputContext *> *contexts = [webView synchronouslyRequestTextInputContextsInRect:[webView bounds]];
    EXPECT_EQ(1UL, contexts.count);
    RetainPtr<_WKTextInputContext> inputElement = contexts[0];
    EXPECT_EQ((UIResponder<UITextInput> *)[webView textInputContentView], [webView synchronouslyFocusTextInputContext:inputElement.get() placeCaretAt:[inputElement boundingRect].origin]);
    EXPECT_WK_STREQ("INPUT", [webView stringByEvaluatingJavaScript:@"document.activeElement.tagName"]);
    EXPECT_EQ(1, [[webView objectByEvaluatingJavaScript:@"document.activeElement.selectionStart"] intValue]);
    EXPECT_EQ(1, [[webView objectByEvaluatingJavaScript:@"document.activeElement.selectionEnd"] intValue]);
}

TEST(RequestTextInputContext, FocusTextFieldThenProgrammaticallyReplaceWithTextAreaAndFocusTextArea)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView synchronouslyLoadHTMLString:applyStyle(@"<input type='text' style='width: 50px; height: 50px;'>")];
    NSArray<_WKTextInputContext *> *contexts = [webView synchronouslyRequestTextInputContextsInRect:[webView bounds]];
    EXPECT_EQ(1UL, contexts.count);

    RetainPtr<_WKTextInputContext> originalInput = contexts[0];
    EXPECT_EQ((UIResponder<UITextInput> *)[webView textInputContentView], [webView synchronouslyFocusTextInputContext:originalInput.get() placeCaretAt:[originalInput boundingRect].origin]);
    EXPECT_WK_STREQ("INPUT", [webView stringByEvaluatingJavaScript:@"document.activeElement.tagName"]);

    // Replace the <input> with a <textarea> with script; the <input> should no longer be focusable.
    [webView objectByEvaluatingJavaScript:@"document.body.innerHTML = '<textarea>'"];
    contexts = [webView synchronouslyRequestTextInputContextsInRect:[webView bounds]];
    EXPECT_EQ(1UL, contexts.count);
    RetainPtr<_WKTextInputContext> textArea = contexts[0];
    EXPECT_NULL([webView synchronouslyFocusTextInputContext:originalInput.get() placeCaretAt:[originalInput boundingRect].origin]);
    EXPECT_EQ((UIResponder<UITextInput> *)[webView textInputContentView], [webView synchronouslyFocusTextInputContext:textArea.get() placeCaretAt:[textArea boundingRect].origin]);
    EXPECT_WK_STREQ("TEXTAREA", [webView stringByEvaluatingJavaScript:@"document.activeElement.tagName"]);
}

TEST(RequestTextInputContext, FocusFieldAndPlaceCaretAtStart)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView synchronouslyLoadHTMLString:applyStyle(@"<input type='text' value='hello world' style='width: 100px; height: 50px;'>")];
    NSArray<_WKTextInputContext *> *contexts = [webView synchronouslyRequestTextInputContextsInRect:[webView bounds]];
    EXPECT_EQ(1UL, contexts.count);
    RetainPtr<_WKTextInputContext> inputElement = contexts[0];

    EXPECT_EQ((UIResponder<UITextInput> *)[webView textInputContentView], [webView synchronouslyFocusTextInputContext:inputElement.get() placeCaretAt:[inputElement boundingRect].origin]);
    EXPECT_WK_STREQ("INPUT", [webView stringByEvaluatingJavaScript:@"document.activeElement.tagName"]);
    EXPECT_EQ(0, [[webView objectByEvaluatingJavaScript:@"document.activeElement.selectionStart"] intValue]);
    EXPECT_EQ(0, [[webView objectByEvaluatingJavaScript:@"document.activeElement.selectionEnd"] intValue]);
}

TEST(RequestTextInputContext, FocusFieldAndPlaceCaretAtEnd)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    constexpr char exampleText[] = "hello world";
    constexpr size_t exampleTextLength = sizeof(exampleText) - 1;
    [webView synchronouslyLoadHTMLString:applyStyle([NSString stringWithFormat:@"<input type='text' value='%s' style='width: 100px; height: 50px;'>", exampleText])];
    NSArray<_WKTextInputContext *> *contexts = [webView synchronouslyRequestTextInputContextsInRect:[webView bounds]];
    EXPECT_EQ(1UL, contexts.count);
    RetainPtr<_WKTextInputContext> inputElement = contexts[0];

    CGRect boundingRect = [inputElement boundingRect];
    CGPoint endPosition = CGPointMake(boundingRect.origin.x + boundingRect.size.width, boundingRect.origin.y + boundingRect.size.height / 2);
    EXPECT_EQ((UIResponder<UITextInput> *)[webView textInputContentView], [webView synchronouslyFocusTextInputContext:inputElement.get() placeCaretAt:endPosition]);
    EXPECT_WK_STREQ("INPUT", [webView stringByEvaluatingJavaScript:@"document.activeElement.tagName"]);
    EXPECT_EQ(static_cast<int>(exampleTextLength), [[webView objectByEvaluatingJavaScript:@"document.activeElement.selectionStart"] intValue]);
    EXPECT_EQ(static_cast<int>(exampleTextLength), [[webView objectByEvaluatingJavaScript:@"document.activeElement.selectionEnd"] intValue]);
}

TEST(RequestTextInputContext, FocusFieldAndPlaceCaretOutsideField)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    constexpr char exampleText[] = "hello world";
    constexpr size_t exampleTextLength = sizeof(exampleText) - 1;
    [webView synchronouslyLoadHTMLString:applyStyle([NSString stringWithFormat:@"<input type='text' value='%s' style='width: 100px; height: 50px;'>", exampleText])];
    NSArray<_WKTextInputContext *> *contexts = [webView synchronouslyRequestTextInputContextsInRect:[webView bounds]];
    EXPECT_EQ(1UL, contexts.count);
    RetainPtr<_WKTextInputContext> inputElement = contexts[0];

    auto resetTest = [&] {
        [webView stringByEvaluatingJavaScript:@"document.activeElement.blur()"];
        EXPECT_WK_STREQ("BODY", [webView stringByEvaluatingJavaScript:@"document.activeElement.tagName"]);
    };

    // Point before the field
    EXPECT_EQ((UIResponder<UITextInput> *)[webView textInputContentView], [webView synchronouslyFocusTextInputContext:inputElement.get() placeCaretAt:CGPointMake(-1000, -500)]);
    EXPECT_WK_STREQ("INPUT", [webView stringByEvaluatingJavaScript:@"document.activeElement.tagName"]);
    EXPECT_EQ(0, [[webView objectByEvaluatingJavaScript:@"document.activeElement.selectionStart"] intValue]);
    EXPECT_EQ(0, [[webView objectByEvaluatingJavaScript:@"document.activeElement.selectionEnd"] intValue]);
    resetTest();

    // Point after the field
    EXPECT_EQ((UIResponder<UITextInput> *)[webView textInputContentView], [webView synchronouslyFocusTextInputContext:inputElement.get() placeCaretAt:CGPointMake(1000, 500)]);
    EXPECT_WK_STREQ("INPUT", [webView stringByEvaluatingJavaScript:@"document.activeElement.tagName"]);
    EXPECT_EQ(static_cast<int>(exampleTextLength), [[webView objectByEvaluatingJavaScript:@"document.activeElement.selectionStart"] intValue]);
    EXPECT_EQ(static_cast<int>(exampleTextLength), [[webView objectByEvaluatingJavaScript:@"document.activeElement.selectionEnd"] intValue]);
    resetTest();
}

TEST(RequestTextInputContext, FocusFieldInFrame)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setAllowUniversalAccessFromFileURLs:YES];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    auto testPage = applyStyle([NSString stringWithFormat:@"<input type='text' value='mainFrameField' style='width: 100px; height: 50px;'>%@", applyIframe(@"<input type='text' value='iframeField' style='width: 120px; height: 70px;'>")]);
    webViewLoadHTMLStringAndWaitForAllFramesToPaint(webView.get(), testPage);
    NSArray<_WKTextInputContext *> *contexts = [webView synchronouslyRequestTextInputContextsInRect:[webView bounds]];
    EXPECT_EQ(2UL, contexts.count);
    RetainPtr<_WKTextInputContext> frameBField = contexts[0];

    EXPECT_EQ((UIResponder<UITextInput> *)[webView textInputContentView], [webView synchronouslyFocusTextInputContext:frameBField.get() placeCaretAt:[frameBField boundingRect].origin]);
    EXPECT_WK_STREQ("IFRAME", [webView stringByEvaluatingJavaScript:@"document.activeElement.tagName"]);
    EXPECT_WK_STREQ("INPUT", [webView stringByEvaluatingJavaScript:@"document.querySelector('iframe').contentDocument.activeElement.tagName"]);
    EXPECT_WK_STREQ("iframeField", [webView stringByEvaluatingJavaScript:@"document.querySelector('iframe').contentDocument.activeElement.value"]);
    EXPECT_EQ(0, [[webView objectByEvaluatingJavaScript:@"document.querySelector('iframe').contentDocument.activeElement.selectionStart"] intValue]);
    EXPECT_EQ(0, [[webView objectByEvaluatingJavaScript:@"document.querySelector('iframe').contentDocument.activeElement.selectionEnd"] intValue]);
}

TEST(RequestTextInputContext, SwitchFocusBetweenFrames)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setAllowUniversalAccessFromFileURLs:YES];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    auto testPage = applyStyle([NSString stringWithFormat:@"<input type='text' value='mainFrameField' style='width: 100px; height: 50px;'>%@", applyIframe(@"<input type='text' value='iframeField' style='width: 100px; height: 50px;'>")]);
    webViewLoadHTMLStringAndWaitForAllFramesToPaint(webView.get(), testPage);
    NSArray<_WKTextInputContext *> *contexts = [webView synchronouslyRequestTextInputContextsInRect:[webView bounds]];
    EXPECT_EQ(2UL, contexts.count);
    // Note that returned contexts are in hit-test order.
    RetainPtr<_WKTextInputContext> frameAField = contexts[1];
    RetainPtr<_WKTextInputContext> frameBField = contexts[0];

    EXPECT_WK_STREQ("BODY", [webView stringByEvaluatingJavaScript:@"document.activeElement.tagName"]);
    EXPECT_WK_STREQ("BODY", [webView stringByEvaluatingJavaScript:@"document.querySelector('iframe').contentDocument.activeElement.tagName"]);
    EXPECT_EQ((UIResponder<UITextInput> *)[webView textInputContentView], [webView synchronouslyFocusTextInputContext:frameAField.get() placeCaretAt:[frameAField boundingRect].origin]);
    EXPECT_WK_STREQ("INPUT", [webView stringByEvaluatingJavaScript:@"document.activeElement.tagName"]);
    EXPECT_WK_STREQ("mainFrameField", [webView stringByEvaluatingJavaScript:@"document.activeElement.value"]);
    EXPECT_WK_STREQ("BODY", [webView stringByEvaluatingJavaScript:@"document.querySelector('iframe').contentDocument.activeElement.tagName"]);

    EXPECT_EQ((UIResponder<UITextInput> *)[webView textInputContentView], [webView synchronouslyFocusTextInputContext:frameBField.get() placeCaretAt:[frameBField boundingRect].origin]);
    EXPECT_WK_STREQ("IFRAME", [webView stringByEvaluatingJavaScript:@"document.activeElement.tagName"]);
    EXPECT_WK_STREQ("INPUT", [webView stringByEvaluatingJavaScript:@"document.querySelector('iframe').contentDocument.activeElement.tagName"]);
    EXPECT_WK_STREQ("iframeField", [webView stringByEvaluatingJavaScript:@"document.querySelector('iframe').contentDocument.activeElement.value"]);
}

#endif // PLATFORM(IOS_FAMILY)
