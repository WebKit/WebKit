/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "config.h"

#if PLATFORM(IOS_FAMILY)

#import "PlatformUtilities.h"
#import "TestInputDelegate.h"
#import "TestWKWebView.h"
#import "UIKitSPI.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/unicode/CharacterNames.h>

namespace TestWebKitAPI {

TEST(InsertTextAlternatives, Simple)
{
    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration]);
    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[] (WKWebView *, id <_WKFocusedElementInfo>) { return _WKFocusStartsInputSessionPolicyAllow; }];
    [webView _setInputDelegate:inputDelegate.get()];

    [webView synchronouslyLoadHTMLString:@"<body contenteditable='true'>A big&nbsp;</body>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.body.focus()"];
    [webView _synchronouslyExecuteEditCommand:@"MoveToEndOfDocument" argument:nil];
    [[webView textInputContentView] insertText:@"hello" alternatives:@[@"yellow"] style:UITextAlternativeStyleNone];
    [webView _synchronouslyExecuteEditCommand:@"MoveToBeginningOfDocument" argument:nil];
    [webView waitForNextPresentationUpdate];

    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"internals.hasDictationAlternativesMarker(0, 1)"] boolValue]); // A
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"internals.hasDictationAlternativesMarker(2, 3)"] boolValue]); // big
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"internals.hasDictationAlternativesMarker(0, 6)"] boolValue]); // "A big "
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"internals.hasDictationAlternativesMarker(6, 5)"] boolValue]); // hello

    [webView objectByEvaluatingJavaScript:@"getSelection().setPosition(document.body.firstChild, 8)"]; // in the middle of "hello"
    [webView waitForNextPresentationUpdate];

    EXPECT_WK_STREQ("hello", [[webView textInputContentView] selectedText]);

    auto *alternatives = [(id<UIWKInteractionViewProtocol_Staging66646042>)[webView textInputContentView] alternativesForSelectedText];
    EXPECT_EQ(1ul, alternatives.count);
    EXPECT_WK_STREQ("hello", alternatives[0].primaryString);
    EXPECT_EQ(1ul, alternatives[0].alternativeStrings.count);
    EXPECT_WK_STREQ("yellow", alternatives[0].alternativeStrings[0]);
    EXPECT_FALSE(alternatives[0].isLowConfidence);
}

TEST(InsertTextAlternatives, InsertLeadingSpace)
{
    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration]);
    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[] (WKWebView *, id <_WKFocusedElementInfo>) { return _WKFocusStartsInputSessionPolicyAllow; }];
    [webView _setInputDelegate:inputDelegate.get()];

    [webView synchronouslyLoadHTMLString:@"<body contenteditable='true'></body>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.body.focus()"];
    [[webView textInputContentView] insertText:@"hello" alternatives:@[@"yellow"] style:UITextAlternativeStyleNone];
    [webView waitForNextPresentationUpdate];

    [webView _synchronouslyExecuteEditCommand:@"MoveToBeginningOfDocument" argument:nil];
    [[webView textInputContentView] insertText:@" "];
    [webView _synchronouslyExecuteEditCommand:@"MoveToBeginningOfDocument" argument:nil];
    [webView waitForNextPresentationUpdate];

    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"internals.hasDictationAlternativesMarker(0, 1)"] boolValue]); // <space>
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"internals.hasDictationAlternativesMarker(1, 5)"] boolValue]); // hello
}

