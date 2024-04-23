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

#pragma once

#if ENABLE(GPU_PROCESS) && ENABLE(WEB_AUDIO) && PLATFORM(COCOA)

#include "GPUProcessConnection.h"
#include <WebCore/MediaPlayerIdentifier.h>
#include <WebCore/WebAudioSourceProviderCocoa.h>
#include <wtf/LoggerHelper.h>

namespace WebKit {

class RemoteAudioSourceProvider final
    : public WebCore::WebAudioSourceProviderCocoa
#if !RELEASE_LOG_DISABLED
    , protected WTF::LoggerHelper
#endif
{
public:
    static Ref<RemoteAudioSourceProvider> create(WebCore::MediaPlayerIdentifier, WTF::LoggerHelper&);
    ~RemoteAudioSourceProvider();

    void audioSamplesAvailable(const WebCore::PlatformAudioData&, const WebCore::AudioStreamDescription&, size_t);
    void close();

    WebCore::MediaPlayerIdentifier identifier() const { return m_identifier; }

private:
    RemoteAudioSourceProvider(WebCore::MediaPlayerIdentifier, WTF::LoggerHelper&);

    // WebCore::WebAudioSourceProviderCocoa
    void hasNewClient(WebCore::AudioSourceProviderClient*) final;

#if !RELEASE_LOG_DISABLED
    WTF::LoggerHelper& loggerHelper() final { return *this; }

    // WTF::LoggerHelper
    const Logger& logger() const final { return m_logger.get(); }
    const void* logIdentifier() const final { return m_logIdentifier; }
    ASCIILiteral logClassName() const final { return "RemoteAudioSourceProvider"_s; }
    WTFLogChannel& logChannel() const final;
#endif

    WebCore::MediaPlayerIdentifier m_identifier;
    ThreadSafeWeakPtr<GPUProcessConnection> m_gpuProcessConnection;
#if !RELEASE_LOG_DISABLED
    Ref<const Logger> m_logger;
    const void* m_logIdentifier;
#endif
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(WEB_AUDIO) && PLATFORM(COCOA)
