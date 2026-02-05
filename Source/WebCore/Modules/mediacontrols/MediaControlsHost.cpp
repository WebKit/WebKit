/*
 * Copyright (C) 2013-2025 Apple Inc. All rights reserved.
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

#include "AbortSignal.h"
#include "AudioTrackList.h"
#include "CaptionUserPreferences.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "ContextMenu.h"
#include "ContextMenuController.h"
#include "ContextMenuItem.h"
#include "ContextMenuProvider.h"
#include "DocumentPage.h"
#include "DocumentQuirks.h"
#include "Event.h"
#include "EventListener.h"
#include "EventNames.h"
#include "EventTarget.h"
#include "FloatRect.h"
#include "FrameDestructionObserverInlines.h"
#include "HTMLElement.h"
#include "HTMLMediaElement.h"
#include "HTMLVideoElement.h"
#include "LocalDOMWindow.h"
#include "LocalizedStrings.h"
#include "Logging.h"
#include "MediaControlTextTrackContainerElement.h"
#include "MediaControlsContextMenuItem.h"
#include "Navigator.h"
#include "NavigatorMediaSession.h"
#include "Node.h"
#include "NodeDocument.h"
#include "Page.h"
#include "PageGroup.h"
#include "RenderTheme.h"
#include "ShadowRoot.h"
#include "Settings.h"
#include "TextTrack.h"
#include "TextTrackCueList.h"
#include "TextTrackList.h"
#include "UserGestureIndicator.h"
#include "VTTCue.h"
#include "VoidCallback.h"
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <wtf/Function.h>
#include <wtf/JSONValues.h>
#include <wtf/Scope.h>
#include <wtf/UUID.h>

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

MediaControlsHost::MediaControlsHost(HTMLMediaElement& mediaElement)
    : m_mediaElement(mediaElement)
{
}

void MediaControlsHost::ref() const
{
    m_mediaElement->ref();
}

void MediaControlsHost::deref() const
{
    m_mediaElement->deref();
}

MediaControlsHost::~MediaControlsHost()
{
#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)
    if (auto showMediaControlsContextMenuCallback = std::exchange(m_showMediaControlsContextMenuCallback, nullptr))
        showMediaControlsContextMenuCallback->invoke();
#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)
}

String MediaControlsHost::layoutTraitsClassName() const
{
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    return "MacOSLayoutTraits"_s;
#elif PLATFORM(IOS)
    return "IOSLayoutTraits"_s;
#elif PLATFORM(APPLETV)
    return "TVOSLayoutTraits"_s;
#elif PLATFORM(VISION)
    return "VisionLayoutTraits"_s;
#elif PLATFORM(WATCHOS)
    return "WatchOSLayoutTraits"_s;
#elif USE(THEME_ADWAITA)
    return "AdwaitaLayoutTraits"_s;
#else
    ASSERT_NOT_REACHED();
    return nullString();
#endif
}

const AtomString& MediaControlsHost::mediaControlsContainerClassName() const
{
    static MainThreadNeverDestroyed<const AtomString> className("media-controls-container"_s);
    return className;
}

Vector<Ref<TextTrack>> MediaControlsHost::sortedTrackListForMenu(TextTrackList& trackList)
{
    RefPtr page = protectedMediaElement()->document().page();
    if (!page)
        return { };

    return page->checkedGroup()->ensureProtectedCaptionPreferences()->sortedTrackListForMenu(&trackList, { TextTrack::Kind::Subtitles, TextTrack::Kind::Captions, TextTrack::Kind::Descriptions });
}

Vector<Ref<AudioTrack>> MediaControlsHost::sortedTrackListForMenu(AudioTrackList& trackList)
{
    RefPtr page = protectedMediaElement()->document().page();
    if (!page)
        return { };

    return page->checkedGroup()->ensureProtectedCaptionPreferences()->sortedTrackListForMenu(&trackList);
}

String MediaControlsHost::displayNameForTrack(const std::optional<TextOrAudioTrack>& track)
{
    if (!track)
        return emptyString();

    RefPtr page = protectedMediaElement()->document().page();
    if (!page)
        return emptyString();

    return WTF::visit([page](auto& track) {
        return page->checkedGroup()->ensureCaptionPreferences().displayNameForTrack(*track);
    }, track.value());
}

TextTrack& MediaControlsHost::captionMenuOffItem()
{
    return TextTrack::captionMenuOffItemSingleton();
}

TextTrack& MediaControlsHost::captionMenuAutomaticItem()
{
    return TextTrack::captionMenuAutomaticItemSingleton();
}

TextTrack& MediaControlsHost::captionMenuOnItem()
{
    return TextTrack::captionMenuOnItemSingleton();
}

AtomString MediaControlsHost::captionDisplayMode() const
{
    RefPtr page = protectedMediaElement()->document().page();
    if (!page)
        return emptyAtom();

    switch (page->checkedGroup()->ensureProtectedCaptionPreferences()->captionDisplayMode()) {
    case CaptionUserPreferences::CaptionDisplayMode::Automatic:
        return automaticKeyword();
    case CaptionUserPreferences::CaptionDisplayMode::ForcedOnly:
        return forcedOnlyKeyword();
    case CaptionUserPreferences::CaptionDisplayMode::AlwaysOn:
        return alwaysOnKeyword();
    case CaptionUserPreferences::CaptionDisplayMode::Manual:
        return manualKeyword();
    default:
        ASSERT_NOT_REACHED();
        return emptyAtom();
    }
}

void MediaControlsHost::setSelectedTextTrack(TextTrack* track)
{
    protectedMediaElement()->setSelectedTextTrack(track);
}

MediaControlTextTrackContainerElement* MediaControlsHost::ensureTextTrackContainer()
{
    if (!m_textTrackContainer) {
        Ref mediaElement = m_mediaElement.get();
        m_textTrackContainer = MediaControlTextTrackContainerElement::create(protect(mediaElement->document()).get(), mediaElement);
    }

    return m_textTrackContainer.get();
}

Element* MediaControlsHost::textTrackContainer()
{
    return ensureTextTrackContainer();
}

void MediaControlsHost::updateTextTrackContainer()
{
    if (RefPtr textTrackContainer = m_textTrackContainer)
        textTrackContainer->updateDisplay();
}

TextTrackRepresentation* MediaControlsHost::textTrackRepresentation() const
{
    if (m_textTrackContainer)
        return m_textTrackContainer->textTrackRepresentation();
    return nullptr;
}

void MediaControlsHost::updateTextTrackRepresentationImageIfNeeded()
{
    if (RefPtr textTrackContainer = m_textTrackContainer)
        textTrackContainer->updateTextTrackRepresentationImageIfNeeded();
}

void MediaControlsHost::requiresTextTrackRepresentationChanged()
{
    if (RefPtr textTrackContainer = m_textTrackContainer)
        textTrackContainer->requiresTextTrackRepresentationChanged();
}

void MediaControlsHost::enteredFullscreen()
{
    if (RefPtr textTrackContainer = m_textTrackContainer)
        textTrackContainer->enteredFullscreen();
}

void MediaControlsHost::exitedFullscreen()
{
    if (RefPtr textTrackContainer = m_textTrackContainer)
        textTrackContainer->exitedFullscreen();
}

void MediaControlsHost::captionPreferencesChanged()
{
    if (RefPtr textTrackContainer = m_textTrackContainer)
        textTrackContainer->captionPreferencesChanged();
}

void MediaControlsHost::updateCaptionDisplaySizes(ForceUpdate force)
{
    if (RefPtr textTrackContainer = m_textTrackContainer)
        textTrackContainer->updateSizes(force == ForceUpdate::Yes ? MediaControlTextTrackContainerElement::ForceUpdate::Yes : MediaControlTextTrackContainerElement::ForceUpdate::No);
}

bool MediaControlsHost::allowsInlineMediaPlayback() const
{
    return !protectedMediaElement()->protectedMediaSession()->requiresFullscreenForVideoPlayback();
}

bool MediaControlsHost::supportsFullscreen() const
{
    return protectedMediaElement()->supportsFullscreen(HTMLMediaElementEnums::VideoFullscreenModeStandard);
}

bool MediaControlsHost::isVideoLayerInline() const
{
    return protectedMediaElement()->isVideoLayerInline();
}

bool MediaControlsHost::isInMediaDocument() const
{
    return protectedMediaElement()->document().isMediaDocument();
}

bool MediaControlsHost::userGestureRequired() const
{
    return !protectedMediaElement()->protectedMediaSession()->playbackStateChangePermitted(MediaPlaybackState::Playing);
}

bool MediaControlsHost::shouldForceControlsDisplay() const
{
    return protectedMediaElement()->shouldForceControlsDisplay();
}

bool MediaControlsHost::supportsSeeking() const
{
    return protectedMediaElement()->supportsSeeking();
}

bool MediaControlsHost::inWindowFullscreen() const
{
#if ENABLE(VIDEO_PRESENTATION_MODE)
    if (RefPtr videoElement = dynamicDowncast<HTMLVideoElement>(m_mediaElement.get()))
        return videoElement->webkitPresentationMode() == HTMLVideoElement::VideoPresentationMode::InWindow;
#endif
    return false;
}

bool MediaControlsHost::supportsRewind() const
{
    if (auto sourceType = this->sourceType())
        return *sourceType == SourceType::HLS || *sourceType == SourceType::File;
    return false;
}

bool MediaControlsHost::needsChromeMediaControlsPseudoElement() const
{
    return protect(protectedMediaElement()->document())->quirks().needsChromeMediaControlsPseudoElement();
}

bool MediaControlsHost::isMediaControlsMacInlineSizeSpecsEnabled() const
{
#if HAVE(MATERIAL_HOSTING)
    return protectedMediaElement()->document().settings().mediaControlsMacInlineSizeSpecsEnabled();
#else
    return false;
#endif
}

String MediaControlsHost::externalDeviceDisplayName() const
{
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    RefPtr player = protectedMediaElement()->player();
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
    RefPtr player = protectedMediaElement()->player();
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
    return protectedMediaElement()->mediaControlsDependOnPageScaleFactor();
}

void MediaControlsHost::setControlsDependOnPageScaleFactor(bool value)
{
    protectedMediaElement()->setMediaControlsDependOnPageScaleFactor(value);
}

String MediaControlsHost::generateUUID()
{
    return createVersion4UUIDString();
}

Vector<String, 2> MediaControlsHost::shadowRootStyleSheets() const
{
    return RenderTheme::singleton().mediaControlsStyleSheets(protectedMediaElement());
}

String MediaControlsHost::base64StringForIconNameAndType(const String& iconName, const String& iconType)
{
    return RenderTheme::singleton().mediaControlsBase64StringForIconNameAndType(iconName, iconType);
}

#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)

#if ENABLE(CONTEXT_MENUS) && USE(ACCESSIBILITY_CONTEXT_MENUS)
class MediaControlsContextMenuProvider final : public ContextMenuProvider {
public:
    static Ref<MediaControlsContextMenuProvider> create(HTMLMediaElementIdentifier identifier, Vector<ContextMenuItem>&& items, Function<void(uint64_t)>&& callback)
    {
        return adoptRef(*new MediaControlsContextMenuProvider(identifier, WTF::move(items), WTF::move(callback)));
    }

private:
    MediaControlsContextMenuProvider(HTMLMediaElementIdentifier identifier, Vector<ContextMenuItem>&& items, Function<void(uint64_t)>&& callback)
        : m_identifier(identifier)
        , m_items(WTF::move(items))
        , m_callback(WTF::move(callback))
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

    void prepareContext(ContextMenuContext& context) override
    {
        context.setMediaElementIdentifier(m_identifier);
    }

    HTMLMediaElementIdentifier m_identifier;
    Vector<ContextMenuItem> m_items;
    Function<void(uint64_t)> m_callback;
    bool m_didDismiss { false };
};

class MediaControlsContextMenuEventListener final : public EventListener {
public:
    static Ref<MediaControlsContextMenuEventListener> create(Ref<MediaControlsContextMenuProvider>&& contextMenuProvider)
    {
        return adoptRef(*new MediaControlsContextMenuEventListener(WTF::move(contextMenuProvider)));
    }

    void handleEvent(ScriptExecutionContext&, Event& event) override
    {
        ASSERT(event.type() == eventNames().contextmenuEvent);

        RefPtr target = dynamicDowncast<Node>(event.target());
        if (!target)
            return;

        RefPtr page = target->document().page();
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
        , m_contextMenuProvider(WTF::move(contextMenuProvider))
    {
    }

    const Ref<MediaControlsContextMenuProvider> m_contextMenuProvider;
};

#endif // ENABLE(CONTEXT_MENUS) && USE(ACCESSIBILITY_CONTEXT_MENUS)

enum class MediaControlsHost::PlaybackSpeed {
    x0_5,
    x1_0,
    x1_25,
    x1_5,
    x2_0,
};

enum class MediaControlsHost::PictureInPictureTag { IncludePictureInPicture };
enum class MediaControlsHost::ShowMediaStatsTag { IncludeShowMediaStats };

auto MediaControlsHost::mediaControlsContextMenuItems(String&& optionsJSONString) -> std::pair<Vector<MenuItem>, MenuDataMap>
{
#if USE(UICONTEXTMENU) || (ENABLE(CONTEXT_MENUS) && USE(ACCESSIBILITY_CONTEXT_MENUS))
    Ref mediaElement = m_mediaElement.get();
    RefPtr page = mediaElement->document().page();
    if (!page)
        return { };

    auto optionsJSON = JSON::Value::parseJSON(optionsJSONString);
    if (!optionsJSON)
        return { };

    auto optionsJSONObject = optionsJSON->asObject();
    if (!optionsJSONObject)
        return { };

#if ENABLE(CONTEXT_MENUS) && USE(ACCESSIBILITY_CONTEXT_MENUS)
    constexpr auto invalidMenuItemIdentifier = ContextMenuItemTagNoAction;
#endif

    MenuDataMap idMap;

    auto createSubmenu = [] (const String& title, const String& icon, Vector<MenuItem>&& children) -> MenuItem {
#if USE(UICONTEXTMENU)
        return { MediaControlsContextMenuItem::invalidID, title, icon, /* checked */ false, WTF::move(children) };
