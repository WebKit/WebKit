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

#import "config.h"
#import "AudioSessionRoutingArbitratorProxy.h"

#if ENABLE(ROUTING_ARBITRATION) && HAVE(AVAUDIO_ROUTING_ARBITER)

#import "AudioSessionRoutingArbitratorProxyMessages.h"
#import "Logging.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import <WebCore/AudioSession.h>

namespace WebKit {

using namespace WebCore;

AudioSessionRoutingArbitratorProxy::AudioSessionRoutingArbitratorProxy(WebProcessProxy& proxy)
    : m_process(proxy)
    , m_token(SharedRoutingArbitrator::Token::create())
{
    m_logIdentifier = m_token->logIdentifier();
    SharedRoutingArbitrator::sharedInstance().setLogger(logger());
    proxy.addMessageReceiver(Messages::AudioSessionRoutingArbitratorProxy::messageReceiverName(), destinationId(), *this);
}

AudioSessionRoutingArbitratorProxy::~AudioSessionRoutingArbitratorProxy()
{
    // Unable to ref the process because it may have started destruction.
    CheckedRef checkedProcess = m_process.get();
    checkedProcess->removeMessageReceiver(Messages::AudioSessionRoutingArbitratorProxy::messageReceiverName(), destinationId());
}

void AudioSessionRoutingArbitratorProxy::processDidTerminate()
{
    if (SharedRoutingArbitrator::sharedInstance().isInRoutingArbitrationForToken(m_token))
        endRoutingArbitration();
}

void AudioSessionRoutingArbitratorProxy::beginRoutingArbitrationWithCategory(WebCore::AudioSession::CategoryType category, ArbitrationCallback&& callback)
{
    auto identifier = LOGIDENTIFIER;
    ALWAYS_LOG(identifier, category);

    m_category = category;
    m_arbitrationStatus = ArbitrationStatus::Pending;
    m_arbitrationUpdateTime = WallTime::now();
    SharedRoutingArbitrator::sharedInstance().beginRoutingArbitrationForToken(m_token, category, [this, weakThis = WeakPtr { *this }, callback = WTFMove(callback), identifier = WTFMove(identifier)] (RoutingArbitrationError error, DefaultRouteChanged routeChanged) mutable {
        if (weakThis) {
            ALWAYS_LOG(identifier, "callback, error = ", error, ", routeChanged = ", routeChanged);
            weakThis->m_arbitrationStatus = error == RoutingArbitrationError::None ? ArbitrationStatus::Active : ArbitrationStatus::None;
        }
        callback(error, routeChanged);
    });
}

void AudioSessionRoutingArbitratorProxy::endRoutingArbitration()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    SharedRoutingArbitrator::sharedInstance().endRoutingArbitrationForToken(m_token);
    m_arbitrationStatus = ArbitrationStatus::None;
}

}

#endif
