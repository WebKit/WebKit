/*
 * Copyright (C) 2012-2017 Apple Inc. All rights reserved.
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

#if WK_HAVE_C_SPI && PLATFORM(MAC)

#import "Test.h"

#import "PlatformUtilities.h"
#import "TestBrowsingContextLoadDelegate.h"
#import "TestProtocol.h"
#import <WebKit/WKContextPrivate.h>
#import <WebKit/WKProcessGroupPrivate.h>
#import <WebKit/WKViewPrivate.h>
#import <WebKit/WKWebsiteDataStoreRef.h>
#import <wtf/RetainPtr.h>

static bool testFinished;

@interface CustomProtocolsLoadDelegate : NSObject <WKBrowsingContextLoadDelegate>
@end

@implementation CustomProtocolsLoadDelegate

- (void)browsingContextControllerDidStartProvisionalLoad:(WKBrowsingContextController *)sender
{
    EXPECT_TRUE([sender.provisionalURL.absoluteString isEqualToString:@"http://redirect/?test"]);
}

- (void)browsingContextControllerDidReceiveServerRedirectForProvisionalLoad:(WKBrowsingContextController *)sender
{
    EXPECT_TRUE([sender.provisionalURL.absoluteString isEqualToString:@"http://test/"]);
}

- (void)browsingContextControllerDidCommitLoad:(WKBrowsingContextController *)sender
{
    EXPECT_TRUE([sender.committedURL.absoluteString isEqualToString:@"http://test/"]);
}

- (void)browsingContextControllerDidFinishLoad:(WKBrowsingContextController *)sender
{
    EXPECT_FALSE(sender.isLoading);
    testFinished = true;
}

@end

static WKProcessGroup *processGroup()
{
    static WKProcessGroup *processGroup = [[WKProcessGroup alloc] init];
    return processGroup;
}

static RetainPtr<WKView> wkView;

@interface CloseWhileStartingProtocol : TestProtocol
@end

@implementation CloseWhileStartingProtocol

- (void)startLoading
{
    dispatch_async(dispatch_get_main_queue(), ^ {
        WKContextClientV0 client;
        memset(&client, 0, sizeof(client));
        client.base.clientInfo = self;
        client.networkProcessDidCrash = [](WKContextRef context, const void* clientInfo) {
            auto protocol = (CloseWhileStartingProtocol *)clientInfo;
            [protocol.client URLProtocol:protocol didFailWithError:[NSError errorWithDomain:NSCocoaErrorDomain code:0 userInfo:nil]];
        };
        WKContextSetClient([processGroup() _contextRef], &client.base);

        kill(WKWebsiteDataStoreGetNetworkProcessIdentifier(WKPageGetWebsiteDataStore([wkView pageRef])), SIGKILL);
        [self.client URLProtocol:self didFailWithError:[NSError errorWithDomain:NSCocoaErrorDomain code:0 userInfo:nil]];
    });
}

- (void)stopLoading
{
    dispatch_async(dispatch_get_main_queue(), ^ {
        testFinished = true;
    });
}

@end

@interface ProcessPoolDestroyedDuringLoadingProtocol : TestProtocol
-(void)finishTheLoad;
@end

static bool isDone;
static RetainPtr<ProcessPoolDestroyedDuringLoadingProtocol> processPoolProtocolInstance;

@implementation ProcessPoolDestroyedDuringLoadingProtocol

- (void)startLoading
{
    NSURL *requestURL = self.request.URL;
    NSData *data = [@"PASS" dataUsingEncoding:NSASCIIStringEncoding];
    RetainPtr<NSURLResponse> response = adoptNS([[NSURLResponse alloc] initWithURL:requestURL MIMEType:@"text/html" expectedContentLength:data.length textEncodingName:nil]);

    [self.client URLProtocol:self didReceiveResponse:response.get() cacheStoragePolicy:NSURLCacheStorageNotAllowed];
    [self.client URLProtocol:self didLoadData:data];
    
    processPoolProtocolInstance = self;
    isDone = true;
}

- (void)finishTheLoad
{
    [self.client URLProtocolDidFinishLoading:self];
}

- (void)stopLoading
{
    isDone = true;
}

@end

namespace TestWebKitAPI {

static void runTest()
{
    RetainPtr<WKBrowsingContextGroup> browsingContextGroup = adoptNS([[WKBrowsingContextGroup alloc] initWithIdentifier:@"TestIdentifier"]);
    wkView = adoptNS([[WKView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) processGroup:processGroup() browsingContextGroup:browsingContextGroup.get()]);
    RetainPtr<CustomProtocolsLoadDelegate> loadDelegate = adoptNS([[CustomProtocolsLoadDelegate alloc] init]);
    [wkView browsingContextController].loadDelegate = loadDelegate.get();
    [[wkView browsingContextController] loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"%@://redirect?test", [TestProtocol scheme]]]]];

    Util::run(&testFinished);
}

TEST(WebKit2CustomProtocolsTest, MainResource)
{
    [TestProtocol registerWithScheme:@"http"];
    runTest();
    [TestProtocol unregister];
}

TEST(WebKit2CustomProtocolsTest, CloseDuringCustomProtocolLoad)
{
    [CloseWhileStartingProtocol registerWithScheme:@"http"];
    runTest();
    [CloseWhileStartingProtocol unregister];
}

TEST(WebKit2CustomProtocolsTest, ProcessPoolDestroyedDuringLoading)
{
    [ProcessPoolDestroyedDuringLoadingProtocol registerWithScheme:@"custom"];

    @autoreleasepool {
        auto browsingContextGroup = adoptNS([[WKBrowsingContextGroup alloc] initWithIdentifier:@"TestIdentifier"]);
        auto processGroup = adoptNS([[WKProcessGroup alloc] init]);
        auto wkView = adoptNS([[WKView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) processGroup:processGroup.get() browsingContextGroup:browsingContextGroup.get()]);
    
        [[wkView browsingContextController] loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"custom:///test"]]];

        Util::run(&isDone);
        isDone = false;

        // Instead of relying on the block going out of scope, manually release these objects in this order
        processGroup = nil;
        wkView = nil;
        browsingContextGroup = nil;
    }

    ASSERT(processPoolProtocolInstance);
    [processPoolProtocolInstance finishTheLoad];
    
    // isDone might already be true if the protocol has already been told to stopLoading.
    if (!isDone)
        Util::run(&isDone);
    
    // To crash reliably we need to spin the runloop a few times after the custom protocol has completed.
    Util::spinRunLoop(10);

    [ProcessPoolDestroyedDuringLoadingProtocol unregister];
}

} // namespace TestWebKitAPI

#endif
