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

#pragma once

#if ENABLE(VIDEO)

#include "VideoColorSpace.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

struct VideoTrackConfigurationInit {
    String codec;
    uint32_t width { 0 };
    uint32_t height { 0 };
    RefPtr<VideoColorSpace> colorSpace;
    double framerate { 0 };
    uint64_t bitrate { 0 };
};

class VideoTrackConfiguration : public RefCounted<VideoTrackConfiguration> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<VideoTrackConfiguration> create(VideoTrackConfigurationInit&& init) { return adoptRef(*new VideoTrackConfiguration(WTFMove(init))); }
    static Ref<VideoTrackConfiguration> create() { return adoptRef(*new VideoTrackConfiguration()); }

    String codec() const { return m_state.codec; }
    void setCodec(String codec) { m_state.codec = codec; }

    uint32_t width() const { return m_state.width; }
    void setWidth(uint32_t width) { m_state.width = width; }

    uint32_t height() const { return m_state.height; }
    void setHeight(uint32_t height) { m_state.height = height; }

    RefPtr<VideoColorSpace> colorSpace() const { return m_state.colorSpace; }
    void setColorSpace(RefPtr<VideoColorSpace>&& colorSpace) { m_state.colorSpace = WTFMove(colorSpace); }

    double framerate() const { return m_state.framerate; }
    void setFramerate(double framerate) { m_state.framerate = framerate; }

    uint64_t bitrate() const { return m_state.bitrate; }
    void setBitrate(uint64_t bitrate) { m_state.bitrate = bitrate; }

private:
    VideoTrackConfiguration(VideoTrackConfigurationInit&& init)
        : m_state(init)
    {
    }
    VideoTrackConfiguration() = default;

    VideoTrackConfigurationInit m_state;
};

}

#endif
