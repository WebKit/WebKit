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

#include "ConnectionStack.h"
#include "NetworkProcess.h"
#include "NetworkResourceLoader.h"
#include "RemoteNetworkingContext.h"
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
    ASSERT(!m_connection);
    ASSERT(m_observers.isEmpty());
}

void NetworkConnectionToWebProcess::registerObserver(NetworkConnectionToWebProcessObserver* observer)
{
    ASSERT(!m_observers.contains(observer));
    m_observers.add(observer);
}

void NetworkConnectionToWebProcess::unregisterObserver(NetworkConnectionToWebProcessObserver* observer)
{
    ASSERT(m_observers.contains(observer));
    m_observers.remove(observer);
}
    
void NetworkConnectionToWebProcess::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::MessageDecoder& decoder)
{
    if (messageID.is<CoreIPC::MessageClassNetworkConnectionToWebProcess>()) {
        didReceiveNetworkConnectionToWebProcessMessage(connection, messageID, decoder);
        return;
    }
    
    if (messageID.is<CoreIPC::MessageClassNetworkResourceLoader>()) {
        NetworkResourceLoader* loader = NetworkProcess::shared().networkResourceLoadScheduler().networkResourceLoaderForIdentifier(decoder.destinationID());
        if (loader)
            loader->didReceiveNetworkResourceLoaderMessage(connection, messageID, decoder);
        return;
    }
    ASSERT_NOT_REACHED();
}

void NetworkConnectionToWebProcess::didReceiveSyncMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::MessageDecoder& decoder, OwnPtr<CoreIPC::MessageEncoder>& reply)
{
    if (messageID.is<CoreIPC::MessageClassNetworkConnectionToWebProcess>()) {
        didReceiveSyncNetworkConnectionToWebProcessMessage(connection, messageID, decoder, reply);
        return;
    }
    ASSERT_NOT_REACHED();
}

void NetworkConnectionToWebProcess::didClose(CoreIPC::Connection*)
{
    // Protect ourself as we might be otherwise be deleted during this function
    RefPtr<NetworkConnectionToWebProcess> protector(this);
    
    NetworkProcess::shared().removeNetworkConnectionToWebProcess(this);
    
    Vector<NetworkConnectionToWebProcessObserver*> observers;
    copyToVector(m_observers, observers);
    for (size_t i = 0; i < observers.size(); ++i)
        observers[i]->connectionToWebProcessDidClose(this);
    
    // FIXME (NetworkProcess): We might consider actively clearing out all requests for this connection.
    // But that might not be necessary as the observer mechanism used above is much more direct.
    
    m_connection = 0;
}

void NetworkConnectionToWebProcess::didReceiveInvalidMessage(CoreIPC::Connection*, CoreIPC::StringReference, CoreIPC::StringReference)
{
}

void NetworkConnectionToWebProcess::scheduleResourceLoad(const NetworkResourceLoadParameters& loadParameters, ResourceLoadIdentifier& resourceLoadIdentifier)
{
    resourceLoadIdentifier = NetworkProcess::shared().networkResourceLoadScheduler().scheduleResourceLoad(loadParameters, this);
}

void NetworkConnectionToWebProcess::addLoadInProgress(const KURL& url, ResourceLoadIdentifier& identifier)
{
    identifier = NetworkProcess::shared().networkResourceLoadScheduler().addLoadInProgress(url);
}

void NetworkConnectionToWebProcess::removeLoadIdentifier(ResourceLoadIdentifier identifier)
{
    NetworkProcess::shared().networkResourceLoadScheduler().removeLoadIdentifier(identifier);
}

void NetworkConnectionToWebProcess::servePendingRequests(uint32_t resourceLoadPriority)
{
    NetworkProcess::shared().networkResourceLoadScheduler().servePendingRequests(static_cast<ResourceLoadPriority>(resourceLoadPriority));
}

void NetworkConnectionToWebProcess::suspendPendingRequests()
{
    NetworkProcess::shared().networkResourceLoadScheduler().suspendPendingRequests();
}

void NetworkConnectionToWebProcess::resumePendingRequests()
{
    NetworkProcess::shared().networkResourceLoadScheduler().resumePendingRequests();
}

void NetworkConnectionToWebProcess::setSerialLoadingEnabled(bool enabled)
{
    m_serialLoadingEnabled = enabled;
}

void NetworkConnectionToWebProcess::cookiesForDOM(const KURL& firstParty, const KURL& url, String& result)
{
    // FIXME (NetworkProcess): Use a correct storage session.
    result = WebCore::cookiesForDOM(RemoteNetworkingContext::create(false, false, false).get(), firstParty, url);
}

void NetworkConnectionToWebProcess::setCookiesFromDOM(const KURL& firstParty, const KURL& url, const String& cookieString)
{
    // FIXME (NetworkProcess): Use a correct storage session.
    WebCore::setCookiesFromDOM(RemoteNetworkingContext::create(false, false, false).get(), firstParty, url, cookieString);
}

void NetworkConnectionToWebProcess::cookiesEnabled(const KURL& firstParty, const KURL& url, bool& result)
{
    // FIXME (NetworkProcess): Use a correct storage session.
    result = WebCore::cookiesEnabled(RemoteNetworkingContext::create(false, false, false).get(), firstParty, url);
}

void NetworkConnectionToWebProcess::cookieRequestHeaderFieldValue(const KURL& firstParty, const KURL& url, String& result)
{
    // FIXME (NetworkProcess): Use a correct storage session.
    result = WebCore::cookieRequestHeaderFieldValue(0, firstParty, url);
}

void NetworkConnectionToWebProcess::getRawCookies(const KURL& firstParty, const KURL& url, Vector<Cookie>& result)
{
    // FIXME (NetworkProcess): Use a correct storage session.
    WebCore::getRawCookies(RemoteNetworkingContext::create(false, false, false).get(), firstParty, url, result);
}

void NetworkConnectionToWebProcess::deleteCookie(const KURL& url, const String& cookieName)
{
    // FIXME (NetworkProcess): Use a correct storage session.
    WebCore::deleteCookie(RemoteNetworkingContext::create(false, false, false).get(), url, cookieName);
}

void NetworkConnectionToWebProcess::getHostnamesWithCookies(Vector<String>& hostnames)
{
    // FIXME (NetworkProcess): Use a correct storage session.
    HashSet<String> hostnamesSet;
    WebCore::getHostnamesWithCookies(RemoteNetworkingContext::create(false, false, false).get(), hostnamesSet);
    WTF::copyToVector(hostnamesSet, hostnames);
}

void NetworkConnectionToWebProcess::deleteCookiesForHostname(const String& hostname)
{
    // FIXME (NetworkProcess): Use a correct storage session.
    WebCore::deleteCookiesForHostname(RemoteNetworkingContext::create(false, false, false).get(), hostname);
}

void NetworkConnectionToWebProcess::deleteAllCookies()
{
    // FIXME (NetworkProcess): Use a correct storage session.
    WebCore::deleteAllCookies(RemoteNetworkingContext::create(false, false, false).get());
}

} // namespace WebKit

#endif // ENABLE(NETWORK_PROCESS)
