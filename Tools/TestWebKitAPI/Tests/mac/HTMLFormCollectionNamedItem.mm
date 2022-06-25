/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#import "config.h"
#import "PlatformUtilities.h"
#import "PlatformWebView.h"
#import <wtf/RetainPtr.h>

#import <WebKit/DOM.h>
#import <WebKit/WebViewPrivate.h>

@interface HTMLFormCollectionNamedItemTest : NSObject <WebFrameLoadDelegate> {
}
@end

static bool didFinishLoad;

@implementation HTMLFormCollectionNamedItemTest

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    didFinishLoad = true;
}
@end

namespace TestWebKitAPI {

TEST(WebKitLegacy, HTMLFormCollectionNamedItemTest)
{
    RetainPtr<WebView> webView = adoptNS([[WebView alloc] initWithFrame:NSMakeRect(0, 0, 120, 200) frameName:nil groupName:nil]);
    RetainPtr<HTMLFormCollectionNamedItemTest> testController = adoptNS([HTMLFormCollectionNamedItemTest new]);

    webView.get().frameLoadDelegate = testController.get();
    [[webView.get() mainFrame] loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle]
        URLForResource:@"HTMLFormCollectionNamedItem" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];

    Util::run(&didFinishLoad);
    didFinishLoad = false;

    DOMDocument *document = webView.get().mainFrameDocument;
    DOMHTMLFormElement *form = (DOMHTMLFormElement *)[document querySelector:@"form"];
    RetainPtr<DOMHTMLCollection> collection = [form elements];

    EXPECT_EQ([collection.get() length], (unsigned)2);
    EXPECT_WK_STREQ([(DOMHTMLInputElement *)[collection item:0] value], @"firstItem");
    EXPECT_WK_STREQ([(DOMHTMLInputElement *)[collection item:1] value], @"secondItem");
    EXPECT_WK_STREQ([(DOMHTMLInputElement *)[collection namedItem:@"nameForTwoTextFields"] value], @"firstItem");
    EXPECT_WK_STREQ([(DOMHTMLInputElement *)[collection item:1] value], @"secondItem");
    EXPECT_WK_STREQ([(DOMHTMLInputElement *)[collection item:0] value], @"firstItem");
}

} // namespace TestWebKitAPI
