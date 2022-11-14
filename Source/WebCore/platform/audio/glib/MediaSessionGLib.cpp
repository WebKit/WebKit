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

#include "config.h"
#include "MediaSessionGLib.h"

#if USE(GLIB) && ENABLE(MEDIA_SESSION)

#include "ApplicationGLib.h"
#include "MediaSessionManagerGLib.h"
#include <gio/gio.h>
#include <wtf/SortedArrayMap.h>
#include <wtf/glib/GUniquePtr.h>

namespace WebCore {

#define DBUS_MPRIS_OBJECT_PATH "/org/mpris/MediaPlayer2"
#define DBUS_MPRIS_PLAYER_INTERFACE "org.mpris.MediaPlayer2.Player"
#define DBUS_MPRIS_TRACK_PATH "/org/mpris/MediaPlayer2/webkit"

static std::optional<PlatformMediaSession::RemoteControlCommandType> getCommand(const char* name)
{
    static const std::pair<ComparableASCIILiteral, PlatformMediaSession::RemoteControlCommandType> commandList[] = {
        { "Next", PlatformMediaSession::NextTrackCommand },
        { "Pause", PlatformMediaSession::PauseCommand },
        { "Play", PlatformMediaSession::PlayCommand },
        { "PlayPause", PlatformMediaSession::TogglePlayPauseCommand },
        { "Previous", PlatformMediaSession::PreviousTrackCommand },
        { "Seek", PlatformMediaSession::SeekToPlaybackPositionCommand },
        { "Stop", PlatformMediaSession::StopCommand }
    };

    static const SortedArrayMap map { commandList };
    auto value = map.get(StringView::fromLatin1(name), PlatformMediaSession::RemoteControlCommandType::NoCommand);
    if (value == PlatformMediaSession::RemoteControlCommandType::NoCommand)
        return { };
    return value;
}

static void handleMethodCall(GDBusConnection* /* connection */, const char* /* sender */, const char* objectPath, const char* interfaceName, const char* methodName, GVariant* parameters, GDBusMethodInvocation* invocation, gpointer userData)
{
    ASSERT(isMainThread());
    auto command = getCommand(methodName);
    if (!command) {
        g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR, G_DBUS_ERROR_FAILED, "%s.%s.%s is not available now", objectPath, interfaceName, methodName);
        return;
    }
    auto& session = *reinterpret_cast<MediaSessionGLib*>(userData);
    auto& manager = session.manager();
    PlatformMediaSession::RemoteCommandArgument argument;
    if (*command == PlatformMediaSession::SeekToPlaybackPositionCommand) {
        int64_t offset;
        g_variant_get(parameters, "(x)", &offset);
        argument.time = offset / 1000000;
    }
    manager.dispatch(*command, argument);
    g_dbus_method_invocation_return_value(invocation, nullptr);
}

enum class MprisProperty : uint8_t {
    NoProperty,
    CanControl,
    CanGoNext,
    CanGoPrevious,
    CanPause,
    CanPlay,
    CanQuit,
    CanRaise,
    CanSeek,
    DesktopEntry,
    GetMetadata,
    GetPlaybackStatus,
    GetPosition,
    HasTrackList,
    Identity,
    SupportedMimeTypes,
    SupportedUriSchemes,
};

static std::optional<MprisProperty> getMprisProperty(const char* propertyName)
{
    static constexpr std::pair<ComparableASCIILiteral, MprisProperty> propertiesList[] {
        { "CanControl", MprisProperty::CanControl },
        { "CanGoNext", MprisProperty::CanGoNext },
        { "CanGoPrevious", MprisProperty::CanGoPrevious },
        { "CanPause", MprisProperty::CanPause },
        { "CanPlay", MprisProperty::CanPlay },
        { "CanQuit", MprisProperty::CanQuit },
        { "CanRaise", MprisProperty::CanRaise },
        { "CanSeek", MprisProperty::CanSeek },
        { "DesktopEntry", MprisProperty::DesktopEntry },
        { "HasTrackList", MprisProperty::HasTrackList },
        { "Identity", MprisProperty::Identity },
        { "Metadata", MprisProperty::GetMetadata },
        { "PlaybackStatus", MprisProperty::GetPlaybackStatus },
        { "Position", MprisProperty::GetPosition },
        { "SupportedMimeTypes", MprisProperty::SupportedMimeTypes },
        { "SupportedUriSchemes", MprisProperty::SupportedUriSchemes }
    };
    static constexpr SortedArrayMap map { propertiesList };
    auto value = map.get(StringView::fromLatin1(propertyName), MprisProperty::NoProperty);
    if (value == MprisProperty::NoProperty)
        return { };
    return value;
}

