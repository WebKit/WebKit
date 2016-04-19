/*
 * Copyright (C) 2012-2016 Apple Inc. All rights reserved.
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

#include "BlockingResponseMap.h"
#include "Connection.h"
#include "DownloadID.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include <WebCore/ResourceLoadPriority.h>
#include <wtf/HashSet.h>
#include <wtf/RefCounted.h>

namespace WebCore {
class BlobDataFileReference;
class ResourceRequest;
}

namespace WebKit {

class NetworkConnectionToWebProcess;
class NetworkResourceLoader;
class SyncNetworkResourceLoader;
typedef uint64_t ResourceLoadIdentifier;

class NetworkConnectionToWebProcess : public RefCounted<NetworkConnectionToWebProcess>, IPC::Connection::Client {
public:
    static Ref<NetworkConnectionToWebProcess> create(IPC::Connection::Identifier);
    virtual ~NetworkConnectionToWebProcess();

    IPC::Connection* connection() const { return m_connection.get(); }

    void didCleanupResourceLoader(NetworkResourceLoader&);

    RefPtr<WebCore::BlobDataFileReference> getBlobDataFileReferenceForPath(const String& path);

private:
    NetworkConnectionToWebProcess(IPC::Connection::Identifier);

    // IPC::Connection::Client
    void didReceiveMessage(IPC::Connection&, IPC::MessageDecoder&) override;
    void didReceiveSyncMessage(IPC::Connection&, IPC::MessageDecoder&, std::unique_ptr<IPC::MessageEncoder>&) override;
    void didClose(IPC::Connection&) override;
    void didReceiveInvalidMessage(IPC::Connection&, IPC::StringReference messageReceiverName, IPC::StringReference messageName) override;
    IPC::ProcessType localProcessType() override { return IPC::ProcessType::Network; }
    IPC::ProcessType remoteProcessType() override { return IPC::ProcessType::Web; }

    // Message handlers.
    void didReceiveNetworkConnectionToWebProcessMessage(IPC::Connection&, IPC::MessageDecoder&);
    void didReceiveSyncNetworkConnectionToWebProcessMessage(IPC::Connection&, IPC::MessageDecoder&, std::unique_ptr<IPC::MessageEncoder>&);
    
    void scheduleResourceLoad(const NetworkResourceLoadParameters&);
    void performSynchronousLoad(const NetworkResourceLoadParameters&, RefPtr<Messages::NetworkConnectionToWebProcess::PerformSynchronousLoad::DelayedReply>&&);
    void loadPing(const NetworkResourceLoadParameters&);
    void prefetchDNS(const String&);

    void removeLoadIdentifier(ResourceLoadIdentifier);
    void setDefersLoading(ResourceLoadIdentifier, bool);
    void crossOriginRedirectReceived(ResourceLoadIdentifier, const WebCore::URL& redirectURL);
    void startDownload(WebCore::SessionID, DownloadID, const WebCore::ResourceRequest&, const String& suggestedName = { });
    void convertMainResourceLoadToDownload(WebCore::SessionID, uint64_t mainResourceLoadIdentifier, DownloadID, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&);

    void cookiesForDOM(WebCore::SessionID, const WebCore::URL& firstParty, const WebCore::URL&, String& result);
    void setCookiesFromDOM(WebCore::SessionID, const WebCore::URL& firstParty, const WebCore::URL&, const String&);
    void cookiesEnabled(WebCore::SessionID, const WebCore::URL& firstParty, const WebCore::URL&, bool& result);
    void cookieRequestHeaderFieldValue(WebCore::SessionID, const WebCore::URL& firstParty, const WebCore::URL&, String& result);
    void getRawCookies(WebCore::SessionID, const WebCore::URL& firstParty, const WebCore::URL&, Vector<WebCore::Cookie>&);
    void deleteCookie(WebCore::SessionID, const WebCore::URL&, const String& cookieName);
    void addCookie(WebCore::SessionID, const WebCore::URL&, const WebCore::Cookie&);

    void registerFileBlobURL(const WebCore::URL&, const String& path, const SandboxExtension::Handle&, const String& contentType);
    void registerBlobURL(const WebCore::URL&, Vector<WebCore::BlobPart>, const String& contentType);
    void registerBlobURLFromURL(const WebCore::URL&, const WebCore::URL& srcURL);
    void preregisterSandboxExtensionsForOptionallyFileBackedBlob(const Vector<String>& fileBackedPath, const SandboxExtension::HandleArray&);
    void registerBlobURLOptionallyFileBacked(const WebCore::URL&, const WebCore::URL& srcURL, const String& fileBackedPath);
    void registerBlobURLForSlice(const WebCore::URL&, const WebCore::URL& srcURL, int64_t start, int64_t end);
    void blobSize(const WebCore::URL&, uint64_t& resultSize);
    void unregisterBlobURL(const WebCore::URL&);
    void writeBlobsToTemporaryFiles(const Vector<String>& blobURLs, uint64_t requestIdentifier);

    void ensureLegacyPrivateBrowsingSession();

    RefPtr<IPC::Connection> m_connection;

    HashMap<ResourceLoadIdentifier, RefPtr<NetworkResourceLoader>> m_networkResourceLoaders;
    HashMap<String, RefPtr<WebCore::BlobDataFileReference>> m_blobDataFileReferences;
};

} // namespace WebKit

#endif // NetworkConnectionToWebProcess_h
