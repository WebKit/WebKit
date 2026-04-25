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

#include "PlatformVideoTrackConfiguration.h"
#include "VideoColorSpace.h"
#include <wtf/Observer.h>
#include <wtf/OptionSet.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakHashSet.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

using VideoTrackConfigurationInit = PlatformVideoTrackConfiguration;

class VideoTrackConfiguration : public RefCounted<VideoTrackConfiguration> {
    WTF_MAKE_TZONE_ALLOCATED(VideoTrackConfiguration);
public:
    static Ref<VideoTrackConfiguration> create(VideoTrackConfigurationInit&& init) { return adoptRef(*new VideoTrackConfiguration(WTF::move(init))); }
    static Ref<VideoTrackConfiguration> create() { return adoptRef(*new VideoTrackConfiguration()); }

    enum class StateChanged : bool { No, Yes };
    StateChanged updateState(const VideoTrackConfigurationInit&);

    String codec() const { return m_state.codec; }
    void setCodec(String);

    uint32_t width() const { return m_state.width; }
    void setWidth(uint32_t);

    uint32_t height() const { return m_state.height; }
    void setHeight(uint32_t);

    Ref<VideoColorSpace> colorSpace() const { return m_colorSpace; }
    void setColorSpace(Ref<VideoColorSpace>&&);

    double framerate() const { return m_state.framerate; }
    void setFramerate(double);

    uint64_t bitrate() const { return m_state.bitrate; }
    void setBitrate(uint64_t);

    std::optional<ImmersiveVideoMetadata> immersiveVideoMetadata() const { return m_state.immersiveVideoMetadata; }
    void setImmersiveVideoMetadata(std::optional<ImmersiveVideoMetadata>);

    bool isProtected() const { return m_state.isProtected; }
    void setProtected(bool);

    using ChangedStateObserver = Observer<void()>;
    void addStateObserver(ChangedStateObserver&);
    void removeStateObserver(const ChangedStateObserver&);

    Ref<JSON::Object> toJSON() const;

private:
    VideoTrackConfiguration(VideoTrackConfigurationInit&& init)
        : m_state(init)
        , m_colorSpace(VideoColorSpace::create(init.colorSpace))
    {
    }
    VideoTrackConfiguration()
        : m_colorSpace(VideoColorSpace::create())
    {
    }

    void notifyObservers();

    VideoTrackConfigurationInit m_state;
    Ref<VideoColorSpace> m_colorSpace;

    WeakHashSet<ChangedStateObserver> m_observers;
};

}

#endif
