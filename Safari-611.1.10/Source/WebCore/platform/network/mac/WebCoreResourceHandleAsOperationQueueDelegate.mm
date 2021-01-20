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
#import "ResourceHandle.h"
#import "ResourceHandleClient.h"
#import "ResourceRequest.h"
#import "ResourceResponse.h"
#import "SharedBuffer.h"
#import "SynchronousLoaderClient.h"
#import "WebCoreURLResponse.h"
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/MainThread.h>

using namespace WebCore;

static bool scheduledWithCustomRunLoopMode(const Optional<SchedulePairHashSet>& pairs)
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
        return m_messageQueue->append(makeUnique<Function<void()>>(WTFMove(function)));

    // This is the common case.
    if (!scheduledWithCustomRunLoopMode(m_scheduledPairs))
        return callOnMainThread(WTFMove(function));

    // If we have been scheduled in a custom run loop mode, schedule a block in that mode.
    auto block = makeBlockPtr([alreadyCalled = false, function = WTFMove(function)] () mutable {
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
    m_messageQueue = WTFMove(messageQueue);

    return self;
}

- (void)detachHandle
{
    LockHolder lock(m_mutex);

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
        LOG(Network, "Handle %p delegate connection:%p willSendRequest:%@ redirectResponse:%d, Location:<%@>", m_handle, connection, [newRequest description], static_cast<int>([(id)redirectResponse statusCode]), [[(id)redirectResponse allHeaderFields] objectForKey:@"Location"]);
    else
        LOG(Network, "Handle %p delegate connection:%p willSendRequest:%@ redirectResponse:non-HTTP", m_handle, connection, [newRequest description]); 
#endif

    auto protectedSelf = retainPtr(self);
    auto work = [self, protectedSelf, newRequest = retainPtr(newRequest), redirectResponse = retainPtr(redirectResponse)] () mutable {
        if (!m_handle) {
            m_requestResult = nullptr;
            m_semaphore.signal();
            return;
        }

        ResourceRequest redirectRequest = newRequest.get();
        if ([newRequest.get() HTTPBodyStream]) {
            ASSERT(m_handle->firstRequest().httpBody());
            redirectRequest.setHTTPBody(m_handle->firstRequest().httpBody());
        }
        if (m_handle->firstRequest().httpContentType().isEmpty())
            redirectRequest.clearHTTPContentType();
        m_handle->willSendRequest(WTFMove(redirectRequest), redirectResponse.get(), [self, protectedSelf = WTFMove(protectedSelf)](ResourceRequest&& request) {
            m_requestResult = request.nsURLRequest(HTTPBodyUpdatePolicy::UpdateHTTPBody);
            m_semaphore.signal();
        });
    };

    [self callFunctionOnMainThread:WTFMove(work)];
    m_semaphore.wait();

    LockHolder lock(m_mutex);
    if (!m_handle)
        return nil;

    RetainPtr<NSURLRequest> requestResult = m_requestResult;

    // Make sure protectedSelf gets destroyed on the main thread in case this is the last strong reference to self
    // as we do not want to get destroyed on a non-main thread.
    [self callFunctionOnMainThread:[protectedSelf = WTFMove(protectedSelf)] { }];

    return requestResult.autorelease();
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (void)connection:(NSURLConnection *)connection didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    ASSERT(!isMainThread());
    UNUSED_PARAM(connection);

    LOG(Network, "Handle %p delegate connection:%p didReceiveAuthenticationChallenge:%p", m_handle, connection, challenge);

    auto work = [self, protectedSelf = retainPtr(self), challenge = retainPtr(challenge)] () mutable {
        if (!m_handle) {
            [[challenge sender] cancelAuthenticationChallenge:challenge.get()];
            return;
        }
        m_handle->didReceiveAuthenticationChallenge(core(challenge.get()));
    };

    [self callFunctionOnMainThread:WTFMove(work)];
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (BOOL)connection:(NSURLConnection *)connection canAuthenticateAgainstProtectionSpace:(NSURLProtectionSpace *)protectionSpace
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    ASSERT(!isMainThread());
    UNUSED_PARAM(connection);

    LOG(Network, "Handle %p delegate connection:%p canAuthenticateAgainstProtectionSpace:%@://%@:%u realm:%@ method:%@ %@%@", m_handle, connection, [protectionSpace protocol], [protectionSpace host], [protectionSpace port], [protectionSpace realm], [protectionSpace authenticationMethod], [protectionSpace isProxy] ? @"proxy:" : @"", [protectionSpace isProxy] ? [protectionSpace proxyType] : @"");

    auto protectedSelf = retainPtr(self);
    auto work = [self, protectedSelf, protectionSpace = retainPtr(protectionSpace)] () mutable {
        if (!m_handle) {
            m_boolResult = NO;
            m_semaphore.signal();
            return;
        }
        m_handle->canAuthenticateAgainstProtectionSpace(ProtectionSpace(protectionSpace.get()), [self, protectedSelf = WTFMove(protectedSelf)] (bool result) mutable {
            m_boolResult = result;
            m_semaphore.signal();
        });
    };

    [self callFunctionOnMainThread:WTFMove(work)];
    m_semaphore.wait();

    LockHolder lock(m_mutex);
    if (!m_handle)
        return NO;

    auto boolResult = m_boolResult;

    // Make sure protectedSelf gets destroyed on the main thread in case this is the last strong reference to self
    // as we do not want to get destroyed on a non-main thread.
    [self callFunctionOnMainThread:[protectedSelf = WTFMove(protectedSelf)] { }];

    return boolResult;
}

- (void)connection:(NSURLConnection *)connection didReceiveResponse:(NSURLResponse *)r
{
    ASSERT(!isMainThread());

    LOG(Network, "Handle %p delegate connection:%p didReceiveResponse:%p (HTTP status %d, reported MIMEType '%s')", m_handle, connection, r, [r respondsToSelector:@selector(statusCode)] ? [(id)r statusCode] : 0, [[r MIMEType] UTF8String]);

    auto protectedSelf = retainPtr(self);
    auto work = [self, protectedSelf, r = retainPtr(r), connection = retainPtr(connection)] () mutable {
        RefPtr<ResourceHandle> protectedHandle(m_handle);
        if (!m_handle || !m_handle->client()) {
            m_semaphore.signal();
            return;
        }

        // Avoid MIME type sniffing if the response comes back as 304 Not Modified.
        int statusCode = [r respondsToSelector:@selector(statusCode)] ? [(id)r statusCode] : 0;
        if (statusCode != 304) {
            bool isMainResourceLoad = m_handle->firstRequest().requester() == ResourceRequest::Requester::Main;
            adjustMIMETypeIfNecessary([r _CFURLResponse], isMainResourceLoad);
        }

        if ([m_handle->firstRequest().nsURLRequest(HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody) _propertyForKey:@"ForceHTMLMIMEType"])
            [r _setMIMEType:@"text/html"];

        ResourceResponse resourceResponse(r.get());
        resourceResponse.setSource(ResourceResponse::Source::Network);
        resourceResponse.setDeprecatedNetworkLoadMetrics(ResourceHandle::getConnectionTimingData(connection.get()));

        m_handle->didReceiveResponse(WTFMove(resourceResponse), [self, protectedSelf = WTFMove(protectedSelf)] {
            m_semaphore.signal();
        });
    };

    [self callFunctionOnMainThread:WTFMove(work)];
    m_semaphore.wait();

    // Make sure we get destroyed on the main thread.
    [self callFunctionOnMainThread:[protectedSelf = WTFMove(protectedSelf)] { }];
}

- (void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data lengthReceived:(long long)lengthReceived
{
    ASSERT(!isMainThread());
    UNUSED_PARAM(connection);
    UNUSED_PARAM(lengthReceived);

    LOG(Network, "Handle %p delegate connection:%p didReceiveData:%p lengthReceived:%lld", m_handle, connection, data, lengthReceived);

    auto work = [self = self, protectedSelf = retainPtr(self), data = retainPtr(data)] () mutable {
        if (!m_handle || !m_handle->client())
            return;
        // FIXME: If we get more than 2B bytes in a single chunk, this code won't do the right thing.
        // However, with today's computers and networking speeds, this won't happen in practice.
        // Could be an issue with a giant local file.

        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=19793
        // -1 means we do not provide any data about transfer size to inspector so it would use
        // Content-Length headers or content size to show transfer size.
        m_handle->client()->didReceiveBuffer(m_handle, SharedBuffer::create(data.get()), -1);
    };

    [self callFunctionOnMainThread:WTFMove(work)];
}

- (void)connection:(NSURLConnection *)connection didSendBodyData:(NSInteger)bytesWritten totalBytesWritten:(NSInteger)totalBytesWritten totalBytesExpectedToWrite:(NSInteger)totalBytesExpectedToWrite
{
    ASSERT(!isMainThread());
    UNUSED_PARAM(connection);
    UNUSED_PARAM(bytesWritten);

    LOG(Network, "Handle %p delegate connection:%p didSendBodyData:%d totalBytesWritten:%d totalBytesExpectedToWrite:%d", m_handle, connection, bytesWritten, totalBytesWritten, totalBytesExpectedToWrite);

    auto work = [self = self, protectedSelf = retainPtr(self), totalBytesWritten = totalBytesWritten, totalBytesExpectedToWrite = totalBytesExpectedToWrite] () mutable {
        if (!m_handle || !m_handle->client())
            return;
        m_handle->client()->didSendData(m_handle, totalBytesWritten, totalBytesExpectedToWrite);
    };

    [self callFunctionOnMainThread:WTFMove(work)];
}

- (void)connectionDidFinishLoading:(NSURLConnection *)connection
{
    ASSERT(!isMainThread());
    UNUSED_PARAM(connection);

    LOG(Network, "Handle %p delegate connectionDidFinishLoading:%p", m_handle, connection);

    auto work = [self = self, protectedSelf = retainPtr(self)] () mutable {
        if (!m_handle || !m_handle->client())
            return;

        m_handle->client()->didFinishLoading(m_handle);
        if (m_messageQueue) {
            m_messageQueue->kill();
            m_messageQueue = nullptr;
        }
    };

    [self callFunctionOnMainThread:WTFMove(work)];
}

- (void)connection:(NSURLConnection *)connection didFailWithError:(NSError *)error
{
    ASSERT(!isMainThread());
    UNUSED_PARAM(connection);

    LOG(Network, "Handle %p delegate connection:%p didFailWithError:%@", m_handle, connection, error);

    auto work = [self = self, protectedSelf = retainPtr(self), error = retainPtr(error)] () mutable {
        if (!m_handle || !m_handle->client())
            return;

        m_handle->client()->didFail(m_handle, error.get());
        if (m_messageQueue) {
            m_messageQueue->kill();
            m_messageQueue = nullptr;
        }
    };

    [self callFunctionOnMainThread:WTFMove(work)];
}


- (NSCachedURLResponse *)connection:(NSURLConnection *)connection willCacheResponse:(NSCachedURLResponse *)cachedResponse
{
    ASSERT(!isMainThread());
    UNUSED_PARAM(connection);

    LOG(Network, "Handle %p delegate connection:%p willCacheResponse:%p", m_handle, connection, cachedResponse);

    auto protectedSelf = retainPtr(self);
    auto work = [self, protectedSelf, cachedResponse = retainPtr(cachedResponse)] () mutable {
        if (!m_handle || !m_handle->client()) {
            m_cachedResponseResult = nullptr;
            m_semaphore.signal();
            return;
        }

        m_handle->client()->willCacheResponseAsync(m_handle, cachedResponse.get(), [self, protectedSelf = WTFMove(protectedSelf)] (NSCachedURLResponse * response) mutable {
            m_cachedResponseResult = response;
            m_semaphore.signal();
        });
    };

    [self callFunctionOnMainThread:WTFMove(work)];
    m_semaphore.wait();

    LockHolder lock(m_mutex);
    if (!m_handle)
        return nil;

    RetainPtr<NSCachedURLResponse> cachedResponseResult = m_cachedResponseResult;

    // Make sure protectedSelf gets destroyed on the main thread in case this is the last strong reference to self
    // as we do not want to get destroyed on a non-main thread.
    [self callFunctionOnMainThread:[protectedSelf = WTFMove(protectedSelf)] { }];

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
