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
#import "TestWKWebView.h"
#import <WebKit/WebKit.h>
#import <WebKit/WebKitPrivate.h>
#import <wtf/RetainPtr.h>

#if WK_API_ENABLED

static RetainPtr<TestWKWebView> webViewForTestingAttachments()
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setAttachmentElementEnabled:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    [webView synchronouslyLoadHTMLString:@"<body contenteditable></body><script>document.body.focus();</script>"];

    return webView;
}

static NSData *testHTMLData()
{
    return [@"<a href='#'>This is some HTML data</a>" dataUsingEncoding:NSUTF8StringEncoding];
}

static NSData *testImageData()
{
    NSURL *url = [[NSBundle mainBundle] URLForResource:@"icon" withExtension:@"png" subdirectory:@"TestWebKitAPI.resources"];
    return [NSData dataWithContentsOfURL:url];
}

@implementation TestWKWebView (AttachmentTesting)

- (_WKAttachment *)synchronouslyInsertAttachmentWithFilename:(NSString *)filename contentType:(NSString *)contentType data:(NSData *)data options:(_WKAttachmentDisplayOptions *)options
{
    __block bool done = false;
    RetainPtr<_WKAttachment> attachment = [self _insertAttachmentWithFilename:filename contentType:contentType data:data options:options completion:^(BOOL) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    return attachment.autorelease();
}

- (NSString *)valueOfAttribute:(NSString *)attributeName forQuerySelector:(NSString *)querySelector
{
    return [self stringByEvaluatingJavaScript:[NSString stringWithFormat:@"document.querySelector('%@').getAttribute('%@')", querySelector, attributeName]];
}

@end

namespace TestWebKitAPI {

TEST(WKAttachmentTests, AttachmentElementInsertion)
{
    auto webView = webViewForTestingAttachments();

    // Use the given content type for the attachment element's type.
    [webView synchronouslyInsertAttachmentWithFilename:@"foo" contentType:@"text/html" data:testHTMLData() options:nil];
    EXPECT_WK_STREQ(@"foo", [webView valueOfAttribute:@"title" forQuerySelector:@"attachment"]);
    EXPECT_WK_STREQ(@"text/html", [webView valueOfAttribute:@"type" forQuerySelector:@"attachment"]);
    EXPECT_WK_STREQ(@"38 bytes", [webView valueOfAttribute:@"subtitle" forQuerySelector:@"attachment"]);

    // Since no content type is explicitly specified, compute it from the file extension.
    [webView _executeEditCommand:@"DeleteBackward" argument:nil completion:nil];
    [webView synchronouslyInsertAttachmentWithFilename:@"bar.png" contentType:nil data:testImageData() options:nil];
    EXPECT_WK_STREQ(@"bar.png", [webView valueOfAttribute:@"title" forQuerySelector:@"attachment"]);
    EXPECT_WK_STREQ(@"image/png", [webView valueOfAttribute:@"type" forQuerySelector:@"attachment"]);
    EXPECT_WK_STREQ(@"37 KB", [webView valueOfAttribute:@"subtitle" forQuerySelector:@"attachment"]);
}

} // namespace TestWebKitAPI

#endif // WK_API_ENABLED