#elif ENABLE(CONTEXT_MENUS) && USE(ACCESSIBILITY_CONTEXT_MENUS)
        UNUSED_PARAM(icon);
        return { ContextMenuItemTagNoAction, title, /* enabled */ true, /* checked */ false, WTF::move(children) };
#endif
    };

    auto createMenuItem = [&idMap] (MenuData data, const String& title, bool checked = false, const String& icon = nullString()) -> MenuItem {
        auto id = idMap.size() + 1;
        idMap.add(id, data);

#if USE(UICONTEXTMENU)
        return { id, title, icon, checked, /* children */ { } };
#elif ENABLE(CONTEXT_MENUS) && USE(ACCESSIBILITY_CONTEXT_MENUS)
        UNUSED_PARAM(icon);
        return { ContextMenuItemType::CheckableAction, static_cast<ContextMenuAction>(ContextMenuItemBaseCustomTag + id), title, /* enabled */ true, checked };
#endif
    };

    auto createSeparator = [] () -> MenuItem {
#if USE(UICONTEXTMENU)
        return { MediaControlsContextMenuItem::invalidID, /* title */ nullString(), /* icon */ nullString(), /* checked */ false, /* children */ { } };
#elif ENABLE(CONTEXT_MENUS) && USE(ACCESSIBILITY_CONTEXT_MENUS)
        return { ContextMenuItemType::Separator, ContextMenuItemTagNoAction, /* title */ nullString() };
#endif
    };

    Vector<MenuItem> items;

