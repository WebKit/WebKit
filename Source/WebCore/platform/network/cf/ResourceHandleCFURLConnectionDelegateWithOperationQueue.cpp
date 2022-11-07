/*
 * Copyright (C) 2013-2018 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ResourceHandleCFURLConnectionDelegateWithOperationQueue.h"

#if USE(CFURLCONNECTION)

#include "AuthenticationCF.h"
#include "AuthenticationChallenge.h"
#include "Logging.h"
#include "MIMETypeRegistry.h"
#include "NetworkLoadMetrics.h"
#include "ResourceHandle.h"
#include "ResourceHandleClient.h"
#include "ResourceResponse.h"
#include "SharedBuffer.h"
#include "SynchronousLoaderClient.h"
#include <pal/spi/win/CFNetworkSPIWin.h>
#include <wtf/CompletionHandler.h>
#include <wtf/MainThread.h>
#include <wtf/Threading.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

ResourceHandleCFURLConnectionDelegateWithOperationQueue::ResourceHandleCFURLConnectionDelegateWithOperationQueue(ResourceHandle* handle, RefPtr<SynchronousLoaderMessageQueue>&& messageQueue)
    : ResourceHandleCFURLConnectionDelegate(handle)
    , m_messageQueue(WTFMove(messageQueue))
{
}

ResourceHandleCFURLConnectionDelegateWithOperationQueue::~ResourceHandleCFURLConnectionDelegateWithOperationQueue()
{
}

bool ResourceHandleCFURLConnectionDelegateWithOperationQueue::hasHandle() const
{
    return !!m_handle;
}

void ResourceHandleCFURLConnectionDelegateWithOperationQueue::releaseHandle()
{
    ResourceHandleCFURLConnectionDelegate::releaseHandle();
    m_requestResult = nullptr;
    m_cachedResponseResult = nullptr;
    m_semaphore.signal();
}

void ResourceHandleCFURLConnectionDelegateWithOperationQueue::setupRequest(CFMutableURLRequestRef request)
{
    CFURLRef requestURL = CFURLRequestGetURL(request);
    if (!requestURL)
        return;
    m_originalScheme = adoptCF(CFURLCopyScheme(requestURL));
}

LRESULT CALLBACK hookToRemoveCFNetworkMessage(int code, WPARAM wParam, LPARAM lParam)
{
    MSG* msg = reinterpret_cast<MSG*>(lParam);
    // This message which CFNetwork sends to itself, will block the main thread, remove it.
    if (msg->message == WM_USER + 0xcf)
        msg->message = WM_NULL;
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

static void installHookToRemoveCFNetworkMessageBlockingMainThread()
{
    static HHOOK hook = nullptr;
    if (!hook) {
        DWORD threadID = ::GetCurrentThreadId();
        hook = ::SetWindowsHookExW(WH_GETMESSAGE, hookToRemoveCFNetworkMessage, 0, threadID);
    }
}

static void emptyPerform(void*)
{
}

static CFRunLoopRef getRunLoop()
{
    static CFRunLoopRef runLoop = nullptr;

    if (!runLoop) {
        BinarySemaphore sem;
        Thread::create("CFNetwork Loader", [&] {
            runLoop = CFRunLoopGetCurrent();

            // Must add a source to the run loop to prevent CFRunLoopRun() from exiting.
            CFRunLoopSourceContext ctxt = { 0, (void*)1 /*must be non-null*/, 0, 0, 0, 0, 0, 0, 0, emptyPerform };
            auto bogusSource = adoptCF(CFRunLoopSourceCreate(0, 0, &ctxt));
            CFRunLoopAddSource(runLoop, bogusSource.get(), kCFRunLoopDefaultMode);
            sem.signal();

            while (true)
                CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1E30, true);
        }, ThreadType::Network);
        sem.wait();
    }

    return runLoop;
}

void ResourceHandleCFURLConnectionDelegateWithOperationQueue::setupConnectionScheduling(CFURLConnectionRef connection)
{
    installHookToRemoveCFNetworkMessageBlockingMainThread();
    CFRunLoopRef runLoop = getRunLoop();
    CFURLConnectionScheduleWithRunLoop(connection, runLoop, kCFRunLoopDefaultMode);
    CFURLConnectionScheduleDownloadWithRunLoop(connection, runLoop, kCFRunLoopDefaultMode);
}

