/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#import "config.h"

#if USE(LIBWEBRTC)
#import "NetworkRTCSharedMonitor.h"

#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/darwin/DispatchExtras.h>

namespace WebKit {

static RetainPtr<nw_path_monitor_t> createNWPathMonitor()
{
    RetainPtr nwMonitor = adoptNS(nw_path_monitor_create());
    nw_path_monitor_set_queue(nwMonitor.get(), mainDispatchQueueSingleton());
    nw_path_monitor_set_update_handler(nwMonitor.get(), makeBlockPtr([](nw_path_t path) {
        NetworkRTCSharedMonitor::singleton().updateNetworksFromPath(path);
    }).get());
    return nwMonitor;
}

static webrtc::AdapterType interfaceAdapterType(nw_interface_t interface)
{
    switch (nw_interface_get_type(interface)) {
    case nw_interface_type_other:
        return webrtc::ADAPTER_TYPE_VPN;
    case nw_interface_type_wifi:
        return webrtc::ADAPTER_TYPE_WIFI;
    case nw_interface_type_cellular:
        return webrtc::ADAPTER_TYPE_CELLULAR;
    case nw_interface_type_wired:
        return webrtc::ADAPTER_TYPE_ETHERNET;
    case nw_interface_type_loopback:
        return webrtc::ADAPTER_TYPE_LOOPBACK;
    }
    return webrtc::ADAPTER_TYPE_UNKNOWN;
}

void NetworkRTCSharedMonitor::setupNWPathMonitor()
{
    if (auto nwMonitor = std::exchange(m_nwMonitor, { }))
        nw_path_monitor_cancel(m_nwMonitor.get());

    RELEASE_LOG(WebRTC, "NetworkRTCSharedMonitor::createNWPathMonitor");

    m_nwMonitor = createNWPathMonitor();
    nw_path_monitor_start(m_nwMonitor.get());
}

void NetworkRTCSharedMonitor::updateNetworksFromPath(nw_path_t path)
{
    RELEASE_LOG(WebRTC, "NetworkRTCSharedMonitor::updateNetworksFromPath");

    auto status = nw_path_get_status(path);
    if (status != nw_path_status_satisfied && status != nw_path_status_satisfiable)
        return;

    nw_path_enumerate_interfaces(path, makeBlockPtr([](nw_interface_t interface) -> bool {
        NetworkRTCSharedMonitor::singleton().m_adapterTypes.set(String::fromUTF8(nw_interface_get_name(interface)), interfaceAdapterType(interface));
        return true;
    }).get());
    updateNetworks();
}

} // namespace WebKit

#endif // USE(LIBWEBRTC)
