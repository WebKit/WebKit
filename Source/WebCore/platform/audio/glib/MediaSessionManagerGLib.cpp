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
#include "MediaSessionManagerGLib.h"

#if USE(GLIB) && ENABLE(MEDIA_SESSION)

#include "ApplicationGLib.h"
#include "AudioSession.h"
#include "HTMLMediaElement.h"
#include "MediaPlayer.h"
#include "MediaStrategy.h"
#include "NowPlayingInfo.h"
#include "PlatformMediaSession.h"
#include "PlatformStrategies.h"

#include <gio/gio.h>
#include <wtf/SortedArrayMap.h>

// https://specifications.freedesktop.org/mpris-spec/latest/
static const char s_mprisInterface[] =
    "<node>"
        "<interface name=\"org.mpris.MediaPlayer2\">"
            "<annotation name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\" value=\"true\"/>"
            "<method name=\"Raise\"/>"
            "<method name=\"Quit\"/>"
            "<property name=\"CanQuit\" type=\"b\" access=\"read\"/>"
            "<property name=\"CanRaise\" type=\"b\" access=\"read\"/>"
            "<property name=\"HasTrackList\" type=\"b\" access=\"read\"/>"
            "<property name=\"Identity\" type=\"s\" access=\"read\"/>"
            "<property name=\"DesktopEntry\" type=\"s\" access=\"read\"/>"
            "<property name=\"SupportedUriSchemes\" type=\"as\" access=\"read\"/>"
            "<property name=\"SupportedMimeTypes\" type=\"as\" access=\"read\"/>"
        "</interface>"
        "<interface name=\"org.mpris.MediaPlayer2.Player\">"
            "<method name=\"Next\"/>"
            "<method name=\"Previous\"/>"
            "<method name=\"Pause\"/>"
            "<method name=\"PlayPause\"/>"
            "<method name=\"Stop\"/>"
            "<method name=\"Play\"/>"
            "<method name=\"Seek\">"
                "<arg direction=\"in\" type=\"x\" name=\"Offset\"/>"
            "</method>"
            "<method name=\"SetPosition\">"
                "<arg direction=\"in\" type=\"o\" name=\"TrackId\"/>"
                "<arg direction=\"in\" type=\"x\" name=\"Position\"/>"
            "</method>"
            "<method name=\"OpenUri\">"
                "<arg direction=\"in\" type=\"s\" name=\"Uri\"/>"
            "</method>"
            "<property name=\"PlaybackStatus\" type=\"s\" access=\"read\">"
                "<annotation name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\" value=\"true\"/>"
            "</property>"
            "<property name=\"Rate\" type=\"d\" access=\"readwrite\">"
                "<annotation name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\" value=\"true\"/>"
            "</property>"
            "<property name=\"Metadata\" type=\"a{sv}\" access=\"read\">"
                "<annotation name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\" value=\"true\"/>"
            "</property>"
            "<property name=\"Volume\" type=\"d\" access=\"readwrite\">"
                "<annotation name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\" value=\"true\"/>"
            "</property>"
            "<property name=\"Position\" type=\"x\" access=\"read\">"
                "<annotation name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\" value=\"false\"/>"
            "</property>"
            "<property name=\"MinimumRate\" type=\"d\" access=\"read\">"
                "<annotation name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\" value=\"true\"/>"
            "</property>"
            "<property name=\"MaximumRate\" type=\"d\" access=\"read\">"
                "<annotation name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\" value=\"true\"/>"
            "</property>"
            "<property name=\"CanGoNext\" type=\"b\" access=\"read\">"
                "<annotation name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\" value=\"true\"/>"
            "</property>"
            "<property name=\"CanGoPrevious\" type=\"b\" access=\"read\">"
                "<annotation name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\" value=\"true\"/>"
            "</property>"
            "<property name=\"CanPlay\" type=\"b\" access=\"read\">"
                "<annotation name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\" value=\"true\"/>"
            "</property>"
            "<property name=\"CanPause\" type=\"b\" access=\"read\">"
                "<annotation name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\" value=\"true\"/>"
            "</property>"
            "<property name=\"CanSeek\" type=\"b\" access=\"read\">"
                "<annotation name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\" value=\"true\"/>"
            "</property>"
            "<property name=\"CanControl\" type=\"b\" access=\"read\">"
                "<annotation name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\" value=\"false\"/>"
            "</property>"
            "<signal name=\"Seeked\">"
                "<arg name=\"Position\" type=\"x\"/>"
            "</signal>"
        "</interface>"
    "</node>";

