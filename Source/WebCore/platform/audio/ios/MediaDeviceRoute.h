/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if HAVE(AVROUTING_FRAMEWORK)

#include <WebKitAdditions/MediaDeviceRouteInterfaceAdditions.h>
#include <wtf/AbstractRefCountedAndCanMakeWeakPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>

OBJC_CLASS WebMediaDeviceRoute;

namespace WebCore {

struct MediaTimelineSegment {
    enum class Type : uint8_t {
        Primary,
        Secondary,
    };

    const Type type;
    const bool isMarked;
    const bool requiresLinearPlayback;
    const MediaTimeRange timeRange;
    const String identifier;
};

enum class MediaPlaybackSourceState : uint8_t {
    Ready,
    Loading,
    Seeking,
    Scanning,
    Scrubbing,
};

enum class MediaPlaybackSourceSupportedMode : uint8_t {
    ScanForward = 1 << 0,
    ScanBackward = 1 << 1,
    Seek = 1 << 2,
};

enum class MediaPlaybackSourcePlaybackType : uint8_t {
    Regular = 1 << 0,
    Live = 1 << 1,
};

struct MediaPlaybackSourceError {
    const long code;
    const String domain;
    const String localizedDescription;
};

struct MediaSelectionOption {
    enum class Type : uint8_t {
        Audio,
        Legible,
    };

    const String displayName;
    const String identifier;
    const Type type;
    const String extendedLanguageTag;
};

class MediaDeviceRoute;

class MediaDeviceRouteClient : public AbstractRefCountedAndCanMakeWeakPtr<MediaDeviceRouteClient> {
public:
    virtual ~MediaDeviceRouteClient() = default;

    virtual void minValueDidChange(MediaDeviceRoute&) = 0;
    virtual void maxValueDidChange(MediaDeviceRoute&) = 0;
    virtual void currentValueDidChange(MediaDeviceRoute&) = 0;
    virtual void segmentsDidChange(MediaDeviceRoute&) = 0;
    virtual void currentSegmentDidChange(MediaDeviceRoute&) = 0;
    virtual void isPlayingDidChange(MediaDeviceRoute&) = 0;
    virtual void playbackSpeedDidChange(MediaDeviceRoute&) = 0;
    virtual void scanSpeedDidChange(MediaDeviceRoute&) = 0;
    virtual void stateDidChange(MediaDeviceRoute&) = 0;
    virtual void supportedModesDidChange(MediaDeviceRoute&) = 0;
    virtual void playbackTypeDidChange(MediaDeviceRoute&) = 0;
    virtual void playbackErrorDidChange(MediaDeviceRoute&) = 0;
    virtual void currentAudioOptionDidChange(MediaDeviceRoute&) = 0;
    virtual void currentSubtitleOptionDidChange(MediaDeviceRoute&) = 0;
    virtual void optionsDidChange(MediaDeviceRoute&) = 0;
    virtual void hasAudioDidChange(MediaDeviceRoute&) = 0;
    virtual void mutedDidChange(MediaDeviceRoute&) = 0;
    virtual void volumeDidChange(MediaDeviceRoute&) = 0;
};

class MediaDeviceRoute : public RefCountedAndCanMakeWeakPtr<MediaDeviceRoute> {
    WTF_MAKE_TZONE_ALLOCATED(MediaDeviceRoute);
public:
    static Ref<MediaDeviceRoute> create(WebMediaDevicePlatformRoute *);

    ~MediaDeviceRoute();

    MediaDeviceRouteClient* client() const { return m_client.get(); }
    void setClient(MediaDeviceRouteClient* client) { m_client = client; }

    float minValue() const;
    float maxValue() const;
    float currentValue() const;
    Vector<MediaTimelineSegment> segments() const;
    std::optional<MediaTimelineSegment> currentSegment() const;
    bool isPlaying() const;
    double playbackSpeed() const;
    double scanSpeed() const;
    MediaPlaybackSourceState state() const;
    OptionSet<MediaPlaybackSourceSupportedMode> supportedModes() const;
    OptionSet<MediaPlaybackSourcePlaybackType> playbackType() const;
    std::optional<MediaPlaybackSourceError> playbackError() const;
    std::optional<MediaSelectionOption> currentAudioOption() const;
    std::optional<MediaSelectionOption> currentSubtitleOption() const;
    Vector<MediaSelectionOption> options() const;
    bool hasAudio() const;
    bool muted() const;
    double volume() const;

    void setCurrentValue(float);
    void setIsPlaying(bool);
    void setPlaybackSpeed(double);
    void setScanSpeed(double);
    void setCurrentAudioOption(std::optional<MediaSelectionOption>);
    void setCurrentSubtitleOption(std::optional<MediaSelectionOption>);
    void setMuted(bool);
    void setVolume(double);

private:
    explicit MediaDeviceRoute(WebMediaDevicePlatformRoute *);

    RetainPtr<WebMediaDeviceRoute> m_route;
    WeakPtr<MediaDeviceRouteClient> m_client;
};

} // namespace WebCore

#endif // HAVE(AVROUTING_FRAMEWORK)
