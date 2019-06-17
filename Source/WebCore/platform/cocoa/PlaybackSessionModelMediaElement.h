/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))

#include "EventListener.h"
#include "HTMLMediaElementEnums.h"
#include "PlaybackSessionModel.h"
#include <wtf/HashSet.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class AudioTrack;
class HTMLMediaElement;
class TextTrack;
class PlaybackSessionInterface;

class PlaybackSessionModelMediaElement final : public PlaybackSessionModel, public EventListener {
public:
    static Ref<PlaybackSessionModelMediaElement> create()
    {
        return adoptRef(*new PlaybackSessionModelMediaElement());
    }
    WEBCORE_EXPORT virtual ~PlaybackSessionModelMediaElement();
    WEBCORE_EXPORT void setMediaElement(HTMLMediaElement*);
    HTMLMediaElement* mediaElement() const { return m_mediaElement.get(); }

    WEBCORE_EXPORT void handleEvent(WebCore::ScriptExecutionContext&, WebCore::Event&) final;
    void updateForEventName(const WTF::AtomString&);
    bool operator==(const EventListener& rhs) const final { return static_cast<const WebCore::EventListener*>(this) == &rhs; }

    WEBCORE_EXPORT void addClient(PlaybackSessionModelClient&);
    WEBCORE_EXPORT void removeClient(PlaybackSessionModelClient&);
    WEBCORE_EXPORT void play() final;
    WEBCORE_EXPORT void pause() final;
    WEBCORE_EXPORT void togglePlayState() final;
    WEBCORE_EXPORT void beginScrubbing() final;
    WEBCORE_EXPORT void endScrubbing() final;
    WEBCORE_EXPORT void seekToTime(double time, double toleranceBefore, double toleranceAfter) final;
    WEBCORE_EXPORT void fastSeek(double time) final;
    WEBCORE_EXPORT void beginScanningForward() final;
    WEBCORE_EXPORT void beginScanningBackward() final;
    WEBCORE_EXPORT void endScanning() final;
    WEBCORE_EXPORT void selectAudioMediaOption(uint64_t index) final;
    WEBCORE_EXPORT void selectLegibleMediaOption(uint64_t index) final;
    WEBCORE_EXPORT void togglePictureInPicture() final;
    WEBCORE_EXPORT void toggleMuted() final;
    WEBCORE_EXPORT void setMuted(bool) final;
    WEBCORE_EXPORT void setVolume(double) final;
    WEBCORE_EXPORT void setPlayingOnSecondScreen(bool) final;

    double duration() const final;
    double currentTime() const final;
    double bufferedTime() const final;
    bool isPlaying() const final;
    bool isScrubbing() const final { return false; }
    float playbackRate() const final;
    Ref<TimeRanges> seekableRanges() const final;
    double seekableTimeRangesLastModifiedTime() const final;
    double liveUpdateInterval() const final;
    bool canPlayFastReverse() const final;
    Vector<MediaSelectionOption> audioMediaSelectionOptions() const final;
    uint64_t audioMediaSelectedIndex() const final;
    Vector<MediaSelectionOption> legibleMediaSelectionOptions() const final;
    uint64_t legibleMediaSelectedIndex() const final;
    bool externalPlaybackEnabled() const final;
    ExternalPlaybackTargetType externalPlaybackTargetType() const final;
    String externalPlaybackLocalizedDeviceName() const final;
    bool wirelessVideoPlaybackDisabled() const final;
    bool isMuted() const final;
    double volume() const final;
    bool isPictureInPictureSupported() const final;
    bool isPictureInPictureActive() const final;

protected:
    WEBCORE_EXPORT PlaybackSessionModelMediaElement();

private:
    void progressEventTimerFired();
    static const Vector<WTF::AtomString>& observedEventNames();
    const WTF::AtomString& eventNameAll();

    RefPtr<HTMLMediaElement> m_mediaElement;
    bool m_isListening { false };
    HashSet<PlaybackSessionModelClient*> m_clients;
    Vector<RefPtr<TextTrack>> m_legibleTracksForMenu;
    Vector<RefPtr<AudioTrack>> m_audioTracksForMenu;

    double playbackStartedTime() const;
    void updateMediaSelectionOptions();
    void updateMediaSelectionIndices();
};

}

#endif
