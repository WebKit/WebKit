/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "LibWebRTCNetworkManager.h"

#if USE(LIBWEBRTC)

#include "LibWebRTCNetwork.h"
#include "Logging.h"
#include "NetworkProcessConnection.h"
#include "NetworkRTCProviderMessages.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/Document.h>
#include <WebCore/LibWebRTCUtils.h>
#include <WebCore/Page.h>
#include <wtf/EnumTraits.h>
#include <wtf/TZoneMalloc.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(LibWebRTCNetworkManager);

RefPtr<LibWebRTCNetworkManager> LibWebRTCNetworkManager::getOrCreate(WebCore::ScriptExecutionContextIdentifier identifier)
{
    RefPtr document = Document::allDocumentsMap().get(identifier);
    if (!document)
        return nullptr;

    auto* networkManager = static_cast<LibWebRTCNetworkManager*>(document->rtcNetworkManager());
    if (!networkManager) {
        auto newNetworkManager = adoptRef(*new LibWebRTCNetworkManager(identifier));
        networkManager = newNetworkManager.ptr();
        document->setRTCNetworkManager(WTFMove(newNetworkManager));
        WebProcess::singleton().libWebRTCNetwork().monitor().addObserver(*networkManager);
    }

    return networkManager;
}

void LibWebRTCNetworkManager::signalUsedInterface(WebCore::ScriptExecutionContextIdentifier contextIdentifier, String&& name)
{
    callOnMainRunLoop([contextIdentifier, name = WTFMove(name).isolatedCopy()]() mutable {
        if (RefPtr manager = LibWebRTCNetworkManager::getOrCreate(contextIdentifier))
            manager->signalUsedInterface(WTFMove(name));
    });
}

LibWebRTCNetworkManager::LibWebRTCNetworkManager(WebCore::ScriptExecutionContextIdentifier documentIdentifier)
    : m_documentIdentifier(documentIdentifier)
{
}

LibWebRTCNetworkManager::~LibWebRTCNetworkManager()
{
    ASSERT(m_isClosed);
}

void LibWebRTCNetworkManager::close()
{
#if ASSERT_ENABLED
    m_isClosed = true;
#endif
    WebProcess::singleton().libWebRTCNetwork().monitor().removeObserver(*this);
}

void LibWebRTCNetworkManager::unregisterMDNSNames()
{
    WebProcess::singleton().protectedLibWebRTCNetwork()->protectedMDNSRegister()->unregisterMDNSNames(m_documentIdentifier);
}

void LibWebRTCNetworkManager::setEnumeratingAllNetworkInterfacesEnabled(bool enabled)
{
    m_enableEnumeratingAllNetworkInterfaces = enabled;
}

void LibWebRTCNetworkManager::setEnumeratingVisibleNetworkInterfacesEnabled(bool enabled)
{
    m_enableEnumeratingVisibleNetworkInterfaces = enabled;
}

void LibWebRTCNetworkManager::StartUpdating()
{
    callOnMainRunLoop([this, weakThis = WeakPtr { *this }] {
        if (!weakThis)
            return;

        auto& monitor = WebProcess::singleton().libWebRTCNetwork().monitor();
        if (m_receivedNetworkList) {
            WebCore::LibWebRTCProvider::callOnWebRTCNetworkThread([this, protectedThis = Ref { *this }] {
                SignalNetworksChanged();
            });
        } else if (monitor.didReceiveNetworkList())
            networksChanged(monitor.networkList() , monitor.ipv4(), monitor.ipv6());
        monitor.startUpdating();
    });
}

void LibWebRTCNetworkManager::StopUpdating()
{
    callOnMainRunLoop([weakThis = WeakPtr { *this }] {
        if (!weakThis)
            return;
        WebProcess::singleton().libWebRTCNetwork().monitor().stopUpdating();
    });
}

webrtc::MdnsResponderInterface* LibWebRTCNetworkManager::GetMdnsResponder() const
{
#if PLATFORM(GTK) || PLATFORM(WPE)
    return nullptr;
#else
    return m_useMDNSCandidates ? const_cast<LibWebRTCNetworkManager*>(this) : nullptr;
#endif
}

void LibWebRTCNetworkManager::networksChanged(const Vector<RTCNetwork>& networks, const RTCNetwork::IPAddress& ipv4, const RTCNetwork::IPAddress& ipv6)
{
    bool forceSignaling = !m_receivedNetworkList;
    m_receivedNetworkList = true;
    networksChanged(networks, ipv4, ipv6, forceSignaling);
}