#define DBUS_MPRIS_OBJECT_PATH "/org/mpris/MediaPlayer2"
#define DBUS_MPRIS_PLAYER_INTERFACE "org.mpris.MediaPlayer2.Player"
#define DBUS_MPRIS_TRACK_PATH "/org/mpris/MediaPlayer2/webkit"

namespace WebCore {

std::unique_ptr<PlatformMediaSessionManager> PlatformMediaSessionManager::create()
{
    return makeUnique<MediaSessionManagerGLib>();
}

MediaSessionManagerGLib::MediaSessionManagerGLib()
    : m_nowPlayingManager(platformStrategies()->mediaStrategy().createNowPlayingManager())
{
}

MediaSessionManagerGLib::~MediaSessionManagerGLib()
{
    if (m_ownerId)
        g_bus_unown_name(m_ownerId);
}

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
    auto value = map.get(name, PlatformMediaSession::RemoteControlCommandType::NoCommand);
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
    auto& manager = *reinterpret_cast<MediaSessionManagerGLib*>(userData);
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
    auto value = map.get(propertyName, MprisProperty::NoProperty);
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

    auto& manager = *reinterpret_cast<MediaSessionManagerGLib*>(userData);
    switch (property.value()) {
    case MprisProperty::NoProperty:
        break;
    case MprisProperty::SupportedUriSchemes:
    case MprisProperty::SupportedMimeTypes:
        return g_variant_new_strv(nullptr, 0);
    case MprisProperty::GetPlaybackStatus:
        return manager.getPlaybackStatusAsGVariant({ });
    case MprisProperty::GetMetadata: {
        auto* variant = manager.getMetadataAsGVariant();
        if (!variant)
            return nullptr;
        return g_variant_ref(variant);
    }
    case MprisProperty::GetPosition:
        return manager.getActiveSessionPosition();
    case MprisProperty::Identity:
        return g_variant_new_string(getApplicationName());
    case MprisProperty::DesktopEntry:
        return g_variant_new_string("");
    case MprisProperty::HasTrackList:
    case MprisProperty::CanQuit:
    case MprisProperty::CanRaise:
        return g_variant_new_boolean(false);
    case MprisProperty::CanSeek:
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

bool MediaSessionManagerGLib::setupMpris()
{
    if (m_ownerId)
        return true;

    const auto& applicationID = getApplicationID();
    m_instanceId = applicationID.isEmpty() ? makeString("org.mpris.MediaPlayer2.webkit.instance", getpid()) : makeString("org.mpris.MediaPlayer2.", applicationID.ascii().data());

    m_ownerId = g_bus_own_name(G_BUS_TYPE_SESSION, m_instanceId.ascii().data(),
        G_BUS_NAME_OWNER_FLAGS_NONE, reinterpret_cast<GBusAcquiredCallback>(+[](GDBusConnection* connection, const gchar*, gpointer userData) {
            auto& manager = *reinterpret_cast<MediaSessionManagerGLib*>(userData);
            manager.busAcquired(connection);
        }),
        reinterpret_cast<GBusNameAcquiredCallback>(+[](GDBusConnection* connection, const char*, gpointer userData) {
            auto& manager = *reinterpret_cast<MediaSessionManagerGLib*>(userData);
            manager.nameAcquired(connection);
        }),
        reinterpret_cast<GBusNameLostCallback>(+[](GDBusConnection* connection, const char*, gpointer userData) {
            auto& manager = *reinterpret_cast<MediaSessionManagerGLib*>(userData);
            manager.nameLost(connection);
        }), this, nullptr);

    GUniqueOutPtr<GError> error;
    m_mprisInterface = adoptGRef(g_dbus_node_info_new_for_xml(s_mprisInterface, &error.outPtr()));
    if (!m_mprisInterface) {
        g_warning("Failed at parsing XML Interface definition: %s", error->message);
        return false;
    }

    return true;
}

void MediaSessionManagerGLib::busAcquired(GDBusConnection* connection)
{
    GUniqueOutPtr<GError> error;
    m_rootRegistrationId = g_dbus_connection_register_object(connection, DBUS_MPRIS_OBJECT_PATH, m_mprisInterface->interfaces[0],
        &gInterfaceVTable, this, nullptr, &error.outPtr());

    if (!m_rootRegistrationId) {
        g_warning("Failed to register MPRIS D-Bus object: %s", error->message);
        return;
    }

    m_playerRegistrationId = g_dbus_connection_register_object(connection, DBUS_MPRIS_OBJECT_PATH, m_mprisInterface->interfaces[1],
        &gInterfaceVTable, this, nullptr, &error.outPtr());

    if (!m_playerRegistrationId)
        g_warning("Failed at MPRIS object registration: %s", error->message);
}

void MediaSessionManagerGLib::nameLost(GDBusConnection* connection)
{
    if (UNLIKELY(!m_connection)) {
        g_warning("Unable to acquire MPRIS D-Bus session ownership for name %s", m_instanceId.ascii().data());
        return;
    }

    m_connection = nullptr;
    if (!m_rootRegistrationId)
        return;

    if (g_dbus_connection_unregister_object(connection, m_rootRegistrationId))
        m_rootRegistrationId = 0;
    else
        g_warning("Unable to unregister MPRIS D-Bus object.");

    if (!m_playerRegistrationId)
        return;

    if (g_dbus_connection_unregister_object(connection, m_playerRegistrationId))
        m_playerRegistrationId = 0;
    else
        g_warning("Unable to unregister MPRIS D-Bus player object.");
}

void MediaSessionManagerGLib::beginInterruption(PlatformMediaSession::InterruptionType type)
{
    if (type == PlatformMediaSession::InterruptionType::SystemInterruption) {
        forEachSession([] (auto& session) {
            session.clearHasPlayedSinceLastInterruption();
        });
    }

    PlatformMediaSessionManager::beginInterruption(type);
}

void MediaSessionManagerGLib::scheduleSessionStatusUpdate()
{
    callOnMainThread([this] () mutable {
        m_nowPlayingManager->setSupportsSeeking(computeSupportsSeeking());
        updateNowPlayingInfo();

        forEachSession([] (auto& session) {
            session.updateMediaUsageIfChanged();
        });
    });
}

bool MediaSessionManagerGLib::sessionWillBeginPlayback(PlatformMediaSession& session)
{
    if (!PlatformMediaSessionManager::sessionWillBeginPlayback(session))
        return false;

    scheduleSessionStatusUpdate();
    return true;
}

void MediaSessionManagerGLib::sessionDidEndRemoteScrubbing(PlatformMediaSession&)
{
    scheduleSessionStatusUpdate();
}

void MediaSessionManagerGLib::addSession(PlatformMediaSession& session)
{
    if (!setupMpris())
        return;

    m_nowPlayingManager->addClient(*this);

    PlatformMediaSessionManager::addSession(session);
}

void MediaSessionManagerGLib::removeSession(PlatformMediaSession& session)
{
    PlatformMediaSessionManager::removeSession(session);

    if (hasNoSession()) {
        m_nowPlayingManager->removeClient(*this);
        if (m_ownerId)
            g_bus_unown_name(m_ownerId);
        m_ownerId = 0;
    }
    scheduleSessionStatusUpdate();
}

void MediaSessionManagerGLib::setCurrentSession(PlatformMediaSession& session)
{
    PlatformMediaSessionManager::setCurrentSession(session);

    m_nowPlayingManager->updateSupportedCommands();
}

void MediaSessionManagerGLib::sessionWillEndPlayback(PlatformMediaSession& session, DelayCallingUpdateNowPlaying delayCallingUpdateNowPlaying)
{
    PlatformMediaSessionManager::sessionWillEndPlayback(session, delayCallingUpdateNowPlaying);

    callOnMainThread([weakSession = makeWeakPtr(session)] {
        if (weakSession)
            weakSession->updateMediaUsageIfChanged();
    });

    if (delayCallingUpdateNowPlaying == DelayCallingUpdateNowPlaying::No)
        updateNowPlayingInfo();
    else {
        callOnMainThread([this] {
            updateNowPlayingInfo();
        });
    }
}

void MediaSessionManagerGLib::sessionStateChanged(PlatformMediaSession& session)
{
    PlatformMediaSessionManager::sessionStateChanged(session);

    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&builder, "{sv}", "PlaybackStatus", getPlaybackStatusAsGVariant(&session));
    emitPropertiesChanged(g_variant_new("(sa{sv}as)", DBUS_MPRIS_PLAYER_INTERFACE, &builder, nullptr));
    g_variant_builder_clear(&builder);
}

void MediaSessionManagerGLib::clientCharacteristicsChanged(PlatformMediaSession& session)
{
    ALWAYS_LOG(LOGIDENTIFIER, session.logIdentifier());
    if (m_isSeeking) {
        m_isSeeking = false;
        GUniqueOutPtr<GError> error;
        int64_t position = session.nowPlayingInfo()->currentTime * 1000000;
        if (!g_dbus_connection_emit_signal(m_connection.get(), nullptr, DBUS_MPRIS_OBJECT_PATH, DBUS_MPRIS_PLAYER_INTERFACE, "Seeked", g_variant_new("(x)", position), &error.outPtr()))
            g_warning("Failed to emit MPRIS Seeked signal: %s", error->message);
    }
    scheduleSessionStatusUpdate();
}

void MediaSessionManagerGLib::sessionCanProduceAudioChanged()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    PlatformMediaSessionManager::sessionCanProduceAudioChanged();
    scheduleSessionStatusUpdate();
}

