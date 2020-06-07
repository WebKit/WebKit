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

#if ENABLE(MEDIA_STREAM)

#include <wtf/LoggerHelper.h>

namespace WTF {
class MediaTime;
}

namespace WebCore {

class AudioStreamDescription;
class PlatformAudioData;

class WEBCORE_EXPORT AudioMediaStreamTrackRenderer : public LoggerHelper {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static std::unique_ptr<AudioMediaStreamTrackRenderer> create();
    virtual ~AudioMediaStreamTrackRenderer() = default;

    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void clear() = 0;
    // May be called on a background thread. It should only be called after start/before stop is called.
    virtual void pushSamples(const WTF::MediaTime&, const PlatformAudioData&, const AudioStreamDescription&, size_t) = 0;

    virtual void setVolume(float);
    float volume() const;

#if !RELEASE_LOG_DISABLED
    void setLogger(const Logger&, const void*);
#endif

    using RendererCreator = std::unique_ptr<AudioMediaStreamTrackRenderer> (*)();
    static void setCreator(RendererCreator);

protected:
#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final;
    const void* logIdentifier() const final;

    const char* logClassName() const final;
    WTFLogChannel& logChannel() const final;
#endif

private:
    static RendererCreator m_rendererCreator;

    // Main thread writable members
    float m_volume { 1 };

#if !RELEASE_LOG_DISABLED
    RefPtr<const Logger> m_logger;
    const void* m_logIdentifier;
#endif
};

inline void AudioMediaStreamTrackRenderer::setVolume(float volume)
{
    m_volume = volume;
}

inline float AudioMediaStreamTrackRenderer::volume() const
{
    return m_volume;
}

#if !RELEASE_LOG_DISABLED
inline const Logger& AudioMediaStreamTrackRenderer::logger() const
{
    return *m_logger;
    
}

inline const void* AudioMediaStreamTrackRenderer::logIdentifier() const
{
    return m_logIdentifier;
}

inline const char* AudioMediaStreamTrackRenderer::logClassName() const
{
    return "AudioMediaStreamTrackRenderer";
}
#endif

}

#endif // ENABLE(MEDIA_STREAM)
