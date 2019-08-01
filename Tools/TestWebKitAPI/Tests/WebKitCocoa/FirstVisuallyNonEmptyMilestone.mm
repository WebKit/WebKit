/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(IOS_FAMILY)
#include <MobileCoreServices/MobileCoreServices.h>
#endif

static bool didFirstVisuallyNonEmptyLayout;
static bool receivedMessage;

@interface FirstPaintMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation FirstPaintMessageHandler
- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    receivedMessage = true;
}
@end

@interface RenderingProgressNavigationDelegate : NSObject <WKNavigationDelegate>
@end

@implementation RenderingProgressNavigationDelegate
- (void)_webView:(WKWebView *)webView renderingProgressDidChange:(_WKRenderingProgressEvents)progressEvents
{
    if (progressEvents & _WKRenderingProgressEventFirstVisuallyNonEmptyLayout)
        didFirstVisuallyNonEmptyLayout = true;
}
@end

TEST(WebKit, FirstVisuallyNonEmptyMilestoneWithDeferredScript)
{
    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto messageHandler = adoptNS([[FirstPaintMessageHandler alloc] init]);
    [[webViewConfiguration userContentController] addScriptMessageHandler:messageHandler.get() name:@"firstpaint"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);

    RetainPtr<RenderingProgressNavigationDelegate> delegate = adoptNS([[RenderingProgressNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    receivedMessage = false;
    didFirstVisuallyNonEmptyLayout = false;

    [webView loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"deferred-script-load" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];

    TestWebKitAPI::Util::run(&receivedMessage);
    EXPECT_TRUE(didFirstVisuallyNonEmptyLayout);
}

@interface NeverFinishLoadingSchemeHandler : NSObject <WKURLSchemeHandler>
@property (nonatomic, readonly, class) NSString *URLScheme;
@end

@implementation NeverFinishLoadingSchemeHandler

+ (NSString *)URLScheme
{
    return @"never-finish-loading";
}

static NSString *contentTypeForFileExtension(NSString *fileExtension)
{
    auto identifier = adoptCF(UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, (__bridge CFStringRef)fileExtension, nullptr));
    auto mimeType = adoptCF(UTTypeCopyPreferredTagWithClass(identifier.get(), kUTTagClassMIMEType));
    return (__bridge NSString *)mimeType.autorelease();
}

- (void)webView:(WKWebView *)webView startURLSchemeTask:(id <WKURLSchemeTask>)task
{
    NSURL *requestURL = task.request.URL;
    NSString *fileName = requestURL.lastPathComponent;
    NSString *fileExtension = fileName.pathExtension;
    NSURL *bundleURL = [NSBundle.mainBundle URLForResource:fileName.stringByDeletingPathExtension withExtension:fileExtension subdirectory:@"TestWebKitAPI.resources"];

    NSData *responseData = [NSData dataWithContentsOfURL:bundleURL];
    NSUInteger responseLength = responseData.length;

    auto response = adoptNS([[NSURLResponse alloc] initWithURL:requestURL MIMEType:contentTypeForFileExtension(fileExtension) expectedContentLength:responseLength textEncodingName:nil]);
    [task didReceiveResponse:response.get()];

    [task didReceiveData:[responseData subdataWithRange:NSMakeRange(0, responseLength - 1024)]];
}

- (void)webView:(WKWebView *)webView stopURLSchemeTask:(id <WKURLSchemeTask>)task
{
}

@end

TEST(WebKit, FirstVisuallyNonEmptyMilestoneWithMediaDocument)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
#if PLATFORM(IOS_FAMILY)
    [configuration setAllowsInlineMediaPlayback:YES];
    [configuration _setInlineMediaPlaybackRequiresPlaysInlineAttribute:NO];
#endif

    auto schemeHandler = adoptNS([[NeverFinishLoadingSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:NeverFinishLoadingSchemeHandler.URLScheme];

    auto navigationDelegate = adoptNS([[RenderingProgressNavigationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView _setAllowsMediaDocumentInlinePlayback:YES];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"never-finish-loading:///large-video-with-audio.mp4"]]];

    didFirstVisuallyNonEmptyLayout = false;
    TestWebKitAPI::Util::run(&didFirstVisuallyNonEmptyLayout);
}
