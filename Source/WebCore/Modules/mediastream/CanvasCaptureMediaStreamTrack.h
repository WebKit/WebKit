/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(MEDIA_STREAM)

#include "CanvasBase.h"
#include "MediaStreamTrack.h"
#include "Timer.h"
#include <wtf/TypeCasts.h>

namespace WebCore {

class Document;
class HTMLCanvasElement;
class Image;

class CanvasCaptureMediaStreamTrack final : public MediaStreamTrack {
    WTF_MAKE_ISO_ALLOCATED(CanvasCaptureMediaStreamTrack);
public:
    static Ref<CanvasCaptureMediaStreamTrack> create(Document&, Ref<HTMLCanvasElement>&&, Optional<double>&& frameRequestRate);

    HTMLCanvasElement& canvas() { return m_canvas.get(); }
    void requestFrame() { static_cast<Source&>(source()).requestFrame(); }

    RefPtr<MediaStreamTrack> clone() final;

private:
    class Source final : public RealtimeMediaSource, private CanvasObserver {
    public:
        static Ref<Source> create(HTMLCanvasElement&, Optional<double>&& frameRequestRate);
        
        void requestFrame() { m_shouldEmitFrame = true; }
        Optional<double> frameRequestRate() const { return m_frameRequestRate; }

    private:
        Source(HTMLCanvasElement&, Optional<double>&&);

        // CanvasObserver API
        void canvasChanged(CanvasBase&, const FloatRect&) final;
        void canvasResized(CanvasBase&) final;
        void canvasDestroyed(CanvasBase&) final;

        // RealtimeMediaSource API
        void startProducingData() final;
        void stopProducingData()  final;
        const RealtimeMediaSourceCapabilities& capabilities() final { return RealtimeMediaSourceCapabilities::emptyCapabilities(); }
        const RealtimeMediaSourceSettings& settings() final;
        void settingsDidChange(OptionSet<RealtimeMediaSourceSettings::Flag>) final;

        void captureCanvas();
        void requestFrameTimerFired();

        bool m_shouldEmitFrame { true };
        Optional<double> m_frameRequestRate;
        Timer m_requestFrameTimer;
        Timer m_canvasChangedTimer;
        Optional<RealtimeMediaSourceSettings> m_currentSettings;
        HTMLCanvasElement* m_canvas;
        RefPtr<Image> m_currentImage;
    };

    CanvasCaptureMediaStreamTrack(Document&, Ref<HTMLCanvasElement>&&, Ref<Source>&&);
    CanvasCaptureMediaStreamTrack(Document&, Ref<HTMLCanvasElement>&&, Ref<MediaStreamTrackPrivate>&&);

    bool isCanvas() const final { return true; }

    Ref<HTMLCanvasElement> m_canvas;
};

}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::CanvasCaptureMediaStreamTrack)
static bool isType(const WebCore::MediaStreamTrack& track) { return track.isCanvas(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(MEDIA_STREAM)
