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

#if ENABLE(MEDIA_CONTROLS_SCRIPT)

#include <wtf/RefCounted.h>
#include <wtf/Variant.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class AudioTrack;
class AudioTrackList;
class Element;
class HTMLMediaElement;
class MediaControlTextTrackContainerElement;
class TextTrack;
class TextTrackList;

class MediaControlsHost : public RefCounted<MediaControlsHost> {
public:
    static Ref<MediaControlsHost> create(HTMLMediaElement*);
    ~MediaControlsHost();

    static const AtomString& automaticKeyword();
    static const AtomString& forcedOnlyKeyword();
    static const AtomString& alwaysOnKeyword();
    static const AtomString& manualKeyword();

    Vector<RefPtr<TextTrack>> sortedTrackListForMenu(TextTrackList&);
    Vector<RefPtr<AudioTrack>> sortedTrackListForMenu(AudioTrackList&);

    using TextOrAudioTrack = WTF::Variant<RefPtr<TextTrack>, RefPtr<AudioTrack>>;
    String displayNameForTrack(const Optional<TextOrAudioTrack>&);

    TextTrack* captionMenuOffItem();
    TextTrack* captionMenuAutomaticItem();
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
    void setPreparedToReturnVideoLayerToInline(bool);

    void updateCaptionDisplaySizes();
    void enteredFullscreen();
    void exitedFullscreen();

    String externalDeviceDisplayName() const;

    enum class DeviceType { None, Airplay, Tvout };
    DeviceType externalDeviceType() const;

    bool compactMode() const;
    void setSimulateCompactMode(bool value) { m_simulateCompactMode = value; }

    bool controlsDependOnPageScaleFactor() const;
    void setControlsDependOnPageScaleFactor(bool v);

    String generateUUID() const;

    String shadowRootCSSText() const;
    String base64StringForIconNameAndType(const String& iconName, const String& iconType) const;
    String formattedStringForDuration(double) const;

private:
    MediaControlsHost(HTMLMediaElement*);

    HTMLMediaElement* m_mediaElement;
    RefPtr<MediaControlTextTrackContainerElement> m_textTrackContainer;
    bool m_simulateCompactMode { false };
};

}

#endif