TEST(InsertTextAlternatives, InsertLeadingNewline)
{
    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration]);
    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[] (WKWebView *, id <_WKFocusedElementInfo>) { return _WKFocusStartsInputSessionPolicyAllow; }];
    [webView _setInputDelegate:inputDelegate.get()];

    // Set CSS white-space and user-modify so that \n is inserted literally. Otherwise, it would be converted into a <br> if <body>
    // only had "contenteditable='true'" because it is considered richly editable.
    [webView synchronouslyLoadHTMLString:@"<body style='white-space: pre; -webkit-user-modify: read-write-plaintext-only'></body>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.body.focus()"];
    [[webView textInputContentView] insertText:@"hello" alternatives:@[@"yellow"] style:UITextAlternativeStyleNone];
    [webView _synchronouslyExecuteEditCommand:@"MoveToBeginningOfDocument" argument:nil];
    [[webView textInputContentView] insertText:@"\n"];
    [webView _synchronouslyExecuteEditCommand:@"MoveToBeginningOfDocument" argument:nil];
    [webView waitForNextPresentationUpdate];

    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"internals.hasDictationAlternativesMarker(0, 1)"] boolValue]); // \n
    [webView _synchronouslyExecuteEditCommand:@"MoveDown" argument:nil];
    [webView waitForNextPresentationUpdate];
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"internals.hasDictationAlternativesMarker(0, 5)"] boolValue]); // hello
}

static inline NSString *makeNSStringWithCharacter(unichar c)
{
    return [NSString stringWithCharacters:&c length:1];
};

TEST(InsertTextAlternatives, InsertLeadingNoBreakSpace_ExpectedFailure)
{
    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration]);
    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[] (WKWebView *, id <_WKFocusedElementInfo>) { return _WKFocusStartsInputSessionPolicyAllow; }];
    [webView _setInputDelegate:inputDelegate.get()];

    [webView synchronouslyLoadHTMLString:@"<body contenteditable='true'></body>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.body.focus()"];
    [[webView textInputContentView] insertText:@"hello" alternatives:@[@"yellow"] style:UITextAlternativeStyleNone];
    [webView waitForNextPresentationUpdate];

    [webView _synchronouslyExecuteEditCommand:@"MoveToBeginningOfDocument" argument:nil];
    [[webView textInputContentView] insertText:makeNSStringWithCharacter(noBreakSpace)];
    [webView _synchronouslyExecuteEditCommand:@"MoveToBeginningOfDocument" argument:nil];
    [webView waitForNextPresentationUpdate];

    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"internals.hasDictationAlternativesMarker(0, 1)"] boolValue]); // <no-break space>
    // FIXME: Change this to EXPECT_TRUE() once leading no-break spaces do not cause removal of alternatives.
    // See <https://webkit.org/b/212098>.
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"internals.hasDictationAlternativesMarker(1, 5)"] boolValue]); // hello
}

TEST(InsertTextAlternatives, InsertTrailingSpace)
{
    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration]);
    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[] (WKWebView *, id <_WKFocusedElementInfo>) { return _WKFocusStartsInputSessionPolicyAllow; }];
    [webView _setInputDelegate:inputDelegate.get()];

    // Set CSS white-space so that ' ' is inserted literally. Otherwise, it would be converted into a no-break space.
    [webView synchronouslyLoadHTMLString:@"<body contenteditable='true' style='white-space: pre'></body>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.body.focus()"];
    [[webView textInputContentView] insertText:@"hello" alternatives:@[@"yellow"] style:UITextAlternativeStyleNone];
    [[webView textInputContentView] insertText:@" "];
    [webView _synchronouslyExecuteEditCommand:@"MoveToBeginningOfDocument" argument:nil];
    [webView waitForNextPresentationUpdate];

    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"internals.hasDictationAlternativesMarker(0, 5)"] boolValue]); // hello
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"internals.hasDictationAlternativesMarker(5, 1)"] boolValue]); // <space>
}

TEST(InsertTextAlternatives, InsertTrailingSpaceWhitespaceRebalance)
{
    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration]);
    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[] (WKWebView *, id <_WKFocusedElementInfo>) { return _WKFocusStartsInputSessionPolicyAllow; }];
    [webView _setInputDelegate:inputDelegate.get()];

    [webView synchronouslyLoadHTMLString:@"<body contenteditable='true'></body>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.body.focus()"];
    [[webView textInputContentView] insertText:@"hello" alternatives:@[@"yellow"] style:UITextAlternativeStyleNone];
    [[webView textInputContentView] insertText:@" "];
    [webView waitForNextPresentationUpdate];

    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"internals.hasDictationAlternativesMarker(0, 5)"] boolValue]); // hello
}

