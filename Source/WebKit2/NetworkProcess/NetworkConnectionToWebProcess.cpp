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

#include "BlobRegistrationData.h"
#include "ConnectionStack.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcess.h"
#include "NetworkResourceLoader.h"
#include "RemoteNetworkingContext.h"
#include "SyncNetworkResourceLoader.h"
#include <WebCore/BlobData.h>
#include <WebCore/BlobRegistry.h>
#include <WebCore/PlatformCookieJar.h>
#include <WebCore/ResourceLoaderOptions.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/RunLoop.h>

#if ENABLE(NETWORK_PROCESS)

using namespace WebCore;

namespace WebKit {

PassRefPtr<NetworkConnectionToWebProcess> NetworkConnectionToWebProcess::create(CoreIPC::Connection::Identifier connectionIdentifier)
{
    return adoptRef(new NetworkConnectionToWebProcess(connectionIdentifier));
}

NetworkConnectionToWebProcess::NetworkConnectionToWebProcess(CoreIPC::Connection::Identifier connectionIdentifier)
    : m_serialLoadingEnabled(false)
{
    m_connection = CoreIPC::Connection::createServerConnection(connectionIdentifier, this, RunLoop::main());
    m_connection->setOnlySendMessagesAsDispatchWhenWaitingForSyncReplyWhenProcessingSuchAMessage(true);
    m_connection->open();
}

NetworkConnectionToWebProcess::~NetworkConnectionToWebProcess()
{
}
    
void NetworkConnectionToWebProcess::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageDecoder& decoder)
{
    if (decoder.messageReceiverName() == Messages::NetworkConnectionToWebProcess::messageReceiverName()) {
        didReceiveNetworkConnectionToWebProcessMessage(connection, decoder);
        return;
    }
    
    ASSERT_NOT_REACHED();
}

void NetworkConnectionToWebProcess::didReceiveSyncMessage(CoreIPC::Connection* connection, CoreIPC::MessageDecoder& decoder, OwnPtr<CoreIPC::MessageEncoder>& reply)
{
    if (decoder.messageReceiverName() == Messages::NetworkConnectionToWebProcess::messageReceiverName()) {
        didReceiveSyncNetworkConnectionToWebProcessMessage(connection, decoder, reply);
        return;
    }
    ASSERT_NOT_REACHED();
}

void NetworkConnectionToWebProcess::didClose(CoreIPC::Connection*)
{
    // Protect ourself as we might be otherwise be deleted during this function.
    RefPtr<NetworkConnectionToWebProcess> protector(this);

    HashMap<ResourceLoadIdentifier, RefPtr<NetworkResourceLoader> >::iterator end = m_networkResourceLoaders.end();
    for (HashMap<ResourceLoadIdentifier, RefPtr<NetworkResourceLoader> >::iterator i = m_networkResourceLoaders.begin(); i != end; ++i)
        i->value->connectionToWebProcessDidClose();

    HashMap<ResourceLoadIdentifier, RefPtr<SyncNetworkResourceLoader> >::iterator syncEnd = m_syncNetworkResourceLoaders.end();
    for (HashMap<ResourceLoadIdentifier, RefPtr<SyncNetworkResourceLoader> >::iterator i = m_syncNetworkResourceLoaders.begin(); i != syncEnd; ++i)
        i->value->connectionToWebProcessDidClose();

    m_networkResourceLoaders.clear();
}

void NetworkConnectionToWebProcess::didReceiveInvalidMessage(CoreIPC::Connection*, CoreIPC::StringReference, CoreIPC::StringReference)
{
}

void NetworkConnectionToWebProcess::scheduleResourceLoad(const NetworkResourceLoadParameters& loadParameters)
{
    RefPtr<NetworkResourceLoader> loader = NetworkResourceLoader::create(loadParameters, this);
    m_networkResourceLoaders.add(loadParameters.identifier(), loader);
    NetworkProcess::shared().networkResourceLoadScheduler().scheduleLoader(loader.get());
}

