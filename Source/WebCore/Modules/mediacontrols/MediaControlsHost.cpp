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
#include "LocalizedStrings.h"
#include "Logging.h"
#include "MediaControlTextTrackContainerElement.h"
#include "MediaControlsContextMenuItem.h"
#include "Node.h"
#include "Page.h"
#include "PageGroup.h"
#include "RenderTheme.h"
#include "TextTrack.h"
#include "TextTrackList.h"
#include "UserGestureIndicator.h"
#include "VoidCallback.h"
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <wtf/CompletionHandler.h>
#include <wtf/JSONValues.h>
#include <wtf/Scope.h>
#include <wtf/UUID.h>
#include <wtf/Variant.h>

#if USE(APPLE_INTERNAL_SDK)
#include <WebKitAdditions/MediaControlsHostAdditions.cpp>
#endif

namespace WebCore {

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

String MediaControlsHost::platform() const
{
#if PLATFORM(MAC)
    return "macos";
#elif PLATFORM(MACCATALYST)
    return "maccatalyst"_s;
#elif PLATFORM(IOS)
    return "ios"_s;
#elif PLATFORM(WATCHOS)
    return "watchos"_s;
#else
    ASSERT_NOT_REACHED();
    return nullString();
#endif
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
    return createCanonicalUUIDString();
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

#if !defined(MediaControlsHostAdditions_showMediaControlsContextMenu_MenuData)
#define MediaControlsHostAdditions_showMediaControlsContextMenu_MenuData
#endif

#if !defined(MediaControlsHostAdditions_showMediaControlsContextMenu_MenuData_switchOn)
#define MediaControlsHostAdditions_showMediaControlsContextMenu_MenuData_switchOn
#endif

#if ENABLE(CONTEXT_MENUS) && USE(ACCESSIBILITY_CONTEXT_MENUS)
class MediaControlsContextMenuProvider final : public ContextMenuProvider {
public:
    static Ref<MediaControlsContextMenuProvider> create(Vector<ContextMenuItem>&& items, CompletionHandler<void(uint64_t)>&& callback)
    {
        return adoptRef(*new MediaControlsContextMenuProvider(WTFMove(items), WTFMove(callback)));
    }

private:
    MediaControlsContextMenuProvider(Vector<ContextMenuItem>&& items, CompletionHandler<void(uint64_t)>&& callback)
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

    void contextMenuItemSelected(ContextMenuAction action, const String&) override
    {
        m_callback(action - ContextMenuItemBaseCustomTag);
    }

    void contextMenuCleared() override
    {
        if (m_callback)
            m_callback(ContextMenuItemTagNoAction);
        m_items.clear();
    }

    ContextMenuContext::Type contextMenuContextType() override
    {
        return ContextMenuContext::Type::MediaControls;
    }

    Vector<ContextMenuItem> m_items;
    CompletionHandler<void(uint64_t)> m_callback;
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
    if (m_showMediaControlsContextMenuCallback) {
#if USE(UICONTEXTMENU)
        return false;
#elif (ENABLE(CONTEXT_MENUS) && USE(ACCESSIBILITY_CONTEXT_MENUS))
        // FIXME: `contextMenuCleared` is invoked between show and item selected so we may have a pending callback.
        std::exchange(m_showMediaControlsContextMenuCallback, nullptr)->handleEvent();
#endif
    }

    m_showMediaControlsContextMenuCallback = WTFMove(callback);

    auto invokeCallbackAtScopeExit = makeScopeExit([&, protectedThis = makeRef(*this)] {
        if (m_showMediaControlsContextMenuCallback)
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

    using MenuData = Variant<
#if ENABLE(VIDEO_PRESENTATION_MODE)
        PictureInPictureTag,
#endif // ENABLE(VIDEO_PRESENTATION_MODE)
        RefPtr<AudioTrack>,
        RefPtr<TextTrack>
        MediaControlsHostAdditions_showMediaControlsContextMenu_MenuData
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

    Vector<MenuItem> items;

#if ENABLE(VIDEO_PRESENTATION_MODE)
    if (optionsJSONObject->getBoolean("includePictureInPicture"_s).valueOr(false)) {
        ASSERT(is<HTMLVideoElement>(mediaElement));
        ASSERT(downcast<HTMLVideoElement>(mediaElement).webkitSupportsPresentationMode(HTMLVideoElement::VideoPresentationMode::PictureInPicture));
        items.append(createMenuItem(PictureInPictureTag::IncludePictureInPicture, WEB_UI_STRING_KEY("Picture in Picture", "Picture in Picture (Media Controls Menu)", "Picture in Picture media controls context menu title"), false, "pip.enter"_s));
    }
#endif // ENABLE(VIDEO_PRESENTATION_MODE)

    if (optionsJSONObject->getBoolean("includeLanguages"_s).valueOr(false)) {
        if (auto* audioTracks = mediaElement.audioTracks(); audioTracks && audioTracks->length() > 1) {
            Vector<MenuItem> languageMenuItems;

            auto& captionPreferences = page->group().captionPreferences();
            for (auto& audioTrack : captionPreferences.sortedTrackListForMenu(audioTracks))
                languageMenuItems.append(createMenuItem(audioTrack, captionPreferences.displayNameForTrack(audioTrack.get()), audioTrack->enabled()));

            if (!languageMenuItems.isEmpty())
                items.append(createSubmenu(WEB_UI_STRING_KEY("Languages", "Languages (Media Controls Menu)", "Languages media controls context menu title"), "globe"_s, WTFMove(languageMenuItems)));
        }
    }

    if (optionsJSONObject->getBoolean("includeSubtitles"_s).valueOr(false)) {
        if (auto* textTracks = mediaElement.textTracks(); textTracks && textTracks->length()) {
            Vector<MenuItem> subtitleMenuItems;

            auto& captionPreferences = page->group().captionPreferences();
            auto sortedTextTracks = captionPreferences.sortedTrackListForMenu(textTracks, { TextTrack::Kind::Subtitles, TextTrack::Kind::Captions, TextTrack::Kind::Descriptions });
            bool allTracksDisabled = notFound == sortedTextTracks.findMatching([] (const auto& textTrack) {
                return textTrack->mode() == TextTrack::Mode::Showing;
            });
            bool usesAutomaticTrack = captionPreferences.captionDisplayMode() == CaptionUserPreferences::Automatic && allTracksDisabled;
            for (auto& textTrack : sortedTextTracks) {
                bool checked = false;
                if (allTracksDisabled && textTrack == &TextTrack::captionMenuOffItem() && (captionPreferences.captionDisplayMode() == CaptionUserPreferences::ForcedOnly || captionPreferences.captionDisplayMode() == CaptionUserPreferences::Manual))
                    checked = true;
                else if (usesAutomaticTrack && textTrack == &TextTrack::captionMenuAutomaticItem())
                    checked = true;
                else if (!usesAutomaticTrack && textTrack->mode() == TextTrack::Mode::Showing)
                    checked = true;
                subtitleMenuItems.append(createMenuItem(textTrack, captionPreferences.displayNameForTrack(textTrack.get()), checked));
            }

            if (!subtitleMenuItems.isEmpty())
                items.append(createSubmenu(WEB_UI_STRING_KEY("Subtitles", "Subtitles (Media Controls Menu)", "Subtitles media controls context menu title"), "captions.bubble"_s, WTFMove(subtitleMenuItems)));
        }
    }

#if defined(MediaControlsHostAdditions_showMediaControlsContextMenu_options)
    MediaControlsHostAdditions_showMediaControlsContextMenu_options
#endif

#if ENABLE(CONTEXT_MENUS) && USE(ACCESSIBILITY_CONTEXT_MENUS)
    if (optionsJSONObject->getBoolean("promoteSubMenus"_s).valueOr(false)) {
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

    if (items.isEmpty())
        return false;

    ASSERT(!idMap.isEmpty());

    auto handleItemSelected = [weakMediaElement = makeWeakPtr(mediaElement), idMap = WTFMove(idMap), invokeCallbackAtScopeExit = WTFMove(invokeCallbackAtScopeExit)] (MenuItemIdentifier selectedItemID) {
        if (selectedItemID == invalidMenuItemIdentifier)
            return;

        if (!weakMediaElement)
            return;

        auto& mediaElement = *weakMediaElement;

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
    };

    auto bounds = target.boundsInRootViewSpace();
#if USE(UICONTEXTMENU)
    page->chrome().client().showMediaControlsContextMenu(bounds, WTFMove(items), WTFMove(handleItemSelected));
#elif ENABLE(CONTEXT_MENUS) && USE(ACCESSIBILITY_CONTEXT_MENUS)
    target.addEventListener(eventNames().contextmenuEvent, MediaControlsContextMenuEventListener::create(MediaControlsContextMenuProvider::create(WTFMove(items), WTFMove(handleItemSelected))), { /*capture */ true, /* passive */ WTF::nullopt, /* once */ true });
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
