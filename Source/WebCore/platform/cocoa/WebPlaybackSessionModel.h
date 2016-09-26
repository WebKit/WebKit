/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#ifndef WebPlaybackSessionModel_h
#define WebPlaybackSessionModel_h

#if PLATFORM(IOS) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))

#include <wtf/Forward.h>
#include <wtf/Ref.h>
#include <wtf/Vector.h>

namespace WebCore {

class TimeRanges;

class WebPlaybackSessionModelClient;

class WebPlaybackSessionModel {
public:
    virtual ~WebPlaybackSessionModel() { };
    virtual void addClient(WebPlaybackSessionModelClient&) = 0;
    virtual void removeClient(WebPlaybackSessionModelClient&) = 0;

    virtual void play() = 0;
    virtual void pause() = 0;
    virtual void togglePlayState() = 0;
    virtual void beginScrubbing() = 0;
    virtual void endScrubbing() = 0;
    virtual void seekToTime(double time) = 0;
    virtual void fastSeek(double time) = 0;
    virtual void beginScanningForward() = 0;
    virtual void beginScanningBackward() = 0;
    virtual void endScanning() = 0;
    virtual void selectAudioMediaOption(uint64_t index) = 0;
    virtual void selectLegibleMediaOption(uint64_t index) = 0;

    enum ExternalPlaybackTargetType { TargetTypeNone, TargetTypeAirPlay, TargetTypeTVOut };

    virtual double playbackStartedTime() const = 0;
    virtual double duration() const = 0;
    virtual double currentTime() const = 0;
    virtual double bufferedTime() const = 0;
    virtual bool isPlaying() const = 0;
    virtual bool isScrubbing() const = 0;
    virtual float playbackRate() const = 0;
    virtual Ref<TimeRanges> seekableRanges() const = 0;
    virtual bool canPlayFastReverse() const = 0;
    virtual Vector<String> audioMediaSelectionOptions() const = 0;
    virtual uint64_t audioMediaSelectedIndex() const = 0;
    virtual Vector<String> legibleMediaSelectionOptions() const = 0;
    virtual uint64_t legibleMediaSelectedIndex() const = 0;
    virtual bool externalPlaybackEnabled() const = 0;
    virtual ExternalPlaybackTargetType externalPlaybackTargetType() const = 0;
    virtual String externalPlaybackLocalizedDeviceName() const = 0;
    virtual bool wirelessVideoPlaybackDisabled() const = 0;
};

class WebPlaybackSessionModelClient {
public:
    virtual ~WebPlaybackSessionModelClient() { };
    virtual void durationChanged(double) { }
    virtual void currentTimeChanged(double /* currentTime */, double /* anchorTime */) { }
    virtual void bufferedTimeChanged(double) { }
    virtual void playbackStartedTimeChanged(double /* playbackStartedTime */) { }
    virtual void rateChanged(bool /* isPlaying */, float /* playbackRate */) { }
    virtual void seekableRangesChanged(const TimeRanges&) { }
    virtual void canPlayFastReverseChanged(bool) { }
    virtual void audioMediaSelectionOptionsChanged(const Vector<String>& /* options */, uint64_t /* selectedIndex */) { }
    virtual void legibleMediaSelectionOptionsChanged(const Vector<String>& /* options */, uint64_t /* selectedIndex */) { }
    virtual void externalPlaybackChanged(bool /* enabled */, WebPlaybackSessionModel::ExternalPlaybackTargetType, const String& /* localizedDeviceName */) { }
    virtual void wirelessVideoPlaybackDisabledChanged(bool) { }
};

}

#endif // PLATFORM(IOS) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))

#endif
