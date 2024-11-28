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
#include "AudioSessionRoutingArbitratorProxy.h"
#include "Logging.h"
#include "WebProcessProxy.h"
#include <wtf/LoggerHelper.h>
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(ROUTING_ARBITRATION)

#include "AudioSessionRoutingArbitratorProxyMessages.h"

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(AudioSessionRoutingArbitratorProxy);

#if !HAVE(AVAUDIO_ROUTING_ARBITER)

AudioSessionRoutingArbitratorProxy::AudioSessionRoutingArbitratorProxy(WebProcessProxy& proxy)
    : m_process(proxy)
    , m_logIdentifier(LoggerHelper::uniqueLogIdentifier())
{
    notImplemented();
}

AudioSessionRoutingArbitratorProxy::~AudioSessionRoutingArbitratorProxy()
{
    notImplemented();
}

void AudioSessionRoutingArbitratorProxy::processDidTerminate()
{
    notImplemented();
}

void AudioSessionRoutingArbitratorProxy::beginRoutingArbitrationWithCategory(WebCore::AudioSession::CategoryType, ArbitrationCallback&& callback)
{
    notImplemented();
    callback(RoutingArbitrationError::Failed, DefaultRouteChanged::No);
}

void AudioSessionRoutingArbitratorProxy::endRoutingArbitration()
{
    notImplemented();
}

#endif // ENABLE(AVAUDIO_ROUTING_ARBITER)

Ref<WebProcessProxy> AudioSessionRoutingArbitratorProxy::protectedProcess()
{
    return m_process.get();
}

Logger& AudioSessionRoutingArbitratorProxy::logger()
{
    return protectedProcess()->logger();
}

WTFLogChannel& AudioSessionRoutingArbitratorProxy::logChannel() const
{
    return WebKit2LogMedia;
}

void AudioSessionRoutingArbitratorProxy::ref() const
{
    return m_process->ref();
}

void AudioSessionRoutingArbitratorProxy::deref() const
{
    return m_process->deref();
}

std::optional<SharedPreferencesForWebProcess> AudioSessionRoutingArbitratorProxy::sharedPreferencesForWebProcess() const
{
    return m_process->sharedPreferencesForWebProcess();
}

} // namespace WebKit

#endif // ENABLE(ROUTING_ARBITRATION)
