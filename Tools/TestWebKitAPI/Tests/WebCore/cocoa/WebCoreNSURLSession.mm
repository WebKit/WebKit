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

#import "Helpers/PlatformUtilities.h"
#import <JavaScriptCore/InitializeThreading.h>
#import <WebCore/FrameLoadRequest.h>
#import <WebCore/LocalFrameInlines.h>
#import <WebCore/Page.h>
#import <WebCore/PageConfiguration.h>
#import <WebCore/Document.h>
#import <WebCore/DocumentLoader.h>
#import <WebCore/HTMLVideoElement.h>
#import <WebCore/MediaResourceLoader.h>
#import <WebCore/NetworkLoadMetrics.h>
#import <WebCore/PlatformMediaResourceLoader.h>
#import <WebCore/ResourceError.h>
#import <WebCore/ResourceRequest.h>
#import <WebCore/ResourceResponse.h>
#import <WebCore/Settings.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/SubresourceLoader.h>
#import <WebCore/WebCoreNSURLSession.h>
#import <WebCore/ResourceLoader.h>
#import <WebKit/WebView.h>
#import <wtf/CompletionHandler.h>
#import <wtf/RetainPtr.h>
#import <wtf/SchedulePair.h>
#import <wtf/ThreadSafeRefCounted.h>
#import <wtf/ThreadSafeWeakPtr.h>
#import <wtf/TZoneMallocInlines.h>

static bool didLoadMainResource;
static bool didRecieveResponse;
static bool didRecieveData;
static bool didComplete;
static bool didInvalidate;

static NSURL *documentURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];
static NSURL *resourceURL = [NSBundle.test_resourcesBundle URLForResource:@"test" withExtension:@"mp4"];

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
- (WebCore::LocalFrame*)_mainCoreFrame;
@end

static bool rangeTestReceivedResponse;
static bool rangeTestFirstChunkDelivered;
static bool rangeTestComplete;
static RetainPtr<NSMutableData> rangeTestReceivedData;
static uint64_t rangeTestFirstChunkThreshold;

@interface TestRangeSessionDelegate : NSObject<NSURLSessionDataDelegate>
@end

@implementation TestRangeSessionDelegate
- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask didReceiveResponse:(NSURLResponse *)response completionHandler:(void (^)(NSURLSessionResponseDisposition disposition))completionHandler
{
    rangeTestReceivedResponse = true;
    completionHandler(NSURLSessionResponseAllow);
}

- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask didReceiveData:(NSData *)data
{
    [rangeTestReceivedData appendData:data];
    if ([rangeTestReceivedData length] >= rangeTestFirstChunkThreshold)
        rangeTestFirstChunkDelivered = true;
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didCompleteWithError:(nullable NSError *)error
{
    rangeTestComplete = true;
}
@end

namespace TestWebKitAPI {

// A PlatformMediaResource whose data delivery is driven directly by the test,
// so we can control exactly when each byte reaches the RangeResponseGenerator.
class TestRangeMediaResource final : public WebCore::PlatformMediaResource {
public:
    static Ref<TestRangeMediaResource> create() { return adoptRef(*new TestRangeMediaResource()); }
};

class TestRangeMediaResourceLoader final
    : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<TestRangeMediaResourceLoader, WTF::DestructionThread::Main>
    , public WebCore::PlatformMediaResourceLoader {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(TestRangeMediaResourceLoader);
public:
    static Ref<TestRangeMediaResourceLoader> create() { return adoptRef(*new TestRangeMediaResourceLoader()); }

    void ref() const final { ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::ref(); }
    void deref() const final { ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::deref(); }
    WTF::ThreadSafeWeakPtrControlBlock& controlBlock() const final { return ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::controlBlock(); }
    uint32_t weakRefCount() const final { return ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::weakRefCount(); }

    RefPtr<TestRangeMediaResource> lastResource;

private:
    void sendH2Ping(const URL&, CompletionHandler<void(std::expected<Seconds, ResourceError>&&)>&& completionHandler) final
    {
        completionHandler(makeUnexpected(ResourceError { }));
    }

    RefPtr<PlatformMediaResource> requestResource(ResourceRequest&&, LoadOptions) final
    {
        lastResource = TestRangeMediaResource::create();
        return lastResource;
    }
};


class WebCoreNSURLSessionTest : public testing::Test {
public:
    RetainPtr<WebView> view;
    LocalFrame* frame { nullptr };
    RetainPtr<TestNSURLSessionDataDelegate> delegate;
    RefPtr<MediaResourceLoader> loader;
    RefPtr<HTMLMediaElement> mediaElement;

    virtual void SetUp()
    {
#if PLATFORM(IOS_FAMILY)
        JSC::initialize();
#endif
        view = adoptNS([[WebView alloc] initWithFrame:NSZeroRect]);
        [view setFrameLoadDelegate:adoptNS([[TestNSURLSessionLoaderDelegate alloc] init]).get()];

        didLoadMainResource = false;
        [view setMainFrameURL:documentURL.absoluteString];
        TestWebKitAPI::Util::run(&didLoadMainResource);

        delegate = adoptNS([[TestNSURLSessionDataDelegate alloc] init]);
        frame = [view _mainCoreFrame];
        mediaElement = HTMLVideoElement::create(*frame->document());
        loader = MediaResourceLoader::create(*frame->document(), *mediaElement.get(), emptyString(), FetchOptions::Destination::Video);
    }

    virtual void TearDown()
    {
        loader = nullptr;
    }
};

TEST_F(WebCoreNSURLSessionTest, BasicOperation)
{
    RetainPtr session = adoptNS([[WebCoreNSURLSession alloc] initWithResourceLoader:*loader delegate:delegate.get() delegateQueue:[NSOperationQueue mainQueue]]);
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
}

// FIXME when webkit.org/b/306646 is resolved.
#if PLATFORM(MAC) && !defined(NDEBUG)
TEST_F(WebCoreNSURLSessionTest, DISABLED_InvalidateEmpty)
#else
TEST_F(WebCoreNSURLSessionTest, InvalidateEmpty)
#endif
{
    RetainPtr session = adoptNS([[WebCoreNSURLSession alloc] initWithResourceLoader:*loader delegate:delegate.get() delegateQueue:[NSOperationQueue mainQueue]]);
    didInvalidate = false;
    [session finishTasksAndInvalidate];
    TestWebKitAPI::Util::run(&didInvalidate);
}

TEST_F(WebCoreNSURLSessionTest, RangeResponseDeliversLastByteAtChunkBoundary)
{
    // Request the closed range "bytes=0-99" (100 bytes, indices 0...99). The
    // last byte of the range only arrives after the buffer has momentarily held
    // exactly `end` bytes. RangeResponseGenerator used to finish the task as
    // soon as the delivered byte index reached `end` (>=), dropping the final
    // byte; it must wait until the index passes `end` (>).
    constexpr uint64_t rangeEnd = 99;
    constexpr uint64_t expectedLength = rangeEnd + 1; // 100 bytes.
    constexpr size_t totalLength = 132;

    Vector<uint8_t> resourceBytes;
    resourceBytes.reserveInitialCapacity(totalLength);
    for (size_t i = 0; i < totalLength; ++i)
        resourceBytes.append(static_cast<uint8_t>(i & 0xFF));

    rangeTestReceivedResponse = false;
    rangeTestFirstChunkDelivered = false;
    rangeTestComplete = false;
    rangeTestReceivedData = adoptNS([[NSMutableData alloc] init]);
    // The first chunk we push is exactly `end` bytes (indices 0...end-1), which
    // gets fully delivered by both the buggy and fixed code paths.
    rangeTestFirstChunkThreshold = rangeEnd;

    RetainPtr rangeDelegate = adoptNS([[TestRangeSessionDelegate alloc] init]);
    Ref rangeLoader = TestRangeMediaResourceLoader::create();
    RetainPtr session = adoptNS([[WebCoreNSURLSession alloc] initWithResourceLoader:rangeLoader.get() delegate:rangeDelegate.get() delegateQueue:[NSOperationQueue mainQueue]]);

    RetainPtr request = adoptNS([[NSMutableURLRequest alloc] initWithURL:[NSURL URLWithString:@"https://example.com/test-range.mp4"]]);
    [request setValue:@"bytes=0-99" forHTTPHeaderField:@"Range"];

    RetainPtr<NSURLSessionDataTask> task = [session dataTaskWithRequest:request.get()];
    [task resume];

    // Wait for the task to request a resource from our loader.
    while (!rangeLoader->lastResource)
        TestWebKitAPI::Util::spinRunLoop();

    RefPtr resource = rangeLoader->lastResource;
    URL url { "https://example.com/test-range.mp4"_str };
    ResourceResponse response(URL { url }, "video/mp4"_s, totalLength, String { });
    response.setHTTPStatusCode(200);

    // Deliver the 200 response so the range response gets synthesized. This
    // reassigns the resource's client to the generator's internal client.
    resource->client()->responseReceived(*resource, response, [] (WebCore::ShouldContinuePolicyCheck) { });

    // First chunk: indices 0...end-1 (buffer size becomes exactly `end`).
    Ref firstChunk = SharedBuffer::create(resourceBytes.subspan(0, rangeEnd));
    resource->client()->dataReceived(*resource, firstChunk.get());
    TestWebKitAPI::Util::run(&rangeTestFirstChunkDelivered);

    // Second chunk: the remaining bytes, including the range's last byte at
    // index `end`. On the fixed code the task is still running and receives it.
    Ref secondChunk = SharedBuffer::create(resourceBytes.subspan(rangeEnd));
    resource->client()->dataReceived(*resource, secondChunk.get());
    resource->client()->loadFinished(*resource, NetworkLoadMetrics { });

    TestWebKitAPI::Util::run(&rangeTestComplete);

    EXPECT_EQ([rangeTestReceivedData length], expectedLength);

    RetainPtr expectedData = [NSData dataWithBytes:resourceBytes.subspan(0, expectedLength).data() length:expectedLength];
    EXPECT_TRUE([rangeTestReceivedData isEqualToData:expectedData.get()]);

    rangeTestReceivedData = nullptr;
}

}

#endif