void MediaSessionManagerGLib::addSupportedCommand(PlatformMediaSession::RemoteControlCommandType command)
{
    m_nowPlayingManager->addSupportedCommand(command);
}

void MediaSessionManagerGLib::removeSupportedCommand(PlatformMediaSession::RemoteControlCommandType command)
{
    m_nowPlayingManager->removeSupportedCommand(command);
}

RemoteCommandListener::RemoteCommandsSet MediaSessionManagerGLib::supportedCommands() const
{
    return m_nowPlayingManager->supportedCommands();
}

PlatformMediaSession* MediaSessionManagerGLib::nowPlayingEligibleSession()
{
    // FIXME: Fix this layering violation.
    if (auto element = HTMLMediaElement::bestMediaElementForRemoteControls(MediaElementSession::PlaybackControlsPurpose::NowPlaying))
        return &element->mediaSession();

    return nullptr;
}

void MediaSessionManagerGLib::updateNowPlayingInfo()
{
    std::optional<NowPlayingInfo> nowPlayingInfo;
    if (auto* session = nowPlayingEligibleSession())
        nowPlayingInfo = session->nowPlayingInfo();

    if (!nowPlayingInfo) {
        if (m_registeredAsNowPlayingApplication) {
            ALWAYS_LOG(LOGIDENTIFIER, "clearing now playing info");
            m_nowPlayingManager->clearNowPlayingInfo();
        }

        m_registeredAsNowPlayingApplication = false;
        m_nowPlayingActive = false;
        m_lastUpdatedNowPlayingTitle = emptyString();
        m_lastUpdatedNowPlayingDuration = NAN;
        m_lastUpdatedNowPlayingElapsedTime = NAN;
        m_lastUpdatedNowPlayingInfoUniqueIdentifier = { };
        m_nowPlayingInfo.clear();
        return;
    }

    m_haveEverRegisteredAsNowPlayingApplication = true;

    if (m_nowPlayingManager->setNowPlayingInfo(*nowPlayingInfo))
        ALWAYS_LOG(LOGIDENTIFIER, "title = \"", nowPlayingInfo->title, "\", isPlaying = ", nowPlayingInfo->isPlaying, ", duration = ", nowPlayingInfo->duration, ", now = ", nowPlayingInfo->currentTime, ", id = ", nowPlayingInfo->uniqueIdentifier.toUInt64(), ", registered = ", m_registeredAsNowPlayingApplication, ", src = \"", nowPlayingInfo->artwork ? nowPlayingInfo->artwork->src : String(), "\"");

    if (!m_registeredAsNowPlayingApplication) {
        m_registeredAsNowPlayingApplication = true;
        providePresentingApplicationPIDIfNecessary();
    }

    if (!nowPlayingInfo->title.isEmpty())
        m_lastUpdatedNowPlayingTitle = nowPlayingInfo->title;

    double duration = nowPlayingInfo->duration;
    if (std::isfinite(duration) && duration != MediaPlayer::invalidTime())
        m_lastUpdatedNowPlayingDuration = duration;

    m_lastUpdatedNowPlayingInfoUniqueIdentifier = nowPlayingInfo->uniqueIdentifier;

    double currentTime = nowPlayingInfo->currentTime;
    if (std::isfinite(currentTime) && currentTime != MediaPlayer::invalidTime() && nowPlayingInfo->supportsSeeking)
        m_lastUpdatedNowPlayingElapsedTime = currentTime;

    m_nowPlayingActive = nowPlayingInfo->allowsNowPlayingControlsVisibility;

    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&builder, "{sv}", "mpris:trackid", g_variant_new("o", DBUS_MPRIS_TRACK_PATH));
    g_variant_builder_add(&builder, "{sv}", "mpris:length", g_variant_new_int64(nowPlayingInfo->duration * 1000000));
    g_variant_builder_add(&builder, "{sv}", "xesam:title", g_variant_new_string(nowPlayingInfo->title.utf8().data()));
    g_variant_builder_add(&builder, "{sv}", "xesam:album", g_variant_new_string(nowPlayingInfo->album.utf8().data()));
    if (nowPlayingInfo->artwork)
        g_variant_builder_add(&builder, "{sv}", "mpris:artUrl", g_variant_new_string(nowPlayingInfo->artwork->src.utf8().data()));

    GVariantBuilder artistBuilder;
    g_variant_builder_init(&artistBuilder, G_VARIANT_TYPE("as"));
    g_variant_builder_add(&artistBuilder, "s", nowPlayingInfo->artist.utf8().data());
    g_variant_builder_add(&builder, "{sv}", "xesam:artist", g_variant_builder_end(&artistBuilder));

    m_nowPlayingInfo = g_variant_builder_end(&builder);

    GVariantBuilder propertiesBuilder;
    g_variant_builder_init(&propertiesBuilder, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&propertiesBuilder, "{sv}", "Metadata", m_nowPlayingInfo.get());
    emitPropertiesChanged(g_variant_new("(sa{sv}as)", DBUS_MPRIS_PLAYER_INTERFACE, &propertiesBuilder, nullptr));
    g_variant_builder_clear(&propertiesBuilder);
}