#if ENABLE(VIDEO_PRESENTATION_MODE)
    if (optionsJSONObject->getBoolean("includePictureInPicture"_s).value_or(false)) {
        ASSERT(is<HTMLVideoElement>(mediaElement));
        ASSERT(downcast<HTMLVideoElement>(mediaElement)->webkitSupportsPresentationMode(HTMLVideoElement::VideoPresentationMode::PictureInPicture));
        items.append(createMenuItem(PictureInPictureTag::IncludePictureInPicture, WEB_UI_STRING_KEY("Picture in Picture", "Picture in Picture (Media Controls Menu)", "Picture in Picture media controls context menu title"), false, "pip.enter"_s));
    }
#endif // ENABLE(VIDEO_PRESENTATION_MODE)

    if (optionsJSONObject->getBoolean("includeLanguages"_s).value_or(false)) {
        if (RefPtr audioTracks = mediaElement->audioTracks(); audioTracks && audioTracks->length() > 1) {
            Ref captionPreferences = page->checkedGroup()->ensureCaptionPreferences();
            auto languageMenuItems = captionPreferences->sortedTrackListForMenu(audioTracks.get()).map([&createMenuItem, captionPreferences](auto& audioTrack) {
                return createMenuItem(audioTrack, captionPreferences->displayNameForTrack(audioTrack.get()), audioTrack->enabled());
            });

            if (!languageMenuItems.isEmpty())
                items.append(createSubmenu(WEB_UI_STRING_KEY("Languages", "Languages (Media Controls Menu)", "Languages media controls context menu title"), "globe"_s, WTF::move(languageMenuItems)));
        }
    }

    if (optionsJSONObject->getBoolean("includeSubtitles"_s).value_or(false)) {
        if (RefPtr textTracks = mediaElement->textTracks(); textTracks && textTracks->length()) {
            Ref captionPreferences = page->checkedGroup()->ensureCaptionPreferences();
            auto sortedTextTracks = captionPreferences->sortedTrackListForMenu(textTracks.get(), { TextTrack::Kind::Subtitles, TextTrack::Kind::Captions, TextTrack::Kind::Descriptions });
            bool allTracksDisabled = notFound == sortedTextTracks.findIf([] (const auto& textTrack) {
                return textTrack->mode() == TextTrack::Mode::Showing;
            });

            if (page->settings().captionDisplaySettingsEnabled()) {
                // Because the top-level menu now has On / Off options,
                // the languages submenu should show either which track
                // is currently enabled if captions are "On", or if
                // captions are "Off" which track would be chosen if
                // captions are turned on.
                RefPtr<TextTrack> bestTrackToEnable;
                if (allTracksDisabled) {
                    int bestScore = 0;
                    for (auto& track : sortedTextTracks) {
                        auto score = captionPreferences->textTrackSelectionScore(track, CaptionUserPreferences::CaptionDisplayMode::AlwaysOn);
                        if (score <= bestScore)
                            continue;
                        bestTrackToEnable = track.ptr();
                        bestScore = score;
                    }
                }

                Vector<MenuItem> subtitleMenuItems;
                subtitleMenuItems.append(createMenuItem(TextTrack::captionMenuOnItemSingleton(), captionPreferences->displayNameForTrack(TextTrack::captionMenuOnItemSingleton()), !allTracksDisabled));
                subtitleMenuItems.append(createMenuItem(TextTrack::captionMenuOffItemSingleton(), captionPreferences->displayNameForTrack(TextTrack::captionMenuOffItemSingleton()), allTracksDisabled));

                subtitleMenuItems.append(createSeparator());

                Vector<MenuItem> languages;
                for (auto& textTrack : sortedTextTracks) {
                    if (textTrack.ptr() == &TextTrack::captionMenuOffItemSingleton()
                        || textTrack.ptr() == &TextTrack::captionMenuOnItemSingleton()
                        || textTrack.ptr() == &TextTrack::captionMenuAutomaticItemSingleton())
                        continue;
                    bool checked = textTrack->mode() == TextTrack::Mode::Showing || textTrack.ptr() == bestTrackToEnable;
                    languages.append(createMenuItem(textTrack, captionPreferences->displayNameForTrack(textTrack.get()), checked));
                }
                subtitleMenuItems.append(createSubmenu(WEB_UI_STRING_KEY("Languages", "Languages (Media Controls Menu)", "Languages media controls context menu title"), "globe"_s, WTF::move(languages)));

                auto title = WEB_UI_STRING_KEY("Styles", "Styles (Media Controls Menu)", "Subtitles media controls menu title");
#if USE(UICONTEXTMENU)
                subtitleMenuItems.append(createSeparator());
                subtitleMenuItems.append(MenuItem { (MediaControlsContextMenuItem::ID)ContextMenuItemCaptionDisplayStyleSubmenu, title, nullString(), false, { } });
#elif ENABLE(CONTEXT_MENUS) && USE(ACCESSIBILITY_CONTEXT_MENUS)
                subtitleMenuItems.append(createSeparator());
                subtitleMenuItems.append({ ContextMenuItemCaptionDisplayStyleSubmenu, title, true, false, { } });
#endif
                if (!subtitleMenuItems.isEmpty())
                    items.append(createSubmenu(WEB_UI_STRING_KEY("Subtitles", "Subtitles (Media Controls Menu)", "Subtitles media controls context menu title"), "captions.bubble"_s, WTF::move(subtitleMenuItems)));

            } else {
                bool usesAutomaticTrack = captionPreferences->captionDisplayMode() == CaptionUserPreferences::CaptionDisplayMode::Automatic && allTracksDisabled;
                auto subtitleMenuItems = sortedTextTracks.map([&](auto& textTrack) {
                    bool checked = false;
                    if (allTracksDisabled && textTrack.ptr() == &TextTrack::captionMenuOffItemSingleton() && (captionPreferences->captionDisplayMode() == CaptionUserPreferences::CaptionDisplayMode::ForcedOnly || captionPreferences->captionDisplayMode() == CaptionUserPreferences::CaptionDisplayMode::Manual))
                        checked = true;
                    else if (usesAutomaticTrack && textTrack.ptr() == &TextTrack::captionMenuAutomaticItemSingleton())
                        checked = true;
                    else if (!usesAutomaticTrack && textTrack->mode() == TextTrack::Mode::Showing)
                        checked = true;
                    return createMenuItem(textTrack, captionPreferences->displayNameForTrack(textTrack.get()), checked);
                });
                if (!subtitleMenuItems.isEmpty())
                    items.append(createSubmenu(WEB_UI_STRING_KEY("Subtitles", "Subtitles (Media Controls Menu)", "Subtitles media controls context menu title"), "captions.bubble"_s, WTF::move(subtitleMenuItems)));
            }
        }
    }

    if (optionsJSONObject->getBoolean("includeChapters"_s).value_or(false)) {
        if (RefPtr textTracks = mediaElement->textTracks(); textTracks && textTracks->length()) {
            Ref captionPreferences = page->checkedGroup()->ensureCaptionPreferences();

            for (auto& textTrack : captionPreferences->sortedTrackListForMenu(textTracks.get(), { TextTrack::Kind::Chapters })) {
                Vector<MenuItem> chapterMenuItems;

                if (RefPtr cues = textTrack->cues()) {
                    for (unsigned i = 0; i < cues->length(); ++i) {
                        if (RefPtr vttCue = dynamicDowncast<VTTCue>(cues->item(i)))
                            chapterMenuItems.append(createMenuItem(*vttCue, vttCue->text()));
                    }
                }

                if (!chapterMenuItems.isEmpty()) {
                    items.append(createSubmenu(captionPreferences->displayNameForTrack(textTrack.get()), "list.bullet"_s, WTF::move(chapterMenuItems)));

                    /* Only show the first valid chapters track. */
                    break;
                }
            }
        }
    }

    if (optionsJSONObject->getBoolean("includePlaybackRates"_s).value_or(false)) {
        auto playbackRate = mediaElement->playbackRate();

        items.append(createSubmenu(WEB_UI_STRING_KEY("Playback Speed", "Playback Speed (Media Controls Menu)", "Playback Speed media controls context menu title"), "speedometer"_s, {
            createMenuItem(PlaybackSpeed::x0_5, WEB_UI_STRING_KEY("0.5×", "0.5× (Media Controls Menu Playback Speed)", "0.5× media controls context menu playback speed label"), playbackRate == 0.5),
            createMenuItem(PlaybackSpeed::x1_0, WEB_UI_STRING_KEY("1×", "1× (Media Controls Menu Playback Speed)", "1× media controls context menu playback speed label"), playbackRate == 1.0),
            createMenuItem(PlaybackSpeed::x1_25, WEB_UI_STRING_KEY("1.25×", "1.25× (Media Controls Menu Playback Speed)", "1.25× media controls context menu playback speed label"), playbackRate == 1.25),
            createMenuItem(PlaybackSpeed::x1_5, WEB_UI_STRING_KEY("1.5×", "1.5× (Media Controls Menu Playback Speed)", "1.5× media controls context menu playback speed label"), playbackRate == 1.5),
            createMenuItem(PlaybackSpeed::x2_0, WEB_UI_STRING_KEY("2×", "2× (Media Controls Menu Playback Speed)", "2× media controls context menu playback speed label"), playbackRate == 2.0),
        }));
    }

