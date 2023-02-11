/*
 * Copyright (C) 2004, 2006 Apple Inc.  All rights reserved.
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

#pragma once

#include "AuthenticationChallenge.h"
#include "NetworkingContext.h"
#include "ResourceHandle.h"
#include "ResourceHandleClient.h"
#include "ResourceRequest.h"
#include "Timer.h"
#include <wtf/MonotonicTime.h>

#if USE(CURL)
#include "CurlRequest.h"
#include "SynchronousLoaderClient.h"
#include <wtf/MessageQueue.h>
#include <wtf/MonotonicTime.h>
#endif

#if PLATFORM(COCOA)
OBJC_CLASS NSURLAuthenticationChallenge;
OBJC_CLASS NSURLConnection;

typedef const struct __CFURLStorageSession* CFURLStorageSessionRef;
#endif

// The allocations and releases in ResourceHandleInternal are
// Cocoa-exception-free (either simple Foundation classes or
// WebCoreResourceLoaderImp which avoids doing work in dealloc).

namespace WebCore {

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(ResourceHandleInternal);
class ResourceHandleInternal {
    WTF_MAKE_NONCOPYABLE(ResourceHandleInternal);
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(ResourceHandleInternal);
public:
    ResourceHandleInternal(ResourceHandle* loader, NetworkingContext* context, const ResourceRequest& request, ResourceHandleClient* client, bool defersLoading, bool shouldContentSniff, ContentEncodingSniffingPolicy contentEncodingSniffingPolicy, RefPtr<SecurityOrigin>&& sourceOrigin, bool isMainFrameNavigation)
        : m_context(context)
        , m_client(client)
        , m_firstRequest(request)
        , m_lastHTTPMethod(request.httpMethod())
        , m_partition(request.cachePartition())
        , m_failureTimer(*loader, &ResourceHandle::failureTimerFired)
        , m_sourceOrigin(WTFMove(sourceOrigin))
        , m_contentEncodingSniffingPolicy(contentEncodingSniffingPolicy)
        , m_defersLoading(defersLoading)
        , m_shouldContentSniff(shouldContentSniff)
        , m_isMainFrameNavigation(isMainFrameNavigation)
    {
        const URL& url = m_firstRequest.url();
        m_user = url.user();
        m_password = url.password();
        m_firstRequest.removeCredentials();
    }
    
    ~ResourceHandleInternal();

    ResourceHandleClient* client() { return m_client; }

    RefPtr<NetworkingContext> m_context;
    ResourceHandleClient* m_client;
    ResourceRequest m_firstRequest;
    String m_lastHTTPMethod;
    String m_partition;

    // Suggested credentials for the current redirection step.
    String m_user;
    String m_password;
    
    Credential m_initialCredential;
    
#if PLATFORM(COCOA)
    RetainPtr<NSURLConnection> m_connection;
    RetainPtr<id> m_delegate;
    RetainPtr<CFURLStorageSessionRef> m_storageSession;

    // We need to keep a reference to the original challenge to be able to cancel it.
    // It is almost identical to m_currentWebChallenge.nsURLAuthenticationChallenge(), but has a different sender.
    NSURLAuthenticationChallenge *m_currentMacChallenge { nil };
#endif
#if USE(CURL)
    std::unique_ptr<CurlResourceHandleDelegate> m_delegate;
    
    unsigned m_authFailureCount { 0 };
    RefPtr<CurlRequest> m_curlRequest;
    RefPtr<SynchronousLoaderMessageQueue> m_messageQueue;
#endif
    Box<NetworkLoadMetrics> m_networkLoadMetrics;
    MonotonicTime m_startTime;

    AuthenticationChallenge m_currentWebChallenge;
    Timer m_failureTimer;
    RefPtr<SecurityOrigin> m_sourceOrigin;

    int status { 0 };

    uint16_t m_redirectCount { 0 };

    ResourceHandle::FailureType m_scheduledFailureType { ResourceHandle::NoFailure };
    ContentEncodingSniffingPolicy m_contentEncodingSniffingPolicy;

    bool m_defersLoading;
    bool m_shouldContentSniff;
    bool m_failsTAOCheck { false };
    bool m_hasCrossOriginRedirect { false };
    bool m_isCrossOrigin { false };
    bool m_isMainFrameNavigation { false };
#if PLATFORM(COCOA)
    bool m_startWhenScheduled { false };
#endif
#if USE(CURL)
    bool m_cancelled { false };
    bool m_addedCacheValidationHeaders { false };
#endif
};

} // namespace WebCore
