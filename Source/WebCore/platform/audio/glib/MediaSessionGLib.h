/*
 *  Copyright (C) 2021 Igalia S.L
 *  Copyright (C) 2024 Alexander M (webkit@sata.lol)
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

#if USE(GLIB) && ENABLE(MEDIA_SESSION)

#include "MediaSessionIdentifier.h"
#include "PlatformMediaSession.h"
#include <wtf/glib/GRefPtr.h>

namespace WebCore {

enum class MediaSessionGLibMprisRegistrationEligiblilty : uint8_t {
    Eligible,
    NotEligible,
};

class MediaSessionManagerGLib;

class MediaSessionGLib {
    WTF_MAKE_FAST_ALLOCATED;

public:
    static std::unique_ptr<MediaSessionGLib> create(MediaSessionManagerGLib&, MediaSessionIdentifier);

    explicit MediaSessionGLib(MediaSessionManagerGLib&, GRefPtr<GDBusConnection>&&, MediaSessionIdentifier);
    ~MediaSessionGLib();

    MediaSessionManagerGLib& manager() const { return m_manager; }

    GVariant* getPlaybackStatusAsGVariant(std::optional<const PlatformMediaSession*>);
    GVariant* getMetadataAsGVariant(std::optional<NowPlayingInfo>);
    GVariant* getPositionAsGVariant();
    GVariant* canSeekAsGVariant();

    void emitPositionChanged(double time);
    void updateNowPlaying(NowPlayingInfo&);
    void playbackStatusChanged(PlatformMediaSession&);

    void unregisterMprisSession();

    using MprisRegistrationEligiblilty = MediaSessionGLibMprisRegistrationEligiblilty;
    void setMprisRegistrationEligibility(MprisRegistrationEligiblilty eligibility) { m_registrationEligibility = eligibility; }
    MprisRegistrationEligiblilty mprisRegistrationEligibility() const { return m_registrationEligibility; }
private:
    void emitPropertiesChanged(GRefPtr<GVariant>&&);
    std::optional<NowPlayingInfo> nowPlayingInfo();
    bool ensureMprisSessionRegistered();

    MediaSessionIdentifier m_identifier;
    MediaSessionManagerGLib& m_manager;
    GRefPtr<GDBusConnection> m_connection;
    MprisRegistrationEligiblilty m_registrationEligibility { MprisRegistrationEligiblilty::NotEligible };
    String m_instanceId;
    unsigned m_ownerId { 0 };
    unsigned m_rootRegistrationId { 0 };
    unsigned m_playerRegistrationId { 0 };
};

String convertEnumerationToString(MediaSessionGLib::MprisRegistrationEligiblilty);

} // namespace WebCore

namespace WTF {

template<typename Type>
struct LogArgument;

template <>
struct LogArgument<WebCore::MediaSessionGLib::MprisRegistrationEligiblilty> {
    static String toString(const WebCore::MediaSessionGLib::MprisRegistrationEligiblilty state)
    {
        return convertEnumerationToString(state);
    }
};

} // namespace WTF

#endif // USE(GLIB) && ENABLE(MEDIA_SESSION)
