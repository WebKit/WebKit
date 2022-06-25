/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#import <WebKit/WebViewPrivate.h>
#import <wtf/RetainPtr.h>

@interface WordBoundaryTypingAttributesController : NSObject <WebFrameLoadDelegate> {
}
@end

static bool didFinishLoad;

@implementation WordBoundaryTypingAttributesController

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    didFinishLoad = true;
}

@end

namespace TestWebKitAPI {

TEST(WebKitLegacy, WordBoundaryTypingAttributes)
{
    RetainPtr<WebView> webView = adoptNS([[WebView alloc] initWithFrame:NSMakeRect(0, 0, 120, 200) frameName:nil groupName:nil]);
    RetainPtr<WordBoundaryTypingAttributesController> controller = adoptNS([WordBoundaryTypingAttributesController new]);

    NSString *testContent =
        @"<div contenteditable id='content'>Hello, world!</div> \
          <script> \
            var node = document.getElementById('content').firstChild; \
            getSelection().setBaseAndExtent(node, 7, node, 12); \
            document.execCommand('underline'); \
          </script>";

    webView.get().frameLoadDelegate = controller.get();
    [webView.get().mainFrame loadHTMLString:testContent baseURL:[NSURL URLWithString:@"about:blank"]];

    Util::run(&didFinishLoad);

    NSDictionary *attributes = [(id)webView.get() typingAttributes];
    EXPECT_TRUE([attributes[NSUnderlineStyleAttributeName] intValue] != 0);
}

} // namespace TestWebKitAPI
