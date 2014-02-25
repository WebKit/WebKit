/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef NetworkResourceLoader_h
#define NetworkResourceLoader_h

#if ENABLE(NETWORK_PROCESS)

#include "HostRecord.h"
#include "MessageSender.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "ShareableResource.h"
#include <WebCore/ResourceHandleClient.h>
#include <WebCore/ResourceLoaderOptions.h>
#include <WebCore/ResourceRequest.h>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>

typedef const struct _CFCachedURLResponse* CFCachedURLResponseRef;

namespace WebCore {
class ResourceBuffer;
class ResourceHandle;
class ResourceRequest;
}

namespace WebKit {

class NetworkConnectionToWebProcess;
class NetworkLoaderClient;
class NetworkResourceLoadParameters;
class RemoteNetworkingContext;
class SandboxExtension;

class NetworkResourceLoader : public RefCounted<NetworkResourceLoader>, public WebCore::ResourceHandleClient, public IPC::MessageSender {
public:
    static RefPtr<NetworkResourceLoader> create(const NetworkResourceLoadParameters& parameters, NetworkConnectionToWebProcess* connection)
    {
        return adoptRef(new NetworkResourceLoader(parameters, connection, nullptr));
    }
    
    static RefPtr<NetworkResourceLoader> create(const NetworkResourceLoadParameters& parameters, NetworkConnectionToWebProcess* connection, PassRefPtr<Messages::NetworkConnectionToWebProcess::PerformSynchronousLoad::DelayedReply> reply)
    {
        return adoptRef(new NetworkResourceLoader(parameters, connection, reply));
    }    
    ~NetworkResourceLoader();

    NetworkConnectionToWebProcess* connectionToWebProcess() const { return m_connection.get(); }

    WebCore::ResourceLoadPriority priority() { return m_priority; }
    WebCore::ResourceRequest& request() { return m_request; }

    WebCore::ResourceHandle* handle() const { return m_handle.get(); }
    void didConvertHandleToDownload();

    void start();
    void abort();

    // ResourceHandleClient methods
    virtual void willSendRequestAsync(WebCore::ResourceHandle*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse& redirectResponse) override;
    virtual void didSendData(WebCore::ResourceHandle*, unsigned long long bytesSent, unsigned long long totalBytesToBeSent) override;
    virtual void didReceiveResponseAsync(WebCore::ResourceHandle*, const WebCore::ResourceResponse&) override;
    virtual void didReceiveData(WebCore::ResourceHandle*, const char*, unsigned, int encodedDataLength) override;
    virtual void didReceiveBuffer(WebCore::ResourceHandle*, PassRefPtr<WebCore::SharedBuffer>, int encodedDataLength) override;
    virtual void didFinishLoading(WebCore::ResourceHandle*, double finishTime) override;
    virtual void didFail(WebCore::ResourceHandle*, const WebCore::ResourceError&) override;
    virtual void wasBlocked(WebCore::ResourceHandle*) override;
    virtual void cannotShowURL(WebCore::ResourceHandle*) override;
    virtual bool shouldUseCredentialStorage(WebCore::ResourceHandle*) override;
    virtual void shouldUseCredentialStorageAsync(WebCore::ResourceHandle*) override;
    virtual void didReceiveAuthenticationChallenge(WebCore::ResourceHandle*, const WebCore::AuthenticationChallenge&) override;
    virtual void didCancelAuthenticationChallenge(WebCore::ResourceHandle*, const WebCore::AuthenticationChallenge&) override;
    virtual void receivedCancellation(WebCore::ResourceHandle*, const WebCore::AuthenticationChallenge&) override;
    virtual bool usesAsyncCallbacks() override { return true; }

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    virtual void canAuthenticateAgainstProtectionSpaceAsync(WebCore::ResourceHandle*, const WebCore::ProtectionSpace&) override;
#endif

#if USE(NETWORK_CFDATA_ARRAY_CALLBACK)
    virtual bool supportsDataArray() override;
    virtual void didReceiveDataArray(WebCore::ResourceHandle*, CFArrayRef) override;
#endif

#if PLATFORM(COCOA)
    static size_t fileBackedResourceMinimumSize();
#if !PLATFORM(IOS)
    virtual void willCacheResponseAsync(WebCore::ResourceHandle*, NSCachedURLResponse *) override;
#endif
#endif // PLATFORM(MAC)