TEST(InsertTextAlternatives, InsertTrailingNewline)
{
    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration]);
    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[] (WKWebView *, id <_WKFocusedElementInfo>) { return _WKFocusStartsInputSessionPolicyAllow; }];
    [webView _setInputDelegate:inputDelegate.get()];

    // Set CSS white-space and user-modify so that \n is inserted literally. Otherwise, it would be converted into a <br> if <body>
    // only had "contenteditable='true'" because it is considered richly editable.
    [webView synchronouslyLoadHTMLString:@"<body style='white-space: pre; -webkit-user-modify: read-write-plaintext-only'></body>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.body.focus()"];
    [[webView textInputContentView] insertText:@"hello" alternatives:@[@"yellow"] style:UITextAlternativeStyleNone];
    [[webView textInputContentView] insertText:@"\n"];
    [webView _synchronouslyExecuteEditCommand:@"MoveToBeginningOfDocument" argument:nil];
    [webView waitForNextPresentationUpdate];

    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"internals.hasDictationAlternativesMarker(0, 5)"] boolValue]); // hello
    [webView _synchronouslyExecuteEditCommand:@"MoveDown" argument:nil];
    [webView waitForNextPresentationUpdate];
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"internals.hasDictationAlternativesMarker(0, 1)"] boolValue]); // \n
}

TEST(InsertTextAlternatives, InsertTrailingNoBreakSpace_ExpectedFailure)
{
    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration]);
    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[] (WKWebView *, id <_WKFocusedElementInfo>) { return _WKFocusStartsInputSessionPolicyAllow; }];
    [webView _setInputDelegate:inputDelegate.get()];

    [webView synchronouslyLoadHTMLString:@"<body contenteditable='true'></body>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.body.focus()"];
    [[webView textInputContentView] insertText:@"hello" alternatives:@[@"yellow"] style:UITextAlternativeStyleNone];
    [[webView textInputContentView] insertText:makeNSStringWithCharacter(noBreakSpace)];
    [webView _synchronouslyExecuteEditCommand:@"MoveToBeginningOfDocument" argument:nil];
    [webView waitForNextPresentationUpdate];

    // FIXME: Change first EXPECT to EXPECT_TRUE() once trailing no-break spaces do not cause removal of alternatives.
    // See <https://webkit.org/b/212098>.
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"internals.hasDictationAlternativesMarker(0, 5)"] boolValue]); // hello
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"internals.hasDictationAlternativesMarker(5, 1)"] boolValue]); // <no-break space>
}

TEST(InsertTextAlternatives, InsertSpaceInMiddle)
{
    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration]);
    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[] (WKWebView *, id <_WKFocusedElementInfo>) { return _WKFocusStartsInputSessionPolicyAllow; }];
    [webView _setInputDelegate:inputDelegate.get()];

    [webView synchronouslyLoadHTMLString:@"<body contenteditable='true'></body>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.body.focus()"];
    [[webView textInputContentView] insertText:@"hello" alternatives:@[@"yellow"] style:UITextAlternativeStyleNone];
    [webView waitForNextPresentationUpdate];

    [webView stringByEvaluatingJavaScript:@"getSelection().setPosition(document.body.firstChild, 2)"];
    [[webView textInputContentView] insertText:@" "];
    [webView _synchronouslyExecuteEditCommand:@"MoveToBeginningOfDocument" argument:nil];
    [webView waitForNextPresentationUpdate];

    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"internals.hasDictationAlternativesMarker(0, 2)"] boolValue]); // he
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"internals.hasDictationAlternativesMarker(2, 1)"] boolValue]); // <space>
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"internals.hasDictationAlternativesMarker(3, 3)"] boolValue]); // llo
}

