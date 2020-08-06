/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2016-2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#include "BaseAudioContext.h"
#include "WebKitAudioListener.h"

namespace WebCore {

class AudioBuffer;
class Document;
class HTMLMediaElement;
class MediaElementAudioSourceNode;
class MediaStream;
class MediaStreamAudioDestinationNode;
class MediaStreamAudioSourceNode;
class PeriodicWave;
class WebKitAudioBufferSourceNode;
class WebKitAudioPannerNode;
class WebKitDynamicsCompressorNode;
class WebKitOscillatorNode;

// AudioContext is the cornerstone of the web audio API and all AudioNodes are created from it.
// For thread safety between the audio thread and the main thread, it has a rendering graph locking mechanism.

class WebKitAudioContext
    : public BaseAudioContext
{
    WTF_MAKE_ISO_ALLOCATED(WebKitAudioContext);
public:
    // Create an WebKitAudioContext for rendering to the audio hardware.
    static ExceptionOr<Ref<WebKitAudioContext>> create(Document&);

    void close(DOMPromiseDeferred<void>&&);

    WebKitAudioListener& listener() { return downcast<WebKitAudioListener>(BaseAudioContext::listener()); }

    // The AudioNode create methods are called on the main thread (from JavaScript).
#if ENABLE(VIDEO)
    ExceptionOr<Ref<MediaElementAudioSourceNode>> createMediaElementSource(HTMLMediaElement&);
#endif
#if ENABLE(MEDIA_STREAM)
    ExceptionOr<Ref<MediaStreamAudioSourceNode>> createMediaStreamSource(MediaStream&);
    ExceptionOr<Ref<MediaStreamAudioDestinationNode>> createMediaStreamDestination();
#endif
    ExceptionOr<Ref<WebKitAudioPannerNode>> createWebKitPanner();
    ExceptionOr<Ref<WebKitOscillatorNode>> createWebKitOscillator();
    ExceptionOr<Ref<PeriodicWave>> createPeriodicWave(Float32Array& real, Float32Array& imaginary);
    ExceptionOr<Ref<WebKitAudioBufferSourceNode>> createWebKitBufferSource();
    ExceptionOr<Ref<WebKitDynamicsCompressorNode>> createWebKitDynamicsCompressor();

protected:
    explicit WebKitAudioContext(Document&);
    WebKitAudioContext(Document&, AudioBuffer* renderTarget);

private:
    // ActiveDOMObject API.
    const char* activeDOMObjectName() const override;

    bool isWebKitAudioContext() const final { return true; }
};

} // WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::WebKitAudioContext)
    static bool isType(const WebCore::BaseAudioContext& context) { return context.isWebKitAudioContext(); }
SPECIALIZE_TYPE_TRAITS_END()
