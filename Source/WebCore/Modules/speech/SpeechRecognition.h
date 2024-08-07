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

#include "ActiveDOMObject.h"
#include "EventTarget.h"
#include "SpeechRecognitionConnection.h"
#include "SpeechRecognitionConnectionClient.h"
#include "SpeechRecognitionResult.h"

namespace WebCore {

class Document;
class SpeechRecognitionResult;

class SpeechRecognition final : public SpeechRecognitionConnectionClient, public ActiveDOMObject, public RefCounted<SpeechRecognition>, public EventTarget  {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(SpeechRecognition);
public:
    static Ref<SpeechRecognition> create(Document&);

    using SpeechRecognitionConnectionClient::weakPtrFactory;
    using SpeechRecognitionConnectionClient::WeakValueType;
    using SpeechRecognitionConnectionClient::WeakPtrImplType;

    const String& lang() const { return m_lang; }
    void setLang(String&& lang) { m_lang = WTFMove(lang); }

    bool continuous() const { return m_continuous; }
    void setContinuous(bool continuous) { m_continuous = continuous; }

    bool interimResults() const { return m_interimResults; }
    void setInterimResults(bool interimResults) { m_interimResults = interimResults; }

    uint64_t maxAlternatives() const { return m_maxAlternatives; }
    void setMaxAlternatives(unsigned maxAlternatives) { m_maxAlternatives = maxAlternatives; }

    ExceptionOr<void> startRecognition();
    void stopRecognition();
    void abortRecognition();

    // ActiveDOMObject.
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    virtual ~SpeechRecognition();

private:
    enum class State {
        Inactive,
        Starting,
        Running,
        Stopping,
        Aborting,
    };

    explicit SpeechRecognition(Document&);

    // SpeechRecognitionConnectionClient
    void didStart() final;
    void didStartCapturingAudio() final;
    void didStartCapturingSound() final;
    void didStartCapturingSpeech() final;
    void didStopCapturingSpeech() final;
    void didStopCapturingSound() final;
    void didStopCapturingAudio() final;
    void didFindNoMatch() final;
    void didReceiveResult(Vector<SpeechRecognitionResultData>&& resultDatas) final;
    void didError(const SpeechRecognitionError&) final;
    void didEnd() final;

    // ActiveDOMObject
    void suspend(ReasonForSuspension) final;
    void stop() final;

    // EventTarget
    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }
    enum EventTargetInterfaceType eventTargetInterface() const final { return EventTargetInterfaceType::SpeechRecognition; }
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }
    bool virtualHasPendingActivity() const final;

    String m_lang;
    bool m_continuous { false };
    bool m_interimResults { false };
    uint64_t m_maxAlternatives { 1 };

    State m_state { State::Inactive };
    Vector<Ref<SpeechRecognitionResult>> m_finalResults;
    RefPtr<SpeechRecognitionConnection> m_connection;
};

} // namespace WebCore
