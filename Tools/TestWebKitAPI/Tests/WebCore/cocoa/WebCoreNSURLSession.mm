/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"

#if !PLATFORM(IOS_FAMILY)

#import "Utilities.h"
#import <JavaScriptCore/InitializeThreading.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameLoadRequest.h>
#import <WebCore/Page.h>
#import <WebCore/PageConfiguration.h>
#import <WebCore/Document.h>
#import <WebCore/DocumentLoader.h>
#import <WebCore/HTMLVideoElement.h>
#import <WebCore/MediaResourceLoader.h>
#import <WebCore/Settings.h>
#import <WebCore/SubresourceLoader.h>
#import <WebCore/WebCoreNSURLSession.h>
#import <WebCore/ResourceLoader.h>
#import <WebKit/WebView.h>
#import <wtf/SchedulePair.h>

static bool didLoadMainResource;
static bool didRecieveResponse;
static bool didRecieveData;
static bool didComplete;
static bool didInvalidate;

static NSURL *documentURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
static NSURL *resourceURL = [[NSBundle mainBundle] URLForResource:@"test" withExtension:@"mp4" subdirectory:@"TestWebKitAPI.resources"];

@interface TestNSURLSessionLoaderDelegate : NSObject<WebFrameLoadDelegate>
@end

@implementation TestNSURLSessionLoaderDelegate
- (void)webView:(WebView *)sender didCommitLoadForFrame:(WebFrame *)frame
{
    UNUSED_PARAM(sender);
    UNUSED_PARAM(frame);
    didLoadMainResource = true;
}
@end

@interface TestNSURLSessionDataDelegate : NSObject<NSURLSessionDataDelegate>
@end

@implementation TestNSURLSessionDataDelegate
- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didCompleteWithError:(nullable NSError *)error
{
    didComplete = true;
}

- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask didReceiveResponse:(NSURLResponse *)response completionHandler:(void (^)(NSURLSessionResponseDisposition disposition))completionHandler
{
    didRecieveResponse = true;
    completionHandler(NSURLSessionResponseAllow);
}

- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask didReceiveData:(NSData *)data
{
    didRecieveData = true;

    NSData* directData = [NSData dataWithContentsOfURL:dataTask.originalRequest.URL];
    NSData* directSubdata = [directData subdataWithRange:NSMakeRange(dataTask.countOfBytesReceived - data.length, data.length)];
    ASSERT_TRUE([data isEqualToData:directSubdata]);
}

- (void)URLSession:(NSURLSession *)session didBecomeInvalidWithError:(nullable NSError *)error
{
    didInvalidate = true;
}
@end

using namespace WebCore;

@interface WebView (WebViewInternalForTesting)
- (WebCore::Frame*)_mainCoreFrame;
@end

namespace TestWebKitAPI {

class WebCoreNSURLSessionTest : public testing::Test {
public:
    WebView *view { nil };
    Frame* frame { nullptr };
    TestNSURLSessionDataDelegate *delegate { nil };
    RefPtr<MediaResourceLoader> loader;
    RefPtr<HTMLMediaElement> mediaElement;

    virtual void SetUp()
    {
#if PLATFORM(IOS_FAMILY)
        JSC::initialize();
#endif
        view = [[WebView alloc] initWithFrame:NSZeroRect];
        view.frameLoadDelegate = [[[TestNSURLSessionLoaderDelegate alloc] init] autorelease];

        didLoadMainResource = false;
        view.mainFrameURL = documentURL.absoluteString;
        TestWebKitAPI::Util::run(&didLoadMainResource);

        delegate = [[TestNSURLSessionDataDelegate alloc] init];
        frame = [view _mainCoreFrame];
        mediaElement = HTMLVideoElement::create(*frame->document());
        loader = adoptRef(new MediaResourceLoader(*frame->document(), *mediaElement.get(), emptyString()));
    }

    virtual void TearDown()
    {
        [view release];
        [delegate release];
        loader = nullptr;
    }
};

TEST_F(WebCoreNSURLSessionTest, BasicOperation)
{
    WebCoreNSURLSession* session = [[WebCoreNSURLSession alloc] initWithResourceLoader:*loader delegate:delegate delegateQueue:[NSOperationQueue mainQueue]];
    didRecieveResponse = false;
    didRecieveData = false;
    didComplete = false;

    NSURLSessionDataTask *task = [session dataTaskWithURL:resourceURL];
    [task resume];

    TestWebKitAPI::Util::run(&didRecieveResponse);
    TestWebKitAPI::Util::run(&didRecieveData);
    TestWebKitAPI::Util::run(&didComplete);

    didInvalidate = false;

    task = [session dataTaskWithURL:resourceURL];
    [task resume];
    [session finishTasksAndInvalidate];

    TestWebKitAPI::Util::run(&didInvalidate);

    [session release];
}

TEST_F(WebCoreNSURLSessionTest, InvalidateEmpty)
{
    WebCoreNSURLSession* session = [[WebCoreNSURLSession alloc] initWithResourceLoader:*loader delegate:delegate delegateQueue:[NSOperationQueue mainQueue]];
    didInvalidate = false;
    [session finishTasksAndInvalidate];
    TestWebKitAPI::Util::run(&didInvalidate);
    [session release];
}

}

#endif
