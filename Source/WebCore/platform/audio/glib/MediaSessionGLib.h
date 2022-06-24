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

#if USE(GLIB) && ENABLE(MEDIA_SESSION)

#include "MediaSessionIdentifier.h"
#include "PlatformMediaSession.h"
#include <wtf/glib/GRefPtr.h>

namespace WebCore {

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

private:
    void emitPropertiesChanged(GVariant*);
    std::optional<NowPlayingInfo> nowPlayingInfo();
    void ensureMprisSessionRegistered();

    MediaSessionIdentifier m_identifier;
    MediaSessionManagerGLib& m_manager;
    GRefPtr<GDBusConnection> m_connection;
    String m_instanceId;
    unsigned m_ownerId { 0 };
    unsigned m_rootRegistrationId { 0 };
    unsigned m_playerRegistrationId { 0 };
};

} // namespace WebCore

#endif // USE(GLIB) && ENABLE(MEDIA_SESSION)