RetainPtr<CFURLRequestRef> ResourceHandleCFURLConnectionDelegateWithOperationQueue::willSendRequest(CFURLRequestRef cfRequest, CFURLResponseRef originalRedirectResponse)
{
    // If the protocols of the new request and the current request match, this is not an HSTS redirect and we don't need to synthesize a redirect response.
    if (!originalRedirectResponse) {
        RetainPtr<CFStringRef> newScheme = adoptCF(CFURLCopyScheme(CFURLRequestGetURL(cfRequest)));
        if (CFStringCompare(newScheme.get(), m_originalScheme.get(), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
            return cfRequest;
    }

    ASSERT(!isMainThread());

    Ref protectedThis { *this };
    auto work = [this, protectedThis = Ref { *this }, cfRequest = RetainPtr<CFURLRequestRef>(cfRequest), originalRedirectResponse = RetainPtr<CFURLResponseRef>(originalRedirectResponse)] () mutable {
        auto& handle = protectedThis->m_handle;
        auto completionHandler = [this, protectedThis = WTFMove(protectedThis)] (ResourceRequest&& request) {
            m_requestResult = request.cfURLRequest(UpdateHTTPBody);
            m_semaphore.signal();
        };

        if (!hasHandle()) {
            completionHandler({ });
            return;
        }

        LOG(Network, "CFNet - ResourceHandleCFURLConnectionDelegateWithOperationQueue::willSendRequest(handle=%p) (%s)", handle, handle->firstRequest().url().string().utf8().data());

        RetainPtr<CFURLResponseRef> redirectResponse = synthesizeRedirectResponseIfNecessary(cfRequest.get(), originalRedirectResponse.get());
        ASSERT(redirectResponse);

        ResourceRequest request = createResourceRequest(cfRequest.get(), redirectResponse.get());
        handle->willSendRequest(WTFMove(request), redirectResponse.get(), WTFMove(completionHandler));
    };

    if (m_messageQueue)
        m_messageQueue->append(makeUnique<Function<void()>>(WTFMove(work)));
    else
        callOnMainThread(WTFMove(work));
    m_semaphore.wait();

    return std::exchange(m_requestResult, nullptr);
}

void ResourceHandleCFURLConnectionDelegateWithOperationQueue::didReceiveResponse(CFURLConnectionRef connection, CFURLResponseRef cfResponse)
{
    Ref protectedThis { *this };
    auto work = [this, protectedThis = Ref { *this }, cfResponse = RetainPtr<CFURLResponseRef>(cfResponse), connection = RetainPtr<CFURLConnectionRef>(connection)] () mutable {
        if (!hasHandle() || !m_handle->client() || !m_handle->connection()) {
            m_semaphore.signal();
            return;
        }

        LOG(Network, "CFNet - ResourceHandleCFURLConnectionDelegateWithOperationQueue::didReceiveResponse(handle=%p) (%s)", m_handle, m_handle->firstRequest().url().string().utf8().data());

        // Avoid MIME type sniffing if the response comes back as 304 Not Modified.
        auto msg = CFURLResponseGetHTTPResponse(cfResponse.get());
        int statusCode = msg ? CFHTTPMessageGetResponseStatusCode(msg) : 0;

        if (statusCode != 304) {
            bool isMainResourceLoad = m_handle->firstRequest().requester() == ResourceRequestRequester::Main;
        }

        if (_CFURLRequestCopyProtocolPropertyForKey(m_handle->firstRequest().cfURLRequest(DoNotUpdateHTTPBody), CFSTR("ForceHTMLMIMEType")))
            CFURLResponseSetMIMEType(cfResponse.get(), CFSTR("text/html"));

        ResourceResponse resourceResponse(cfResponse.get());
        resourceResponse.setSource(ResourceResponse::Source::Network);
        m_handle->didReceiveResponse(WTFMove(resourceResponse), [this, protectedThis = WTFMove(protectedThis)] {
            m_semaphore.signal();
        });
    };

    if (m_messageQueue)
        m_messageQueue->append(makeUnique<Function<void()>>(WTFMove(work)));
    else
        callOnMainThread(WTFMove(work));
    m_semaphore.wait();
}

void ResourceHandleCFURLConnectionDelegateWithOperationQueue::didReceiveData(CFDataRef data, CFIndex originalLength)
{
    auto work = [protectedThis = Ref { *this }, data = RetainPtr<CFDataRef>(data), originalLength = originalLength] () mutable {
        auto& handle = protectedThis->m_handle;
        if (!protectedThis->hasHandle() || !handle->client() || !handle->connection())
            return;
        
        LOG(Network, "CFNet - ResourceHandleCFURLConnectionDelegateWithOperationQueue::didReceiveData(handle=%p) (%s)", handle, handle->firstRequest().url().string().utf8().data());

        handle->client()->didReceiveBuffer(handle, SharedBuffer::create(data.get()), originalLength);
    };
    
    if (m_messageQueue)
        m_messageQueue->append(makeUnique<Function<void()>>(WTFMove(work)));
    else
        callOnMainThread(WTFMove(work));
}

void ResourceHandleCFURLConnectionDelegateWithOperationQueue::didFinishLoading()
{
    auto work = [protectedThis = Ref { *this }] () mutable {
        auto& handle = protectedThis->m_handle;
        if (!protectedThis->hasHandle() || !handle->client() || !handle->connection()) {
            protectedThis->m_handle->deref();
            return;
        }

        LOG(Network, "CFNet - ResourceHandleCFURLConnectionDelegateWithOperationQueue::didFinishLoading(handle=%p) (%s)", handle, handle->firstRequest().url().string().utf8().data());

        handle->client()->didFinishLoading(handle, NetworkLoadMetrics { });
        if (protectedThis->m_messageQueue) {
            protectedThis->m_messageQueue->kill();
            protectedThis->m_messageQueue = nullptr;
        }
        protectedThis->m_handle->deref();
    };
    
    if (m_messageQueue)
        m_messageQueue->append(makeUnique<Function<void()>>(WTFMove(work)));
    else
        callOnMainThread(WTFMove(work));
}

void ResourceHandleCFURLConnectionDelegateWithOperationQueue::didFail(CFErrorRef error)
{
    auto work = [protectedThis = Ref { *this }, error = RetainPtr<CFErrorRef>(error)] () mutable {
        auto& handle = protectedThis->m_handle;
        if (!protectedThis->hasHandle() || !handle->client() || !handle->connection()) {
            protectedThis->m_handle->deref();
            return;
        }
        
        LOG(Network, "CFNet - ResourceHandleCFURLConnectionDelegateWithOperationQueue::didFail(handle=%p) (%s)", handle, handle->firstRequest().url().string().utf8().data());

        handle->client()->didFail(handle, ResourceError(error.get()));
        if (protectedThis->m_messageQueue) {
            protectedThis->m_messageQueue->kill();
            protectedThis->m_messageQueue = nullptr;
        }
        protectedThis->m_handle->deref();
    };

    if (m_messageQueue)
        m_messageQueue->append(makeUnique<Function<void()>>(WTFMove(work)));
    else
        callOnMainThread(WTFMove(work));
}

CFCachedURLResponseRef ResourceHandleCFURLConnectionDelegateWithOperationQueue::willCacheResponse(CFCachedURLResponseRef cachedResponse)
{
    // Workaround for <rdar://problem/6300990> Caching does not respect Vary HTTP header.
    // FIXME: WebCore cache has issues with Vary, too (bug 58797, bug 71509).
    CFURLResponseRef wrappedResponse = CFCachedURLResponseGetWrappedResponse(cachedResponse);
    if (CFHTTPMessageRef httpResponse = CFURLResponseGetHTTPResponse(wrappedResponse)) {
        ASSERT(CFHTTPMessageIsHeaderComplete(httpResponse));
        RetainPtr<CFStringRef> varyValue = adoptCF(CFHTTPMessageCopyHeaderFieldValue(httpResponse, CFSTR("Vary")));
        if (varyValue)
            return nullptr;
    }

    Ref protectedThis { *this };
    auto work = [protectedThis = Ref { *this }, cachedResponse = RetainPtr<CFCachedURLResponseRef>(cachedResponse)] () mutable {
        auto& handle = protectedThis->m_handle;

        auto completionHandler = [protectedThis = WTFMove(protectedThis)] (CFCachedURLResponseRef response) mutable {
            protectedThis->m_cachedResponseResult = response;
            protectedThis->m_semaphore.signal();
        };

        if (!handle || !handle->client() || !handle->connection())
            return completionHandler(nullptr);

        LOG(Network, "CFNet - ResourceHandleCFURLConnectionDelegateWithOperationQueue::willCacheResponse(handle=%p) (%s)", handle, handle->firstRequest().url().string().utf8().data());

        handle->client()->willCacheResponseAsync(handle, cachedResponse.get(), WTFMove(completionHandler));
    };
    
    if (m_messageQueue)
        m_messageQueue->append(makeUnique<Function<void()>>(WTFMove(work)));
    else
        callOnMainThread(WTFMove(work));
    m_semaphore.wait();
    return m_cachedResponseResult.leakRef();
}

void ResourceHandleCFURLConnectionDelegateWithOperationQueue::didReceiveChallenge(CFURLAuthChallengeRef challenge)
{
    auto work = [protectedThis = Ref { *this }, challenge = RetainPtr<CFURLAuthChallengeRef>(challenge)] () mutable {
        auto& handle = protectedThis->m_handle;
        if (!protectedThis->hasHandle())
            return;
        
        LOG(Network, "CFNet - ResourceHandleCFURLConnectionDelegateWithOperationQueue::didReceiveChallenge(handle=%p) (%s)", handle, handle->firstRequest().url().string().utf8().data());

        handle->didReceiveAuthenticationChallenge(AuthenticationChallenge(challenge.get(), handle));
    };

    if (m_messageQueue)
        m_messageQueue->append(makeUnique<Function<void()>>(WTFMove(work)));
    else
        callOnMainThread(WTFMove(work));
}

void ResourceHandleCFURLConnectionDelegateWithOperationQueue::didSendBodyData(CFIndex totalBytesWritten, CFIndex totalBytesExpectedToWrite)
{
    auto work = [protectedThis = Ref { *this }, totalBytesWritten, totalBytesExpectedToWrite] () mutable {
        auto& handle = protectedThis->m_handle;
        if (!protectedThis->hasHandle() || !handle->client() || !handle->connection())
            return;

        LOG(Network, "CFNet - ResourceHandleCFURLConnectionDelegateWithOperationQueue::didSendBodyData(handle=%p) (%s)", handle, handle->firstRequest().url().string().utf8().data());

        handle->client()->didSendData(handle, totalBytesWritten, totalBytesExpectedToWrite);
    };

    if (m_messageQueue)
        m_messageQueue->append(makeUnique<Function<void()>>(WTFMove(work)));
    else
        callOnMainThread(WTFMove(work));
}

Boolean ResourceHandleCFURLConnectionDelegateWithOperationQueue::shouldUseCredentialStorage()
{
    return false;
}

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
Boolean ResourceHandleCFURLConnectionDelegateWithOperationQueue::canRespondToProtectionSpace(CFURLProtectionSpaceRef protectionSpace)
{
    Ref protectedThis { *this };
    auto work = [protectedThis = Ref { *this }, protectionSpace = RetainPtr<CFURLProtectionSpaceRef>(protectionSpace)] () mutable {
        auto& handle = protectedThis->m_handle;
        
        auto completionHandler = [protectedThis = WTFMove(protectedThis)] (bool result) mutable {
            protectedThis->m_boolResult = canAuthenticate;
            protectedThis->m_semaphore.signal();
        }
        
        if (!handle)
            return completionHandler(false);

        LOG(Network, "CFNet - ResourceHandleCFURLConnectionDelegateWithOperationQueue::canRespondToProtectionSpace(handle=%p) (%s)", handle, handle->firstRequest().url().string().utf8().data());

        handle->canAuthenticateAgainstProtectionSpace(ProtectionSpace(protectionSpace.get()), WTFMove(completionHandler));
    };
    
    if (m_messageQueue)
        m_messageQueue->append(makeUnique<Function<void()>>(WTFMove(work)));
    else
        callOnMainThread(WTFMove(work));
    m_semaphore.wait();
    return m_boolResult;
}

void ResourceHandleCFURLConnectionDelegateWithOperationQueue::continueCanAuthenticateAgainstProtectionSpace(bool canAuthenticate)
{
    m_boolResult = canAuthenticate;
    m_semaphore.signal();
}
#endif // USE(PROTECTION_SPACE_AUTH_CALLBACK)

} // namespace WebCore

#endif // USE(CFURLCONNECTION)
