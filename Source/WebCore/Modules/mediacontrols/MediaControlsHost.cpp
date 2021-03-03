/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if ENABLE(VIDEO)

#include "MediaControlsHost.h"

#include "AudioTrackList.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "CaptionUserPreferences.h"
#include "FloatRect.h"
#include "HTMLMediaElement.h"
#include "LocalizedStrings.h"
#include "Logging.h"
#include "MediaControlTextTrackContainerElement.h"
#include "MediaControlsContextMenuItem.h"
#include "Page.h"
#include "PageGroup.h"
#include "RenderTheme.h"
#include "TextTrack.h"
#include "TextTrackList.h"
#include "VoidCallback.h"
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <wtf/JSONValues.h>
#include <wtf/Scope.h>
#include <wtf/UUID.h>
#include <wtf/Variant.h>

#if USE(APPLE_INTERNAL_SDK)
#include <WebKitAdditions/MediaControlsHostAdditions.cpp>
#endif

namespace WebCore {

#if defined(MediaControlsHostAdditions_members)
MediaControlsHostAdditions_members
#endif

const AtomString& MediaControlsHost::automaticKeyword()
{
    static MainThreadNeverDestroyed<const AtomString> automatic("automatic", AtomString::ConstructFromLiteral);
    return automatic;
}

const AtomString& MediaControlsHost::forcedOnlyKeyword()
{
    static MainThreadNeverDestroyed<const AtomString> forcedOnly("forced-only", AtomString::ConstructFromLiteral);
    return forcedOnly;
}

static const AtomString& alwaysOnKeyword()
{
    static MainThreadNeverDestroyed<const AtomString> alwaysOn("always-on", AtomString::ConstructFromLiteral);
    return alwaysOn;
}

static const AtomString& manualKeyword()
{
    static MainThreadNeverDestroyed<const AtomString> alwaysOn("manual", AtomString::ConstructFromLiteral);
    return alwaysOn;
}

Ref<MediaControlsHost> MediaControlsHost::create(HTMLMediaElement& mediaElement)
{
    return adoptRef(*new MediaControlsHost(mediaElement));
}

MediaControlsHost::MediaControlsHost(HTMLMediaElement& mediaElement)
    : m_mediaElement(makeWeakPtr(mediaElement))
{
}

MediaControlsHost::~MediaControlsHost()
{
#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)
    if (auto showMediaControlsContextMenuCallback = std::exchange(m_showMediaControlsContextMenuCallback, nullptr))
        showMediaControlsContextMenuCallback->handleEvent();
#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)
}

Vector<RefPtr<TextTrack>> MediaControlsHost::sortedTrackListForMenu(TextTrackList& trackList)
{
    if (!m_mediaElement)
        return { };

    Page* page = m_mediaElement->document().page();
    if (!page)
        return { };

    return page->group().captionPreferences().sortedTrackListForMenu(&trackList, { TextTrack::Kind::Subtitles, TextTrack::Kind::Captions, TextTrack::Kind::Descriptions });
}

Vector<RefPtr<AudioTrack>> MediaControlsHost::sortedTrackListForMenu(AudioTrackList& trackList)
{
    if (!m_mediaElement)
        return { };

    Page* page = m_mediaElement->document().page();
    if (!page)
        return { };

    return page->group().captionPreferences().sortedTrackListForMenu(&trackList);
}

String MediaControlsHost::displayNameForTrack(const Optional<TextOrAudioTrack>& track)
{
    if (!m_mediaElement || !track)
        return emptyString();

    Page* page = m_mediaElement->document().page();
    if (!page)
        return emptyString();

    return WTF::visit([page] (auto& track) {
        return page->group().captionPreferences().displayNameForTrack(track.get());
    }, track.value());
}

TextTrack& MediaControlsHost::captionMenuOffItem()
{
    return TextTrack::captionMenuOffItem();
}

TextTrack& MediaControlsHost::captionMenuAutomaticItem()
{
    return TextTrack::captionMenuAutomaticItem();
}

AtomString MediaControlsHost::captionDisplayMode() const
{
    if (!m_mediaElement)
        return emptyAtom();

    Page* page = m_mediaElement->document().page();
    if (!page)
        return emptyAtom();

    switch (page->group().captionPreferences().captionDisplayMode()) {
    case CaptionUserPreferences::Automatic:
        return automaticKeyword();
    case CaptionUserPreferences::ForcedOnly:
        return forcedOnlyKeyword();
    case CaptionUserPreferences::AlwaysOn:
        return alwaysOnKeyword();
    case CaptionUserPreferences::Manual:
        return manualKeyword();
    default:
        ASSERT_NOT_REACHED();
        return emptyAtom();
    }
}

void MediaControlsHost::setSelectedTextTrack(TextTrack* track)
{
    if (m_mediaElement)
        m_mediaElement->setSelectedTextTrack(track);
}

Element* MediaControlsHost::textTrackContainer()
{
    if (!m_textTrackContainer && m_mediaElement)
        m_textTrackContainer = MediaControlTextTrackContainerElement::create(m_mediaElement->document(), *m_mediaElement);

    return m_textTrackContainer.get();
}

void MediaControlsHost::updateTextTrackContainer()
{
    if (m_textTrackContainer)
        m_textTrackContainer->updateDisplay();
}

void MediaControlsHost::updateTextTrackRepresentationImageIfNeeded()
{
    if (m_textTrackContainer)
        m_textTrackContainer->updateTextTrackRepresentationImageIfNeeded();
}

void MediaControlsHost::enteredFullscreen()
{
    if (m_textTrackContainer)
        m_textTrackContainer->enteredFullscreen();
}

void MediaControlsHost::exitedFullscreen()
{
    if (m_textTrackContainer)
        m_textTrackContainer->exitedFullscreen();
}

void MediaControlsHost::updateCaptionDisplaySizes(ForceUpdate force)
{
    if (m_textTrackContainer)
        m_textTrackContainer->updateSizes(force == ForceUpdate::Yes ? MediaControlTextTrackContainerElement::ForceUpdate::Yes : MediaControlTextTrackContainerElement::ForceUpdate::No);
}
    
bool MediaControlsHost::allowsInlineMediaPlayback() const
{
    return m_mediaElement && !m_mediaElement->mediaSession().requiresFullscreenForVideoPlayback();
}

bool MediaControlsHost::supportsFullscreen() const
{
    return m_mediaElement && m_mediaElement->supportsFullscreen(HTMLMediaElementEnums::VideoFullscreenModeStandard);
}

bool MediaControlsHost::isVideoLayerInline() const
{
    return m_mediaElement && m_mediaElement->isVideoLayerInline();
}

bool MediaControlsHost::isInMediaDocument() const
{
    return m_mediaElement && m_mediaElement->document().isMediaDocument();
}

bool MediaControlsHost::userGestureRequired() const
{
    return m_mediaElement && !m_mediaElement->mediaSession().playbackPermitted();
}

bool MediaControlsHost::shouldForceControlsDisplay() const
{
    return m_mediaElement && m_mediaElement->shouldForceControlsDisplay();
}

String MediaControlsHost::externalDeviceDisplayName() const
{
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    if (!m_mediaElement)
        return emptyString();

    auto player = m_mediaElement->player();
    if (!player) {
        LOG(Media, "MediaControlsHost::externalDeviceDisplayName - returning \"\" because player is NULL");
        return emptyString();
    }

    String name = player->wirelessPlaybackTargetName();
    LOG(Media, "MediaControlsHost::externalDeviceDisplayName - returning \"%s\"", name.utf8().data());
    return name;
#else
    return emptyString();
#endif
}

auto MediaControlsHost::externalDeviceType() const -> DeviceType
{
#if !ENABLE(WIRELESS_PLAYBACK_TARGET)
    return DeviceType::None;
#else
    if (!m_mediaElement)
        return DeviceType::None;

    auto player = m_mediaElement->player();
    if (!player) {
        LOG(Media, "MediaControlsHost::externalDeviceType - returning \"none\" because player is NULL");
        return DeviceType::None;
    }

    switch (player->wirelessPlaybackTargetType()) {
    case MediaPlayer::WirelessPlaybackTargetType::TargetTypeNone:
        return DeviceType::None;
    case MediaPlayer::WirelessPlaybackTargetType::TargetTypeAirPlay:
        return DeviceType::Airplay;
    case MediaPlayer::WirelessPlaybackTargetType::TargetTypeTVOut:
        return DeviceType::Tvout;
    }

    ASSERT_NOT_REACHED();
    return DeviceType::None;
#endif
}

bool MediaControlsHost::controlsDependOnPageScaleFactor() const
{
    return m_mediaElement && m_mediaElement->mediaControlsDependOnPageScaleFactor();
}

void MediaControlsHost::setControlsDependOnPageScaleFactor(bool value)
{
    if (m_mediaElement)
        m_mediaElement->setMediaControlsDependOnPageScaleFactor(value);
}

String MediaControlsHost::generateUUID()
{
    return createCanonicalUUIDString();
}

String MediaControlsHost::shadowRootCSSText()
{
    return RenderTheme::singleton().modernMediaControlsStyleSheet();
}

String MediaControlsHost::base64StringForIconNameAndType(const String& iconName, const String& iconType)
{
    return RenderTheme::singleton().mediaControlsBase64StringForIconNameAndType(iconName, iconType);
}

String MediaControlsHost::formattedStringForDuration(double durationInSeconds)
{
    return RenderTheme::singleton().mediaControlsFormattedStringForDuration(durationInSeconds);
}

#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)

#if !defined(MediaControlsHostAdditions_showMediaControlsContextMenu_MenuData)
#define MediaControlsHostAdditions_showMediaControlsContextMenu_MenuData
#endif

#if !defined(MediaControlsHostAdditions_showMediaControlsContextMenu_MenuData_switchOn)
#define MediaControlsHostAdditions_showMediaControlsContextMenu_MenuData_switchOn
#endif

bool MediaControlsHost::showMediaControlsContextMenu(HTMLElement& target, String&& optionsJSONString, Ref<VoidCallback>&& callback)
{
    if (m_showMediaControlsContextMenuCallback)
        return false;

    m_showMediaControlsContextMenuCallback = WTFMove(callback);

    auto invokeCallbackAtScopeExit = makeScopeExit([&, protectedThis = makeRef(*this)] {
        std::exchange(m_showMediaControlsContextMenuCallback, nullptr)->handleEvent();
    });

    if (!m_mediaElement)
        return false;

    auto& mediaElement = *m_mediaElement;

    auto* page = mediaElement.document().page();
    if (!page)
        return false;

    auto optionsJSON = JSON::Value::parseJSON(optionsJSONString);
    if (!optionsJSON)
        return false;

    auto optionsJSONObject = optionsJSON->asObject();
    if (!optionsJSONObject)
        return false;

    using MenuData = Variant<RefPtr<AudioTrack>, RefPtr<TextTrack> MediaControlsHostAdditions_showMediaControlsContextMenu_MenuData>;
    HashMap<MediaControlsContextMenuItem::ID, MenuData> idMap;
    auto generateID = [&] (MenuData data) {
        auto id = idMap.size() + 1;
        idMap.add(id, data);
        return id;
    };

    Vector<MediaControlsContextMenuItem> items;

    if (optionsJSONObject->getBoolean("includeAudioTracks"_s).valueOr(false)) {
        if (auto* audioTracks = mediaElement.audioTracks(); audioTracks && audioTracks->length() > 1) {
            MediaControlsContextMenuItem audioTracksItem;
            audioTracksItem.title = WEB_UI_STRING_KEY("Languages", "Languages (Media Controls Menu)", "Languages media controls context menu title");
            audioTracksItem.icon = "globe"_s;
            audioTracksItem.children.reserveCapacity(audioTracks->length());

            auto& captionPreferences = page->group().captionPreferences();

            for (auto& audioTrack : captionPreferences.sortedTrackListForMenu(audioTracks)) {
                MediaControlsContextMenuItem audioTrackItem;
                audioTrackItem.id = generateID(audioTrack);
                audioTrackItem.title = captionPreferences.displayNameForTrack(audioTrack.get());
                audioTrackItem.isChecked = audioTrack->enabled();
                audioTracksItem.children.append(WTFMove(audioTrackItem));
            }

            if (!audioTracksItem.children.isEmpty())
                items.append(WTFMove(audioTracksItem));
        }
    }

    if (optionsJSONObject->getBoolean("includeTextTracks"_s).valueOr(false)) {
        if (auto* textTracks = mediaElement.textTracks(); textTracks && textTracks->length()) {
            MediaControlsContextMenuItem textTracksItem;
            textTracksItem.title = WEB_UI_STRING_KEY("Subtitles", "Subtitles (Media Controls Menu)", "Subtitles media controls context menu title");
            textTracksItem.icon = "captions.bubble"_s;
            textTracksItem.children.reserveCapacity(textTracks->length());

            auto& captionPreferences = page->group().captionPreferences();
            auto sortedTextTracks = captionPreferences.sortedTrackListForMenu(textTracks, { TextTrack::Kind::Subtitles, TextTrack::Kind::Captions, TextTrack::Kind::Descriptions });
            bool allTracksDisabled = notFound == sortedTextTracks.findMatching([] (const auto& textTrack) {
                return textTrack->mode() == TextTrack::Mode::Showing;
            });
            bool usesAutomaticTrack = captionPreferences.captionDisplayMode() == CaptionUserPreferences::Automatic && allTracksDisabled;

            for (auto& textTrack : sortedTextTracks) {
                MediaControlsContextMenuItem textTrackItem;
                textTrackItem.id = generateID(textTrack);
                textTrackItem.title = captionPreferences.displayNameForTrack(textTrack.get());
                if (allTracksDisabled && textTrack == &TextTrack::captionMenuOffItem() && (captionPreferences.captionDisplayMode() == CaptionUserPreferences::ForcedOnly || captionPreferences.captionDisplayMode() == CaptionUserPreferences::Manual))
                    textTrackItem.isChecked = true;
                else if (usesAutomaticTrack && textTrack == &TextTrack::captionMenuAutomaticItem())
                    textTrackItem.isChecked = true;
                else if (!usesAutomaticTrack && textTrack->mode() == TextTrack::Mode::Showing)
                    textTrackItem.isChecked = true;
                else
                    textTrackItem.isChecked = false;
                textTracksItem.children.append(WTFMove(textTrackItem));
            }

            if (!textTracksItem.children.isEmpty())
                items.append(WTFMove(textTracksItem));
        }
    }

#if defined(MediaControlsHostAdditions_showMediaControlsContextMenu_options)
    MediaControlsHostAdditions_showMediaControlsContextMenu_options
#endif

    if (items.isEmpty())
        return false;

    ASSERT(!idMap.isEmpty());

    page->chrome().client().showMediaControlsContextMenu(target.boundsInRootViewSpace(), WTFMove(items), [weakMediaElement = makeWeakPtr(mediaElement), idMap = WTFMove(idMap), invokeCallbackAtScopeExit = WTFMove(invokeCallbackAtScopeExit)] (MediaControlsContextMenuItem::ID selectedItemID) {
        if (selectedItemID == MediaControlsContextMenuItem::invalidID)
            return;

        if (!weakMediaElement)
            return;

        auto& mediaElement = *weakMediaElement;

        auto selectedItem = idMap.get(selectedItemID);
        WTF::switchOn(selectedItem,
            [&] (RefPtr<AudioTrack>& selectedAudioTrack) {
                for (auto& track : idMap.values()) {
                    if (auto* audioTrack = WTF::get_if<RefPtr<AudioTrack>>(track))
                        (*audioTrack)->setEnabled(*audioTrack == selectedAudioTrack);
                }
            },
            [&] (RefPtr<TextTrack>& selectedTextTrack) {
                for (auto& track : idMap.values()) {
                    if (auto* textTrack = WTF::get_if<RefPtr<TextTrack>>(track))
                        (*textTrack)->setMode(TextTrack::Mode::Disabled);
                }
                mediaElement.setSelectedTextTrack(selectedTextTrack.get());
            }
            MediaControlsHostAdditions_showMediaControlsContextMenu_MenuData_switchOn
        );
    });

    return true;
}

#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)

bool MediaControlsHost::compactMode() const
{
#if PLATFORM(WATCHOS)
    return true;
#else
    return m_simulateCompactMode;
#endif
}

}

#endif // ENABLE(VIDEO)
