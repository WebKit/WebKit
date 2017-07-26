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

#if USE(CFURLCONNECTION)
#include "ResourceHandleCFURLConnectionDelegate.h"
#include <CFNetwork/CFURLConnectionPriv.h>
#endif

#if USE(CURL) && PLATFORM(WIN)
#include <winsock2.h>
#include <windows.h>
#endif

#if USE(CURL)
#include "CurlContext.h"
#include "CurlJobManager.h"
#include "FormDataStreamCurl.h"
#include "MultipartHandle.h"
#include <wtf/Lock.h>
#endif

#if USE(SOUP)
#include "GUniquePtrSoup.h"
#include "SoupNetworkSession.h"
#include <libsoup/soup.h>
#include <wtf/RunLoop.h>
#include <wtf/glib/GRefPtr.h>
#endif

#if PLATFORM(COCOA)
OBJC_CLASS NSURLAuthenticationChallenge;
OBJC_CLASS NSURLConnection;
#endif

#if PLATFORM(COCOA) || USE(CFURLCONNECTION)
typedef const struct __CFURLStorageSession* CFURLStorageSessionRef;
#endif

// The allocations and releases in ResourceHandleInternal are
// Cocoa-exception-free (either simple Foundation classes or
// WebCoreResourceLoaderImp which avoids doing work in dealloc).

namespace WebCore {

class ResourceHandleInternal {
    WTF_MAKE_NONCOPYABLE(ResourceHandleInternal); WTF_MAKE_FAST_ALLOCATED;
public:
    ResourceHandleInternal(ResourceHandle* loader, NetworkingContext* context, const ResourceRequest& request, ResourceHandleClient* client, bool defersLoading, bool shouldContentSniff)
        : m_context(context)
        , m_client(client)
        , m_firstRequest(request)
        , m_lastHTTPMethod(request.httpMethod())
        , m_partition(request.cachePartition())
        , m_defersLoading(defersLoading)
        , m_shouldContentSniff(shouldContentSniff)
        , m_usesAsyncCallbacks(client && client->usesAsyncCallbacks())
#if USE(CFURLCONNECTION)
        , m_currentRequest(request)
#endif
#if USE(CURL)
        , m_handle { loader }
        , m_formDataStream { loader }
#endif
#if USE(SOUP)
        , m_timeoutSource(RunLoop::main(), loader, &ResourceHandle::timeoutFired)
#endif
        , m_failureTimer(*loader, &ResourceHandle::failureTimerFired)
    {
        const URL& url = m_firstRequest.url();
        m_user = url.user();
        m_pass = url.pass();
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
    String m_pass;
    
    Credential m_initialCredential;
    
    int status { 0 };

    bool m_defersLoading;
    bool m_shouldContentSniff;
    bool m_usesAsyncCallbacks;
#if USE(CFURLCONNECTION)
    RetainPtr<CFURLConnectionRef> m_connection;
    ResourceRequest m_currentRequest;
    RefPtr<ResourceHandleCFURLConnectionDelegate> m_connectionDelegate;
#endif
#if PLATFORM(COCOA) && !USE(CFURLCONNECTION)
    RetainPtr<NSURLConnection> m_connection;
    RetainPtr<id> m_delegate;
#endif
#if PLATFORM(COCOA)
    bool m_startWhenScheduled { false };
#endif
#if PLATFORM(COCOA) || USE(CFURLCONNECTION)
    RetainPtr<CFURLStorageSessionRef> m_storageSession;
#endif
#if USE(CURL)
    ResourceHandle* m_handle;
    CurlHandle m_curlHandle;

    ResourceResponse m_response;
    bool m_cancelled { false };
    unsigned short m_authFailureCount { 0 };

    FormDataStream m_formDataStream;
    unsigned m_sslErrors { 0 };
    Vector<char> m_postBytes;

    std::unique_ptr<MultipartHandle> m_multipartHandle;
    bool m_addedCacheValidationHeaders { false };
    CurlJobTicket m_job { nullptr };

    Vector<char> m_receivedBuffer;
    Lock m_receivedBufferMutex;

    void initialize();
    void applyAuthentication();
    void setupPOST();
    void setupPUT();
    void setupFormData(bool isPostRequest);

    void didFinish();
    void didFail();

    size_t willPrepareSendData(char* ptr, size_t blockSize, size_t numberOfBlocks);
    void didReceiveHeaderLine(const String& header);
    void didReceiveAllHeaders(long httpCode, long long contentLength);
    void didReceiveContentData();

    void handleLocalReceiveResponse();

    static size_t readCallback(char* ptr, size_t blockSize, size_t numberOfBlocks, void* data);
    static size_t headerCallback(char* ptr, size_t blockSize, size_t numberOfBlocks, void* data);
    static size_t writeCallback(char* ptr, size_t blockSize, size_t numberOfBlocks, void* data);

    void dispatchSynchronousJob();
    void handleDataURL();

    void calculateWebTimingInformations();

#endif

#if USE(SOUP)
    SoupNetworkSession* m_session { nullptr };
    GRefPtr<SoupMessage> m_soupMessage;
    ResourceResponse m_response;
    bool m_cancelled { false };
    GRefPtr<SoupRequest> m_soupRequest;
    GRefPtr<GInputStream> m_inputStream;
    GRefPtr<SoupMultipartInputStream> m_multipartInputStream;
    GRefPtr<GCancellable> m_cancellable;
    GRefPtr<GAsyncResult> m_deferredResult;
    RunLoop::Timer<ResourceHandle> m_timeoutSource;
    GUniquePtr<SoupBuffer> m_soupBuffer;
    unsigned long m_bodySize { 0 };
    unsigned long m_bodyDataSent { 0 };
    SoupSession* soupSession();
    int m_redirectCount { 0 };
    size_t m_previousPosition { 0 };
    bool m_useAuthenticationManager { true };
    struct {
        Credential credential;
        ProtectionSpace protectionSpace;
    } m_credentialDataToSaveInPersistentStore;
#endif

#if PLATFORM(COCOA)
    // We need to keep a reference to the original challenge to be able to cancel it.
    // It is almost identical to m_currentWebChallenge.nsURLAuthenticationChallenge(), but has a different sender.
    NSURLAuthenticationChallenge *m_currentMacChallenge { nil };
#endif

    AuthenticationChallenge m_currentWebChallenge;
    ResourceHandle::FailureType m_scheduledFailureType { ResourceHandle::NoFailure };
    Timer m_failureTimer;
};

} // namespace WebCore