void LibWebRTCNetworkManager::networksChanged(const Vector<RTCNetwork>& networks, const RTCNetwork::IPAddress& ipv4, const RTCNetwork::IPAddress& ipv6, bool forceSignaling)
{
    ASSERT(isMainRunLoop());

    Vector<RTCNetwork> filteredNetworks;
    if (m_enableEnumeratingAllNetworkInterfaces)
        filteredNetworks = networks;
    else {
#if PLATFORM(COCOA)
        if (!m_useMDNSCandidates && m_enableEnumeratingVisibleNetworkInterfaces && m_allowedInterfaces.isEmpty() && !m_hasQueriedInterface) {
            RefPtr document = WebCore::Document::allDocumentsMap().get(m_documentIdentifier);
            RefPtr page = document ? document->page() : nullptr;
            RefPtr webPage = page ? WebPage::fromCorePage(*page) : nullptr;
            if (webPage) {
                m_hasQueriedInterface = true;

                RegistrableDomain domain { document->url() };
                bool isFirstParty = domain == RegistrableDomain(document->firstPartyForCookies());
                bool isRelayDisabled = true;
                WebProcess::singleton().ensureNetworkProcessConnection().protectedConnection()->sendWithAsyncReply(Messages::NetworkRTCProvider::GetInterfaceName { document->url(), webPage->webPageProxyIdentifier(), isFirstParty, isRelayDisabled, WTFMove(domain) }, [weakThis = WeakPtr { *this }] (auto&& interfaceName) {
                    RefPtr protectedThis = weakThis.get();
                    if (protectedThis && !interfaceName.isNull())
                        protectedThis->signalUsedInterface(WTFMove(interfaceName));
                }, 0);
            }
        }
#endif
        for (auto& network : networks) {
            if (WTF::anyOf(network.ips, [&](const auto& ip) { return ipv4.rtcAddress() == ip.rtcAddress() || ipv6.rtcAddress() == ip.rtcAddress(); }) || (!m_useMDNSCandidates && m_enableEnumeratingVisibleNetworkInterfaces && m_allowedInterfaces.contains(String::fromUTF8(network.name))))
                filteredNetworks.append(network);
        }
    }

    WebCore::LibWebRTCProvider::callOnWebRTCNetworkThread([this, protectedThis = Ref { *this }, networks = WTFMove(filteredNetworks), ipv4, ipv6, forceSignaling] {
        std::vector<std::unique_ptr<rtc::Network>> networkList(networks.size());
        for (size_t index = 0; index < networks.size(); ++index)
            networkList[index] = std::make_unique<rtc::Network>(networks[index].value());

        bool hasChanged;
        set_default_local_addresses(ipv4.rtcAddress(), ipv6.rtcAddress());
        MergeNetworkList(WTFMove(networkList), &hasChanged);
        if (hasChanged || forceSignaling)
            SignalNetworksChanged();
    });

}

const String& LibWebRTCNetworkManager::interfaceNameForTesting() const
{
    ASSERT(isMainRunLoop());
    for (auto& name : m_allowedInterfaces)
        return name;
    return emptyString();
}

void LibWebRTCNetworkManager::signalUsedInterface(String&& name)
{
    ASSERT(isMainRunLoop());
    if (!m_allowedInterfaces.add(WTFMove(name)).isNewEntry || m_useMDNSCandidates || !m_enableEnumeratingVisibleNetworkInterfaces)
        return;

    auto& monitor = WebProcess::singleton().libWebRTCNetwork().monitor();
    if (monitor.didReceiveNetworkList())
        networksChanged(monitor.networkList() , monitor.ipv4(), monitor.ipv6(), false);
}

void LibWebRTCNetworkManager::networkProcessCrashed()
{
    m_receivedNetworkList = false;
    if (!WebCore::LibWebRTCProvider::hasWebRTCThreads())
        return;

    // In case we have clients waiting for networksChanged, we call SignalNetworksChanged to make sure they do not wait for nothing.
    WebCore::LibWebRTCProvider::callOnWebRTCNetworkThread([this, protectedThis = Ref { *this }] {
        SignalNetworksChanged();
    });
}

void LibWebRTCNetworkManager::CreateNameForAddress(const rtc::IPAddress& address, NameCreatedCallback callback)
{
    callOnMainRunLoop([weakThis = WeakPtr { *this }, address, callback = std::move(callback)]() mutable {
        if (!weakThis)
            return;

        WebProcess::singleton().protectedLibWebRTCNetwork()->protectedMDNSRegister()->registerMDNSName(weakThis->m_documentIdentifier, fromStdString(address.ToString()), [address, callback = std::move(callback)](auto name, auto error) mutable {
            WebCore::LibWebRTCProvider::callOnWebRTCNetworkThread([address, callback = std::move(callback), name = WTFMove(name).isolatedCopy(), error] {
                RELEASE_LOG_ERROR_IF(error, WebRTC, "MDNS registration of a host candidate failed with error %hhu", enumToUnderlyingType(*error));
                // In case of error, we provide the name to let gathering complete.
                callback(address, name.utf8().data());
            });
        });
    });
}

void LibWebRTCNetworkManager::RemoveNameForAddress(const rtc::IPAddress&, NameRemovedCallback)
{
    // LibWebRTC backend defines this method but does not call it.
    ASSERT_NOT_REACHED();
}

} // namespace WebKit

#endif // USE(LIBWEBRTC)
