/*
 * Copyright (C) 2004-2018 Apple Inc. All rights reserved.
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
#import "WebCoreResourceHandleAsOperationQueueDelegate.h"

#import "AuthenticationChallenge.h"
#import "AuthenticationMac.h"
#import "Logging.h"
#import "NetworkingContext.h"
#import "OriginAccessPatterns.h"
#import "ResourceHandle.h"
#import "ResourceHandleClient.h"
#import "ResourceRequest.h"
#import "ResourceResponse.h"
#import "SecurityOrigin.h"
#import "SharedBuffer.h"
#import "SynchronousLoaderClient.h"
#import "WebCoreURLResponse.h"
#import <pal/spi/cf/CFNetworkSPI.h>
#import <pal/spi/cocoa/NSURLConnectionSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/MainThread.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

using namespace WebCore;

static bool scheduledWithCustomRunLoopMode(const std::optional<SchedulePairHashSet>& pairs)
{
    if (!pairs)
        return false;
    for (auto& pair : *pairs) {
        auto mode = pair->mode();
        if (mode != kCFRunLoopCommonModes && mode != kCFRunLoopDefaultMode)
            return true;
    }
    return false;
}

@implementation WebCoreResourceHandleAsOperationQueueDelegate

- (void)callFunctionOnMainThread:(Function<void()>&&)function
{
    // Sync xhr uses the message queue.
    if (m_messageQueue)
        return m_messageQueue->append(makeUnique<Function<void()>>(WTF::move(function)));

    // This is the common case.
    if (!scheduledWithCustomRunLoopMode(m_scheduledPairs))
        return callOnMainThread(WTF::move(function));

    // If we have been scheduled in a custom run loop mode, schedule a block in that mode.
    auto block = makeBlockPtr([alreadyCalled = false, function = WTF::move(function)] mutable {
        if (alreadyCalled)
            return;
        alreadyCalled = true;
        function();
        function = nullptr;
    });
    for (auto& pair : *m_scheduledPairs)
        CFRunLoopPerformBlock(pair->runLoop(), pair->mode(), block.get());
}

- (id)initWithHandle:(WebCore::ResourceHandle*)handle messageQueue:(RefPtr<WebCore::SynchronousLoaderMessageQueue>&&)messageQueue
{
    self = [self init];
    if (!self)
        return nil;

    m_handle = handle;
    if (m_handle && m_handle->context()) {
        if (auto* pairs = m_handle->context()->scheduledRunLoopPairs())
            m_scheduledPairs = *pairs;
    }
    m_messageQueue = WTF::move(messageQueue);

    return self;
}

- (void)detachHandle
{
    Locker locker { m_lock };

    m_handle = nullptr;

    m_messageQueue = nullptr;
    m_requestResult = nullptr;
    m_cachedResponseResult = nullptr;
    m_boolResult = NO;
    m_semaphore.signal(); // OK to signal even if we are not waiting.
}

- (void)dealloc
{
    [super dealloc];
}

- (NSURLRequest *)connection:(NSURLConnection *)connection willSendRequest:(NSURLRequest *)newRequest redirectResponse:(NSURLResponse *)redirectResponse
{
    ASSERT(!isMainThread());
    UNUSED_PARAM(connection);

    redirectResponse = synthesizeRedirectResponseIfNecessary([connection currentRequest], newRequest, redirectResponse);

    // See <rdar://problem/5380697>. This is a workaround for a behavior change in CFNetwork where willSendRequest gets called more often.
    if (!redirectResponse)
        return newRequest;

#if !LOG_DISABLED
    if ([redirectResponse isKindOfClass:[NSHTTPURLResponse class]])
        LOG(Network, "Handle %p delegate connection:%p willSendRequest:%@ redirectResponse:%d, Location:<%@>", m_handle.get(), connection, [newRequest description], static_cast<int>([(id)redirectResponse statusCode]), [[(id)redirectResponse allHeaderFields] objectForKey:@"Location"]);
    else
        LOG(Network, "Handle %p delegate connection:%p willSendRequest:%@ redirectResponse:non-HTTP", m_handle.get(), connection, [newRequest description]);
#endif

    auto protectedSelf = retainPtr(self);
    auto work = [protectedSelf, newRequest = retainPtr(newRequest), redirectResponse = retainPtr(redirectResponse)] mutable {
        if (!protectedSelf->m_handle) {
            protectedSelf->m_requestResult = nullptr;
            protectedSelf->m_semaphore.signal();
            return;
        }

        ResourceResponse response(redirectResponse.get());
        ResourceRequest redirectRequest = newRequest.get();
        if ([newRequest HTTPBodyStream]) {
            ASSERT(protectedSelf->m_handle->firstRequest().httpBody());
            redirectRequest.setHTTPBody(protectedSelf->m_handle->firstRequest().httpBody());
        }
        if (protectedSelf->m_handle->firstRequest().httpContentType().isEmpty())
            redirectRequest.clearHTTPContentType();

        // Check if the redirected url is allowed to access the redirecting url's timing information.
        if (!protectedSelf->m_handle->hasCrossOriginRedirect() && !WebCore::SecurityOrigin::create(redirectRequest.url())->canRequest(redirectResponse.get().URL, OriginAccessPatternsForWebProcess::singleton()))
            protectedSelf->m_handle->markAsHavingCrossOriginRedirect();
        protectedSelf->m_handle->checkTAO(response);

        protectedSelf->m_handle->incrementRedirectCount();

        protectedSelf->m_handle->willSendRequest(WTF::move(redirectRequest), WTF::move(response), [protectedSelf = WTF::move(protectedSelf)](ResourceRequest&& request) {
            protectedSelf->m_requestResult = request.nsURLRequest(HTTPBodyUpdatePolicy::UpdateHTTPBody);
            protectedSelf->m_semaphore.signal();
        });
    };

    [self callFunctionOnMainThread:WTF::move(work)];
    m_semaphore.wait();

    Locker locker { m_lock };
    if (!m_handle)
        return nil;

    RetainPtr<NSURLRequest> requestResult = m_requestResult;

    // Make sure protectedSelf gets destroyed on the main thread in case this is the last strong reference to self
    // as we do not want to get destroyed on a non-main thread.
    [self callFunctionOnMainThread:[protectedSelf = WTF::move(protectedSelf)] { }];

    return requestResult.autorelease();
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (void)connection:(NSURLConnection *)connection didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    ASSERT(!isMainThread());
    UNUSED_PARAM(connection);

    LOG(Network, "Handle %p delegate connection:%p didReceiveAuthenticationChallenge:%p", m_handle.get(), connection, challenge);

    auto work = [protectedSelf = retainPtr(self), challenge = retainPtr(challenge)] mutable {
        if (!protectedSelf->m_handle) {
            [[challenge sender] cancelAuthenticationChallenge:challenge.get()];
            return;
        }
        protectedSelf->m_handle->didReceiveAuthenticationChallenge(core(challenge.get()));
    };

    [self callFunctionOnMainThread:WTF::move(work)];
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (BOOL)connection:(NSURLConnection *)connection canAuthenticateAgainstProtectionSpace:(NSURLProtectionSpace *)protectionSpace
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    ASSERT(!isMainThread());
    UNUSED_PARAM(connection);

    LOG(Network, "Handle %p delegate connection:%p canAuthenticateAgainstProtectionSpace:%@://%@:%zd realm:%@ method:%@ %@%@", m_handle.get(), connection, [protectionSpace protocol], [protectionSpace host], [protectionSpace port], [protectionSpace realm], [protectionSpace authenticationMethod], [protectionSpace isProxy] ? @"proxy:" : @"", [protectionSpace isProxy] ? [protectionSpace proxyType] : @"");

    auto protectedSelf = retainPtr(self);
    auto work = [protectedSelf, protectionSpace = retainPtr(protectionSpace)] mutable {
        if (!protectedSelf->m_handle) {
            protectedSelf->m_boolResult = NO;
            protectedSelf->m_semaphore.signal();
            return;
        }
        protectedSelf->m_handle->canAuthenticateAgainstProtectionSpace(ProtectionSpace(protectionSpace.get()), [protectedSelf = WTF::move(protectedSelf)](bool result) mutable {
            protectedSelf->m_boolResult = result;
            protectedSelf->m_semaphore.signal();
        });
    };

    [self callFunctionOnMainThread:WTF::move(work)];
    m_semaphore.wait();

    Locker locker { m_lock };
    if (!m_handle)
        return NO;

    auto boolResult = m_boolResult;

    // Make sure protectedSelf gets destroyed on the main thread in case this is the last strong reference to self
    // as we do not want to get destroyed on a non-main thread.
    [self callFunctionOnMainThread:[protectedSelf = WTF::move(protectedSelf)] { }];

    return boolResult;
}

- (void)connection:(NSURLConnection *)connection didReceiveResponse:(NSURLResponse *)r
{
    ASSERT(!isMainThread());

    LOG(Network, "Handle %p delegate connection:%p didReceiveResponse:%p (HTTP status %zd, reported MIMEType '%s')", m_handle.get(), connection, r, [r respondsToSelector:@selector(statusCode)] ? [(id)r statusCode] : 0, [[r MIMEType] UTF8String]);

    auto protectedSelf = retainPtr(self);
    auto work = [protectedSelf, r = retainPtr(r), connection = retainPtr(connection)] mutable {
        RefPtr handle = protectedSelf->m_handle.get();
        if (!handle || !handle->client()) {
            protectedSelf->m_semaphore.signal();
            return;
        }

        // Avoid MIME type sniffing if the response comes back as 304 Not Modified.
        int statusCode = [r respondsToSelector:@selector(statusCode)] ? [(id)r statusCode] : 0;
        if (statusCode != 304) {
            bool isMainResourceLoad = handle->firstRequest().requester() == ResourceRequestRequester::Main;
            adjustMIMETypeIfNecessary([r _CFURLResponse], isMainResourceLoad ? IsMainResourceLoad::Yes : IsMainResourceLoad::No, IsNoSniffSet::No);
        }

        if ([protect(handle->firstRequest().nsURLRequest(HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody)) _propertyForKey:@"ForceHTMLMIMEType"])
            [r _setMIMEType:@"text/html"];

        ResourceResponse resourceResponse(r.get());
        handle->checkTAO(resourceResponse);

        auto metrics = copyTimingData(connection.get(), *handle);
        resourceResponse.setSource(ResourceResponse::Source::Network);
        resourceResponse.setDeprecatedNetworkLoadMetrics(Box<NetworkLoadMetrics> { metrics });

        handle->setNetworkLoadMetrics(WTF::move(metrics));

        handle->didReceiveResponse(WTF::move(resourceResponse), [protectedSelf = WTF::move(protectedSelf)] {
            protectedSelf->m_semaphore.signal();
        });
    };

    [self callFunctionOnMainThread:WTF::move(work)];
    m_semaphore.wait();

    // Make sure we get destroyed on the main thread.
    [self callFunctionOnMainThread:[protectedSelf = WTF::move(protectedSelf)] { }];
}

- (void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data lengthReceived:(long long)lengthReceived
{
    ASSERT(!isMainThread());
    UNUSED_PARAM(connection);
    UNUSED_PARAM(lengthReceived);

    LOG(Network, "Handle %p delegate connection:%p didReceiveData:%p lengthReceived:%lld", m_handle.get(), connection, data, lengthReceived);

    auto work = [protectedSelf = retainPtr(self), data = retainPtr(data)] mutable {
        if (!protectedSelf->m_handle || !protectedSelf->m_handle->client())
            return;
        // FIXME: If we get more than 2B bytes in a single chunk, this code won't do the right thing.
        // However, with today's computers and networking speeds, this won't happen in practice.
        // Could be an issue with a giant local file.

        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=19793
        // -1 means we do not provide any data about transfer size to inspector so it would use
        // Content-Length headers or content size to show transfer size.
        protectedSelf->m_handle->client()->didReceiveData(protectedSelf->m_handle.get(), SharedBuffer::create(data.get()), -1);
    };

    [self callFunctionOnMainThread:WTF::move(work)];
}

- (void)connection:(NSURLConnection *)connection didSendBodyData:(NSInteger)bytesWritten totalBytesWritten:(NSInteger)totalBytesWritten totalBytesExpectedToWrite:(NSInteger)totalBytesExpectedToWrite
{
    ASSERT(!isMainThread());
    UNUSED_PARAM(connection);
    UNUSED_PARAM(bytesWritten);

    LOG(Network, "Handle %p delegate connection:%p didSendBodyData:%zd totalBytesWritten:%zd totalBytesExpectedToWrite:%zd", m_handle.get(), connection, bytesWritten, totalBytesWritten, totalBytesExpectedToWrite);

    auto work = [protectedSelf = retainPtr(self), totalBytesWritten = totalBytesWritten, totalBytesExpectedToWrite = totalBytesExpectedToWrite] mutable {
        if (!protectedSelf->m_handle || !protectedSelf->m_handle->client())
            return;
        protectedSelf->m_handle->client()->didSendData(protectedSelf->m_handle.get(), totalBytesWritten, totalBytesExpectedToWrite);
    };

    [self callFunctionOnMainThread:WTF::move(work)];
}

- (void)connectionDidFinishLoading:(NSURLConnection *)connection
{
    ASSERT(!isMainThread());
    UNUSED_PARAM(connection);

    LOG(Network, "Handle %p delegate connectionDidFinishLoading:%p", m_handle.get(), connection);

    auto work = [protectedSelf = retainPtr(self), connection = retainPtr(connection), timingData = retainPtr([connection _timingData])] mutable {
        if (!protectedSelf->m_handle || !protectedSelf->m_handle->client())
            return;

        if (auto metrics = protectedSelf->m_handle->networkLoadMetrics()) {
            if (double responseEndTime = [[timingData objectForKey:@"_kCFNTimingDataResponseEnd"] doubleValue])
                metrics->responseEnd = WallTime::fromRawSeconds(adoptNS([[NSDate alloc] initWithTimeIntervalSinceReferenceDate:responseEndTime]).get().timeIntervalSince1970).approximateMonotonicTime();
            else
                metrics->responseEnd = metrics->responseStart;
            metrics->protocol = checked_objc_cast<NSString>([timingData objectForKey:@"_kCFNTimingDataNetworkProtocolName"]);
            metrics->responseBodyBytesReceived = [[timingData objectForKey:@"_kCFNTimingDataResponseBodyBytesReceived"] unsignedLongLongValue];
            metrics->responseBodyDecodedSize = [[timingData objectForKey:@"_kCFNTimingDataResponseBodyBytesDecoded"] unsignedLongLongValue];
            metrics->markComplete();
            protectedSelf->m_handle->client()->didFinishLoading(protectedSelf->m_handle.get(), *metrics);
        } else {
            NetworkLoadMetrics emptyMetrics;
            emptyMetrics.markComplete();
            protectedSelf->m_handle->client()->didFinishLoading(protectedSelf->m_handle.get(), emptyMetrics);
        }

        if (protectedSelf->m_messageQueue) {
            protectedSelf->m_messageQueue->kill();
            protectedSelf->m_messageQueue = nullptr;
        }
    };

    [self callFunctionOnMainThread:WTF::move(work)];
}

- (void)connection:(NSURLConnection *)connection didFailWithError:(NSError *)error
{
    ASSERT(!isMainThread());
    UNUSED_PARAM(connection);

    LOG(Network, "Handle %p delegate connection:%p didFailWithError:%@", m_handle.get(), connection, error);

    auto work = [protectedSelf = retainPtr(self), error = retainPtr(error)] mutable {
        if (!protectedSelf->m_handle || !protectedSelf->m_handle->client())
            return;

        protectedSelf->m_handle->client()->didFail(protectedSelf->m_handle.get(), error.get());
        if (protectedSelf->m_messageQueue) {
            protectedSelf->m_messageQueue->kill();
            protectedSelf->m_messageQueue = nullptr;
        }
    };

    [self callFunctionOnMainThread:WTF::move(work)];
}


- (NSCachedURLResponse *)connection:(NSURLConnection *)connection willCacheResponse:(NSCachedURLResponse *)cachedResponse
{
    ASSERT(!isMainThread());
    UNUSED_PARAM(connection);

    LOG(Network, "Handle %p delegate connection:%p willCacheResponse:%p", m_handle.get(), connection, cachedResponse);

    auto protectedSelf = retainPtr(self);
    auto work = [protectedSelf, cachedResponse = retainPtr(cachedResponse)] mutable {
        if (!protectedSelf->m_handle || !protectedSelf->m_handle->client()) {
            protectedSelf->m_cachedResponseResult = nullptr;
            protectedSelf->m_semaphore.signal();
            return;
        }

        protectedSelf->m_handle->client()->willCacheResponseAsync(protectedSelf->m_handle.get(), cachedResponse.get(), [protectedSelf = WTF::move(protectedSelf)](NSCachedURLResponse * response) mutable {
            protectedSelf->m_cachedResponseResult = response;
            protectedSelf->m_semaphore.signal();
        });
    };

    [self callFunctionOnMainThread:WTF::move(work)];
    m_semaphore.wait();

    Locker locker { m_lock };
    if (!m_handle)
        return nil;

    RetainPtr<NSCachedURLResponse> cachedResponseResult = m_cachedResponseResult;

    // Make sure protectedSelf gets destroyed on the main thread in case this is the last strong reference to self
    // as we do not want to get destroyed on a non-main thread.
    [self callFunctionOnMainThread:[protectedSelf = WTF::move(protectedSelf)] { }];

    return cachedResponseResult.autorelease();
}

@end

@implementation WebCoreResourceHandleWithCredentialStorageAsOperationQueueDelegate

- (BOOL)connectionShouldUseCredentialStorage:(NSURLConnection *)connection
{
    ASSERT(!isMainThread());
    UNUSED_PARAM(connection);
    return NO;
}

@end
