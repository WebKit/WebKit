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
#import <WebKit/WebView.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(MAC)

static bool done;
static RetainPtr<NSString> destination;

@interface DownloadThreadChecker : NSObject <WebDownloadDelegate, WebPolicyDelegate>
@end

@implementation DownloadThreadChecker

- (void)webView:(WebView *)sender decidePolicyForMIMEType:(NSString *)type request:(NSURLRequest *)request frame:(WebFrame *)frame decisionListener:(id<WebPolicyDecisionListener>)listener
{
    [listener download];
}

- (void)downloadDidBegin:(NSURLDownload *)download
{
    EXPECT_TRUE([NSThread isMainThread]);
}

- (BOOL)download:(NSURLDownload *)download shouldDecodeSourceDataOfMIMEType:(NSString *)encodingType
{
    EXPECT_TRUE([NSThread isMainThread]);
    return YES;
}

- (void)download:(NSURLDownload *)download decideDestinationWithSuggestedFilename:(NSString *)filename
{
    EXPECT_TRUE([NSThread isMainThread]);
}

- (void)download:(NSURLDownload *)download didCreateDestination:(NSString *)path
{
    destination = path;
    EXPECT_TRUE([NSThread isMainThread]);
}

- (void)downloadDidFinish:(NSURLDownload *)download
{
    EXPECT_TRUE([NSThread isMainThread]);
    done = true;
}

@end

namespace TestWebKitAPI {

TEST(WebKitLegacy, DownloadThread)
{
    auto delegate = adoptNS([DownloadThreadChecker new]);
    auto webView = adoptNS([[WebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) frameName:nil groupName:nil]);
    [webView setPolicyDelegate:delegate.get()];
    [webView setDownloadDelegate:delegate.get()];

    [[webView mainFrame] loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];
    Util::run(&done);

    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:destination.get()]);
    [[NSFileManager defaultManager] removeItemAtPath:destination.get() error:nil];
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:destination.get()]);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
