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
#include "WebProcess.h"
#include <WebCore/Document.h>
#include <WebCore/LibWebRTCUtils.h>

namespace WebKit {
using namespace WebCore;

LibWebRTCNetworkManager* LibWebRTCNetworkManager::getOrCreate(WebCore::ScriptExecutionContextIdentifier identifier)
{
    auto* document = Document::allDocumentsMap().get(identifier);
    if (!document)
        return nullptr;

    auto* networkManager = static_cast<LibWebRTCNetworkManager*>(document->rtcNetworkManager());
    if (!networkManager) {
        auto newNetworkManager = adoptRef(*new LibWebRTCNetworkManager(identifier));
        networkManager = newNetworkManager.ptr();
        document->setRTCNetworkManager(WTFMove(newNetworkManager));
    }

    return networkManager;
}

LibWebRTCNetworkManager::LibWebRTCNetworkManager(WebCore::ScriptExecutionContextIdentifier documentIdentifier)
    : m_documentIdentifier(documentIdentifier)
{
    WebProcess::singleton().libWebRTCNetwork().monitor().addObserver(*this);
}

LibWebRTCNetworkManager::~LibWebRTCNetworkManager()
{
    WebProcess::singleton().libWebRTCNetwork().monitor().removeObserver(*this);
}

void LibWebRTCNetworkManager::unregisterMDNSNames()
{
    WebProcess::singleton().libWebRTCNetwork().mdnsRegister().unregisterMDNSNames(m_documentIdentifier);
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
    return m_useMDNSCandidates ? const_cast<LibWebRTCNetworkManager*>(this) : nullptr;
}

void LibWebRTCNetworkManager::networksChanged(const Vector<RTCNetwork>& networks, const RTCNetwork::IPAddress& ipv4, const RTCNetwork::IPAddress& ipv6)
{
    bool forceSignaling = !m_receivedNetworkList;
    m_receivedNetworkList = true;

    WebCore::LibWebRTCProvider::callOnWebRTCNetworkThread([this, protectedThis = Ref { *this }, networks, ipv4, ipv6, forceSignaling] {
        std::vector<std::unique_ptr<rtc::Network>> networkList(networks.size());
        for (size_t index = 0; index < networks.size(); ++index)
            networkList[index] = std::make_unique<rtc::Network>(networks[index].value());

        bool hasChanged;
        set_default_local_addresses(ipv4.value, ipv6.value);
        MergeNetworkList(WTFMove(networkList), &hasChanged);
        if (hasChanged || forceSignaling)
            SignalNetworksChanged();
    });

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

        WebProcess::singleton().libWebRTCNetwork().mdnsRegister().registerMDNSName(weakThis->m_documentIdentifier, fromStdString(address.ToString()), [address, callback = std::move(callback)](auto name, auto error) mutable {
            WebCore::LibWebRTCProvider::callOnWebRTCNetworkThread([address, callback = std::move(callback), name = WTFMove(name).isolatedCopy(), error] {
                RELEASE_LOG_ERROR_IF(error, WebRTC, "MDNS registration of a host candidate failed with error %d", *error);
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
