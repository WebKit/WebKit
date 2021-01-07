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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RemoteAudioSourceProvider.h"

#if ENABLE(GPU_PROCESS) && ENABLE(WEB_AUDIO) && PLATFORM(COCOA)

#include "GPUProcessConnection.h"
#include "RemoteAudioSourceProviderManager.h"
#include "RemoteMediaPlayerProxyMessages.h"

namespace WebCore {
#if !RELEASE_LOG_DISABLED
extern WTFLogChannel LogMedia;
#endif
}

namespace WebKit {
using namespace WebCore;

Ref<RemoteAudioSourceProvider> RemoteAudioSourceProvider::create(WebCore::MediaPlayerIdentifier identifier, WTF::LoggerHelper& helper)
{
    auto provider = adoptRef(*new RemoteAudioSourceProvider(identifier, helper));
    provider->m_gpuProcessConnection->audioSourceProviderManager().addProvider(provider.copyRef());
    return provider;
}

RemoteAudioSourceProvider::RemoteAudioSourceProvider(MediaPlayerIdentifier identifier, WTF::LoggerHelper& helper)
    : m_identifier(identifier)
    , m_gpuProcessConnection(makeWeakPtr(WebProcess::singleton().ensureGPUProcessConnection()))
#if !RELEASE_LOG_DISABLED
    , m_logger(helper.logger())
    , m_logIdentifier(helper.logIdentifier())
#endif
{
    ASSERT(isMainThread());
    UNUSED_PARAM(helper);

#if ENABLE(WEB_AUDIO)
    m_gpuProcessConnection->connection().send(Messages::RemoteMediaPlayerProxy::CreateAudioSourceProvider { }, identifier);
#endif
}

RemoteAudioSourceProvider::~RemoteAudioSourceProvider()
{
}

void RemoteAudioSourceProvider::close()
{
    ASSERT(isMainThread());
    if (m_gpuProcessConnection)
        m_gpuProcessConnection->audioSourceProviderManager().removeProvider(m_identifier);
}

void RemoteAudioSourceProvider::hasNewClient(AudioSourceProviderClient* client)
{
    if (m_gpuProcessConnection)
        m_gpuProcessConnection->connection().send(Messages::RemoteMediaPlayerProxy::SetShouldEnableAudioSourceProvider { !!client }, m_identifier);
}

void RemoteAudioSourceProvider::audioSamplesAvailable(const PlatformAudioData& data, const AudioStreamDescription& description, size_t size)
{
    receivedNewAudioSamples(data, description, size);
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& RemoteAudioSourceProvider::logChannel() const
{
    return LogMedia;
}
#endif

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(WEB_AUDIO) && PLATFORM(COCOA)
