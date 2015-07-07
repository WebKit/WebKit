/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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


#ifndef WebVideoFullscreenModelMediaElement_h
#define WebVideoFullscreenModelMediaElement_h

#if PLATFORM(IOS)

#include <WebCore/EventListener.h>
#include <WebCore/FloatRect.h>
#include <WebCore/PlatformLayer.h>
#include <WebCore/WebVideoFullscreenModel.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>

namespace WebCore {
class HTMLMediaElement;
class TextTrack;
class WebVideoFullscreenInterface;

class WebVideoFullscreenModelMediaElement : public WebVideoFullscreenModel, public EventListener {
public:
    WebVideoFullscreenModelMediaElement();
    virtual ~WebVideoFullscreenModelMediaElement();
    void setWebVideoFullscreenInterface(WebVideoFullscreenInterface* interface) {m_videoFullscreenInterface = interface;}
    void setMediaElement(HTMLMediaElement*);
    void setVideoFullscreenLayer(PlatformLayer*);
    
    virtual void handleEvent(WebCore::ScriptExecutionContext*, WebCore::Event*) override;
    void updateForEventName(const WTF::AtomicString&);
    bool operator==(const EventListener& rhs) override
        {return static_cast<WebCore::EventListener*>(this) == &rhs;}

    virtual void play() override;
    virtual void pause() override;
    virtual void togglePlayState() override;
    virtual void beginScrubbing() override;
    virtual void endScrubbing() override;
    virtual void seekToTime(double time) override;
    virtual void fastSeek(double time) override;
    virtual void beginScanningForward() override;
    virtual void beginScanningBackward() override;
    virtual void endScanning() override;
    virtual void requestExitFullscreen() override;
    virtual void setVideoLayerFrame(FloatRect) override;
    virtual void setVideoLayerGravity(WebVideoFullscreenModel::VideoGravity) override;
    virtual void selectAudioMediaOption(uint64_t index) override;
    virtual void selectLegibleMediaOption(uint64_t index) override;

private:
    static const Vector<WTF::AtomicString>& observedEventNames();
    const WTF::AtomicString& eventNameAll();
    
    RefPtr<HTMLMediaElement> m_mediaElement;
    RetainPtr<PlatformLayer> m_videoFullscreenLayer;
    bool m_isListening;
    WebVideoFullscreenInterface* m_videoFullscreenInterface;
    FloatRect m_videoFrame;
    Vector<RefPtr<TextTrack>> m_legibleTracksForMenu;

    void updateLegibleOptions();
};

}

#endif

#endif
