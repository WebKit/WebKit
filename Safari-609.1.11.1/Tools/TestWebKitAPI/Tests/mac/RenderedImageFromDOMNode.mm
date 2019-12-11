/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#import <WebKit/DOMPrivate.h>
#import <wtf/RetainPtr.h>

@interface RenderedImageFromDOMNodeFrameLoadDelegate : NSObject <WebFrameLoadDelegate> {
}
@end

static bool didFinishLoad;

@implementation RenderedImageFromDOMNodeFrameLoadDelegate

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    didFinishLoad = true;
}

@end

namespace TestWebKitAPI {

TEST(WebKitLegacy, RenderedImageFromDOMNode)
{
    RetainPtr<WebView> webView = adoptNS([[WebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) frameName:nil groupName:nil]);
    RetainPtr<RenderedImageFromDOMNodeFrameLoadDelegate> frameLoadDelegate = adoptNS([RenderedImageFromDOMNodeFrameLoadDelegate new]);

    [webView setFrameLoadDelegate:frameLoadDelegate.get()];
    [[webView mainFrame] loadHTMLString:@"<p><span id=\"t1\" style=\"vertical-align: bottom;\">Span</span></p><p><span id=\"t2\">Span</span></p>" baseURL:[NSURL URLWithString:@"about:blank"]];

    Util::run(&didFinishLoad);

    DOMDocument *document = [webView mainFrameDocument];
    NSImage *image1 = [[document getElementById:@"t1"] renderedImage];
    NSImage *image2 = [[document getElementById:@"t2"] renderedImage];

    EXPECT_TRUE([image1.TIFFRepresentation isEqual:image2.TIFFRepresentation]);
}

} // namespace TestWebKitAPI
