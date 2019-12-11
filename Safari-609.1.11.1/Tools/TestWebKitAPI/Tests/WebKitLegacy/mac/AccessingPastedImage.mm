/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#import "WTFStringUtilities.h"

#import <WebKit/DOM.h>
#import <WebKit/WebViewPrivate.h>

#if PLATFORM(IOS_FAMILY)
#include <MobileCoreServices/MobileCoreServices.h>
#endif

#if PLATFORM(MAC)

static void writeRTFDToPasteboard(NSData *data)
{
    [[NSPasteboard generalPasteboard] declareTypes:@[NSRTFDPboardType] owner:nil];
    [[NSPasteboard generalPasteboard] setData:data forType:NSRTFDPboardType];
}
#else

@interface NSAttributedString ()
- (id)initWithRTF:(NSData *)data documentAttributes:(NSDictionary **)dict;
- (id)initWithRTFD:(NSData *)data documentAttributes:(NSDictionary **)dict;
- (NSData *)RTFFromRange:(NSRange)range documentAttributes:(NSDictionary *)dict;
- (NSData *)RTFDFromRange:(NSRange)range documentAttributes:(NSDictionary *)dict;
- (BOOL)containsAttachments;
@end

static void writeRTFDToPasteboard(NSData *data)
{
    [[UIPasteboard generalPasteboard] setItems:@[@{ (__bridge NSString *)kUTTypeFlatRTFD : data}]];
}
#endif

@interface SubresourceForBlobURLFrameLoadDelegate : NSObject <WebFrameLoadDelegate, WebUIDelegate> {
}
@end

static bool didFinishLoad = false;
static bool didAlert = false;

@implementation SubresourceForBlobURLFrameLoadDelegate

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    didFinishLoad = true;
}

- (void)webView:(WebView *)sender runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WebFrame *)frame
{
    didAlert = true;
}

@end

namespace TestWebKitAPI {

TEST(WebKitLegacy, AccessingImageInPastedRTFD)
{
    auto webView = adoptNS([[WebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) frameName:nil groupName:nil]);
    auto delegate = adoptNS([[SubresourceForBlobURLFrameLoadDelegate alloc] init]);
    [webView.get() setFrameLoadDelegate:delegate.get()];
    [webView.get() setUIDelegate:delegate.get()];

    auto *mainFrame = webView.get().mainFrame;
    [[webView.get() mainFrame] loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"paste-rtfd" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];
    Util::run(&didFinishLoad);
    [webView.get() setEditable:YES];

    auto *pngData = [NSData dataWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"sunset-in-cupertino-200px" ofType:@"png" inDirectory:@"TestWebKitAPI.resources"]];
    auto attachment = adoptNS([[NSTextAttachment alloc] initWithData:pngData ofType:(__bridge NSString *)kUTTypePNG]);
    NSAttributedString *string = [NSAttributedString attributedStringWithAttachment:attachment.get()];
    NSData *RTFDData = [string RTFDFromRange:NSMakeRange(0, [string length]) documentAttributes:@{ }];

    writeRTFDToPasteboard(RTFDData);
    [webView paste:nil];
    Util::run(&didAlert);

    DOMDocument *document = [mainFrame DOMDocument];
    DOMElement *documentElement = document.documentElement;
    DOMHTMLImageElement *image = (DOMHTMLImageElement *)[documentElement querySelector:@"img"];
    NSURL *url = image.absoluteImageURL;

    EXPECT_EQ(200, image.width);
    EXPECT_WK_STREQ("webkit-fake-url", url.scheme);

    WebResource *webResource = [mainFrame.dataSource subresourceForURL:url];
    EXPECT_TRUE(webResource);
    EXPECT_TRUE(webResource.data);
    EXPECT_WK_STREQ("image/png", webResource.MIMEType);
}

TEST(WebKitLegacy, AccessingImageInPastedWebArchive)
{
    auto sourceWebView = adoptNS([[WebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) frameName:nil groupName:nil]);
    auto destinationWebView = adoptNS([[WebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) frameName:nil groupName:nil]);
    auto delegate = adoptNS([[SubresourceForBlobURLFrameLoadDelegate alloc] init]);
    [sourceWebView.get() setFrameLoadDelegate:delegate.get()];
    [sourceWebView.get() setUIDelegate:delegate.get()];
    [destinationWebView.get() setFrameLoadDelegate:delegate.get()];
    [destinationWebView.get() setUIDelegate:delegate.get()];

    [[sourceWebView.get() mainFrame] loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"image-and-contenteditable" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];
    Util::run(&didFinishLoad);
    [sourceWebView.get() setEditable:YES];

    didFinishLoad = false;
    [[destinationWebView.get() mainFrame] loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"paste-rtfd" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];
    Util::run(&didFinishLoad);
    [destinationWebView.get() setEditable:YES];

    [sourceWebView stringByEvaluatingJavaScriptFromString:@"document.body.focus(); document.execCommand('selectAll');"];
    [sourceWebView copy:nil];

    [destinationWebView paste:nil];
    Util::run(&didAlert);

    auto *mainFrame = destinationWebView.get().mainFrame;
    DOMDocument *document = [mainFrame DOMDocument];
    DOMElement *documentElement = document.documentElement;
    DOMHTMLImageElement *image = (DOMHTMLImageElement *)[documentElement querySelector:@"img"];
    NSURL *url = image.absoluteImageURL;

    EXPECT_EQ(200, image.width);
    EXPECT_WK_STREQ("file", url.scheme);

    WebResource *webResource = [mainFrame.dataSource subresourceForURL:url];
    EXPECT_TRUE(webResource);
    EXPECT_TRUE(webResource.data);
    EXPECT_WK_STREQ("image/png", webResource.MIMEType);
}

} // namespace TestWebKitAPI