TEST(InsertTextAlternatives, InsertNewlineInMiddle_ExpectedFailure)
{
    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration]);
    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[] (WKWebView *, id <_WKFocusedElementInfo>) { return _WKFocusStartsInputSessionPolicyAllow; }];
    [webView _setInputDelegate:inputDelegate.get()];

    // Set CSS white-space and user-modify so that \n is inserted literally. Otherwise, it would be converted into a <br> if <body>
    // only had "contenteditable='true'" because it is considered richly editable.
    [webView synchronouslyLoadHTMLString:@"<body style='white-space: pre; -webkit-user-modify: read-write-plaintext-only'></body>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.body.focus()"];
    [[webView textInputContentView] insertText:@"hello" alternatives:@[@"yellow"] style:UITextAlternativeStyleNone];
    [webView waitForNextPresentationUpdate];

    [webView stringByEvaluatingJavaScript:@"getSelection().setPosition(document.body.firstChild, 2)"];
    [[webView textInputContentView] insertText:@"\n"];
    [webView _synchronouslyExecuteEditCommand:@"MoveToBeginningOfDocument" argument:nil];
    [webView waitForNextPresentationUpdate];

    // FIXME: These results coincidentally match UIKit, but they just feel weird: they are asymmetric to
    // what happens when a space is inserted in the middle. Marker should be removed when marker is split.
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"internals.hasDictationAlternativesMarker(0, 2)"] boolValue]); // he
    [webView _synchronouslyExecuteEditCommand:@"MoveDown" argument:nil];
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"internals.hasDictationAlternativesMarker(0, 3)"] boolValue]); // llo
}

TEST(InsertTextAlternatives, InsertNoBreakSpaceInMiddle)
{
    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration]);
    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[] (WKWebView *, id <_WKFocusedElementInfo>) { return _WKFocusStartsInputSessionPolicyAllow; }];
    [webView _setInputDelegate:inputDelegate.get()];

    // Set CSS white-space and user-modify so that \n is inserted literally. Otherwise, it would be converted into a <br> if <body>
    // only had "contenteditable='true'" because it is considered richly editable.
    [webView synchronouslyLoadHTMLString:@"<body style='white-space: pre; -webkit-user-modify: read-write-plaintext-only'></body>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.body.focus()"];
    [[webView textInputContentView] insertText:@"hello" alternatives:@[@"yellow"] style:UITextAlternativeStyleNone];
    [webView waitForNextPresentationUpdate];

    [webView stringByEvaluatingJavaScript:@"getSelection().setPosition(document.body.firstChild, 2)"];
    [[webView textInputContentView] insertText:makeNSStringWithCharacter(noBreakSpace)];
    [webView _synchronouslyExecuteEditCommand:@"MoveToBeginningOfDocument" argument:nil];
    [webView waitForNextPresentationUpdate];

    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"internals.hasDictationAlternativesMarker(0, 2)"] boolValue]); // he
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"internals.hasDictationAlternativesMarker(2, 1)"] boolValue]); // <no-break space>
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"internals.hasDictationAlternativesMarker(3, 3)"] boolValue]); // llo
}

TEST(InsertTextAlternatives, InsertLeadingNonWhitespaceCharacter)
{
    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration]);
    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[] (WKWebView *, id <_WKFocusedElementInfo>) { return _WKFocusStartsInputSessionPolicyAllow; }];
    [webView _setInputDelegate:inputDelegate.get()];

    [webView synchronouslyLoadHTMLString:@"<body contenteditable='true'></body>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.body.focus()"];
    [[webView textInputContentView] insertText:@"hello" alternatives:@[@"yellow"] style:UITextAlternativeStyleNone];
    [webView waitForNextPresentationUpdate];

    [webView _synchronouslyExecuteEditCommand:@"MoveToBeginningOfDocument" argument:nil];
    [[webView textInputContentView] insertText:@"a"];
    [webView _synchronouslyExecuteEditCommand:@"MoveToBeginningOfDocument" argument:nil];
    [webView waitForNextPresentationUpdate];

    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"internals.hasDictationAlternativesMarker(0, 1)"] boolValue]); // a
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"internals.hasDictationAlternativesMarker(1, 5)"] boolValue]); // hello
}