#if ENABLE(CONTEXT_MENUS) && USE(ACCESSIBILITY_CONTEXT_MENUS)
    if ((items.size() == 1 && items[0].type() == ContextMenuItemType::Submenu) || optionsJSONObject->getBoolean("promoteSubMenus"_s).value_or(false)) {
        for (auto&& item : std::exchange(items, { })) {
            if (!items.isEmpty())
                items.append({ ContextMenuItemType::Separator, invalidMenuItemIdentifier, /* title */ nullString() });

            ASSERT(item.type() == ContextMenuItemType::Submenu);
            items.append({ ContextMenuItemType::Action, invalidMenuItemIdentifier, item.title(), /* enabled */ false, /* checked */ false });
            items.appendVector(WTF::map(item.subMenuItems(), [] (const auto& item) -> ContextMenuItem {
                // The disabled inline item used instead of an actual submenu should be indented less than the submenu items.
                constexpr unsigned indentationLevel = 1;
                if (item.type() == ContextMenuItemType::Submenu)
                    return { item.action(), item.title(), item.enabled(), item.checked(), item.subMenuItems(), indentationLevel };
                return { item.type(), item.action(), item.title(), item.enabled(), item.checked(), indentationLevel };
            }));
        }
    }
#endif // ENABLE(CONTEXT_MENUS) && USE(ACCESSIBILITY_CONTEXT_MENUS)

    if (page->settings().showMediaStatsContextMenuItemEnabled() && page->settings().developerExtrasEnabled() && optionsJSONObject->getBoolean("includeShowMediaStats"_s).value_or(false)) {
        items.append(createSeparator());
        items.append(createMenuItem(ShowMediaStatsTag::IncludeShowMediaStats, contextMenuItemTagShowMediaStats(), mediaElement->showingStats(), "chart.bar.xaxis"_s));
    }

    ASSERT(!idMap.isEmpty());

    return std::make_pair(WTF::move(items), WTF::move(idMap));
}

