/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include "PlatformUtilities.h"
#include "PlatformWebView.h"
#include <wtf/RetainPtr.h>

#import <WebKit/DOM.h>
#import <WebKit/WebViewPrivate.h>

@interface SetDocumentURITest : NSObject <WebFrameLoadDelegate> {
}
@end

static bool didFinishLoad;

@implementation SetDocumentURITest

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    didFinishLoad = true;
}
@end

namespace TestWebKitAPI {

TEST(WebKitLegacy, SetDocumentURITestFile)
{
    RetainPtr<WebView> webView = adoptNS([[WebView alloc] initWithFrame:NSMakeRect(0, 0, 120, 200) frameName:nil groupName:nil]);
    RetainPtr<SetDocumentURITest> testController = adoptNS([SetDocumentURITest new]);
    webView.get().frameLoadDelegate = testController.get();
    [[webView.get() mainFrame] loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"SetDocumentURI" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];
    Util::run(&didFinishLoad);
    didFinishLoad = false;
    DOMDocument *document = webView.get().mainFrameDocument;

    [document setDocumentURI:@"file:///test"];
    // documentURI set correctly.
    EXPECT_WK_STREQ(@"file:///test", [document documentURI]);
    // baseURI follows along.
    EXPECT_WK_STREQ(@"file:///test", [document baseURI]);
}

TEST(WebKitLegacy, SetDocumentURITestURL)
{
    RetainPtr<WebView> webView = adoptNS([[WebView alloc] initWithFrame:NSMakeRect(0, 0, 120, 200) frameName:nil groupName:nil]);
    RetainPtr<SetDocumentURITest> testController = adoptNS([SetDocumentURITest new]);
    webView.get().frameLoadDelegate = testController.get();
    [[webView.get() mainFrame] loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"SetDocumentURI" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];
    Util::run(&didFinishLoad);
    didFinishLoad = false;
    DOMDocument *document = webView.get().mainFrameDocument;

    [document setDocumentURI:@"http://example.com/"];
    // documentURI set correctly.
    EXPECT_WK_STREQ(@"http://example.com/", [document documentURI]);
    // baseURI follows along.
    EXPECT_WK_STREQ(@"http://example.com/", [document baseURI]);
    // Relative links too.
    NSString *result = [webView.get() stringByEvaluatingJavaScriptFromString:@"document.getElementById('relative').href"];
    EXPECT_WK_STREQ(@"http://example.com/relativeURL.html", result);
}

TEST(WebKitLegacy, SetDocumentURITestString)
{
    RetainPtr<WebView> webView = adoptNS([[WebView alloc] initWithFrame:NSMakeRect(0, 0, 120, 200) frameName:nil groupName:nil]);
    RetainPtr<SetDocumentURITest> testController = adoptNS([SetDocumentURITest new]);
    webView.get().frameLoadDelegate = testController.get();
    [[webView.get() mainFrame] loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"SetDocumentURI" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];
    Util::run(&didFinishLoad);
    didFinishLoad = false;
    DOMDocument *document = webView.get().mainFrameDocument;

    [document setDocumentURI:@"A non-URL string."];
    // documentURI accepts random strings.
    EXPECT_WK_STREQ(@"A non-URL string.", [document documentURI]);
    EXPECT_WK_STREQ(@"about:blank", [document baseURI]);
}

TEST(WebKitLegacy, SetDocumentURITestNull)
{
    RetainPtr<WebView> webView = adoptNS([[WebView alloc] initWithFrame:NSMakeRect(0, 0, 120, 200) frameName:nil groupName:nil]);
    RetainPtr<SetDocumentURITest> testController = adoptNS([SetDocumentURITest new]);
    webView.get().frameLoadDelegate = testController.get();
    [[webView.get() mainFrame] loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"SetDocumentURI" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];
    Util::run(&didFinishLoad);
    didFinishLoad = false;
    DOMDocument *document = webView.get().mainFrameDocument;

    [document setDocumentURI:nil];
    // documenturi is empty.
    EXPECT_WK_STREQ(@"", [document documentURI]);
    EXPECT_WK_STREQ(@"about:blank", [document baseURI]);
}

} // namespace TestWebKitAPI
