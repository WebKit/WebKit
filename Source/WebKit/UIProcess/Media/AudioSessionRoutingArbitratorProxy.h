/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#if ENABLE(ROUTING_ARBITRATION)

#include "MessageReceiver.h"
#include <WebCore/AudioSession.h>
#include <wtf/WeakPtr.h>

namespace WebKit {

class WebProcessProxy;

class AudioSessionRoutingArbitratorProxy
    : public IPC::MessageReceiver
    , public CanMakeWeakPtr<AudioSessionRoutingArbitratorProxy> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    AudioSessionRoutingArbitratorProxy(WebProcessProxy&);
    virtual ~AudioSessionRoutingArbitratorProxy();

    void processDidTerminate();
    WebCore::AudioSession::CategoryType category() const { return m_category; }

    static uint64_t destinationId() { return 1; }

    using RoutingArbitrationError = WebCore::AudioSessionRoutingArbitrationClient::RoutingArbitrationError;
    using DefaultRouteChanged = WebCore::AudioSessionRoutingArbitrationClient::DefaultRouteChanged;
    using ArbitrationCallback = CompletionHandler<void(RoutingArbitrationError, DefaultRouteChanged)>;

    enum class ArbitrationStatus : uint8_t {
        None,
        Pending,
        Active,
    };

    ArbitrationStatus arbitrationStatus() const { return m_arbitrationStatus; }

private:
    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    // Messages
    void beginRoutingArbitrationWithCategory(WebCore::AudioSession::CategoryType, ArbitrationCallback&&);
    void endRoutingArbitration();

    WebProcessProxy& m_process;
    WebCore::AudioSession::CategoryType m_category { WebCore::AudioSession::None };
    ArbitrationStatus m_arbitrationStatus { ArbitrationStatus::None };
};

}

#endif