static GVariant* handleGetProperty(GDBusConnection*, const char* /* sender */, const char* objectPath, const char* interfaceName, const char* propertyName, GError** error, gpointer userData)
{
    ASSERT(isMainThread());
    auto property = getMprisProperty(propertyName);
    if (!property) {
        g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED, "%s.%s %s is not supported", objectPath, interfaceName, propertyName);
        return nullptr;
    }

    auto& session = *reinterpret_cast<MediaSessionGLib*>(userData);
    switch (property.value()) {
    case MprisProperty::NoProperty:
        break;
    case MprisProperty::SupportedUriSchemes:
    case MprisProperty::SupportedMimeTypes:
        return g_variant_new_strv(nullptr, 0);
    case MprisProperty::GetPlaybackStatus:
        return session.getPlaybackStatusAsGVariant({ });
    case MprisProperty::GetMetadata:
        return session.getMetadataAsGVariant({ });
    case MprisProperty::GetPosition:
        return session.getPositionAsGVariant();
    case MprisProperty::Identity:
        return g_variant_new_string(getApplicationName().ascii().data());
    case MprisProperty::DesktopEntry:
        return g_variant_new_string("");
    case MprisProperty::CanSeek:
        return session.canSeekAsGVariant();
    case MprisProperty::HasTrackList:
    case MprisProperty::CanQuit:
    case MprisProperty::CanRaise:
        return g_variant_new_boolean(false);
    case MprisProperty::CanControl:
    case MprisProperty::CanGoNext:
    case MprisProperty::CanGoPrevious:
    case MprisProperty::CanPlay:
    case MprisProperty::CanPause:
        return g_variant_new_boolean(true);
    }

    return nullptr;
}

static gboolean handleSetProperty(GDBusConnection*, const char* /* sender */, const char* /* objectPath */, const char* interfaceName, const char* propertyName, GVariant*, GError** error, gpointer)
{
    ASSERT(isMainThread());
    g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED, "%s:%s setting is not supported", interfaceName, propertyName);
    return FALSE;
}

static const GDBusInterfaceVTable gInterfaceVTable = {
    handleMethodCall, handleGetProperty, handleSetProperty, { nullptr }
};

std::unique_ptr<MediaSessionGLib> MediaSessionGLib::create(MediaSessionManagerGLib& manager, MediaSessionIdentifier identifier)
{
    if (!manager.areDBusNotificationsEnabled())
        return makeUnique<MediaSessionGLib>(manager, nullptr, identifier);

    GUniqueOutPtr<GError> error;
    GUniquePtr<char> address(g_dbus_address_get_for_bus_sync(G_BUS_TYPE_SESSION, nullptr, &error.outPtr()));
    if (error) {
        g_warning("Unable to get session D-Bus address: %s", error->message);
        return nullptr;
    }
    auto connection = adoptGRef(reinterpret_cast<GDBusConnection*>(g_object_new(G_TYPE_DBUS_CONNECTION,
        "address", address.get(),
        "flags", G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT | G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,
        "exit-on-close", TRUE, nullptr)));
    g_initable_init(G_INITABLE(connection.get()), nullptr, &error.outPtr());
    if (error) {
        g_warning("Unable to connect to D-Bus session bus: %s", error->message);
        return nullptr;
    }
    return makeUnique<MediaSessionGLib>(manager, WTFMove(connection), identifier);
}

