/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#import "PlatformWebView.h"
#import "Test.h"
#import "WebKitAgnosticTest.h"
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

static NSAttributedString *attributedString(WebView *webView, NSRange range)
{
    return [(NSView <NSTextInput> *)[[[webView mainFrame] frameView] documentView] attributedSubstringFromRange:range];
}

class AttributedStringTest_CustomFont : public WebKitAgnosticTest {
public:
    template <typename View> void runSyncTest(View);

    // WebKitAgnosticTest
    virtual void didLoadURL(WebView *webView) { runSyncTest(webView); }
    // FIXME: Reimplement the test using async NSTextInputClient interface.
    virtual void didLoadURL(WKView *wkView) { }

    virtual NSURL *url() const { return [[NSBundle mainBundle] URLForResource:@"attributedStringCustomFont" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]; }
};

template <typename View>
void AttributedStringTest_CustomFont::runSyncTest(View view)
{
    NSAttributedString *attrString = attributedString(view, NSMakeRange(0, 5));
    EXPECT_WK_STREQ("Lorem", [attrString string]);
}

TEST_F(AttributedStringTest_CustomFont, WebKit)
{
    runWebKit1Test();
}

class AttributedStringTest_Strikethrough : public WebKitAgnosticTest {
public:
    template <typename View> void runSyncTest(View);

    // WebKitAgnosticTest
    virtual void didLoadURL(WebView *webView) { runSyncTest(webView); }
    // FIXME: Reimplement the test using async NSTextInputClient interface.
    virtual void didLoadURL(WKView *wkView) { }

    virtual NSURL *url() const { return [[NSBundle mainBundle] URLForResource:@"attributedStringStrikethrough" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]; }
};

template <typename View>
void AttributedStringTest_Strikethrough::runSyncTest(View view)
{
    NSAttributedString *attrString = attributedString(view, NSMakeRange(0, 5));

    EXPECT_WK_STREQ("Lorem", [attrString string]);
    
    NSDictionary *attributes = [attrString attributesAtIndex:0 effectiveRange:0];
    ASSERT_NOT_NULL([attributes objectForKey:NSStrikethroughStyleAttributeName]);
    ASSERT_TRUE([[attributes objectForKey:NSStrikethroughStyleAttributeName] isKindOfClass:[NSNumber class]]);
    ASSERT_EQ(NSUnderlineStyleSingle, [(NSNumber *)[attributes objectForKey:NSStrikethroughStyleAttributeName] intValue]);
}

TEST_F(AttributedStringTest_Strikethrough, WebKit)
{
    runWebKit1Test();
}

class AttributedStringTest_NewlineAtEndOfDocument : public WebKitAgnosticTest {
public:
    template <typename View> void runSyncTest(View);

    virtual void didLoadURL(WebView *webView) { runSyncTest(webView); }
    virtual void didLoadURL(WKView *wkView) { }

    virtual NSURL *url() const { return [[NSBundle mainBundle] URLForResource:@"attributedStringNewlineAtEndOfDocument" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]; }
};

template <typename View>
void AttributedStringTest_NewlineAtEndOfDocument::runSyncTest(View view)
{
    EXPECT_WK_STREQ("a\n", attributedString(view, NSMakeRange(0, 2)).string);
}

TEST_F(AttributedStringTest_NewlineAtEndOfDocument, WebKit)
{
    runWebKit1Test();
}

} // namespace TestWebKitAPI
