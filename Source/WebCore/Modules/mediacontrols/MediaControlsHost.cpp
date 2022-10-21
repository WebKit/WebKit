/*
 * Copyright (C) 2013-2022 Apple Inc. All rights reserved.
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

#include "AddEventListenerOptions.h"
#include "AudioTrackList.h"
#include "CaptionUserPreferences.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "ContextMenu.h"
#include "ContextMenuController.h"
#include "ContextMenuItem.h"
#include "ContextMenuProvider.h"
#include "Event.h"
#include "EventListener.h"
#include "EventNames.h"
#include "EventTarget.h"
#include "FloatRect.h"
#include "HTMLElement.h"
#include "HTMLMediaElement.h"
#include "HTMLVideoElement.h"
#include "InspectorController.h"
#include "LocalizedStrings.h"
#include "Logging.h"
#include "MediaControlTextTrackContainerElement.h"
#include "MediaControlsContextMenuItem.h"
#include "Node.h"
#include "Page.h"
#include "PageGroup.h"
#include "RenderTheme.h"
#include "TextTrack.h"
#include "TextTrackCueList.h"
#include "TextTrackList.h"
#include "UserGestureIndicator.h"
#include "VTTCue.h"
#include "VoidCallback.h"
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <variant>
#include <wtf/Function.h>
#include <wtf/JSONValues.h>
#include <wtf/Scope.h>
#include <wtf/UUID.h>

#if USE(APPLE_INTERNAL_SDK)
#include <WebKitAdditions/MediaControlsHostAdditions.h>
#endif

namespace WebCore {

const AtomString& MediaControlsHost::automaticKeyword()
{
    static MainThreadNeverDestroyed<const AtomString> automatic("automatic"_s);
    return automatic;
}

const AtomString& MediaControlsHost::forcedOnlyKeyword()
{
    static MainThreadNeverDestroyed<const AtomString> forcedOnly("forced-only"_s);
    return forcedOnly;
}

static const AtomString& alwaysOnKeyword()
{
    static MainThreadNeverDestroyed<const AtomString> alwaysOn("always-on"_s);
    return alwaysOn;
}

static const AtomString& manualKeyword()
{
    static MainThreadNeverDestroyed<const AtomString> alwaysOn("manual"_s);
    return alwaysOn;
}

Ref<MediaControlsHost> MediaControlsHost::create(HTMLMediaElement& mediaElement)
{
    return adoptRef(*new MediaControlsHost(mediaElement));
}

MediaControlsHost::MediaControlsHost(HTMLMediaElement& mediaElement)
    : m_mediaElement(mediaElement)
{
}

MediaControlsHost::~MediaControlsHost()
{
#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)
    if (auto showMediaControlsContextMenuCallback = std::exchange(m_showMediaControlsContextMenuCallback, nullptr))
        showMediaControlsContextMenuCallback->handleEvent();
#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)
}

String MediaControlsHost::layoutTraitsClassName() const
{
#if defined(MEDIA_CONTROLS_HOST_LAYOUT_TRAITS_CLASS_NAME_OVERRIDE)
    return MEDIA_CONTROLS_HOST_LAYOUT_TRAITS_CLASS_NAME_OVERRIDE""_s;
#else
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    return "MacOSLayoutTraits"_s;
#elif PLATFORM(IOS)
    return "IOSLayoutTraits"_s;
#elif PLATFORM(WATCHOS)
    return "WatchOSLayoutTraits"_s;
#elif PLATFORM(GTK) || PLATFORM(WPE) || PLATFORM(WIN_CAIRO)
    return "AdwaitaLayoutTraits"_s;
#else
    ASSERT_NOT_REACHED();
    return nullString();
#endif
#endif
}

const AtomString& MediaControlsHost::mediaControlsContainerClassName() const
{
    static MainThreadNeverDestroyed<const AtomString> className("media-controls-container"_s);
    return className;
}

Vector<RefPtr<TextTrack>> MediaControlsHost::sortedTrackListForMenu(TextTrackList& trackList)
{
    if (!m_mediaElement)
        return { };

    Page* page = m_mediaElement->document().page();
    if (!page)
        return { };

    return page->group().ensureCaptionPreferences().sortedTrackListForMenu(&trackList, { TextTrack::Kind::Subtitles, TextTrack::Kind::Captions, TextTrack::Kind::Descriptions });
}

Vector<RefPtr<AudioTrack>> MediaControlsHost::sortedTrackListForMenu(AudioTrackList& trackList)
{
    if (!m_mediaElement)
        return { };

    Page* page = m_mediaElement->document().page();
    if (!page)
        return { };

    return page->group().ensureCaptionPreferences().sortedTrackListForMenu(&trackList);
}

String MediaControlsHost::displayNameForTrack(const std::optional<TextOrAudioTrack>& track)
{
    if (!m_mediaElement || !track)
        return emptyString();

    Page* page = m_mediaElement->document().page();
    if (!page)
        return emptyString();

    return std::visit([page] (auto& track) {
        return page->group().ensureCaptionPreferences().displayNameForTrack(track.get());
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

    switch (page->group().ensureCaptionPreferences().captionDisplayMode()) {
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
    return m_mediaElement && !m_mediaElement->mediaSession().playbackStateChangePermitted(MediaPlaybackState::Playing);
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
    return createVersion4UUIDString();
}

#if ENABLE(MODERN_MEDIA_CONTROLS)

String MediaControlsHost::shadowRootCSSText()
{
    return RenderTheme::singleton().mediaControlsStyleSheet();
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

#if ENABLE(CONTEXT_MENUS) && USE(ACCESSIBILITY_CONTEXT_MENUS)
class MediaControlsContextMenuProvider final : public ContextMenuProvider {
public:
    static Ref<MediaControlsContextMenuProvider> create(Vector<ContextMenuItem>&& items, Function<void(uint64_t)>&& callback)
    {
        return adoptRef(*new MediaControlsContextMenuProvider(WTFMove(items), WTFMove(callback)));
    }

private:
    MediaControlsContextMenuProvider(Vector<ContextMenuItem>&& items, Function<void(uint64_t)>&& callback)
        : m_items(WTFMove(items))
        , m_callback(WTFMove(callback))
    {
    }

    ~MediaControlsContextMenuProvider() override
    {
        contextMenuCleared();
    }

    void populateContextMenu(ContextMenu* menu) override
    {
        for (auto& item : m_items)
            menu->appendItem(item);
    }

    void didDismissContextMenu() override
    {
        if (!m_didDismiss) {
            m_didDismiss = true;
            m_callback(ContextMenuItemTagNoAction);
        }
    }

    void contextMenuItemSelected(ContextMenuAction action, const String&) override
    {
        m_callback(action - ContextMenuItemBaseCustomTag);
    }

    void contextMenuCleared() override
    {
        didDismissContextMenu();
        m_items.clear();
    }

    ContextMenuContext::Type contextMenuContextType() override
    {
        return ContextMenuContext::Type::MediaControls;
    }

    Vector<ContextMenuItem> m_items;
    Function<void(uint64_t)> m_callback;
    bool m_didDismiss { false };
};

class MediaControlsContextMenuEventListener final : public EventListener {
public:
    static Ref<MediaControlsContextMenuEventListener> create(Ref<MediaControlsContextMenuProvider>&& contextMenuProvider)
    {
        return adoptRef(*new MediaControlsContextMenuEventListener(WTFMove(contextMenuProvider)));
    }

    bool operator==(const EventListener& other) const override
    {
        return this == &other;
    }

    void handleEvent(ScriptExecutionContext&, Event& event) override
    {
        ASSERT(event.type() == eventNames().contextmenuEvent);

        auto* target = event.target();
        if (!is<Node>(target))
            return;
        auto& node = downcast<Node>(*target);

        auto* page = node.document().page();
        if (!page)
            return;

        page->contextMenuController().showContextMenu(event, m_contextMenuProvider);
        event.preventDefault();
        event.stopPropagation();
        event.stopImmediatePropagation();
    }

private:
    MediaControlsContextMenuEventListener(Ref<MediaControlsContextMenuProvider>&& contextMenuProvider)
        : EventListener(EventListener::CPPEventListenerType)
        , m_contextMenuProvider(WTFMove(contextMenuProvider))
    {
    }

    Ref<MediaControlsContextMenuProvider> m_contextMenuProvider;
};

#endif // ENABLE(CONTEXT_MENUS) && USE(ACCESSIBILITY_CONTEXT_MENUS)

bool MediaControlsHost::showMediaControlsContextMenu(HTMLElement& target, String&& optionsJSONString, Ref<VoidCallback>&& callback)
{
#if USE(UICONTEXTMENU) || (ENABLE(CONTEXT_MENUS) && USE(ACCESSIBILITY_CONTEXT_MENUS))
    if (m_showMediaControlsContextMenuCallback)
        return false;

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

#if USE(UICONTEXTMENU)
    using MenuItem = MediaControlsContextMenuItem;
    using MenuItemIdentifier = MediaControlsContextMenuItem::ID;
    constexpr auto invalidMenuItemIdentifier = MediaControlsContextMenuItem::invalidID;
#elif ENABLE(CONTEXT_MENUS) && USE(ACCESSIBILITY_CONTEXT_MENUS)
    using MenuItem = ContextMenuItem;
    using MenuItemIdentifier = uint64_t;
    constexpr auto invalidMenuItemIdentifier = ContextMenuItemTagNoAction;
#endif

#if ENABLE(VIDEO_PRESENTATION_MODE)
    enum class PictureInPictureTag { IncludePictureInPicture };
#endif // ENABLE(VIDEO_PRESENTATION_MODE)

    enum class PlaybackSpeed {
        x0_5,
        x1_0,
        x1_25,
        x1_5,
        x2_0,
    };

    enum class ShowMediaStatsTag { IncludeShowMediaStats };

    using MenuData = std::variant<
#if ENABLE(VIDEO_PRESENTATION_MODE)
        PictureInPictureTag,
#endif // ENABLE(VIDEO_PRESENTATION_MODE)
        RefPtr<AudioTrack>,
        RefPtr<TextTrack>,
        RefPtr<VTTCue>,
        PlaybackSpeed,
        ShowMediaStatsTag
    >;
    HashMap<MenuItemIdentifier, MenuData> idMap;

    auto createSubmenu = [] (const String& title, const String& icon, Vector<MenuItem>&& children) -> MenuItem {
#if USE(UICONTEXTMENU)
        return { MediaControlsContextMenuItem::invalidID, title, icon, /* checked */ false, WTFMove(children) };
