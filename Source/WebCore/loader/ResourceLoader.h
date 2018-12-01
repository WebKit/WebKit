/*
 * Copyright (C) 2005-2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "LoadTiming.h"
#include "ResourceHandleClient.h"
#include "ResourceLoaderOptions.h"
#include "ResourceLoaderTypes.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include <wtf/Forward.h>

#if ENABLE(CONTENT_EXTENSIONS)
#include "ResourceLoadInfo.h"
#endif

namespace WTF {
class SchedulePair;
}

namespace WebCore {

class AuthenticationChallenge;
class DocumentLoader;
class Frame;
class FrameLoader;
class NetworkLoadMetrics;
class PreviewLoader;

class ResourceLoader : public RefCounted<ResourceLoader>, protected ResourceHandleClient {
public:
    virtual ~ResourceLoader() = 0;

    WEBCORE_EXPORT void cancel();

    virtual void init(ResourceRequest&&, CompletionHandler<void(bool)>&&);

    void deliverResponseAndData(const ResourceResponse&, RefPtr<SharedBuffer>&&);

#if PLATFORM(IOS_FAMILY)
    virtual void startLoading()
    {
        start();
    }

    virtual const ResourceRequest& iOSOriginalRequest() const { return request(); }
#endif

    WEBCORE_EXPORT FrameLoader* frameLoader() const;
    DocumentLoader* documentLoader() const { return m_documentLoader.get(); }
    const ResourceRequest& originalRequest() const { return m_originalRequest; }

    WEBCORE_EXPORT void start();
    WEBCORE_EXPORT void cancel(const ResourceError&);
    WEBCORE_EXPORT ResourceError cancelledError();
    ResourceError blockedError();
    ResourceError blockedByContentBlockerError();
    ResourceError cannotShowURLError();
    
    virtual void setDefersLoading(bool);
    bool defersLoading() const { return m_defersLoading; }

    unsigned long identifier() const { return m_identifier; }

    bool wasAuthenticationChallengeBlocked() const { return m_wasAuthenticationChallengeBlocked; }

    virtual void releaseResources();
    const ResourceResponse& response() const { return m_response; }

    SharedBuffer* resourceData() const { return m_resourceData.get(); }
    void clearResourceData();
    
    virtual bool isSubresourceLoader() const;

    virtual void willSendRequest(ResourceRequest&&, const ResourceResponse& redirectResponse, CompletionHandler<void(ResourceRequest&&)>&& callback);
    virtual void didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent);
    virtual void didReceiveResponse(const ResourceResponse&, CompletionHandler<void()>&& policyCompletionHandler);
    virtual void didReceiveData(const char*, unsigned, long long encodedDataLength, DataPayloadType);
    virtual void didReceiveBuffer(Ref<SharedBuffer>&&, long long encodedDataLength, DataPayloadType);
    virtual void didFinishLoading(const NetworkLoadMetrics&);
    virtual void didFail(const ResourceError&);
    virtual void didRetrieveDerivedDataFromCache(const String& type, SharedBuffer&);

    WEBCORE_EXPORT void didBlockAuthenticationChallenge();

    virtual bool shouldUseCredentialStorage();
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    virtual bool canAuthenticateAgainstProtectionSpace(const ProtectionSpace&);
#endif
    virtual void receivedCancellation(const AuthenticationChallenge&);

#if USE(QUICK_LOOK)
    bool isQuickLookResource() const;
#endif

    const URL& url() const { return m_request.url(); }
    ResourceHandle* handle() const { return m_handle.get(); }
    bool shouldSendResourceLoadCallbacks() const { return m_options.sendLoadCallbacks == SendCallbackPolicy::SendCallbacks; }
    void setSendCallbackPolicy(SendCallbackPolicy sendLoadCallbacks) { m_options.sendLoadCallbacks = sendLoadCallbacks; }
    bool shouldSniffContent() const { return m_options.sniffContent == ContentSniffingPolicy::SniffContent; }
    bool shouldSniffContentEncoding() const { return m_options.sniffContentEncoding == ContentEncodingSniffingPolicy::Sniff; }
    WEBCORE_EXPORT bool isAllowedToAskUserForCredentials() const;
    WEBCORE_EXPORT bool shouldIncludeCertificateInfo() const;

    bool reachedTerminalState() const { return m_reachedTerminalState; }


    const ResourceRequest& request() const { return m_request; }
    void setRequest(ResourceRequest&& request) { m_request = WTFMove(request); }

    void setDataBufferingPolicy(DataBufferingPolicy);

    void willSwitchToSubstituteResource();

    const LoadTiming& loadTiming() { return m_loadTiming; }

#if PLATFORM(COCOA)
    void schedule(WTF::SchedulePair&);
    void unschedule(WTF::SchedulePair&);
#endif

    const Frame* frame() const { return m_frame.get(); }
    WEBCORE_EXPORT bool isAlwaysOnLoggingAllowed() const;

    const ResourceLoaderOptions& options() const { return m_options; }

    const ResourceRequest& deferredRequest() const { return m_deferredRequest; }
    ResourceRequest takeDeferredRequest() { return std::exchange(m_deferredRequest, { }); }

protected:
    ResourceLoader(Frame&, ResourceLoaderOptions);

    void didFinishLoadingOnePart(const NetworkLoadMetrics&);
    void cleanupForError(const ResourceError&);

    bool wasCancelled() const { return m_cancellationStatus >= Cancelled; }

    void didReceiveDataOrBuffer(const char*, unsigned, RefPtr<SharedBuffer>&&, long long encodedDataLength, DataPayloadType);
    
    void setReferrerPolicy(ReferrerPolicy referrerPolicy) { m_options.referrerPolicy = referrerPolicy; }
    ReferrerPolicy referrerPolicy() const { return m_options.referrerPolicy; }

#if PLATFORM(COCOA)
    void willCacheResponseAsync(ResourceHandle*, NSCachedURLResponse*, CompletionHandler<void(NSCachedURLResponse *)>&&) override;
#endif

    virtual void willSendRequestInternal(ResourceRequest&&, const ResourceResponse& redirectResponse, CompletionHandler<void(ResourceRequest&&)>&&);

    RefPtr<ResourceHandle> m_handle;
    RefPtr<Frame> m_frame;
    RefPtr<DocumentLoader> m_documentLoader;
    ResourceResponse m_response;
    LoadTiming m_loadTiming;
#if USE(QUICK_LOOK)
    std::unique_ptr<PreviewLoader> m_previewLoader;
#endif
    bool m_canCrossOriginRequestsAskUserForCredentials { true };

private:
    virtual void willCancel(const ResourceError&) = 0;
    virtual void didCancel(const ResourceError&) = 0;

    void addDataOrBuffer(const char*, unsigned, SharedBuffer*, DataPayloadType);
    void loadDataURL();
    void finishNetworkLoad();

    bool shouldAllowResourceToAskForCredentials() const;

    // ResourceHandleClient
    void didSendData(ResourceHandle*, unsigned long long bytesSent, unsigned long long totalBytesToBeSent) override;
    void didReceiveResponseAsync(ResourceHandle*, ResourceResponse&&, CompletionHandler<void()>&&) override;
    void willSendRequestAsync(ResourceHandle*, ResourceRequest&&, ResourceResponse&&, CompletionHandler<void(ResourceRequest&&)>&&) override;
    void didReceiveData(ResourceHandle*, const char*, unsigned, int encodedDataLength) override;
    void didReceiveBuffer(ResourceHandle*, Ref<SharedBuffer>&&, int encodedDataLength) override;
    void didFinishLoading(ResourceHandle*) override;
    void didFail(ResourceHandle*, const ResourceError&) override;
    void wasBlocked(ResourceHandle*) override;
    void cannotShowURL(ResourceHandle*) override;
    bool shouldUseCredentialStorage(ResourceHandle*) override { return shouldUseCredentialStorage(); }
    void didReceiveAuthenticationChallenge(ResourceHandle*, const AuthenticationChallenge&) override;
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    void canAuthenticateAgainstProtectionSpaceAsync(ResourceHandle*, const ProtectionSpace&, CompletionHandler<void(bool)>&&) override;
#endif
    void receivedCancellation(ResourceHandle*, const AuthenticationChallenge& challenge) override { receivedCancellation(challenge); }
#if PLATFORM(IOS_FAMILY)
    RetainPtr<CFDictionaryRef> connectionProperties(ResourceHandle*) override;
#endif
#if USE(CFURLCONNECTION)
    // FIXME: Windows should use willCacheResponse - <https://bugs.webkit.org/show_bug.cgi?id=57257>.
    bool shouldCacheResponse(ResourceHandle*, CFCachedURLResponseRef) override;
#endif

#if USE(SOUP)
    void loadGResource();
#endif

    ResourceRequest m_request;
    ResourceRequest m_originalRequest; // Before redirects.
    RefPtr<SharedBuffer> m_resourceData;
    
    unsigned long m_identifier { 0 };

    bool m_reachedTerminalState { false };
    bool m_notifiedLoadComplete { false };

    enum CancellationStatus {
        NotCancelled,
        CalledWillCancel,
        Cancelled,
        FinishedCancel
    };
    CancellationStatus m_cancellationStatus { NotCancelled };

    bool m_defersLoading;
    bool m_wasAuthenticationChallengeBlocked { false };
    ResourceRequest m_deferredRequest;
    ResourceLoaderOptions m_options;

#if ENABLE(CONTENT_EXTENSIONS)
protected:
    ResourceType m_resourceType { ResourceType::Invalid };
#endif
};

} // namespace WebCore