bool MediaControlsHost::showMediaControlsContextMenu(HTMLElement& target, String&& optionsJSONString, Ref<VoidCallback>&& callback)
{
    Ref mediaElement = m_mediaElement.get();
    RefPtr page = mediaElement->document().page();
    if (!page)
        return false;

    m_showMediaControlsContextMenuCallback = WTF::move(callback);
    if (!m_showMediaControlsContextMenuCallback)
        return false;

    auto [items, idMap] = mediaControlsContextMenuItems(WTF::move(optionsJSONString));
    if (items.isEmpty())
        return false;

#if USE(UICONTEXTMENU)
    constexpr auto invalidMenuItemIdentifier = MediaControlsContextMenuItem::invalidID;
#elif ENABLE(CONTEXT_MENUS) && USE(ACCESSIBILITY_CONTEXT_MENUS)
    constexpr auto invalidMenuItemIdentifier = ContextMenuItemTagNoAction;
#endif

    auto handleItemSelected = [weakThis = WeakPtr { *this }, idMap = WTF::move(idMap)] (MenuItemIdentifier selectedItemID) {
        if (!weakThis)
            return;
        Ref protectedThis = *weakThis;

        auto invokeCallbackAtScopeExit = makeScopeExit([protectedThis] {
            if (auto showMediaControlsContextMenuCallback = std::exchange(protectedThis->m_showMediaControlsContextMenuCallback, nullptr))
                showMediaControlsContextMenuCallback->invoke();
        });

        if (selectedItemID == invalidMenuItemIdentifier)
            return;

        Ref mediaElement = protectedThis->m_mediaElement.get();
        UserGestureIndicator gestureIndicator(IsProcessingUserGesture::Yes, &mediaElement->document());

        auto selectedItem = idMap.get(selectedItemID);
        WTF::switchOn(selectedItem,
            [] (std::monostate) {
                ASSERT_NOT_REACHED();
            },
#if ENABLE(VIDEO_PRESENTATION_MODE)
            [&] (PictureInPictureTag) {
                // Media controls are not shown when in PiP so we can assume that we're not in PiP.
                downcast<HTMLVideoElement>(mediaElement)->webkitSetPresentationMode(HTMLVideoElement::VideoPresentationMode::PictureInPicture);
            },
#endif // ENABLE(VIDEO_PRESENTATION_MODE)
            [&] (Ref<AudioTrack>& selectedAudioTrack) {
                for (auto& track : idMap.values()) {
                    if (auto* audioTrack = std::get_if<Ref<AudioTrack>>(&track))
                        (*audioTrack)->setEnabled(*audioTrack == selectedAudioTrack);
                }
            },
            [&] (Ref<TextTrack>& selectedTextTrack) {
                protectedThis->savePreviouslySelectedTextTrackIfNecessary();
                for (auto& track : idMap.values()) {
                    if (auto* textTrack = std::get_if<Ref<TextTrack>>(&track))
                        (*textTrack)->setMode(TextTrack::Mode::Disabled);
                }
                mediaElement->setSelectedTextTrack(selectedTextTrack.ptr());
            },
            [&] (Ref<VTTCue>& cue) {
                mediaElement->setCurrentTime(cue->startMediaTime());
            },
            [&] (PlaybackSpeed playbackSpeed) {
                switch (playbackSpeed) {
                case PlaybackSpeed::x0_5:
                    mediaElement->setDefaultPlaybackRate(0.5);
                    mediaElement->setPlaybackRate(0.5);
                    return;

                case PlaybackSpeed::x1_0:
                    mediaElement->setDefaultPlaybackRate(1.0);
                    mediaElement->setPlaybackRate(1.0);
                    return;

                case PlaybackSpeed::x1_25:
                    mediaElement->setDefaultPlaybackRate(1.25);
                    mediaElement->setPlaybackRate(1.25);
                    return;

                case PlaybackSpeed::x1_5:
                    mediaElement->setDefaultPlaybackRate(1.5);
                    mediaElement->setPlaybackRate(1.5);
                    return;

                case PlaybackSpeed::x2_0:
                    mediaElement->setDefaultPlaybackRate(2.0);
                    mediaElement->setPlaybackRate(2.0);
                    return;
                }

                ASSERT_NOT_REACHED();
            },
            [&] (ShowMediaStatsTag) {
                mediaElement->setShowingStats(!mediaElement->showingStats());
            }
        );

    };

    auto bounds = target.boundsInRootViewSpace();
#if USE(UICONTEXTMENU)
    page->chrome().client().showMediaControlsContextMenu(bounds, WTF::move(items), mediaElement.get(), WTF::move(handleItemSelected));
#elif ENABLE(CONTEXT_MENUS) && USE(ACCESSIBILITY_CONTEXT_MENUS)
    target.addEventListener(eventNames().contextmenuEvent, MediaControlsContextMenuEventListener::create(MediaControlsContextMenuProvider::create(mediaElement->identifier(), WTF::move(items), WTF::move(handleItemSelected))), { { /*capture */ true }, /* passive */ std::nullopt, /* once */ true, nullptr, false });
    page->contextMenuController().showContextMenuAt(*target.document().protectedFrame(), bounds.center());
#endif

    return true;
#else // USE(UICONTEXTMENU) || (ENABLE(CONTEXT_MENUS) && USE(ACCESSIBILITY_CONTEXT_MENUS))
    return false;
#endif
}