MediaSessionGLib::MediaSessionGLib(MediaSessionManagerGLib& manager, GRefPtr<GDBusConnection>&& connection, MediaSessionIdentifier identifier)
    : m_identifier(identifier)
    , m_manager(manager)
    , m_connection(WTFMove(connection))
{
}

MediaSessionGLib::~MediaSessionGLib()
{
    if (m_connection) {
        if (m_rootRegistrationId && !g_dbus_connection_unregister_object(m_connection.get(), m_rootRegistrationId))
            g_warning("Unable to unregister MPRIS D-Bus object.");
        if (m_playerRegistrationId && !g_dbus_connection_unregister_object(m_connection.get(), m_playerRegistrationId))
            g_warning("Unable to unregister MPRIS D-Bus player object.");
    }
    if (m_ownerId)
        g_bus_unown_name(m_ownerId);
}

void MediaSessionGLib::ensureMprisSessionRegistered()
{
    if (m_ownerId || !m_connection)
        return;

    const auto& mprisInterface = m_manager.mprisInterface();
    GUniqueOutPtr<GError> error;
    m_rootRegistrationId = g_dbus_connection_register_object(m_connection.get(), DBUS_MPRIS_OBJECT_PATH, mprisInterface->interfaces[0],
        &gInterfaceVTable, this, nullptr, &error.outPtr());

    if (!m_rootRegistrationId) {
        g_warning("Failed to register MPRIS D-Bus object: %s", error->message);
        return;
    }

    m_playerRegistrationId = g_dbus_connection_register_object(m_connection.get(), DBUS_MPRIS_OBJECT_PATH, mprisInterface->interfaces[1],
        &gInterfaceVTable, this, nullptr, &error.outPtr());

    if (!m_playerRegistrationId) {
        g_warning("Failed at MPRIS object registration: %s", error->message);
        return;
    }

    const auto& applicationID = getApplicationID();
    m_instanceId = applicationID.isEmpty() ? makeString("org.mpris.MediaPlayer2.webkit.instance", getpid(), "-", m_identifier.toUInt64()) : makeString("org.mpris.MediaPlayer2.", applicationID.ascii().data(), ".Sandboxed.instance-", m_identifier.toUInt64());

    m_ownerId = g_bus_own_name_on_connection(m_connection.get(), m_instanceId.ascii().data(), G_BUS_NAME_OWNER_FLAGS_NONE, nullptr, nullptr, this, nullptr);
}

void MediaSessionGLib::emitPositionChanged(double time)
{
    if (!m_connection)
        return;

    ensureMprisSessionRegistered();

    GUniqueOutPtr<GError> error;
    int64_t position = time * 1000000;
    if (!g_dbus_connection_emit_signal(m_connection.get(), nullptr, DBUS_MPRIS_OBJECT_PATH, DBUS_MPRIS_PLAYER_INTERFACE, "Seeked", g_variant_new("(x)", position), &error.outPtr()))
        g_warning("Failed to emit MPRIS Seeked signal: %s", error->message);
}

void MediaSessionGLib::updateNowPlaying(NowPlayingInfo& nowPlayingInfo)
{
    if (!m_connection)
        return;

    GVariantBuilder propertiesBuilder;
    g_variant_builder_init(&propertiesBuilder, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&propertiesBuilder, "{sv}", "Metadata", getMetadataAsGVariant(nowPlayingInfo));
    emitPropertiesChanged(g_variant_new("(sa{sv}as)", DBUS_MPRIS_PLAYER_INTERFACE, &propertiesBuilder, nullptr));
    g_variant_builder_clear(&propertiesBuilder);
}

