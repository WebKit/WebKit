/*
 *  Copyright (C) 2021 Igalia S.L
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#if USE(GLIB)

#include "NowPlayingManager.h"
#include "PlatformMediaSessionManager.h"
#include <wtf/glib/GRefPtr.h>

namespace WebCore {

struct NowPlayingInfo;

class MediaSessionManagerGLib
    : public PlatformMediaSessionManager
    , private NowPlayingManager::Client {
    WTF_MAKE_FAST_ALLOCATED;
public:
    MediaSessionManagerGLib();
    ~MediaSessionManagerGLib();

    bool setupMpris();

    void beginInterruption(PlatformMediaSession::InterruptionType) final;

    bool hasActiveNowPlayingSession() const final { return m_nowPlayingActive; }
    String lastUpdatedNowPlayingTitle() const final { return m_lastUpdatedNowPlayingTitle; }
    double lastUpdatedNowPlayingDuration() const final { return m_lastUpdatedNowPlayingDuration; }
    double lastUpdatedNowPlayingElapsedTime() const final { return m_lastUpdatedNowPlayingElapsedTime; }
    MediaUniqueIdentifier lastUpdatedNowPlayingInfoUniqueIdentifier() const final { return m_lastUpdatedNowPlayingInfoUniqueIdentifier; }
    bool registeredAsNowPlayingApplication() const final { return m_registeredAsNowPlayingApplication; }
    bool haveEverRegisteredAsNowPlayingApplication() const final { return m_haveEverRegisteredAsNowPlayingApplication; }

    void busAcquired(GDBusConnection*);
    void nameAcquired(GDBusConnection* connection) { m_connection = connection; }
    void nameLost(GDBusConnection*);
    GVariant* getMetadataAsGVariant() const { return m_nowPlayingInfo.get(); }
    GVariant* getPlaybackStatusAsGVariant(std::optional<const PlatformMediaSession*>);
    GVariant* getActiveSessionPosition();
    void dispatch(PlatformMediaSession::RemoteControlCommandType, PlatformMediaSession::RemoteCommandArgument);

protected:
    void scheduleSessionStatusUpdate() final;
    void updateNowPlayingInfo();

    void removeSession(PlatformMediaSession&) final;
    void addSession(PlatformMediaSession&) final;
    void setCurrentSession(PlatformMediaSession&) final;

    bool sessionWillBeginPlayback(PlatformMediaSession&) override;
    void sessionWillEndPlayback(PlatformMediaSession&, DelayCallingUpdateNowPlaying) override;
    void sessionStateChanged(PlatformMediaSession&) override;
    void sessionDidEndRemoteScrubbing(PlatformMediaSession&) final;
    void clientCharacteristicsChanged(PlatformMediaSession&) final;
    void sessionCanProduceAudioChanged() final;

    virtual void providePresentingApplicationPIDIfNecessary() { }

    PlatformMediaSession* nowPlayingEligibleSession();

    void addSupportedCommand(PlatformMediaSession::RemoteControlCommandType) final;
    void removeSupportedCommand(PlatformMediaSession::RemoteControlCommandType) final;
    RemoteCommandListener::RemoteCommandsSet supportedCommands() const final;

    void resetHaveEverRegisteredAsNowPlayingApplicationForTesting() final { m_haveEverRegisteredAsNowPlayingApplication = false; };

private:
    void emitPropertiesChanged(GVariant*);

#if !RELEASE_LOG_DISABLED
    const char* logClassName() const override { return "MediaSessionManagerGLib"; }
#endif

    // NowPlayingManager::Client
    void didReceiveRemoteControlCommand(PlatformMediaSession::RemoteControlCommandType type, const PlatformMediaSession::RemoteCommandArgument& argument) final { processDidReceiveRemoteControlCommand(type, argument); }

    String m_instanceId;
    unsigned m_ownerId { 0 };
    unsigned m_rootRegistrationId { 0 };
    unsigned m_playerRegistrationId { 0 };
    bool m_isSeeking { false };
    GRefPtr<GDBusConnection> m_connection;
    GRefPtr<GDBusNodeInfo> m_mprisInterface;
    GRefPtr<GVariant> m_nowPlayingInfo;

    bool m_nowPlayingActive { false };
    bool m_registeredAsNowPlayingApplication { false };
    bool m_haveEverRegisteredAsNowPlayingApplication { false };

    // For testing purposes only.
    String m_lastUpdatedNowPlayingTitle;
    double m_lastUpdatedNowPlayingDuration { NAN };
    double m_lastUpdatedNowPlayingElapsedTime { NAN };
    MediaUniqueIdentifier m_lastUpdatedNowPlayingInfoUniqueIdentifier;

    const std::unique_ptr<NowPlayingManager> m_nowPlayingManager;
};

} // namespace WebCore

#endif // USE(GLIB)
