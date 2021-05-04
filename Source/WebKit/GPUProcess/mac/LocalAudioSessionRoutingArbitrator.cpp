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
#include "LocalAudioSessionRoutingArbitrator.h"

#if ENABLE(ROUTING_ARBITRATION) && HAVE(AVAUDIO_ROUTING_ARBITER)

namespace WebKit {

using namespace WebCore;

UniqueRef<LocalAudioSessionRoutingArbitrator> LocalAudioSessionRoutingArbitrator::create()
{
    return makeUniqueRef<LocalAudioSessionRoutingArbitrator>();
}

LocalAudioSessionRoutingArbitrator::LocalAudioSessionRoutingArbitrator()
    : m_token(SharedRoutingArbitrator::Token::create())
{
}

LocalAudioSessionRoutingArbitrator::~LocalAudioSessionRoutingArbitrator() = default;

void LocalAudioSessionRoutingArbitrator::processDidTerminate()
{
    if (SharedRoutingArbitrator::sharedInstance().isInRoutingArbitrationForToken(m_token))
        leaveRoutingAbritration();
}

void LocalAudioSessionRoutingArbitrator::beginRoutingArbitrationWithCategory(AudioSession::CategoryType category, CompletionHandler<void(RoutingArbitrationError, DefaultRouteChanged)>&& callback)
{
    m_category = category;
    m_arbitrationStatus = ArbitrationStatus::Pending;
    m_arbitrationUpdateTime = WallTime::now();

    SharedRoutingArbitrator::sharedInstance().beginRoutingArbitrationForToken(m_token, category, [weakThis = makeWeakPtr(*this), callback = WTFMove(callback)] (RoutingArbitrationError error, DefaultRouteChanged routeChanged) mutable {
        if (weakThis)
            weakThis->m_arbitrationStatus = error == RoutingArbitrationError::None ? ArbitrationStatus::Active : ArbitrationStatus::None;
        callback(error, routeChanged);
    });
}

void LocalAudioSessionRoutingArbitrator::leaveRoutingAbritration()
{
    SharedRoutingArbitrator::sharedInstance().endRoutingArbitrationForToken(m_token);
    m_arbitrationStatus = ArbitrationStatus::None;
}

}

#endif