#elif ENABLE(CONTEXT_MENUS) && USE(ACCESSIBILITY_CONTEXT_MENUS)
        UNUSED_PARAM(icon);
        return { ContextMenuItemTagNoAction, title, /* enabled */ true, /* checked */ false, WTFMove(children) };
#endif
    };

    auto createMenuItem = [&] (MenuData data, const String& title, bool checked = false, const String& icon = nullString()) -> MenuItem {
        auto id = idMap.size() + 1;
        idMap.add(id, data);

#if USE(UICONTEXTMENU)
        return { id, title, icon, checked, /* children */ { } };
#elif ENABLE(CONTEXT_MENUS) && USE(ACCESSIBILITY_CONTEXT_MENUS)
        UNUSED_PARAM(icon);
        return { CheckableActionType, static_cast<ContextMenuAction>(ContextMenuItemBaseCustomTag + id), title, /* enabled */ true, checked };
#endif
    };

    auto createSeparator = [] () -> MenuItem {
#if USE(UICONTEXTMENU)
        return { MediaControlsContextMenuItem::invalidID, /* title */ nullString(), /* icon */ nullString(), /* checked */ false, /* children */ { } };
#elif ENABLE(CONTEXT_MENUS) && USE(ACCESSIBILITY_CONTEXT_MENUS)
        return { SeparatorType, ContextMenuItemTagNoAction, /* title */ nullString() };
#endif
    };

    Vector<MenuItem> items;

