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

#pragma once

#if USE(LIBWEBRTC)

#include "WebRTCMonitor.h"
#include <WebCore/LibWebRTCProvider.h>
#include <WebCore/ProcessQualified.h>
#include <WebCore/RTCNetworkManager.h>
#include <WebCore/ScriptExecutionContextIdentifier.h>

namespace WebKit {

class LibWebRTCNetworkManager final : public WebCore::RTCNetworkManager, public rtc::NetworkManagerBase, public webrtc::MdnsResponderInterface, public WebRTCMonitor::Observer {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static LibWebRTCNetworkManager* getOrCreate(WebCore::ScriptExecutionContextIdentifier);
    ~LibWebRTCNetworkManager();

private:
    explicit LibWebRTCNetworkManager(WebCore::ScriptExecutionContextIdentifier);

    // WebCore::RTCNetworkManager
    void setICECandidateFiltering(bool doFiltering) final { m_useMDNSCandidates = doFiltering; }
    void unregisterMDNSNames() final;
    void close() final;

    // webrtc::NetworkManagerBase
    void StartUpdating() final;
    void StopUpdating() final;
    webrtc::MdnsResponderInterface* GetMdnsResponder() const final;

    // webrtc::MdnsResponderInterface
    void CreateNameForAddress(const rtc::IPAddress&, NameCreatedCallback);
    void RemoveNameForAddress(const rtc::IPAddress&, NameRemovedCallback);

    // WebRTCMonitor::Observer
    void networksChanged(const Vector<RTCNetwork>&, const RTCNetwork::IPAddress&, const RTCNetwork::IPAddress&) final;
    void networkProcessCrashed() final;

    WebCore::ScriptExecutionContextIdentifier m_documentIdentifier;
    bool m_useMDNSCandidates { true };
    bool m_receivedNetworkList { false };
#if ASSERT_ENABLED
    bool m_isClosed { false };
#endif
};

} // namespace WebKit

#endif
