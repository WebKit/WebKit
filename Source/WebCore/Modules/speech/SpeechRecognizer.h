/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "SpeechRecognitionCaptureSource.h"
#include "SpeechRecognitionConnectionClientIdentifier.h"

#if HAVE(SPEECHRECOGNIZER)
#include <wtf/RetainPtr.h>
OBJC_CLASS WebSpeechRecognizerTask;
#endif

namespace WebCore {

class SpeechRecognitionUpdate;

class SpeechRecognizer : public CanMakeWeakPtr<SpeechRecognizer> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    using DelegateCallback = Function<void(const SpeechRecognitionUpdate&)>;
    WEBCORE_EXPORT explicit SpeechRecognizer(DelegateCallback&&);
    WEBCORE_EXPORT ~SpeechRecognizer() = default;

#if ENABLE(MEDIA_STREAM)
    WEBCORE_EXPORT void start(SpeechRecognitionConnectionClientIdentifier, Ref<RealtimeMediaSource>&&, bool mockSpeechRecognitionEnabled, const String& localeIdentifier, bool continuous, bool interimResults, uint64_t maxAlternatives);
#endif
    WEBCORE_EXPORT void reset();
    WEBCORE_EXPORT void abort();
    WEBCORE_EXPORT void stop();

    Optional<SpeechRecognitionConnectionClientIdentifier> currentClientIdentifier() const { return m_clientIdentifier; }
    SpeechRecognitionCaptureSource* source() { return m_source.get(); }

private:
    void stopInternal();

#if ENABLE(MEDIA_STREAM)
    void startCapture(Ref<RealtimeMediaSource>&&);
#endif
    void stopCapture();
    void dataCaptured(const WTF::MediaTime&, const PlatformAudioData&, const AudioStreamDescription&, size_t sampleCount);
    bool startRecognition(bool mockSpeechRecognitionEnabled, SpeechRecognitionConnectionClientIdentifier, const String& localeIdentifier, bool continuous, bool interimResults, uint64_t alternatives);
    void abortRecognition();
    void stopRecognition();
    void resetRecognition();

    Optional<SpeechRecognitionConnectionClientIdentifier> m_clientIdentifier;
    DelegateCallback m_delegateCallback;
    std::unique_ptr<SpeechRecognitionCaptureSource> m_source;

#if HAVE(SPEECHRECOGNIZER)
    RetainPtr<WebSpeechRecognizerTask> m_task;
#endif
};

} // namespace WebCore
