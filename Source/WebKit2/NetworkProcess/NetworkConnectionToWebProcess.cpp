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

#include "config.h"
#include "NetworkConnectionToWebProcess.h"

#if ENABLE(NETWORK_PROCESS)

#include "BlobRegistrationData.h"
#include "ConnectionStack.h"
#include "NetworkBlobRegistry.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcess.h"
#include "NetworkResourceLoadParameters.h"
#include "NetworkResourceLoader.h"
#include "NetworkResourceLoaderMessages.h"
#include "RemoteNetworkingContext.h"
#include "SessionTracker.h"
#include <WebCore/BlobData.h>
#include <WebCore/PlatformCookieJar.h>
#include <WebCore/ResourceLoaderOptions.h>
#include <WebCore/ResourceRequest.h>
#include <wtf/RunLoop.h>

using namespace WebCore;

namespace WebKit {

PassRefPtr<NetworkConnectionToWebProcess> NetworkConnectionToWebProcess::create(IPC::Connection::Identifier connectionIdentifier)
{
    return adoptRef(new NetworkConnectionToWebProcess(connectionIdentifier));
}

NetworkConnectionToWebProcess::NetworkConnectionToWebProcess(IPC::Connection::Identifier connectionIdentifier)
    : m_serialLoadingEnabled(false)
{
    m_connection = IPC::Connection::createServerConnection(connectionIdentifier, this, RunLoop::main());
    m_connection->setOnlySendMessagesAsDispatchWhenWaitingForSyncReplyWhenProcessingSuchAMessage(true);
    m_connection->open();
}

NetworkConnectionToWebProcess::~NetworkConnectionToWebProcess()
{
}
    
void NetworkConnectionToWebProcess::didReceiveMessage(IPC::Connection* connection, IPC::MessageDecoder& decoder)
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

void NetworkConnectionToWebProcess::didReceiveSyncMessage(IPC::Connection* connection, IPC::MessageDecoder& decoder, std::unique_ptr<IPC::MessageEncoder>& reply)
{
    if (decoder.messageReceiverName() == Messages::NetworkConnectionToWebProcess::messageReceiverName()) {
        didReceiveSyncNetworkConnectionToWebProcessMessage(connection, decoder, reply);
        return;
    }
    ASSERT_NOT_REACHED();
}

void NetworkConnectionToWebProcess::didClose(IPC::Connection*)
{
    // Protect ourself as we might be otherwise be deleted during this function.
    Ref<NetworkConnectionToWebProcess> protector(*this);

    HashMap<ResourceLoadIdentifier, RefPtr<NetworkResourceLoader>>::iterator end = m_networkResourceLoaders.end();
    for (HashMap<ResourceLoadIdentifier, RefPtr<NetworkResourceLoader>>::iterator i = m_networkResourceLoaders.begin(); i != end; ++i)
        i->value->abort();

    NetworkBlobRegistry::shared().connectionToWebProcessDidClose(this);

    m_networkResourceLoaders.clear();
    
    NetworkProcess::shared().removeNetworkConnectionToWebProcess(this);
}

void NetworkConnectionToWebProcess::didReceiveInvalidMessage(IPC::Connection*, IPC::StringReference, IPC::StringReference)
{
}

void NetworkConnectionToWebProcess::scheduleResourceLoad(const NetworkResourceLoadParameters& loadParameters)
{
    RefPtr<NetworkResourceLoader> loader = NetworkResourceLoader::create(loadParameters, this);
    m_networkResourceLoaders.add(loadParameters.identifier, loader);
    NetworkProcess::shared().networkResourceLoadScheduler().scheduleLoader(loader.get());
}

void NetworkConnectionToWebProcess::performSynchronousLoad(const NetworkResourceLoadParameters& loadParameters, PassRefPtr<Messages::NetworkConnectionToWebProcess::PerformSynchronousLoad::DelayedReply> reply)
{
    RefPtr<NetworkResourceLoader> loader = NetworkResourceLoader::create(loadParameters, this, reply);
    m_networkResourceLoaders.add(loadParameters.identifier, loader);
    NetworkProcess::shared().networkResourceLoadScheduler().scheduleLoader(loader.get());
}

void NetworkConnectionToWebProcess::removeLoadIdentifier(ResourceLoadIdentifier identifier)
{
    RefPtr<NetworkResourceLoader> loader = m_networkResourceLoaders.take(identifier);

    // It's possible we have no loader for this identifier if the NetworkProcess crashed and this was a respawned NetworkProcess.
    if (!loader)
        return;

    // Abort the load now, as the WebProcess won't be able to respond to messages any more which might lead
    // to leaked loader resources (connections, threads, etc).
    loader->abort();
}

void NetworkConnectionToWebProcess::servePendingRequests(uint32_t resourceLoadPriority)
{
    NetworkProcess::shared().networkResourceLoadScheduler().servePendingRequests(static_cast<ResourceLoadPriority>(resourceLoadPriority));
}

void NetworkConnectionToWebProcess::setSerialLoadingEnabled(bool enabled)
{
    m_serialLoadingEnabled = enabled;
}

static NetworkStorageSession& storageSession(uint64_t sessionID)
{
    if (SessionTracker::isEphemeralID(sessionID)) {
        NetworkStorageSession* privateSession = SessionTracker::session(sessionID).get();
        if (privateSession)
            return *privateSession;
        // Some requests with private browsing mode requested may still be coming shortly after NetworkProcess was told to destroy its session.
        // FIXME: Find a way to track private browsing sessions more rigorously.
        LOG_ERROR("Private browsing was requested, but there was no session for it. Please file a bug unless you just disabled private browsing, in which case it's an expected race.");
    }
    return NetworkStorageSession::defaultStorageSession();
}

void NetworkConnectionToWebProcess::startDownload(uint64_t sessionID, uint64_t downloadID, const ResourceRequest& request)
{
    // FIXME: Do something with the session ID.
    NetworkProcess::shared().downloadManager().startDownload(downloadID, request);
}

void NetworkConnectionToWebProcess::convertMainResourceLoadToDownload(uint64_t mainResourceLoadIdentifier, uint64_t downloadID, const ResourceRequest& request, const ResourceResponse& response)
{
    if (!mainResourceLoadIdentifier) {
        NetworkProcess::shared().downloadManager().startDownload(downloadID, request);
        return;
    }

    NetworkResourceLoader* loader = m_networkResourceLoaders.get(mainResourceLoadIdentifier);
    NetworkProcess::shared().downloadManager().convertHandleToDownload(downloadID, loader->handle(), request, response);

    // Unblock the URL connection operation queue.
    loader->handle()->continueDidReceiveResponse();
    
    loader->didConvertHandleToDownload();
}

void NetworkConnectionToWebProcess::cookiesForDOM(uint64_t sessionID, const URL& firstParty, const URL& url, String& result)
{
    result = WebCore::cookiesForDOM(storageSession(sessionID), firstParty, url);
}

void NetworkConnectionToWebProcess::setCookiesFromDOM(uint64_t sessionID, const URL& firstParty, const URL& url, const String& cookieString)
{
    WebCore::setCookiesFromDOM(storageSession(sessionID), firstParty, url, cookieString);
}

void NetworkConnectionToWebProcess::cookiesEnabled(uint64_t sessionID, const URL& firstParty, const URL& url, bool& result)
{
    result = WebCore::cookiesEnabled(storageSession(sessionID), firstParty, url);
}

void NetworkConnectionToWebProcess::cookieRequestHeaderFieldValue(uint64_t sessionID, const URL& firstParty, const URL& url, String& result)
{
    result = WebCore::cookieRequestHeaderFieldValue(storageSession(sessionID), firstParty, url);
}

void NetworkConnectionToWebProcess::getRawCookies(uint64_t sessionID, const URL& firstParty, const URL& url, Vector<Cookie>& result)
{
    WebCore::getRawCookies(storageSession(sessionID), firstParty, url, result);
}

void NetworkConnectionToWebProcess::deleteCookie(uint64_t sessionID, const URL& url, const String& cookieName)
{
    WebCore::deleteCookie(storageSession(sessionID), url, cookieName);
}

void NetworkConnectionToWebProcess::registerBlobURL(const URL& url, const BlobRegistrationData& data)
{
    Vector<RefPtr<SandboxExtension>> extensions;
    for (size_t i = 0, count = data.sandboxExtensions().size(); i < count; ++i) {
        if (RefPtr<SandboxExtension> extension = SandboxExtension::create(data.sandboxExtensions()[i]))
            extensions.append(extension);
    }

    NetworkBlobRegistry::shared().registerBlobURL(this, url, data.releaseData(), extensions);
}

void NetworkConnectionToWebProcess::registerBlobURLFromURL(const URL& url, const URL& srcURL)
{
    NetworkBlobRegistry::shared().registerBlobURL(this, url, srcURL);
}

void NetworkConnectionToWebProcess::unregisterBlobURL(const URL& url)
{
    NetworkBlobRegistry::shared().unregisterBlobURL(this, url);
}

} // namespace WebKit

#endif // ENABLE(NETWORK_PROCESS)
