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

#ifndef WebPlaybackSessionModelMediaElement_h
#define WebPlaybackSessionModelMediaElement_h
#if PLATFORM(IOS) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))

#include "EventListener.h"
#include "HTMLMediaElementEnums.h"
#include "WebPlaybackSessionModel.h"
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>

namespace WebCore {
class AudioTrack;
class HTMLMediaElement;
class TextTrack;
class WebPlaybackSessionInterface;

class WebPlaybackSessionModelMediaElement final : public WebPlaybackSessionModel, public EventListener {
public:
    static Ref<WebPlaybackSessionModelMediaElement> create()
    {
        return adoptRef(*new WebPlaybackSessionModelMediaElement());
    }
    WEBCORE_EXPORT virtual ~WebPlaybackSessionModelMediaElement();
    WEBCORE_EXPORT void setWebPlaybackSessionInterface(WebPlaybackSessionInterface*);
    WEBCORE_EXPORT void setMediaElement(HTMLMediaElement*);
    WEBCORE_EXPORT HTMLMediaElement* mediaElement() const { return m_mediaElement.get(); }

    WEBCORE_EXPORT void handleEvent(WebCore::ScriptExecutionContext*, WebCore::Event*) final;
    void updateForEventName(const WTF::AtomicString&);
    bool operator==(const EventListener& rhs) final { return static_cast<WebCore::EventListener*>(this) == &rhs; }

    WEBCORE_EXPORT void play() final;
    WEBCORE_EXPORT void pause() final;
    WEBCORE_EXPORT void togglePlayState() final;
    WEBCORE_EXPORT void beginScrubbing() final;
    WEBCORE_EXPORT void endScrubbing() final;
    WEBCORE_EXPORT void seekToTime(double time) final;
    WEBCORE_EXPORT void fastSeek(double time) final;
    WEBCORE_EXPORT void beginScanningForward() final;
    WEBCORE_EXPORT void beginScanningBackward() final;
    WEBCORE_EXPORT void endScanning() final;
    WEBCORE_EXPORT void selectAudioMediaOption(uint64_t index) final;
    WEBCORE_EXPORT void selectLegibleMediaOption(uint64_t index) final;

protected:
    WEBCORE_EXPORT WebPlaybackSessionModelMediaElement();

private:
    static const Vector<WTF::AtomicString>& observedEventNames();
    const WTF::AtomicString& eventNameAll();

    RefPtr<HTMLMediaElement> m_mediaElement;
    bool m_isListening { false };
    WebPlaybackSessionInterface* m_playbackSessionInterface { nullptr };
    Vector<RefPtr<TextTrack>> m_legibleTracksForMenu;
    Vector<RefPtr<AudioTrack>> m_audioTracksForMenu;
    
    void updateLegibleOptions();
};
    
}

#endif

#endif