GVariant* MediaSessionManagerGLib::getPlaybackStatusAsGVariant(std::optional<const PlatformMediaSession*> session)
{
    auto state = [this, session = WTFMove(session)]() -> PlatformMediaSession::State {
        if (session)
            return session.value()->state();

        auto* nowPlayingSession = nowPlayingEligibleSession();
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

GVariant* MediaSessionManagerGLib::getActiveSessionPosition()
{
    auto* session = nowPlayingEligibleSession();
    return g_variant_new_int64(session ? session->nowPlayingInfo()->currentTime * 1000000 : 0);
}

void MediaSessionManagerGLib::dispatch(PlatformMediaSession::RemoteControlCommandType platformCommand, PlatformMediaSession::RemoteCommandArgument argument)
{
    m_isSeeking = platformCommand == PlatformMediaSession::SeekToPlaybackPositionCommand;
    m_nowPlayingManager->didReceiveRemoteControlCommand(platformCommand, argument);
}

void MediaSessionManagerGLib::emitPropertiesChanged(GVariant* parameters)
{
    if (!m_connection)
        return;

    GUniqueOutPtr<GError> error;
    if (!g_dbus_connection_emit_signal(m_connection.get(), nullptr, DBUS_MPRIS_OBJECT_PATH, "org.freedesktop.DBus.Properties", "PropertiesChanged", parameters, &error.outPtr()))
        g_warning("Failed to emit MPRIS properties changed: %s", error->message);
}

} // namespace WebCore

#endif // USE(GLIB)
