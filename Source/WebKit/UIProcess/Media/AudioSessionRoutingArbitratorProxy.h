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
#include <wtf/CheckedRef.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WallTime.h>
#include <wtf/WeakPtr.h>

#if HAVE(AVAUDIO_ROUTING_ARBITER)
#import <WebCore/SharedRoutingArbitrator.h>
#endif

namespace WebKit {

class WebProcessProxy;
struct SharedPreferencesForWebProcess;

class AudioSessionRoutingArbitratorProxy
    : public IPC::MessageReceiver {
    WTF_MAKE_TZONE_ALLOCATED(AudioSessionRoutingArbitratorProxy);
public:
    explicit AudioSessionRoutingArbitratorProxy(WebProcessProxy&);
    virtual ~AudioSessionRoutingArbitratorProxy();

    void processDidTerminate();
    WebCore::AudioSession::CategoryType category() const { return m_category; }

    static uint64_t destinationId() { return 1; }

    using RoutingArbitrationError = WebCore::AudioSessionRoutingArbitrationClient::RoutingArbitrationError;
    using DefaultRouteChanged = WebCore::AudioSessionRoutingArbitrationClient::DefaultRouteChanged;
    using ArbitrationCallback = WebCore::AudioSessionRoutingArbitrationClient::ArbitrationCallback;

    enum class ArbitrationStatus : uint8_t {
        None,
        Pending,
        Active,
    };

    ArbitrationStatus arbitrationStatus() const { return m_arbitrationStatus; }
    WallTime arbitrationUpdateTime() const { return m_arbitrationUpdateTime; }
    std::optional<SharedPreferencesForWebProcess> sharedPreferencesForWebProcess() const;

    void ref() const;
    void deref() const;

protected:
    Logger& logger();
    uint64_t logIdentifier() const { return m_logIdentifier; }
    ASCIILiteral logClassName() const { return "AudioSessionRoutingArbitrator"_s; }
    WTFLogChannel& logChannel() const;

private:
    Ref<WebProcessProxy> protectedProcess();

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    // Messages
    void beginRoutingArbitrationWithCategory(WebCore::AudioSession::CategoryType, ArbitrationCallback&&);
    void endRoutingArbitration();

    WeakRef<WebProcessProxy> m_process;
    WebCore::AudioSession::CategoryType m_category { WebCore::AudioSession::CategoryType::None };
    ArbitrationStatus m_arbitrationStatus { ArbitrationStatus::None };
    WallTime m_arbitrationUpdateTime;
    uint64_t m_logIdentifier { 0 };

#if HAVE(AVAUDIO_ROUTING_ARBITER)
    UniqueRef<WebCore::SharedRoutingArbitratorToken> m_token;
#endif
};

}

#endif