Vector<MediaControlsContextMenuItem> MediaControlsHost::mediaControlsContextMenuItemsForBindings(String&& optionsJSONString)
{
    auto items = mediaControlsContextMenuItems(WTF::move(optionsJSONString)).first;
#if ENABLE(CONTEXT_MENUS) && USE(ACCESSIBILITY_CONTEXT_MENUS)
    Function<MediaControlsContextMenuItem(const ContextMenuItem&)> contextMenuItemToMediaControlsContextMenuItem;
    contextMenuItemToMediaControlsContextMenuItem = [&](const ContextMenuItem& item) -> MediaControlsContextMenuItem {
        return {
            .id = static_cast<MenuItemIdentifier>(item.action()) - ContextMenuItemBaseCustomTag,
            .title = item.title(),
            .icon = emptyString(),
            .checked = item.checked(),
            .children = item.subMenuItems().map(contextMenuItemToMediaControlsContextMenuItem),
        };
    };
    return items.map(contextMenuItemToMediaControlsContextMenuItem);
#else
    return items;
#endif
}

void MediaControlsHost::showCaptionDisplaySettingsPreview()
{
    if (RefPtr textTrackContainer = ensureTextTrackContainer())
        textTrackContainer->showCaptionDisplaySettingsPreview();
}

void MediaControlsHost::hideCaptionDisplaySettingsPreview()
{
    if (RefPtr textTrackContainer = ensureTextTrackContainer())
        textTrackContainer->hideCaptionDisplaySettingsPreview();
}