#if ENABLE(VIDEO_PRESENTATION_MODE)
    if (optionsJSONObject->getBoolean("includePictureInPicture"_s).value_or(false)) {
        ASSERT(is<HTMLVideoElement>(mediaElement));
        ASSERT(downcast<HTMLVideoElement>(mediaElement).webkitSupportsPresentationMode(HTMLVideoElement::VideoPresentationMode::PictureInPicture));
        items.append(createMenuItem(PictureInPictureTag::IncludePictureInPicture, WEB_UI_STRING_KEY("Picture in Picture", "Picture in Picture (Media Controls Menu)", "Picture in Picture media controls context menu title"), false, "pip.enter"_s));
    }
#endif // ENABLE(VIDEO_PRESENTATION_MODE)

    if (optionsJSONObject->getBoolean("includeLanguages"_s).value_or(false)) {
        if (auto* audioTracks = mediaElement.audioTracks(); audioTracks && audioTracks->length() > 1) {
            auto& captionPreferences = page->group().ensureCaptionPreferences();
            auto languageMenuItems = captionPreferences.sortedTrackListForMenu(audioTracks).map([&](auto& audioTrack) {
                return createMenuItem(audioTrack, captionPreferences.displayNameForTrack(audioTrack.get()), audioTrack->enabled());
            });

            if (!languageMenuItems.isEmpty())
                items.append(createSubmenu(WEB_UI_STRING_KEY("Languages", "Languages (Media Controls Menu)", "Languages media controls context menu title"), "globe"_s, WTFMove(languageMenuItems)));
        }
    }

    if (optionsJSONObject->getBoolean("includeSubtitles"_s).value_or(false)) {
        if (auto* textTracks = mediaElement.textTracks(); textTracks && textTracks->length()) {
            auto& captionPreferences = page->group().ensureCaptionPreferences();
            auto sortedTextTracks = captionPreferences.sortedTrackListForMenu(textTracks, { TextTrack::Kind::Subtitles, TextTrack::Kind::Captions, TextTrack::Kind::Descriptions });
            bool allTracksDisabled = notFound == sortedTextTracks.findIf([] (const auto& textTrack) {
                return textTrack->mode() == TextTrack::Mode::Showing;
            });
            bool usesAutomaticTrack = captionPreferences.captionDisplayMode() == CaptionUserPreferences::Automatic && allTracksDisabled;
            auto subtitleMenuItems = sortedTextTracks.map([&](auto& textTrack) {
                bool checked = false;
                if (allTracksDisabled && textTrack == &TextTrack::captionMenuOffItem() && (captionPreferences.captionDisplayMode() == CaptionUserPreferences::ForcedOnly || captionPreferences.captionDisplayMode() == CaptionUserPreferences::Manual))
                    checked = true;
                else if (usesAutomaticTrack && textTrack == &TextTrack::captionMenuAutomaticItem())
                    checked = true;
                else if (!usesAutomaticTrack && textTrack->mode() == TextTrack::Mode::Showing)
                    checked = true;
                return createMenuItem(textTrack, captionPreferences.displayNameForTrack(textTrack.get()), checked);
            });

            if (!subtitleMenuItems.isEmpty())
                items.append(createSubmenu(WEB_UI_STRING_KEY("Subtitles", "Subtitles (Media Controls Menu)", "Subtitles media controls context menu title"), "captions.bubble"_s, WTFMove(subtitleMenuItems)));
        }
    }

    if (optionsJSONObject->getBoolean("includeChapters"_s).value_or(false)) {
        if (auto* textTracks = mediaElement.textTracks(); textTracks && textTracks->length()) {
            auto& captionPreferences = page->group().ensureCaptionPreferences();

            for (auto& textTrack : captionPreferences.sortedTrackListForMenu(textTracks, { TextTrack::Kind::Chapters })) {
                Vector<MenuItem> chapterMenuItems;

                if (auto* cues = textTrack->cues()) {
                    for (unsigned i = 0; i < cues->length(); ++i) {
                        auto* cue = cues->item(i);
                        if (!is<VTTCue>(cue))
                            continue;

                        auto& vttCue = downcast<VTTCue>(*cue);
                        chapterMenuItems.append(createMenuItem(RefPtr { &vttCue }, vttCue.text()));
                    }
                }

                if (!chapterMenuItems.isEmpty()) {
                    items.append(createSubmenu(captionPreferences.displayNameForTrack(textTrack.get()), "list.bullet"_s, WTFMove(chapterMenuItems)));

                    /* Only show the first valid chapters track. */
                    break;
                }
            }
        }
    }

    if (optionsJSONObject->getBoolean("includePlaybackRates"_s).value_or(false)) {
        auto playbackRate = mediaElement.playbackRate();

        items.append(createSubmenu(WEB_UI_STRING_KEY("Playback Speed", "Playback Speed (Media Controls Menu)", "Playback Speed media controls context menu title"), "speedometer"_s, {
            createMenuItem(PlaybackSpeed::x0_5, WEB_UI_STRING_KEY("0.5×", "0.5× (Media Controls Menu Playback Speed)", "0.5× media controls context menu playback speed label"), playbackRate == 0.5),
            createMenuItem(PlaybackSpeed::x1_0, WEB_UI_STRING_KEY("1×", "1× (Media Controls Menu Playback Speed)", "1× media controls context menu playback speed label"), playbackRate == 1.0),
            createMenuItem(PlaybackSpeed::x1_25, WEB_UI_STRING_KEY("1.25×", "1.25× (Media Controls Menu Playback Speed)", "1.25× media controls context menu playback speed label"), playbackRate == 1.25),
            createMenuItem(PlaybackSpeed::x1_5, WEB_UI_STRING_KEY("1.5×", "1.5× (Media Controls Menu Playback Speed)", "1.5× media controls context menu playback speed label"), playbackRate == 1.5),
            createMenuItem(PlaybackSpeed::x2_0, WEB_UI_STRING_KEY("2×", "2× (Media Controls Menu Playback Speed)", "2× media controls context menu playback speed label"), playbackRate == 2.0),
        }));
    }

