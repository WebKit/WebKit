/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if ENABLE(ALTERNATE_WEBM_PLAYER)

#include "MediaPlayerPrivate.h"
#include "PlatformLayer.h"
#include <wtf/LoggerHelper.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class MediaPlayerPrivateWebM
    : public MediaPlayerPrivateInterface
    , private LoggerHelper {
    WTF_MAKE_FAST_ALLOCATED;
public:
    MediaPlayerPrivateWebM(MediaPlayer*);
    ~MediaPlayerPrivateWebM();
    
    static void registerMediaEngine(MediaEngineRegistrar);
    
    void load(const String&) final;
    
#if ENABLE(MEDIA_SOURCE)
    void load(const URL&, const ContentType&, MediaSourcePrivateClient&) final;
#endif
#if ENABLE(MEDIA_STREAM)
    void load(MediaStreamPrivate&) final;
#endif
    
    void cancelLoad() final;
    
    PlatformLayer* platformLayer() const final;
    
    void play() final;
    void pause() final;
    
    FloatSize naturalSize() const final { return m_naturalSize; };
    
    bool hasAudio() const final { return m_hasAudio; };
    bool hasVideo() const final { return m_hasVideo; };
    
    void setPageIsVisible(bool) final;
    
    MediaTime currentMediaTime() const final;
    MediaTime durationMediaTime() const final { return m_duration; };
    
    bool seeking() const final { return m_seeking; };
    
    bool paused() const final { return m_paused; };
    
    MediaPlayer::NetworkState networkState() const final { return m_networkState; };
    MediaPlayer::ReadyState readyState() const final { return m_readyState; };
    
    std::unique_ptr<PlatformTimeRanges> buffered() const final;
    
    bool didLoadingProgress() const final;
    
    void paint(GraphicsContext&, const FloatRect&) final { };
    
    DestinationColorSpace colorSpace() final { return DestinationColorSpace::SRGB(); };
    
    bool supportsAcceleratedRendering() const final { return false; };
    
    const Logger& logger() const final { return m_logger.get(); }
    const char* logClassName() const final { return "MediaPlayerPrivateWebM"; }
    const void* logIdentifier() const final { return reinterpret_cast<const void*>(m_logIdentifier); }
    WTFLogChannel& logChannel() const final;
    
private:
    friend class MediaPlayerFactoryWebM;
    static void getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>&);
    static MediaPlayer::SupportsType supportsType(const MediaEngineSupportParameters&);
    
    MediaPlayer* m_player;
    
    MediaPlayer::NetworkState m_networkState { MediaPlayer::NetworkState::Empty };
    MediaPlayer::ReadyState m_readyState { MediaPlayer::ReadyState::HaveNothing };
    
    Ref<const Logger> m_logger;
    const void* m_logIdentifier;
    
    FloatSize m_naturalSize;
    bool m_hasAudio { false };
    bool m_hasVideo { false };
    MediaTime m_currentTime;
    MediaTime m_duration;
    bool m_seeking { false };
    bool m_paused { false };
};

} // namespace WebCore

#endif // ENABLE(ALTERNATE_WEBM_PLAYER)
