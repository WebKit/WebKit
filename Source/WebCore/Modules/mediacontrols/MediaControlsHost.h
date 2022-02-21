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

#pragma once

#if ENABLE(VIDEO)

#include <variant>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class AudioTrack;
class AudioTrackList;
class Element;
class HTMLElement;
class HTMLMediaElement;
class MediaControlTextTrackContainerElement;
class TextTrack;
class TextTrackList;
class VoidCallback;

class MediaControlsHost final : public RefCounted<MediaControlsHost>, public CanMakeWeakPtr<MediaControlsHost> {
    WTF_MAKE_FAST_ALLOCATED(MediaControlsHost);
public:
    static Ref<MediaControlsHost> create(HTMLMediaElement&);
    ~MediaControlsHost();

    static const AtomString& automaticKeyword();
    static const AtomString& forcedOnlyKeyword();

    String layoutTraitsClassName() const;
    const AtomString& mediaControlsContainerClassName() const;

    double brightness() const { return 1; }
    void setBrightness(double) { }

    Vector<RefPtr<TextTrack>> sortedTrackListForMenu(TextTrackList&);
    Vector<RefPtr<AudioTrack>> sortedTrackListForMenu(AudioTrackList&);

    using TextOrAudioTrack = std::variant<RefPtr<TextTrack>, RefPtr<AudioTrack>>;
    String displayNameForTrack(const std::optional<TextOrAudioTrack>&);

    static TextTrack& captionMenuOffItem();
    static TextTrack& captionMenuAutomaticItem();
    AtomString captionDisplayMode() const;
    void setSelectedTextTrack(TextTrack*);
    Element* textTrackContainer();
    void updateTextTrackContainer();
    bool allowsInlineMediaPlayback() const;
    bool supportsFullscreen() const;
    bool isVideoLayerInline() const;
    bool isInMediaDocument() const;
    bool userGestureRequired() const;
    bool shouldForceControlsDisplay() const;

    enum class ForceUpdate { Yes, No };
    void updateCaptionDisplaySizes(ForceUpdate = ForceUpdate::No);
    void updateTextTrackRepresentationImageIfNeeded();
    void enteredFullscreen();
    void exitedFullscreen();

    String externalDeviceDisplayName() const;

    enum class DeviceType { None, Airplay, Tvout };
    DeviceType externalDeviceType() const;

    bool controlsDependOnPageScaleFactor() const;
    void setControlsDependOnPageScaleFactor(bool v);

    static String generateUUID();

#if ENABLE(MODERN_MEDIA_CONTROLS)
    static String shadowRootCSSText();
    static String base64StringForIconNameAndType(const String& iconName, const String& iconType);
    static String formattedStringForDuration(double);
#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)
    bool showMediaControlsContextMenu(HTMLElement&, String&& optionsJSONString, Ref<VoidCallback>&&);
#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)
#endif // ENABLE(MODERN_MEDIA_CONTROLS)

private:
    explicit MediaControlsHost(HTMLMediaElement&);

    WeakPtr<HTMLMediaElement> m_mediaElement;
    RefPtr<MediaControlTextTrackContainerElement> m_textTrackContainer;

#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)
    RefPtr<VoidCallback> m_showMediaControlsContextMenuCallback;
#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)
};

} // namespace WebCore

#endif // ENABLE(VIDEO)