#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)

auto MediaControlsHost::sourceType() const -> std::optional<SourceType>
{
    return protectedMediaElement()->sourceType();
}


void MediaControlsHost::presentationModeChanged()
{
    restorePreviouslySelectedTextTrackIfNecessary();
}

void MediaControlsHost::savePreviouslySelectedTextTrackIfNecessary()
{
    if (!inWindowFullscreen())
        return;

    if (m_previouslySelectedTextTrack)
        return;

    Ref mediaElement = m_mediaElement.get();
    RefPtr page = mediaElement->document().page();
    if (!page)
        return;

    RefPtr textTracks = mediaElement->textTracks();
    for (unsigned i = 0; textTracks && i < textTracks->length(); ++i) {
        RefPtr textTrack = textTracks->item(i);
        ASSERT(textTrack);
        if (!textTrack)
            continue;

        if (textTrack->mode() == TextTrack::Mode::Showing) {
            m_previouslySelectedTextTrack = WTF::move(textTrack);
            return;
        }
    }

    switch (page->checkedGroup()->ensureProtectedCaptionPreferences()->captionDisplayMode()) {
    case CaptionUserPreferences::CaptionDisplayMode::Automatic:
        m_previouslySelectedTextTrack = TextTrack::captionMenuAutomaticItemSingleton();
        return;
    case CaptionUserPreferences::CaptionDisplayMode::ForcedOnly:
    case CaptionUserPreferences::CaptionDisplayMode::Manual:
    case CaptionUserPreferences::CaptionDisplayMode::AlwaysOn:
        m_previouslySelectedTextTrack = TextTrack::captionMenuOffItemSingleton();
        return;
    }
}