    // Message handlers.
    void didReceiveNetworkResourceLoaderMessage(IPC::Connection*, IPC::MessageDecoder&);

#if PLATFORM(IOS) || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090)
    static void tryGetShareableHandleFromCFURLCachedResponse(ShareableResource::Handle&, CFCachedURLResponseRef);
#endif

    bool isSynchronous() const;
    bool isLoadingMainResource() const { return m_isLoadingMainResource; }
    
    void setHostRecord(HostRecord* hostRecord) { ASSERT(RunLoop::isMain()); m_hostRecord = hostRecord; }
    HostRecord* hostRecord() const { ASSERT(RunLoop::isMain()); return m_hostRecord.get(); }

    template<typename T>
    bool sendAbortingOnFailure(T&& message, unsigned messageSendFlags = 0)
    {
        bool result = messageSenderConnection()->send(std::forward<T>(message), messageSenderDestinationID(), messageSendFlags);
        if (!result)
            abort();
        return result;
    }


#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    void continueCanAuthenticateAgainstProtectionSpace(bool);
#endif
    void continueWillSendRequest(const WebCore::ResourceRequest& newRequest);

#if PLATFORM(IOS) || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090)
    static void tryGetShareableHandleFromSharedBuffer(ShareableResource::Handle&, WebCore::SharedBuffer*);
#endif

private:
    NetworkResourceLoader(const NetworkResourceLoadParameters&, NetworkConnectionToWebProcess*, PassRefPtr<Messages::NetworkConnectionToWebProcess::PerformSynchronousLoad::DelayedReply>);

    // IPC::MessageSender
    virtual IPC::Connection* messageSenderConnection() override;
    virtual uint64_t messageSenderDestinationID() override { return m_identifier; }

    void continueDidReceiveResponse();

    void cleanup();
    
    void platformDidReceiveResponse(const WebCore::ResourceResponse&);

    void consumeSandboxExtensions();
    void invalidateSandboxExtensions();

    RefPtr<RemoteNetworkingContext> m_networkingContext;
    RefPtr<WebCore::ResourceHandle> m_handle;

    // Keep the suggested request around while asynchronously asking to update it, because some parts of the request don't survive IPC.
    WebCore::ResourceRequest m_suggestedRequestForWillSendRequest;

    uint64_t m_bytesReceived;

    bool m_handleConvertedToDownload;
    std::unique_ptr<NetworkLoaderClient> m_networkLoaderClient;

    ResourceLoadIdentifier m_identifier;
    uint64_t m_webPageID;
    uint64_t m_webFrameID;
    uint64_t m_sessionID;
    WebCore::ResourceRequest m_request;
    WebCore::ResourceLoadPriority m_priority;
    WebCore::ContentSniffingPolicy m_contentSniffingPolicy;
    WebCore::StoredCredentials m_allowStoredCredentials;
    WebCore::ClientCredentialPolicy m_clientCredentialPolicy;
    bool m_shouldClearReferrerOnHTTPSToHTTPRedirect;
    bool m_isLoadingMainResource;

    Vector<RefPtr<SandboxExtension>> m_requestBodySandboxExtensions;
    Vector<RefPtr<SandboxExtension>> m_resourceSandboxExtensions;
    bool m_sandboxExtensionsAreConsumed;

    RefPtr<NetworkConnectionToWebProcess> m_connection;
    
    RefPtr<HostRecord> m_hostRecord;
};

} // namespace WebKit

#endif // ENABLE(NETWORK_PROCESS)

#endif // NetworkResourceLoader_h
