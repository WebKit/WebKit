/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#import <WebKit/WebDocumentPrivate.h>
#import <WebKit/DOMHTMLVideoElement.h>
#import <WebKit/DOMPrivate.h>
#import <wtf/RetainPtr.h>

@interface VideoWrapperFrameLoadDelegate : NSObject <WebFrameLoadDelegate> {
}
@end

static bool didFinishLoad;

@implementation VideoWrapperFrameLoadDelegate

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    didFinishLoad = true;
}

@end

namespace TestWebKitAPI {

TEST(WebKitLegacy, DOMHTMLVideoElementWrapper)
{
    RetainPtr<WebView> webView = adoptNS([[WebView alloc] initWithFrame:NSMakeRect(0, 0, 120, 200) frameName:nil groupName:nil]);
    RetainPtr<VideoWrapperFrameLoadDelegate> frameLoadDelegate = adoptNS([VideoWrapperFrameLoadDelegate new]);

    webView.get().frameLoadDelegate = frameLoadDelegate.get();
    [webView.get().mainFrame loadHTMLString:@"" baseURL:[NSURL URLWithString:@"about:blank"]];

    Util::run(&didFinishLoad);

    DOMDocument *document = webView.get().mainFrameDocument;
    DOMElement *video = [document createElement:@"video"];
    EXPECT_TRUE([video isKindOfClass:[DOMHTMLVideoElement class]]);
}

} // namespace TestWebKitAPI