void MediaControlsHost::restorePreviouslySelectedTextTrackIfNecessary()
{
    if (inWindowFullscreen())
        return;

    RefPtr previouslySelectedTextTrack = m_previouslySelectedTextTrack;
    if (!previouslySelectedTextTrack)
        return;

    RefPtr textTracks = protectedMediaElement()->textTracks();
    for (unsigned i = 0; textTracks && i < textTracks->length(); ++i) {
        RefPtr textTrack = textTracks->item(i);
        ASSERT(textTrack);
        if (!textTrack)
            continue;

        if (previouslySelectedTextTrack != textTrack)
            textTrack->setMode(TextTrack::Mode::Disabled);
    }
    previouslySelectedTextTrack->setMode(TextTrack::Mode::Showing);
    m_previouslySelectedTextTrack = nullptr;
}

#if ENABLE(MEDIA_SESSION)
RefPtr<MediaSession> MediaControlsHost::mediaSession() const
{
    RefPtr window = protectedMediaElement()->document().window();
    if (!window)
        return { };

    return NavigatorMediaSession::mediaSessionIfExists(window->protectedNavigator());
}

void MediaControlsHost::ensureMediaSessionObserver()
{
    RefPtr mediaSession = this->mediaSession();
    if (!mediaSession || mediaSession->hasObserver(*this))
        return;

    mediaSession->addObserver(*this);
}

void MediaControlsHost::metadataChanged(const RefPtr<MediaMetadata>&)
{
    RefPtr shadowRoot = protectedMediaElement()->userAgentShadowRoot();
    if (!shadowRoot)
        return;

    shadowRoot->dispatchEvent(Event::create(eventNames().webkitmediasessionmetadatachangedEvent, Event::CanBubble::No, Event::IsCancelable::No));
}
#endif // ENABLE(MEDIA_SESSION)

Ref<HTMLMediaElement> MediaControlsHost::protectedMediaElement() const
{
    return m_mediaElement.get();
}

} // namespace WebCore

#endif // ENABLE(VIDEO)
