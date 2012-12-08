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

#include "MessageSender.h"
#include "NetworkConnectionToWebProcess.h"
#include "NetworkResourceLoadParameters.h"
#include <WebCore/AuthenticationChallenge.h>
#include <WebCore/ResourceHandleClient.h>
#include <WebCore/ResourceLoaderOptions.h>
#include <WebCore/ResourceRequest.h>

namespace WebCore {
class ResourceBuffer;
class ResourceHandle;
}

namespace WebKit {

class RemoteNetworkingContext;
typedef uint64_t ResourceLoadIdentifier;

class NetworkResourceLoader : public RefCounted<NetworkResourceLoader>, public NetworkConnectionToWebProcessObserver, public WebCore::ResourceHandleClient, public CoreIPC::MessageSender<NetworkResourceLoader> {
public:
    static RefPtr<NetworkResourceLoader> create(const NetworkResourceLoadParameters& parameters, ResourceLoadIdentifier identifier, NetworkConnectionToWebProcess* connection)
    {
        return adoptRef(new NetworkResourceLoader(parameters, identifier, connection));
    }
    
    ~NetworkResourceLoader();

    // Used by MessageSender.
    CoreIPC::Connection* connection() const;
    uint64_t destinationID() const { return identifier(); }
    
    void didReceiveNetworkResourceLoaderMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::MessageDecoder&);

    void start();

    virtual void connectionToWebProcessDidClose(NetworkConnectionToWebProcess*) OVERRIDE;
    
    ResourceLoadIdentifier identifier() const { return m_identifier; }
    WebCore::ResourceLoadPriority priority() const;
    
    NetworkConnectionToWebProcess* connectionToWebProcess() { return m_connection.get(); }

    // ResourceHandleClient methods
    virtual void willSendRequest(WebCore::ResourceHandle*, WebCore::ResourceRequest&, const WebCore::ResourceResponse& /*redirectResponse*/) OVERRIDE;
    virtual void didSendData(WebCore::ResourceHandle*, unsigned long long /*bytesSent*/, unsigned long long /*totalBytesToBeSent*/) OVERRIDE;
    virtual void didReceiveResponse(WebCore::ResourceHandle*, const WebCore::ResourceResponse&) OVERRIDE;
    virtual void didReceiveData(WebCore::ResourceHandle*, const char*, int, int /*encodedDataLength*/) OVERRIDE;
    virtual void didReceiveCachedMetadata(WebCore::ResourceHandle*, const char*, int) OVERRIDE;
    virtual void didFinishLoading(WebCore::ResourceHandle*, double /*finishTime*/) OVERRIDE;
    virtual void didFail(WebCore::ResourceHandle*, const WebCore::ResourceError&) OVERRIDE;
    virtual void wasBlocked(WebCore::ResourceHandle*) OVERRIDE;
    virtual void cannotShowURL(WebCore::ResourceHandle*) OVERRIDE;
    virtual bool shouldUseCredentialStorage(WebCore::ResourceHandle*) OVERRIDE;
    virtual void didReceiveAuthenticationChallenge(WebCore::ResourceHandle*, const WebCore::AuthenticationChallenge&) OVERRIDE;
    virtual void didCancelAuthenticationChallenge(WebCore::ResourceHandle*, const WebCore::AuthenticationChallenge&) OVERRIDE;
    virtual void receivedCancellation(WebCore::ResourceHandle*, const WebCore::AuthenticationChallenge&) OVERRIDE;

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    virtual bool canAuthenticateAgainstProtectionSpace(WebCore::ResourceHandle*, const WebCore::ProtectionSpace&) OVERRIDE;
#endif

#if HAVE(NETWORK_CFDATA_ARRAY_CALLBACK)
    virtual bool supportsDataArray() OVERRIDE;
    virtual void didReceiveDataArray(WebCore::ResourceHandle*, CFArrayRef) OVERRIDE;
#endif

#if PLATFORM(MAC)
#if USE(CFNETWORK)
    virtual CFCachedURLResponseRef willCacheResponse(WebCore::ResourceHandle*, CFCachedURLResponseRef) OVERRIDE;
#else
    virtual NSCachedURLResponse* willCacheResponse(WebCore::ResourceHandle*, NSCachedURLResponse*) OVERRIDE;
#endif
    virtual void willStopBufferingData(WebCore::ResourceHandle*, const char*, int) OVERRIDE;
#endif // PLATFORM(MAC)

#if ENABLE(BLOB)
    virtual WebCore::AsyncFileStream* createAsyncFileStream(WebCore::FileStreamClient*) OVERRIDE;
#endif

private:
    NetworkResourceLoader(const NetworkResourceLoadParameters&, ResourceLoadIdentifier, NetworkConnectionToWebProcess*);

    void willSendRequestHandled(uint64_t requestID, const WebCore::ResourceRequest&);
    void canAuthenticateAgainstProtectionSpaceHandled(uint64_t requestID, bool canAuthenticate);
    void receivedAuthenticationCredential(const WebCore::AuthenticationChallenge&, const WebCore::Credential&);
    void receivedRequestToContinueWithoutAuthenticationCredential(const WebCore::AuthenticationChallenge&);
    void receivedAuthenticationCancellation(const WebCore::AuthenticationChallenge&);
    
    void scheduleStopOnMainThread();
    static void performStops(void*);

    void stop();

    NetworkResourceLoadParameters m_requestParameters;
    ResourceLoadIdentifier m_identifier;

    RefPtr<RemoteNetworkingContext> m_networkingContext;
    RefPtr<WebCore::ResourceHandle> m_handle;    
    RefPtr<NetworkConnectionToWebProcess> m_connection;
    
    OwnPtr<WebCore::AuthenticationChallenge> m_currentAuthenticationChallenge;
};

} // namespace WebKit

#endif // ENABLE(NETWORK_PROCESS)

#endif // NetworkResourceLoader_h
