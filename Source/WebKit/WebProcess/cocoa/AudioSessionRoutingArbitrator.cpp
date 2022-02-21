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

#include "config.h"
#include "AudioSessionRoutingArbitrator.h"

#if ENABLE(ROUTING_ARBITRATION)

#include "AudioSessionRoutingArbitratorProxy.h"
#include "AudioSessionRoutingArbitratorProxyMessages.h"
#include "WebConnectionToUIProcess.h"
#include "WebProcess.h"

namespace WebKit {

using namespace WebCore;

AudioSessionRoutingArbitrator::AudioSessionRoutingArbitrator(WebProcess& process)
    : m_process(process)
    , m_observer([this] (AudioSession& session) { session.setRoutingArbitrationClient(*this); })
{
    AudioSession::addAudioSessionChangedObserver(m_observer);
}

AudioSessionRoutingArbitrator::~AudioSessionRoutingArbitrator() = default;

const char* AudioSessionRoutingArbitrator::supplementName()
{
    return "AudioSessionRoutingArbitrator";
}

void AudioSessionRoutingArbitrator::beginRoutingArbitrationWithCategory(AudioSession::CategoryType category, CompletionHandler<void(RoutingArbitrationError, DefaultRouteChanged)>&& callback)
{
    m_process.parentProcessConnection()->sendWithAsyncReply(Messages::AudioSessionRoutingArbitratorProxy::BeginRoutingArbitrationWithCategory(category), WTFMove(callback), AudioSessionRoutingArbitratorProxy::destinationId());
}

void AudioSessionRoutingArbitrator::leaveRoutingAbritration()
{
    m_process.parentProcessConnection()->send(Messages::AudioSessionRoutingArbitratorProxy::EndRoutingArbitration(), AudioSessionRoutingArbitratorProxy::destinationId());
}

}

#endif
