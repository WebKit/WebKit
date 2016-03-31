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

#include "config.h"
#include "NetworkConnectionToWebProcess.h"

#include "NetworkBlobRegistry.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkLoad.h"
#include "NetworkProcess.h"
#include "NetworkResourceLoadParameters.h"
#include "NetworkResourceLoader.h"
#include "NetworkResourceLoaderMessages.h"
#include "RemoteNetworkingContext.h"
#include "SessionTracker.h"
#include <WebCore/NotImplemented.h>
#include <WebCore/PingHandle.h>
#include <WebCore/PlatformCookieJar.h>
#include <WebCore/ResourceLoaderOptions.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/SessionID.h>
#include <wtf/RunLoop.h>

#if USE(NETWORK_SESSION)
#include "PingLoad.h"
#endif

using namespace WebCore;

namespace WebKit {

Ref<NetworkConnectionToWebProcess> NetworkConnectionToWebProcess::create(IPC::Connection::Identifier connectionIdentifier)
{
    return adoptRef(*new NetworkConnectionToWebProcess(connectionIdentifier));
}

NetworkConnectionToWebProcess::NetworkConnectionToWebProcess(IPC::Connection::Identifier connectionIdentifier)
{
    m_connection = IPC::Connection::createServerConnection(connectionIdentifier, *this);
    m_connection->open();
}

NetworkConnectionToWebProcess::~NetworkConnectionToWebProcess()
{
}

void NetworkConnectionToWebProcess::didCleanupResourceLoader(NetworkResourceLoader& loader)
{
    ASSERT(m_networkResourceLoaders.get(loader.identifier()) == &loader);

    m_networkResourceLoaders.remove(loader.identifier());
}
    
void NetworkConnectionToWebProcess::didReceiveMessage(IPC::Connection& connection, IPC::MessageDecoder& decoder)
{
    if (decoder.messageReceiverName() == Messages::NetworkConnectionToWebProcess::messageReceiverName()) {
        didReceiveNetworkConnectionToWebProcessMessage(connection, decoder);
        return;
    }

    if (decoder.messageReceiverName() == Messages::NetworkResourceLoader::messageReceiverName()) {
        HashMap<ResourceLoadIdentifier, RefPtr<NetworkResourceLoader>>::iterator loaderIterator = m_networkResourceLoaders.find(decoder.destinationID());
        if (loaderIterator != m_networkResourceLoaders.end())
            loaderIterator->value->didReceiveNetworkResourceLoaderMessage(connection, decoder);
        return;
    }
    
    ASSERT_NOT_REACHED();
}

void NetworkConnectionToWebProcess::didReceiveSyncMessage(IPC::Connection& connection, IPC::MessageDecoder& decoder, std::unique_ptr<IPC::MessageEncoder>& reply)
{
    if (decoder.messageReceiverName() == Messages::NetworkConnectionToWebProcess::messageReceiverName()) {
        didReceiveSyncNetworkConnectionToWebProcessMessage(connection, decoder, reply);
        return;
    }
    ASSERT_NOT_REACHED();
}

void NetworkConnectionToWebProcess::didClose(IPC::Connection&)
{
    // Protect ourself as we might be otherwise be deleted during this function.
    Ref<NetworkConnectionToWebProcess> protector(*this);

    Vector<RefPtr<NetworkResourceLoader>> loaders;
    copyValuesToVector(m_networkResourceLoaders, loaders);
    for (auto& loader : loaders)
        loader->abort();
    ASSERT(m_networkResourceLoaders.isEmpty());

    NetworkBlobRegistry::singleton().connectionToWebProcessDidClose(this);
    NetworkProcess::singleton().removeNetworkConnectionToWebProcess(this);
}

void NetworkConnectionToWebProcess::didReceiveInvalidMessage(IPC::Connection&, IPC::StringReference, IPC::StringReference)
{
}

void NetworkConnectionToWebProcess::scheduleResourceLoad(const NetworkResourceLoadParameters& loadParameters)
{
    auto loader = NetworkResourceLoader::create(loadParameters, *this);
    m_networkResourceLoaders.add(loadParameters.identifier, loader.ptr());
    loader->start();
}

void NetworkConnectionToWebProcess::performSynchronousLoad(const NetworkResourceLoadParameters& loadParameters, RefPtr<Messages::NetworkConnectionToWebProcess::PerformSynchronousLoad::DelayedReply>&& reply)
{
    auto loader = NetworkResourceLoader::create(loadParameters, *this, WTFMove(reply));
    m_networkResourceLoaders.add(loadParameters.identifier, loader.ptr());
    loader->start();
}

void NetworkConnectionToWebProcess::loadPing(const NetworkResourceLoadParameters& loadParameters)
{
#if USE(NETWORK_SESSION)
    // PingLoad manages its own lifetime, deleting itself when its purpose has been fulfilled.
    new PingLoad(loadParameters);
#else
    RefPtr<NetworkingContext> context = RemoteNetworkingContext::create(loadParameters.sessionID, loadParameters.shouldClearReferrerOnHTTPSToHTTPRedirect);

    // PingHandle manages its own lifetime, deleting itself when its purpose has been fulfilled.
    new PingHandle(context.get(), loadParameters.request, loadParameters.allowStoredCredentials == AllowStoredCredentials, PingHandle::UsesAsyncCallbacks::Yes);
#endif
}

void NetworkConnectionToWebProcess::removeLoadIdentifier(ResourceLoadIdentifier identifier)
{
    RefPtr<NetworkResourceLoader> loader = m_networkResourceLoaders.get(identifier);

    // It's possible we have no loader for this identifier if the NetworkProcess crashed and this was a respawned NetworkProcess.
    if (!loader)
        return;

    // Abort the load now, as the WebProcess won't be able to respond to messages any more which might lead
    // to leaked loader resources (connections, threads, etc).
    loader->abort();
    ASSERT(!m_networkResourceLoaders.contains(identifier));
}

void NetworkConnectionToWebProcess::setDefersLoading(ResourceLoadIdentifier identifier, bool defers)
{
    RefPtr<NetworkResourceLoader> loader = m_networkResourceLoaders.get(identifier);
    if (!loader)
        return;

    loader->setDefersLoading(defers);
}

void NetworkConnectionToWebProcess::prefetchDNS(const String& hostname)
{
    NetworkProcess::singleton().prefetchDNS(hostname);
}

static NetworkStorageSession& storageSession(SessionID sessionID)
{
    if (sessionID.isEphemeral()) {
        NetworkStorageSession* privateStorageSession = SessionTracker::storageSession(sessionID);
        if (privateStorageSession)
            return *privateStorageSession;
        // Some requests with private browsing mode requested may still be coming shortly after NetworkProcess was told to destroy its session.
        // FIXME: Find a way to track private browsing sessions more rigorously.
        LOG_ERROR("Private browsing was requested, but there was no session for it. Please file a bug unless you just disabled private browsing, in which case it's an expected race.");
    }
    return NetworkStorageSession::defaultStorageSession();
}

void NetworkConnectionToWebProcess::startDownload(SessionID sessionID, DownloadID downloadID, const ResourceRequest& request, const String& suggestedName)
{
    NetworkProcess::singleton().downloadManager().startDownload(sessionID, downloadID, request, suggestedName);
}

void NetworkConnectionToWebProcess::convertMainResourceLoadToDownload(SessionID sessionID, uint64_t mainResourceLoadIdentifier, DownloadID downloadID, const ResourceRequest& request, const ResourceResponse& response)
{
    auto& networkProcess = NetworkProcess::singleton();
    if (!mainResourceLoadIdentifier) {
        networkProcess.downloadManager().startDownload(sessionID, downloadID, request);
        return;
    }

    NetworkResourceLoader* loader = m_networkResourceLoaders.get(mainResourceLoadIdentifier);
    if (!loader) {
        // If we're trying to download a blob here loader can be null.
        return;
    }

#if USE(NETWORK_SESSION)
    loader->networkLoad()->convertTaskToDownload(downloadID, request);
#else
    networkProcess.downloadManager().convertHandleToDownload(downloadID, loader->networkLoad()->handle(), request, response);

    // Unblock the URL connection operation queue.
    loader->networkLoad()->handle()->continueDidReceiveResponse();
    
#endif
    loader->didConvertToDownload();
}

void NetworkConnectionToWebProcess::cookiesForDOM(SessionID sessionID, const URL& firstParty, const URL& url, String& result)
{
    result = WebCore::cookiesForDOM(storageSession(sessionID), firstParty, url);
}

void NetworkConnectionToWebProcess::setCookiesFromDOM(SessionID sessionID, const URL& firstParty, const URL& url, const String& cookieString)
{
    WebCore::setCookiesFromDOM(storageSession(sessionID), firstParty, url, cookieString);
}

void NetworkConnectionToWebProcess::cookiesEnabled(SessionID sessionID, const URL& firstParty, const URL& url, bool& result)
{
    result = WebCore::cookiesEnabled(storageSession(sessionID), firstParty, url);
}

void NetworkConnectionToWebProcess::cookieRequestHeaderFieldValue(SessionID sessionID, const URL& firstParty, const URL& url, String& result)
{
    result = WebCore::cookieRequestHeaderFieldValue(storageSession(sessionID), firstParty, url);
}

void NetworkConnectionToWebProcess::getRawCookies(SessionID sessionID, const URL& firstParty, const URL& url, Vector<Cookie>& result)
{
    WebCore::getRawCookies(storageSession(sessionID), firstParty, url, result);
}

void NetworkConnectionToWebProcess::deleteCookie(SessionID sessionID, const URL& url, const String& cookieName)
{
    WebCore::deleteCookie(storageSession(sessionID), url, cookieName);
}

void NetworkConnectionToWebProcess::registerFileBlobURL(const URL& url, const String& path, const SandboxExtension::Handle& extensionHandle, const String& contentType)
{
    RefPtr<SandboxExtension> extension = SandboxExtension::create(extensionHandle);

    NetworkBlobRegistry::singleton().registerFileBlobURL(this, url, path, WTFMove(extension), contentType);
}

void NetworkConnectionToWebProcess::registerBlobURL(const URL& url, Vector<BlobPart> blobParts, const String& contentType)
{
    NetworkBlobRegistry::singleton().registerBlobURL(this, url, WTFMove(blobParts), contentType);
}

void NetworkConnectionToWebProcess::registerBlobURLFromURL(const URL& url, const URL& srcURL)
{
    NetworkBlobRegistry::singleton().registerBlobURL(this, url, srcURL);
}

void NetworkConnectionToWebProcess::registerBlobURLForSlice(const URL& url, const URL& srcURL, int64_t start, int64_t end)
{
    NetworkBlobRegistry::singleton().registerBlobURLForSlice(this, url, srcURL, start, end);
}

void NetworkConnectionToWebProcess::unregisterBlobURL(const URL& url)
{
    NetworkBlobRegistry::singleton().unregisterBlobURL(this, url);
}

void NetworkConnectionToWebProcess::blobSize(const URL& url, uint64_t& resultSize)
{
    resultSize = NetworkBlobRegistry::singleton().blobSize(this, url);
}

void NetworkConnectionToWebProcess::ensureLegacyPrivateBrowsingSession()
{
    NetworkProcess::singleton().ensurePrivateBrowsingSession(SessionID::legacyPrivateSessionID());
}

} // namespace WebKit