TEST(InsertTextAlternatives, InsertTrailingNonWhitespaceCharacter)
{
    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration]);
    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[] (WKWebView *, id <_WKFocusedElementInfo>) { return _WKFocusStartsInputSessionPolicyAllow; }];
    [webView _setInputDelegate:inputDelegate.get()];

    [webView synchronouslyLoadHTMLString:@"<body contenteditable='true'></body>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.body.focus()"];
    [[webView textInputContentView] insertText:@"hello" alternatives:@[@"yellow"] style:UITextAlternativeStyleNone];
    [[webView textInputContentView] insertText:@"a"];
    [webView _synchronouslyExecuteEditCommand:@"MoveToBeginningOfDocument" argument:nil];
    [webView waitForNextPresentationUpdate];

    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"internals.hasDictationAlternativesMarker(0, 5)"] boolValue]); // hello
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"internals.hasDictationAlternativesMarker(5, 1)"] boolValue]); // a
}

TEST(InsertTextAlternatives, InsertNonWhitespaceCharacterInMiddle)
{
    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration]);
    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[] (WKWebView *, id <_WKFocusedElementInfo>) { return _WKFocusStartsInputSessionPolicyAllow; }];
    [webView _setInputDelegate:inputDelegate.get()];

    [webView synchronouslyLoadHTMLString:@"<body contenteditable='true'></body>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.body.focus()"];
    [[webView textInputContentView] insertText:@"hello" alternatives:@[@"yellow"] style:UITextAlternativeStyleNone];
    [webView waitForNextPresentationUpdate];

    [webView stringByEvaluatingJavaScript:@"getSelection().setPosition(document.body.firstChild, 2)"];
    [[webView textInputContentView] insertText:@"a"];
    [webView _synchronouslyExecuteEditCommand:@"MoveToBeginningOfDocument" argument:nil];
    [webView waitForNextPresentationUpdate];

    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"internals.hasDictationAlternativesMarker(0, 2)"] boolValue]); // he
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"internals.hasDictationAlternativesMarker(2, 1)"] boolValue]); // a
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"internals.hasDictationAlternativesMarker(3, 3)"] boolValue]); // llo
}

TEST(InsertTextAlternatives, InsertMultipleWordsWithAlternatives)
{
    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration]);
    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[] (WKWebView *, id <_WKFocusedElementInfo>) { return _WKFocusStartsInputSessionPolicyAllow; }];
    [webView _setInputDelegate:inputDelegate.get()];

    // Set CSS white-space so that ' ' is inserted literally. Otherwise, it would be converted into a no-break space.
    [webView synchronouslyLoadHTMLString:@"<body contenteditable='true' style='white-space: pre'></body>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.body.focus()"];
    [[webView textInputContentView] insertText:@"hello" alternatives:@[@"yellow"] style:UITextAlternativeStyleNone];
    [[webView textInputContentView] insertText:@" "];
    [[webView textInputContentView] insertText:@"worlds" alternatives:@[@"worm"] style:UITextAlternativeStyleNone];
    [webView _synchronouslyExecuteEditCommand:@"MoveToBeginningOfDocument" argument:nil];
    [webView waitForNextPresentationUpdate];

    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"internals.hasDictationAlternativesMarker(0, 5)"] boolValue]); // hello
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"internals.hasDictationAlternativesMarker(5, 1)"] boolValue]); // <space>
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"internals.hasDictationAlternativesMarker(6, 5)"] boolValue]); // worlds
}

} // namespace TestWebKitAPI

#endif // PLATFORM(IOS_FAMILY)

