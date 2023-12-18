/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WebKit.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPIForTesting.h"
#endif

static void insertText(WKWebView *webView, NSString *text)
{
#if PLATFORM(IOS_FAMILY)
    [[webView textInputContentView] insertText:text];
#else
    [webView insertText:text];
#endif
}

TEST(WKWebView, InsertTextWithRangedSelection)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 320)]);
    [webView synchronouslyLoadHTMLString:@"<div id='editor' contenteditable>foo bar baz</div>"];

    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(editor.childNodes[0], 4, editor.childNodes[0], 8)"];
    EXPECT_WK_STREQ("bar ", [webView selectedText]);

    insertText(webView.get(), @"text ");
    EXPECT_WK_STREQ("foo text baz", [webView contentsAsString]);

    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(editor.childNodes[0], 4, editor.childNodes[0], 9)"];
    EXPECT_WK_STREQ("text ", [webView selectedText]);

    insertText(webView.get(), @"");
    EXPECT_WK_STREQ("foo baz", [webView contentsAsString]);
}

TEST(WKWebView, InsertTextWithCaretSelection)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 320)]);
    [webView synchronouslyLoadHTMLString:@"<div id='editor' contenteditable>foo bar baz</div>"];

    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(editor.childNodes[0], 3, editor.childNodes[0], 3)"];
    EXPECT_WK_STREQ("", [webView selectedText]);

    insertText(webView.get(), @" text");
    EXPECT_WK_STREQ("foo text bar baz", [webView contentsAsString]);

    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(editor.childNodes[0], 3, editor.childNodes[0], 3)"];
    EXPECT_WK_STREQ("", [webView selectedText]);

    insertText(webView.get(), @"");
    EXPECT_WK_STREQ("foo text bar baz", [webView contentsAsString]);
}

#if PLATFORM(MAC)

TEST(WKWebView, ShouldHaveInputContextForEditableContent)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadTestPageNamed:@"editable-body"];
    [webView waitForNextPresentationUpdate];

    EXPECT_NOT_NULL([webView inputContext]);
}

#endif // PLATFORM(MAC)