GVariant* MediaSessionGLib::getMetadataAsGVariant(std::optional<NowPlayingInfo> info)
{
    if (!info)
        info = nowPlayingInfo();

    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));

    if (!info)
        return g_variant_builder_end(&builder);

    g_variant_builder_add(&builder, "{sv}", "mpris:trackid", g_variant_new("o", DBUS_MPRIS_TRACK_PATH));
    g_variant_builder_add(&builder, "{sv}", "mpris:length", g_variant_new_int64(info->duration * 1000000));
    g_variant_builder_add(&builder, "{sv}", "xesam:title", g_variant_new_string(info->title.utf8().data()));
    g_variant_builder_add(&builder, "{sv}", "xesam:album", g_variant_new_string(info->album.utf8().data()));
    if (info->artwork)
        g_variant_builder_add(&builder, "{sv}", "mpris:artUrl", g_variant_new_string(info->artwork->src.utf8().data()));

    GVariantBuilder artistBuilder;
    g_variant_builder_init(&artistBuilder, G_VARIANT_TYPE("as"));
    g_variant_builder_add(&artistBuilder, "s", info->artist.utf8().data());
    g_variant_builder_add(&builder, "{sv}", "xesam:artist", g_variant_builder_end(&artistBuilder));

    return g_variant_builder_end(&builder);
}

GVariant* MediaSessionGLib::getPlaybackStatusAsGVariant(std::optional<const PlatformMediaSession*> session)
{
    auto state = [this, session = WTFMove(session)]() -> PlatformMediaSession::State {
        if (session)
            return session.value()->state();

        auto* nowPlayingSession = m_manager.nowPlayingEligibleSession();
        if (nowPlayingSession)
            return nowPlayingSession->state();

        return PlatformMediaSession::State::Idle;
    }();

    switch (state) {
    case PlatformMediaSession::State::Autoplaying:
    case PlatformMediaSession::State::Playing:
        return g_variant_new_string("Playing");
    case PlatformMediaSession::State::Paused:
        return g_variant_new_string("Paused");
    case PlatformMediaSession::State::Idle:
    case PlatformMediaSession::State::Interrupted:
        return g_variant_new_string("Stopped");
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

void MediaSessionGLib::emitPropertiesChanged(GVariant* parameters)
{
    if (!m_connection)
        return;

    ensureMprisSessionRegistered();

    GUniqueOutPtr<GError> error;
    if (!g_dbus_connection_emit_signal(m_connection.get(), nullptr, DBUS_MPRIS_OBJECT_PATH, "org.freedesktop.DBus.Properties", "PropertiesChanged", parameters, &error.outPtr()))
        g_warning("Failed to emit MPRIS properties changed: %s", error->message);
}

void MediaSessionGLib::playbackStatusChanged(PlatformMediaSession& platformSession)
{
    if (!m_connection)
        return;

    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&builder, "{sv}", "PlaybackStatus", getPlaybackStatusAsGVariant(&platformSession));
    emitPropertiesChanged(g_variant_new("(sa{sv}as)", DBUS_MPRIS_PLAYER_INTERFACE, &builder, nullptr));
    g_variant_builder_clear(&builder);
}

std::optional<NowPlayingInfo> MediaSessionGLib::nowPlayingInfo()
{
    std::optional<NowPlayingInfo> nowPlayingInfo;
    m_manager.forEachMatchingSession([&](auto& session) {
        return session.mediaSessionIdentifier() == m_identifier;
    }, [&](auto& session) {
        nowPlayingInfo = session.nowPlayingInfo();
    });
    return nowPlayingInfo;
}

GVariant* MediaSessionGLib::getPositionAsGVariant()
{
    auto info = nowPlayingInfo();
    return g_variant_new_int64(info ? info->currentTime * 1000000 : 0);
}

GVariant* MediaSessionGLib::canSeekAsGVariant()
{
    bool canSeek = false;
    m_manager.forEachMatchingSession([&](auto& session) {
        return session.mediaSessionIdentifier() == m_identifier;
    }, [&](auto& session) {
        canSeek = session.supportsSeeking();
    });

    return g_variant_new_boolean(canSeek);
}

} // namespace WebCore

#endif // USE(GLIB) && ENABLE(MEDIA_SESSION)
