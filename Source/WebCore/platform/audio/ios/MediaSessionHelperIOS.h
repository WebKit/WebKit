/*
 * Copyright (C) 2020-2023 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)

#include <WebCore/MediaDeviceRouteController.h>
#include <WebCore/MediaPlaybackTarget.h>
#include <wtf/AbstractRefCountedAndCanMakeWeakPtr.h>
#include <wtf/ProcessID.h>
#include <wtf/WeakHashSet.h>

namespace WebCore {

enum class SuspendedUnderLock : bool { No, Yes };
enum class HasAvailableTargets : bool { No, Yes };
enum class PlayingToAutomotiveHeadUnit : bool { No, Yes };
enum class ShouldPause : bool { No, Yes };
enum class SupportsAirPlayVideo : bool { No, Yes };
enum class SupportsSpatialAudioPlayback : bool { No, Yes };

class MediaSessionHelperClient : public AbstractRefCountedAndCanMakeWeakPtr<MediaSessionHelperClient> {
public:
    virtual ~MediaSessionHelperClient() = default;

    using SuspendedUnderLock = WebCore::SuspendedUnderLock;
    virtual void uiApplicationWillEnterForeground(SuspendedUnderLock) = 0;
    virtual void uiApplicationDidEnterBackground(SuspendedUnderLock) = 0;
    virtual void uiApplicationWillBecomeInactive() = 0;
    virtual void uiApplicationDidBecomeActive() = 0;

    using HasAvailableTargets = WebCore::HasAvailableTargets;
    virtual void externalOutputDeviceAvailableDidChange(HasAvailableTargets) = 0;

    using PlayingToAutomotiveHeadUnit = WebCore::PlayingToAutomotiveHeadUnit;
    virtual void isPlayingToAutomotiveHeadUnitDidChange(PlayingToAutomotiveHeadUnit) = 0;

    using ShouldPause = WebCore::ShouldPause;
    virtual void activeAudioRouteDidChange(ShouldPause) = 0;

    using SupportsAirPlayVideo = WebCore::SupportsAirPlayVideo;
    virtual void activeVideoRouteDidChange(SupportsAirPlayVideo, Ref<MediaPlaybackTarget>&&) = 0;

    using SupportsSpatialAudioPlayback = WebCore::SupportsSpatialAudioPlayback;
    virtual void activeAudioRouteSupportsSpatialPlaybackDidChange(SupportsSpatialAudioPlayback) = 0;
};

class WEBCORE_EXPORT MediaSessionHelper
#if ENABLE(WIRELESS_PLAYBACK_MEDIA_PLAYER)
    : public MediaDeviceRouteControllerClient
#else
    : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<MediaSessionHelper>
#endif
{
public:
    static MediaSessionHelper& sharedHelper();
    static Ref<MediaSessionHelper> protectedSharedHelper();
    static void setSharedHelper(Ref<MediaSessionHelper>&&);
    static void resetSharedHelper();

    MediaSessionHelper() = default;
    explicit MediaSessionHelper(bool isExternalOutputDeviceAvailable);
    virtual ~MediaSessionHelper() = default;

    void addClient(MediaSessionHelperClient&);
    void removeClient(MediaSessionHelperClient&);

    void startMonitoringWirelessRoutes();
    void stopMonitoringWirelessRoutes();

    virtual std::optional<ProcessID> presentedApplicationPID() const;
    virtual void providePresentingApplicationPID(ProcessID);

    void setIsExternalOutputDeviceAvailable(bool);

    bool isMonitoringWirelessRoutes() const { return m_monitoringWirelessRoutesCount; }
    bool isExternalOutputDeviceAvailable() const { return m_isExternalOutputDeviceAvailable; }
    bool activeVideoRouteSupportsAirPlayVideo() const { return m_activeVideoRouteSupportsAirPlayVideo; }
    bool isPlayingToAutomotiveHeadUnit() const { return m_isPlayingToAutomotiveHeadUnit; }

    MediaPlaybackTarget* playbackTarget() const { return m_playbackTarget.get(); }

    using HasAvailableTargets = MediaSessionHelperClient::HasAvailableTargets;
    using PlayingToAutomotiveHeadUnit = MediaSessionHelperClient::PlayingToAutomotiveHeadUnit;
    using ShouldPause = MediaSessionHelperClient::ShouldPause;
    using SupportsAirPlayVideo = MediaSessionHelperClient::SupportsAirPlayVideo;
    using SuspendedUnderLock = MediaSessionHelperClient::SuspendedUnderLock;
    using SupportsSpatialAudioPlayback = MediaSessionHelperClient::SupportsSpatialAudioPlayback;

    void activeAudioRouteDidChange(ShouldPause);
    void applicationWillEnterForeground(SuspendedUnderLock);
    void applicationDidEnterBackground(SuspendedUnderLock);
    void applicationWillBecomeInactive();
    void applicationDidBecomeActive();

    void setActiveAudioRouteSupportsSpatialPlayback(bool);
    void updateActiveAudioRouteSupportsSpatialPlayback();

#if ENABLE(WIRELESS_PLAYBACK_MEDIA_PLAYER)
    void activeRoutesDidChange(MediaDeviceRouteController&) final;
#endif

protected:
    void externalOutputDeviceAvailableDidChange(HasAvailableTargets);
    void isPlayingToAutomotiveHeadUnitDidChange(PlayingToAutomotiveHeadUnit);
    void activeVideoRouteDidChange(SupportsAirPlayVideo, Ref<MediaPlaybackTarget>&&);
    void activeAudioRouteSupportsSpatialPlaybackDidChange(SupportsSpatialAudioPlayback);

private:
    virtual void startMonitoringWirelessRoutesInternal() = 0;
    virtual void stopMonitoringWirelessRoutesInternal() = 0;

    WeakHashSet<MediaSessionHelperClient> m_clients;
    bool m_isExternalOutputDeviceAvailable { false };
    uint32_t m_monitoringWirelessRoutesCount { 0 };
    bool m_activeVideoRouteSupportsAirPlayVideo { false };
    bool m_isPlayingToAutomotiveHeadUnit { false };
    SupportsSpatialAudioPlayback m_activeAudioRouteSupportsSpatialPlayback { SupportsSpatialAudioPlayback::No };
    RefPtr<MediaPlaybackTarget> m_playbackTarget;
};

inline MediaSessionHelper::MediaSessionHelper(bool isExternalOutputDeviceAvailable)
    : m_isExternalOutputDeviceAvailable(isExternalOutputDeviceAvailable)
{
}

inline void MediaSessionHelper::setIsExternalOutputDeviceAvailable(bool isExternalOutputDeviceAvailable)
{
    m_isExternalOutputDeviceAvailable = isExternalOutputDeviceAvailable;
}

}

#endif
