/*
 * Copyright (C) 2005, 2006, 2011 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#ifndef ResourceLoader_h
#define ResourceLoader_h

#include "ResourceHandleClient.h"
#include "ResourceLoaderOptions.h"
#include "ResourceLoaderTypes.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"

#include <wtf/Forward.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class AuthenticationChallenge;
class DocumentLoader;
class Frame;
class FrameLoader;
class URL;
class ResourceBuffer;
class ResourceHandle;

class ResourceLoader : public RefCounted<ResourceLoader>, protected ResourceHandleClient {
public:
    virtual ~ResourceLoader();

    void cancel();

    virtual bool init(const ResourceRequest&);

#if PLATFORM(IOS)
    virtual bool startLoading()
    {
        start();
        return true;
    }

    virtual const ResourceRequest& iOSOriginalRequest() const { return request(); }
    virtual RetainPtr<CFDictionaryRef> connectionProperties(ResourceHandle*) override;
#endif

    FrameLoader* frameLoader() const;
    DocumentLoader* documentLoader() const { return m_documentLoader.get(); }
    const ResourceRequest& originalRequest() const { return m_originalRequest; }
    
    virtual void cancel(const ResourceError&);
    ResourceError cancelledError();
    ResourceError blockedError();
    ResourceError cannotShowURLError();
    
    virtual void setDefersLoading(bool);
    bool defersLoading() const { return m_defersLoading; }

    unsigned long identifier() const { return m_identifier; }

    virtual void releaseResources();
    const ResourceResponse& response() const;

    ResourceBuffer* resourceData() const { return m_resourceData.get(); }
    void clearResourceData();
    
    virtual bool isSubresourceLoader();
    
    virtual void willSendRequest(ResourceRequest&, const ResourceResponse& redirectResponse);
    virtual void didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent);
    virtual void didReceiveResponse(const ResourceResponse&);
    virtual void didReceiveData(const char*, unsigned, long long encodedDataLength, DataPayloadType);
    virtual void didReceiveBuffer(PassRefPtr<SharedBuffer>, long long encodedDataLength, DataPayloadType);
    virtual void didFinishLoading(double finishTime);
    virtual void didFail(const ResourceError&);
#if USE(NETWORK_CFDATA_ARRAY_CALLBACK)
    virtual void didReceiveDataArray(CFArrayRef dataArray);
#endif
    void didChangePriority(ResourceLoadPriority);

    virtual bool shouldUseCredentialStorage();
    virtual void didReceiveAuthenticationChallenge(const AuthenticationChallenge&);
    void didCancelAuthenticationChallenge(const AuthenticationChallenge&);
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    virtual bool canAuthenticateAgainstProtectionSpace(const ProtectionSpace&);
#endif
    virtual void receivedCancellation(const AuthenticationChallenge&);

    // ResourceHandleClient
    virtual void willSendRequest(ResourceHandle*, ResourceRequest&, const ResourceResponse& redirectResponse) override;
    virtual void didSendData(ResourceHandle*, unsigned long long bytesSent, unsigned long long totalBytesToBeSent) override;
    virtual void didReceiveResponse(ResourceHandle*, const ResourceResponse&) override;
    virtual void didReceiveData(ResourceHandle*, const char*, unsigned, int encodedDataLength) override;
    virtual void didReceiveBuffer(ResourceHandle*, PassRefPtr<SharedBuffer>, int encodedDataLength) override;
    virtual void didFinishLoading(ResourceHandle*, double finishTime) override;
    virtual void didFail(ResourceHandle*, const ResourceError&) override;
    virtual void wasBlocked(ResourceHandle*) override;
    virtual void cannotShowURL(ResourceHandle*) override;
    virtual bool shouldUseCredentialStorage(ResourceHandle*) override { return shouldUseCredentialStorage(); }
    virtual void didReceiveAuthenticationChallenge(ResourceHandle*, const AuthenticationChallenge& challenge) override { didReceiveAuthenticationChallenge(challenge); } 
    virtual void didCancelAuthenticationChallenge(ResourceHandle*, const AuthenticationChallenge& challenge) override { didCancelAuthenticationChallenge(challenge); } 
#if USE(NETWORK_CFDATA_ARRAY_CALLBACK)
    virtual void didReceiveDataArray(ResourceHandle*, CFArrayRef dataArray) override;
#endif
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    virtual bool canAuthenticateAgainstProtectionSpace(ResourceHandle*, const ProtectionSpace& protectionSpace) override { return canAuthenticateAgainstProtectionSpace(protectionSpace); }
#endif
    virtual void receivedCancellation(ResourceHandle*, const AuthenticationChallenge& challenge) override { receivedCancellation(challenge); }
#if PLATFORM(COCOA)
#if USE(CFNETWORK)
    virtual CFCachedURLResponseRef willCacheResponse(ResourceHandle*, CFCachedURLResponseRef) override;
#else
    virtual NSCachedURLResponse* willCacheResponse(ResourceHandle*, NSCachedURLResponse*) override;
#endif
#endif // PLATFORM(COCOA)
#if PLATFORM(WIN) && USE(CFNETWORK)
    // FIXME: Windows should use willCacheResponse - <https://bugs.webkit.org/show_bug.cgi?id=57257>.
    virtual bool shouldCacheResponse(ResourceHandle*, CFCachedURLResponseRef) override;
#endif

    const URL& url() const { return m_request.url(); } 
    ResourceHandle* handle() const { return m_handle.get(); }
    bool shouldSendResourceLoadCallbacks() const { return m_options.sendLoadCallbacks == SendCallbacks; }
    void setSendCallbackPolicy(SendCallbackPolicy sendLoadCallbacks) { m_options.sendLoadCallbacks = sendLoadCallbacks; }
    bool shouldSniffContent() const { return m_options.sniffContent == SniffContent; }
    ClientCredentialPolicy clientCredentialPolicy() const { return m_options.clientCredentialPolicy; }

    bool reachedTerminalState() const { return m_reachedTerminalState; }

    const ResourceRequest& request() const { return m_request; }

    void setDataBufferingPolicy(DataBufferingPolicy);

protected:
    ResourceLoader(Frame*, ResourceLoaderOptions);

    friend class ResourceLoadScheduler; // for access to start()
    // start() actually sends the load to the network (unless the load is being
    // deferred) and should only be called by ResourceLoadScheduler or setDefersLoading().
    void start();

    void didFinishLoadingOnePart(double finishTime);
    void cleanupForError(const ResourceError&);

    bool wasCancelled() const { return m_cancellationStatus >= Cancelled; }

    void didReceiveDataOrBuffer(const char*, unsigned, PassRefPtr<SharedBuffer>, long long encodedDataLength, DataPayloadType);

    const ResourceLoaderOptions& options() { return m_options; }

    RefPtr<ResourceHandle> m_handle;
    RefPtr<Frame> m_frame;
    RefPtr<DocumentLoader> m_documentLoader;
    ResourceResponse m_response;
    
private:
    virtual void willCancel(const ResourceError&) = 0;
    virtual void didCancel(const ResourceError&) = 0;

    void addDataOrBuffer(const char*, unsigned, SharedBuffer*, DataPayloadType);

    ResourceRequest m_request;
    ResourceRequest m_originalRequest; // Before redirects.
    RefPtr<ResourceBuffer> m_resourceData;
    
    unsigned long m_identifier;

    bool m_reachedTerminalState;
    bool m_notifiedLoadComplete;

    enum CancellationStatus {
        NotCancelled,
        CalledWillCancel,
        Cancelled,
        FinishedCancel
    };
    CancellationStatus m_cancellationStatus;

    bool m_defersLoading;
    ResourceRequest m_deferredRequest;
    ResourceLoaderOptions m_options;
};

inline const ResourceResponse& ResourceLoader::response() const
{
    return m_response;
}

}

#endif
