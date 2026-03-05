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

#if ENABLE(WIRELESS_PLAYBACK_MEDIA_PLAYER)

#include "MediaDeviceRouteLoadURLResult.h"
#include <WebKitAdditions/MediaDeviceRouteAdditions.h>
#include <wtf/AbstractRefCountedAndCanMakeWeakPtr.h>
#include <wtf/Forward.h>
#include <wtf/MediaTime.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/UUID.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

OBJC_CLASS WebMediaSourceObserver;

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

    virtual void timeRangeDidChange(MediaDeviceRoute&) { }
    virtual void readyDidChange(MediaDeviceRoute&) { }
    virtual void bufferingDidChange(MediaDeviceRoute&) { }
    virtual void playbackErrorDidChange(MediaDeviceRoute&) { }
    virtual void hasAudioDidChange(MediaDeviceRoute&) { }
    virtual void currentPlaybackPositionDidChange(MediaDeviceRoute&) { }
    virtual void playingDidChange(MediaDeviceRoute&) { }
    virtual void playbackSpeedDidChange(MediaDeviceRoute&) { }
    virtual void scanSpeedDidChange(MediaDeviceRoute&) { }
    virtual void mutedDidChange(MediaDeviceRoute&) { }
    virtual void volumeDidChange(MediaDeviceRoute&) { }
};

class MediaDeviceRoute final : public RefCountedAndCanMakeWeakPtr<MediaDeviceRoute> {
    WTF_MAKE_TZONE_ALLOCATED(MediaDeviceRoute);
public:
    static Ref<MediaDeviceRoute> create(WebMediaDevicePlatformRoute *);

    ~MediaDeviceRoute();

    MediaDeviceRouteClient* client() const { return m_client.get(); }
    void setClient(MediaDeviceRouteClient* client) { m_client = client; }

    const WTF::UUID& identifier() const LIFETIME_BOUND { return m_identifier; }
    String deviceName() const;
    WebMediaDevicePlatformRoute *platformRoute() const;

    void loadURL(const URL&, CompletionHandler<void(const MediaDeviceRouteLoadURLResult&)>&&);

    MediaTimeRange timeRange() const;
    bool ready() const;
    bool buffering() const;
    std::optional<MediaPlaybackSourceError> playbackError() const;
    bool hasAudio() const;
    MediaTime currentPlaybackPosition() const;
    bool playing() const;
    float playbackSpeed() const;
    float scanSpeed() const;
    bool muted() const;
    float volume() const;

    void setCurrentPlaybackPosition(MediaTime);
    void setPlaying(bool);
    void setPlaybackSpeed(float);
    void setScanSpeed(float);
    void setMuted(bool);
    void setVolume(float);

private:
    explicit MediaDeviceRoute(WebMediaDevicePlatformRoute *);

    WTF::UUID m_identifier;
    RetainPtr<WebMediaDevicePlatformRoute> m_platformRoute;
    RetainPtr<WebMediaSourceObserver> m_mediaSourceObserver;
    WeakPtr<MediaDeviceRouteClient> m_client;
#if HAVE(AVROUTING_FRAMEWORK)
    RetainPtr<WebMediaDevicePlatformRouteSession> m_routeSession;
#endif
};

} // namespace WebCore

#endif // ENABLE(WIRELESS_PLAYBACK_MEDIA_PLAYER)
