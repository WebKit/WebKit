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

#pragma once

#if ENABLE(VIDEO)

#include "JSValueInWrappedObject.h"
#include "MediaSession.h"
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class AudioTrack;
class AudioTrackList;
class ContextMenuItem;
class DOMPromise;
class Element;
class WeakPtrImplWithEventTargetData;
class HTMLElement;
class HTMLMediaElement;
class MediaControlTextTrackContainerElement;
class TextTrack;
class TextTrackList;
class TextTrackRepresentation;
class VTTCue;
class VoidCallback;

struct MediaControlsContextMenuItem;

enum class HTMLMediaElementSourceType : uint8_t;

class MediaControlsHost final
    : public CanMakeWeakPtr<MediaControlsHost>
#if ENABLE(MEDIA_SESSION)
    , private MediaSessionObserver
#endif
    {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(MediaControlsHost);
public:
    USING_CAN_MAKE_WEAKPTR(CanMakeWeakPtr<MediaControlsHost>);

    explicit MediaControlsHost(HTMLMediaElement&);
    ~MediaControlsHost();

#if ENABLE(MEDIA_SESSION)
    WEBCORE_EXPORT void ref() const final;
    WEBCORE_EXPORT void deref() const final;
#else
    WEBCORE_EXPORT void ref() const;
    WEBCORE_EXPORT void deref() const;
#endif

    static const AtomString& automaticKeyword();
    static const AtomString& forcedOnlyKeyword();

    String layoutTraitsClassName() const;
    const AtomString& mediaControlsContainerClassName() const;

    double brightness() const { return 1; }
    void setBrightness(double) { }

    Vector<Ref<TextTrack>> sortedTrackListForMenu(TextTrackList&);
    Vector<Ref<AudioTrack>> sortedTrackListForMenu(AudioTrackList&);

    using TextOrAudioTrack = Variant<RefPtr<TextTrack>, RefPtr<AudioTrack>>;
    String displayNameForTrack(const std::optional<TextOrAudioTrack>&);

    static TextTrack& captionMenuOffItem();
    static TextTrack& captionMenuAutomaticItem();
    static TextTrack& captionMenuOnItem();
    AtomString captionDisplayMode() const;
    void setSelectedTextTrack(TextTrack*);
    Element* textTrackContainer();
    void updateTextTrackContainer();
    TextTrackRepresentation* textTrackRepresentation() const;
    bool allowsInlineMediaPlayback() const;
    bool supportsFullscreen() const;
    bool isVideoLayerInline() const;
    bool isInMediaDocument() const;
    bool userGestureRequired() const;
    bool shouldForceControlsDisplay() const;
    bool supportsSeeking() const;
    bool inWindowFullscreen() const;
    bool supportsRewind() const;
    bool needsChromeMediaControlsPseudoElement() const;
    bool isMediaControlsMacInlineSizeSpecsEnabled() const;

    void captionPreferencesChanged();
    enum class ForceUpdate : bool { No, Yes };
    void updateCaptionDisplaySizes(ForceUpdate = ForceUpdate::No);
    void updateTextTrackRepresentationImageIfNeeded();
    void enteredFullscreen();
    void exitedFullscreen();
    void requiresTextTrackRepresentationChanged();

    String externalDeviceDisplayName() const;

    enum class DeviceType { None, Airplay, Tvout };
    DeviceType externalDeviceType() const;

    bool controlsDependOnPageScaleFactor() const;
    void setControlsDependOnPageScaleFactor(bool v);

    static String generateUUID();

    Vector<String, 2> shadowRootStyleSheets() const;
    static String base64StringForIconNameAndType(const String& iconName, const String& iconType);

#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)
    Vector<MediaControlsContextMenuItem> mediaControlsContextMenuItemsForBindings(String&& optionsJSONString);
    bool showMediaControlsContextMenu(HTMLElement&, String&& optionsJSONString, Ref<VoidCallback>&&);
    void showCaptionDisplaySettingsPreview();
    void hideCaptionDisplaySettingsPreview();
#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)

    using SourceType = HTMLMediaElementSourceType;
    std::optional<SourceType> sourceType() const;

    void presentationModeChanged();

#if ENABLE(MEDIA_SESSION)
    void ensureMediaSessionObserver();
#endif

    const JSValueInWrappedObject& controllerWrapper() const { return m_controllerWrapper; }
    JSValueInWrappedObject& controllerWrapper() { return m_controllerWrapper; }

private:
    void savePreviouslySelectedTextTrackIfNecessary();
    void restorePreviouslySelectedTextTrackIfNecessary();

    MediaControlTextTrackContainerElement* ensureTextTrackContainer();

#if ENABLE(MEDIA_SESSION)
    RefPtr<MediaSession> mediaSession() const;

    // MediaSessionObserver
    void metadataChanged(const RefPtr<MediaMetadata>&) final;
#endif

    enum class PlaybackSpeed;
    enum class PictureInPictureTag;
    enum class ShowMediaStatsTag;

#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)
#if ENABLE(CONTEXT_MENUS) && USE(ACCESSIBILITY_CONTEXT_MENUS)
    using MenuItem = ContextMenuItem;
#else
    using MenuItem = MediaControlsContextMenuItem;
#endif
    using MenuItemIdentifier = uint64_t;

    using MenuData = Variant<
        std::monostate, // This must be the first alternative for the empty value of HashTraits
#if ENABLE(VIDEO_PRESENTATION_MODE)
        PictureInPictureTag,
#endif
        Ref<AudioTrack>,
        Ref<TextTrack>,
        Ref<VTTCue>,
        PlaybackSpeed,
        ShowMediaStatsTag
    >;
    using MenuDataMap = HashMap<MenuItemIdentifier, MenuData>;

    std::pair<Vector<MenuItem>, MenuDataMap> mediaControlsContextMenuItems(String&& optionsJSONString);
#endif

    Ref<HTMLMediaElement> protectedMediaElement() const;

    WeakRef<HTMLMediaElement> m_mediaElement;
    RefPtr<MediaControlTextTrackContainerElement> m_textTrackContainer;
    RefPtr<TextTrack> m_previouslySelectedTextTrack;

#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)
    RefPtr<VoidCallback> m_showMediaControlsContextMenuCallback;
#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)

    JSValueInWrappedObject m_controllerWrapper;
};

} // namespace WebCore

#endif // ENABLE(VIDEO)