#if ENABLE(CONTEXT_MENUS) && USE(ACCESSIBILITY_CONTEXT_MENUS)
    if ((items.size() == 1 && items[0].type() == SubmenuType) || optionsJSONObject->getBoolean("promoteSubMenus"_s).value_or(false)) {
        for (auto&& item : std::exchange(items, { })) {
            if (!items.isEmpty())
                items.append({ SeparatorType, invalidMenuItemIdentifier, /* title */ nullString() });

            ASSERT(item.type() == SubmenuType);
            items.append({ ActionType, invalidMenuItemIdentifier, item.title(), /* enabled */ false, /* checked */ false });
            items.appendVector(WTF::map(item.subMenuItems(), [] (const auto& item) -> ContextMenuItem {
                // The disabled inline item used instead of an actual submenu should be indented less than the submenu items.
                constexpr unsigned indentationLevel = 1;
                if (item.type() == SubmenuType)
                    return { item.action(), item.title(), item.enabled(), item.checked(), item.subMenuItems(), indentationLevel };
                return { item.type(), item.action(), item.title(), item.enabled(), item.checked(), indentationLevel };
            }));
        }
    }
#endif // ENABLE(CONTEXT_MENUS) && USE(ACCESSIBILITY_CONTEXT_MENUS)

    if (page->settings().showMediaStatsContextMenuItemEnabled() && page->settings().developerExtrasEnabled() && optionsJSONObject->getBoolean("includeShowMediaStats"_s).value_or(false)) {
        items.append(createSeparator());
        items.append(createMenuItem(ShowMediaStatsTag::IncludeShowMediaStats, contextMenuItemTagShowMediaStats(), mediaElement.showingStats(), "chart.bar.xaxis"_s));
    }

    if (items.isEmpty())
        return false;

    ASSERT(!idMap.isEmpty());

    m_showMediaControlsContextMenuCallback = WTFMove(callback);

    auto handleItemSelected = [weakThis = WeakPtr { *this }, idMap = WTFMove(idMap)] (MenuItemIdentifier selectedItemID) {
        if (!weakThis)
            return;
        Ref strongThis = *weakThis;

        auto invokeCallbackAtScopeExit = makeScopeExit([strongThis] {
            if (auto showMediaControlsContextMenuCallback = std::exchange(strongThis->m_showMediaControlsContextMenuCallback, nullptr))
                showMediaControlsContextMenuCallback->handleEvent();
        });

        if (selectedItemID == invalidMenuItemIdentifier)
            return;

        if (!strongThis->m_mediaElement)
            return;
        auto& mediaElement = *strongThis->m_mediaElement;

        UserGestureIndicator gestureIndicator(ProcessingUserGesture, &mediaElement.document());

        auto selectedItem = idMap.get(selectedItemID);
        WTF::switchOn(selectedItem,
#if ENABLE(VIDEO_PRESENTATION_MODE)
            [&] (PictureInPictureTag) {
                // Media controls are not shown when in PiP so we can assume that we're not in PiP.
                downcast<HTMLVideoElement>(mediaElement).webkitSetPresentationMode(HTMLVideoElement::VideoPresentationMode::PictureInPicture);
            },
#endif // ENABLE(VIDEO_PRESENTATION_MODE)
            [&] (RefPtr<AudioTrack>& selectedAudioTrack) {
                for (auto& track : idMap.values()) {
                    if (auto* audioTrack = std::get_if<RefPtr<AudioTrack>>(&track))
                        (*audioTrack)->setEnabled(*audioTrack == selectedAudioTrack);
                }
            },
            [&] (RefPtr<TextTrack>& selectedTextTrack) {
                for (auto& track : idMap.values()) {
                    if (auto* textTrack = std::get_if<RefPtr<TextTrack>>(&track))
                        (*textTrack)->setMode(TextTrack::Mode::Disabled);
                }
                mediaElement.setSelectedTextTrack(selectedTextTrack.get());
            },
            [&] (RefPtr<VTTCue>& cue) {
                mediaElement.setCurrentTime(cue->startMediaTime());
            },
            [&] (PlaybackSpeed playbackSpeed) {
                switch (playbackSpeed) {
                case PlaybackSpeed::x0_5:
                    mediaElement.setDefaultPlaybackRate(0.5);
                    mediaElement.setPlaybackRate(0.5);
                    return;

                case PlaybackSpeed::x1_0:
                    mediaElement.setDefaultPlaybackRate(1.0);
                    mediaElement.setPlaybackRate(1.0);
                    return;

                case PlaybackSpeed::x1_25:
                    mediaElement.setDefaultPlaybackRate(1.25);
                    mediaElement.setPlaybackRate(1.25);
                    return;

                case PlaybackSpeed::x1_5:
                    mediaElement.setDefaultPlaybackRate(1.5);
                    mediaElement.setPlaybackRate(1.5);
                    return;

                case PlaybackSpeed::x2_0:
                    mediaElement.setDefaultPlaybackRate(2.0);
                    mediaElement.setPlaybackRate(2.0);
                    return;
                }

                ASSERT_NOT_REACHED();
            },
            [&] (ShowMediaStatsTag) {
                mediaElement.setShowingStats(!mediaElement.showingStats());
            }
        );

    };

    auto bounds = target.boundsInRootViewSpace();
#if USE(UICONTEXTMENU)
    page->chrome().client().showMediaControlsContextMenu(bounds, WTFMove(items), WTFMove(handleItemSelected));
#elif ENABLE(CONTEXT_MENUS) && USE(ACCESSIBILITY_CONTEXT_MENUS)
    target.addEventListener(eventNames().contextmenuEvent, MediaControlsContextMenuEventListener::create(MediaControlsContextMenuProvider::create(WTFMove(items), WTFMove(handleItemSelected))), { /*capture */ true, /* passive */ std::nullopt, /* once */ true });
    page->contextMenuController().showContextMenuAt(*target.document().frame(), bounds.center());
#endif

    return true;
#else // USE(UICONTEXTMENU) || (ENABLE(CONTEXT_MENUS) && USE(ACCESSIBILITY_CONTEXT_MENUS))
    return false;
#endif
}

#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)

#endif // ENABLE(MODERN_MEDIA_CONTROLS)

} // namespace WebCore

#endif // ENABLE(VIDEO)