void NetworkConnectionToWebProcess::performSynchronousLoad(const NetworkResourceLoadParameters& loadParameters, PassRefPtr<Messages::NetworkConnectionToWebProcess::PerformSynchronousLoad::DelayedReply> reply)
{
    RefPtr<SyncNetworkResourceLoader> loader = SyncNetworkResourceLoader::create(loadParameters, this, reply);
    m_syncNetworkResourceLoaders.add(loadParameters.identifier(), loader);
    NetworkProcess::shared().networkResourceLoadScheduler().scheduleLoader(loader.get());
}

void NetworkConnectionToWebProcess::removeLoadIdentifier(ResourceLoadIdentifier identifier)
{
    RefPtr<SchedulableLoader> loader = m_networkResourceLoaders.take(identifier);
    if (!loader)
        loader = m_syncNetworkResourceLoaders.take(identifier);

    // It's possible we have no loader for this identifier if the NetworkProcess crashed and this was a respawned NetworkProcess.
    if (!loader)
        return;

    NetworkProcess::shared().networkResourceLoadScheduler().removeLoader(loader.get());
}

void NetworkConnectionToWebProcess::servePendingRequests(uint32_t resourceLoadPriority)
{
    NetworkProcess::shared().networkResourceLoadScheduler().servePendingRequests(static_cast<ResourceLoadPriority>(resourceLoadPriority));
}

void NetworkConnectionToWebProcess::setSerialLoadingEnabled(bool enabled)
{
    m_serialLoadingEnabled = enabled;
}

static NetworkStorageSession& storageSession(bool privateBrowsingEnabled)
{
    return privateBrowsingEnabled ? RemoteNetworkingContext::privateBrowsingSession() : NetworkStorageSession::defaultStorageSession();
}

void NetworkConnectionToWebProcess::startDownload(bool privateBrowsingEnabled, uint64_t downloadID, const ResourceRequest& request)
{
    // FIXME: Do something with the private browsing flag.
    NetworkProcess::shared().downloadManager().startDownload(downloadID, request);
}

void NetworkConnectionToWebProcess::cookiesForDOM(bool privateBrowsingEnabled, const KURL& firstParty, const KURL& url, String& result)
{
    result = WebCore::cookiesForDOM(storageSession(privateBrowsingEnabled), firstParty, url);
}

void NetworkConnectionToWebProcess::setCookiesFromDOM(bool privateBrowsingEnabled, const KURL& firstParty, const KURL& url, const String& cookieString)
{
    WebCore::setCookiesFromDOM(storageSession(privateBrowsingEnabled), firstParty, url, cookieString);
}

void NetworkConnectionToWebProcess::cookiesEnabled(bool privateBrowsingEnabled, const KURL& firstParty, const KURL& url, bool& result)
{
    result = WebCore::cookiesEnabled(storageSession(privateBrowsingEnabled), firstParty, url);
}

void NetworkConnectionToWebProcess::cookieRequestHeaderFieldValue(bool privateBrowsingEnabled, const KURL& firstParty, const KURL& url, String& result)
{
    result = WebCore::cookieRequestHeaderFieldValue(storageSession(privateBrowsingEnabled), firstParty, url);
}

void NetworkConnectionToWebProcess::getRawCookies(bool privateBrowsingEnabled, const KURL& firstParty, const KURL& url, Vector<Cookie>& result)
{
    WebCore::getRawCookies(storageSession(privateBrowsingEnabled), firstParty, url, result);
}

void NetworkConnectionToWebProcess::deleteCookie(bool privateBrowsingEnabled, const KURL& url, const String& cookieName)
{
    WebCore::deleteCookie(storageSession(privateBrowsingEnabled), url, cookieName);
}

void NetworkConnectionToWebProcess::registerBlobURL(const KURL& url, const BlobRegistrationData& data)
{
    // FIXME: Track sandbox extensions.
    // FIXME: unregister all URLs when process connection closes.
    blobRegistry().registerBlobURL(url, data.releaseData());
}

void NetworkConnectionToWebProcess::registerBlobURLFromURL(const KURL& url, const KURL& srcURL)
{
    blobRegistry().registerBlobURL(url, srcURL);
}

void NetworkConnectionToWebProcess::unregisterBlobURL(const KURL& url)
{
    blobRegistry().unregisterBlobURL(url);
}

} // namespace WebKit

#endif // ENABLE(NETWORK_PROCESS)
