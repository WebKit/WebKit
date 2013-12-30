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

#ifndef NetworkConnectionToWebProcess_h
#define NetworkConnectionToWebProcess_h

#if ENABLE(NETWORK_PROCESS)

#include "BlockingResponseMap.h"
#include "Connection.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include <WebCore/ResourceLoadPriority.h>
#include <wtf/HashSet.h>
#include <wtf/RefCounted.h>

namespace WebCore {
class ResourceRequest;
}

namespace WebKit {

class BlobRegistrationData;
class NetworkConnectionToWebProcess;
class NetworkResourceLoader;
class SyncNetworkResourceLoader;
typedef uint64_t ResourceLoadIdentifier;

class NetworkConnectionToWebProcess : public RefCounted<NetworkConnectionToWebProcess>, IPC::Connection::Client {
public:
    static PassRefPtr<NetworkConnectionToWebProcess> create(IPC::Connection::Identifier);
    virtual ~NetworkConnectionToWebProcess();

    IPC::Connection* connection() const { return m_connection.get(); }

    bool isSerialLoadingEnabled() const { return m_serialLoadingEnabled; }

private:
    NetworkConnectionToWebProcess(IPC::Connection::Identifier);

    // IPC::Connection::Client
    virtual void didReceiveMessage(IPC::Connection*, IPC::MessageDecoder&);
    virtual void didReceiveSyncMessage(IPC::Connection*, IPC::MessageDecoder&, std::unique_ptr<IPC::MessageEncoder>&);
    virtual void didClose(IPC::Connection*);
    virtual void didReceiveInvalidMessage(IPC::Connection*, IPC::StringReference messageReceiverName, IPC::StringReference messageName);

    // Message handlers.
    void didReceiveNetworkConnectionToWebProcessMessage(IPC::Connection*, IPC::MessageDecoder&);
    void didReceiveSyncNetworkConnectionToWebProcessMessage(IPC::Connection*, IPC::MessageDecoder&, std::unique_ptr<IPC::MessageEncoder>&);
    
    void scheduleResourceLoad(const NetworkResourceLoadParameters&);
    void performSynchronousLoad(const NetworkResourceLoadParameters&, PassRefPtr<Messages::NetworkConnectionToWebProcess::PerformSynchronousLoad::DelayedReply>);

    void removeLoadIdentifier(ResourceLoadIdentifier);
    void crossOriginRedirectReceived(ResourceLoadIdentifier, const WebCore::URL& redirectURL);
    void servePendingRequests(uint32_t resourceLoadPriority);
    void setSerialLoadingEnabled(bool);
    void startDownload(bool privateBrowsingEnabled, uint64_t downloadID, const WebCore::ResourceRequest&);
    void convertMainResourceLoadToDownload(uint64_t mainResourceLoadIdentifier, uint64_t downloadID, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&);

    void cookiesForDOM(bool privateBrowsingEnabled, const WebCore::URL& firstParty, const WebCore::URL&, String& result);
    void setCookiesFromDOM(bool privateBrowsingEnabled, const WebCore::URL& firstParty, const WebCore::URL&, const String&);
    void cookiesEnabled(bool privateBrowsingEnabled, const WebCore::URL& firstParty, const WebCore::URL&, bool& result);
    void cookieRequestHeaderFieldValue(bool privateBrowsingEnabled, const WebCore::URL& firstParty, const WebCore::URL&, String& result);
    void getRawCookies(bool privateBrowsingEnabled, const WebCore::URL& firstParty, const WebCore::URL&, Vector<WebCore::Cookie>&);
    void deleteCookie(bool privateBrowsingEnabled, const WebCore::URL&, const String& cookieName);

    void registerBlobURL(const WebCore::URL&, const BlobRegistrationData&);
    void registerBlobURLFromURL(const WebCore::URL&, const WebCore::URL& srcURL);
    void unregisterBlobURL(const WebCore::URL&);

    RefPtr<IPC::Connection> m_connection;

    HashMap<ResourceLoadIdentifier, RefPtr<NetworkResourceLoader>> m_networkResourceLoaders;

    bool m_serialLoadingEnabled;
};

} // namespace WebKit

#endif // ENABLE(NETWORK_PROCESS)

#endif // NetworkConnectionToWebProcess_h
