/*
 * Copyright (C) 2012-2015 Apple Inc. All rights reserved.
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

#include "MessageSender.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkLoadClient.h"
#include "NetworkResourceLoadParameters.h"
#include "ShareableResource.h"
#include <WebCore/Timer.h>
#include <wtf/Optional.h>

namespace WebCore {
class BlobDataFileReference;
class ResourceRequest;
}

namespace WebKit {

class NetworkConnectionToWebProcess;
class NetworkLoad;
class SandboxExtension;

namespace NetworkCache {
class Entry;
}

class NetworkResourceLoader final : public RefCounted<NetworkResourceLoader>, public NetworkLoadClient, public IPC::MessageSender {
public:
    static Ref<NetworkResourceLoader> create(const NetworkResourceLoadParameters& parameters, NetworkConnectionToWebProcess& connection, RefPtr<Messages::NetworkConnectionToWebProcess::PerformSynchronousLoad::DelayedReply>&& reply = nullptr)
    {
        return adoptRef(*new NetworkResourceLoader(parameters, connection, WTFMove(reply)));
    }
    virtual ~NetworkResourceLoader();

    const WebCore::ResourceRequest& originalRequest() const { return m_parameters.request; }

    NetworkLoad* networkLoad() const { return m_networkLoad.get(); }

    void start();
    void abort();

    void setDefersLoading(bool);

    // Message handlers.
    void didReceiveNetworkResourceLoaderMessage(IPC::Connection&, IPC::MessageDecoder&);

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    void continueCanAuthenticateAgainstProtectionSpace(bool);
#endif
    void continueWillSendRequest(const WebCore::ResourceRequest& newRequest);

    WebCore::SharedBuffer* bufferedData() { return m_bufferedData.get(); }
    const WebCore::ResourceResponse& response() const { return m_response; }

    NetworkConnectionToWebProcess& connectionToWebProcess() { return m_connection; }
    WebCore::SessionID sessionID() const { return m_parameters.sessionID; }
    ResourceLoadIdentifier identifier() const { return m_parameters.identifier; }

    struct SynchronousLoadData;

    // NetworkLoadClient.
    void didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent) override;
    void canAuthenticateAgainstProtectionSpaceAsync(const WebCore::ProtectionSpace&) override;
    bool isSynchronous() const override;
    void willSendRedirectedRequest(const WebCore::ResourceRequest&, const WebCore::ResourceRequest& redirectRequest, const WebCore::ResourceResponse& redirectResponse) override;
    ShouldContinueDidReceiveResponse didReceiveResponse(const WebCore::ResourceResponse&) override;
    void didReceiveBuffer(RefPtr<WebCore::SharedBuffer>&&, int reportedEncodedDataLength) override;
    void didFinishLoading(double finishTime) override;
    void didFailLoading(const WebCore::ResourceError&) override;
#if USE(NETWORK_SESSION)
    void didBecomeDownload() override;
#endif
    
    void didConvertToDownload();

private:
    NetworkResourceLoader(const NetworkResourceLoadParameters&, NetworkConnectionToWebProcess&, RefPtr<Messages::NetworkConnectionToWebProcess::PerformSynchronousLoad::DelayedReply>&&);

    // IPC::MessageSender
    IPC::Connection* messageSenderConnection() override;
    uint64_t messageSenderDestinationID() override { return m_parameters.identifier; }

#if ENABLE(NETWORK_CACHE)
    bool canUseCache(const WebCore::ResourceRequest&) const;
    bool canUseCachedRedirect(const WebCore::ResourceRequest&) const;

    void tryStoreAsCacheEntry();
    void retrieveCacheEntry(const WebCore::ResourceRequest&);
    void didRetrieveCacheEntry(std::unique_ptr<NetworkCache::Entry>);
    void validateCacheEntry(std::unique_ptr<NetworkCache::Entry>);
    void dispatchWillSendRequestForCacheEntry(std::unique_ptr<NetworkCache::Entry>);
#endif

    void startNetworkLoad(const WebCore::ResourceRequest&);
    void continueDidReceiveResponse();

    void cleanup();
    
    void platformDidReceiveResponse(const WebCore::ResourceResponse&);

    void startBufferingTimerIfNeeded();
    void bufferingTimerFired();
    bool sendBufferMaybeAborting(WebCore::SharedBuffer&, size_t encodedDataLength);

    void consumeSandboxExtensions();
    void invalidateSandboxExtensions();

    template<typename T> bool sendAbortingOnFailure(T&& message, unsigned messageSendFlags = 0);

    const NetworkResourceLoadParameters m_parameters;

    Ref<NetworkConnectionToWebProcess> m_connection;

    std::unique_ptr<NetworkLoad> m_networkLoad;

    WebCore::ResourceResponse m_response;

    size_t m_bytesReceived { 0 };
    size_t m_bufferedDataEncodedDataLength { 0 };
    RefPtr<WebCore::SharedBuffer> m_bufferedData;
    unsigned m_redirectCount { 0 };

    std::unique_ptr<SynchronousLoadData> m_synchronousLoadData;
    Vector<RefPtr<WebCore::BlobDataFileReference>> m_fileReferences;

    bool m_didConvertToDownload { false };
    bool m_didConsumeSandboxExtensions { false };
    bool m_defersLoading { false };

    WebCore::Timer m_bufferingTimer;
#if ENABLE(NETWORK_CACHE)
    RefPtr<WebCore::SharedBuffer> m_bufferedDataForCache;
    std::unique_ptr<NetworkCache::Entry> m_cacheEntryForValidation;
    bool m_isWaitingContinueWillSendRequestForCachedRedirect { false };
#endif
};

} // namespace WebKit

#endif // NetworkResourceLoader_h
