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
#include <WebCore/PlatformLayer.h>
#include <WebCore/WebVideoFullscreenModel.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>

namespace WebCore {
class HTMLMediaElement;
class WebVideoFullscreenInterface;

class WebVideoFullscreenModelMediaElement : public WebVideoFullscreenModel, public EventListener {
    RefPtr<HTMLMediaElement> m_mediaElement;
    RetainPtr<PlatformLayer> m_borrowedVideoLayer;
    bool m_isListening;
    WebVideoFullscreenInterface* m_videoFullscreenInterface;
    
public:
    WebVideoFullscreenModelMediaElement();
    virtual ~WebVideoFullscreenModelMediaElement();
    void setWebVideoFullscreenInterface(WebVideoFullscreenInterface* interface) {m_videoFullscreenInterface = interface;}
    void setMediaElement(HTMLMediaElement*);
    
    virtual void handleEvent(WebCore::ScriptExecutionContext*, WebCore::Event*) override;
    bool operator==(const EventListener& rhs) override
        {return static_cast<WebCore::EventListener*>(this) == &rhs;}

    virtual void borrowVideoLayer() override;
    virtual void returnVideoLayer() override;
    virtual void play() override;
    virtual void pause() override;
    virtual void togglePlayState() override;
    virtual void seekToTime(double time) override;
    virtual void requestExitFullscreen() override;
};

}

#endif

#endif
