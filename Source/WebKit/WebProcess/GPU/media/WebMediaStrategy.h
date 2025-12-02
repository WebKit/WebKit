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

#include <WebCore/MediaStrategy.h>
#include <atomic>

namespace WebKit {

class WebMediaStrategy final : public WebCore::MediaStrategy {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(WebMediaStrategy);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(WebMediaStrategy);
public:
    virtual ~WebMediaStrategy();

#if ENABLE(GPU_PROCESS)
    void setUseGPUProcess(bool useGPUProcess) { m_useGPUProcess = useGPUProcess; }
#endif

private:
    bool isWebMediaStrategy() const final { return true; }

#if ENABLE(WEB_AUDIO)
    Ref<WebCore::AudioDestination> createAudioDestination(const WebCore::AudioDestinationCreationOptions&) override;
#endif
#if ENABLE(VIDEO) && ENABLE(GPU_PROCESS)
    RefPtr<WebCore::AudioVideoRenderer> createAudioVideoRenderer(LoggerHelper*, WebCore::HTMLMediaElementIdentifier, WebCore::MediaPlayerIdentifier) const final;
#endif
    std::unique_ptr<WebCore::NowPlayingManager> createNowPlayingManager() const final;
    bool hasThreadSafeMediaSourceSupport() const final;
#if ENABLE(MEDIA_SOURCE)
    void enableMockMediaSource() final;
#endif

#if ENABLE(GPU_PROCESS)
    std::atomic<bool> m_useGPUProcess { false };
#endif
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::WebMediaStrategy) \
    static bool isType(const WebCore::MediaStrategy& strategy) { return strategy.isWebMediaStrategy(); } \
SPECIALIZE_TYPE_TRAITS_END()
