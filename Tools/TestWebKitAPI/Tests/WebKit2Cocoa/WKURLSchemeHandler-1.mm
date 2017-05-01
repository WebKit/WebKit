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
#import "Test.h"
#import <WebKit/WKURLSchemeHandler.h>
#import <WebKit/WKURLSchemeTask.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WebKit.h>
#import <wtf/RetainPtr.h>

#if WK_API_ENABLED

static bool receivedScriptMessage;

@interface SchemeHandler : NSObject <WKURLSchemeHandler>
@property (readonly) NSMutableArray<NSURL *> *startedURLs;
@property (readonly) NSMutableArray<NSURL *> *stoppedURLs;
- (instancetype)initWithData:(NSData *)data mimeType:(NSString *)inMIMEType;
@end

@implementation SchemeHandler {
    RetainPtr<NSData> resourceData;
    RetainPtr<NSString> mimeType;
}

- (instancetype)initWithData:(NSData *)data mimeType:(NSString *)inMIMEType
{
    self = [super init];
    if (!self)
        return nil;

    resourceData = data;
    mimeType = inMIMEType;
    _startedURLs = [[NSMutableArray alloc] init];
    _stoppedURLs = [[NSMutableArray alloc] init];

    return self;
}

- (void)dealloc
{
    [_startedURLs release];
    [_stoppedURLs release];
    [super dealloc];
}

- (void)webView:(WKWebView *)webView startURLSchemeTask:(id <WKURLSchemeTask>)task
{
    [_startedURLs addObject:task.request.URL];

    // Always fail the image load.
    if ([task.request.URL.absoluteString isEqualToString:@"testing:image"]) {
        [task didFailWithError:[NSError errorWithDomain:@"TestWebKitAPI" code:1 userInfo:nil]];
        receivedScriptMessage = true;
        return;
    }

    RetainPtr<NSURLResponse> response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:mimeType.get() expectedContentLength:1 textEncodingName:nil]);
    [task didReceiveResponse:response.get()];
    [task didReceiveData:resourceData.get()];
    [task didFinish];
}

- (void)webView:(WKWebView *)webView stopURLSchemeTask:(id <WKURLSchemeTask>)task
{
    [_stoppedURLs addObject:task.request.URL];

    receivedScriptMessage = true;
}

@end

static const char mainBytes[] =
"<html>" \
"<img src='testing:image'>" \
"</html>";

TEST(URLSchemeHandler, Basic)
{
    receivedScriptMessage = false;

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    RetainPtr<SchemeHandler> handler = adoptNS([[SchemeHandler alloc] initWithData:[NSData dataWithBytesNoCopy:(void*)mainBytes length:sizeof(mainBytes)] mimeType:@"text/html"]);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"testing"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"testing:main"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&receivedScriptMessage);

    EXPECT_EQ([handler.get().startedURLs count], 2u);
    EXPECT_TRUE([[handler.get().startedURLs objectAtIndex:0] isEqual:[NSURL URLWithString:@"testing:main"]]);
    EXPECT_TRUE([[handler.get().startedURLs objectAtIndex:1] isEqual:[NSURL URLWithString:@"testing:image"]]);
    EXPECT_EQ([handler.get().stoppedURLs count], 0u);
}

TEST(URLSchemeHandler, NoMIMEType)
{
    // Since there's no MIMEType, and no NavigationDelegate to tell WebKit to do the load anyways, WebKit will ignore (silently fail) the load.
    // This test makes sure that is communicated back to the URLSchemeHandler.

    receivedScriptMessage = false;

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    RetainPtr<SchemeHandler> handler = adoptNS([[SchemeHandler alloc] initWithData:[NSData dataWithBytesNoCopy:(void*)mainBytes length:sizeof(mainBytes)] mimeType:nil]);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"testing"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"testing:main"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&receivedScriptMessage);

    EXPECT_EQ([handler.get().startedURLs count], 1u);
    EXPECT_TRUE([[handler.get().startedURLs objectAtIndex:0] isEqual:[NSURL URLWithString:@"testing:main"]]);
    EXPECT_EQ([handler.get().stoppedURLs count], 1u);
    EXPECT_TRUE([[handler.get().stoppedURLs objectAtIndex:0] isEqual:[NSURL URLWithString:@"testing:main"]]);
}


#endif
