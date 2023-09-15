/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#if ENABLE(SPEECH_SYNTHESIS)

#include "PlatformSpeechSynthesisUtterance.h"
#include "PlatformSpeechSynthesizer.h"
#include "SpeechSynthesisClient.h"
#include "SpeechSynthesisErrorCode.h"
#include "SpeechSynthesisUtterance.h"
#include "SpeechSynthesisVoice.h"
#include <wtf/Deque.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class Document;
class PlatformSpeechSynthesizerClient;
class SpeechSynthesisVoice;

class SpeechSynthesis : public PlatformSpeechSynthesizerClient, public SpeechSynthesisClientObserver, public RefCounted<SpeechSynthesis>, public ActiveDOMObject, public EventTarget {
    WTF_MAKE_ISO_ALLOCATED(SpeechSynthesis);
public:
    static Ref<SpeechSynthesis> create(ScriptExecutionContext&);
    virtual ~SpeechSynthesis();

    using RefCounted::ref;
    using RefCounted::deref;

    bool pending() const;
    bool speaking() const;
    bool paused() const;

    void speak(SpeechSynthesisUtterance&);
    void cancel();
    void pause();
    void resumeSynthesis();

    const Vector<Ref<SpeechSynthesisVoice>>& getVoices();

    // Used in testing to use a mock platform synthesizer
    WEBCORE_EXPORT void setPlatformSynthesizer(Ref<PlatformSpeechSynthesizer>&&);

    // Restrictions to change default behaviors.
    enum BehaviorRestrictionFlags {
        NoRestrictions = 0,
        RequireUserGestureForSpeechStartRestriction = 1 << 0,
    };
    typedef unsigned BehaviorRestrictions;

    bool userGestureRequiredForSpeechStart() const { return m_restrictions & RequireUserGestureForSpeechStartRestriction; }
    void removeBehaviorRestriction(BehaviorRestrictions restriction) { m_restrictions &= ~restriction; }
    WEBCORE_EXPORT void simulateVoicesListChange();

private:
    SpeechSynthesis(ScriptExecutionContext&);
    RefPtr<SpeechSynthesisUtterance> protectedCurrentSpeechUtterance();

    // PlatformSpeechSynthesizerClient
    void voicesDidChange() override;
    void didStartSpeaking(PlatformSpeechSynthesisUtterance&) override;
    void didPauseSpeaking(PlatformSpeechSynthesisUtterance&) override;
    void didResumeSpeaking(PlatformSpeechSynthesisUtterance&) override;
    void didFinishSpeaking(PlatformSpeechSynthesisUtterance&) override;
    void speakingErrorOccurred(PlatformSpeechSynthesisUtterance&) override;
    void boundaryEventOccurred(PlatformSpeechSynthesisUtterance&, SpeechBoundary, unsigned charIndex, unsigned charLength) override;

    // SpeechSynthesisClientObserver
    void didStartSpeaking() override;
    void didFinishSpeaking() override;
    void didPauseSpeaking() override;
    void didResumeSpeaking() override;
    void speakingErrorOccurred() override;
    void boundaryEventOccurred(bool wordBoundary, unsigned charIndex, unsigned charLength) override;
    void voicesChanged() override;

    // ActiveDOMObject
    const char* activeDOMObjectName() const final;
    bool virtualHasPendingActivity() const final;

    void startSpeakingImmediately(SpeechSynthesisUtterance&);
    void handleSpeakingCompleted(SpeechSynthesisUtterance&, bool errorOccurred);

    // EventTarget
    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }
    EventTargetInterface eventTargetInterface() const final { return SpeechSynthesisEventTargetInterfaceType; }
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }
    void eventListenersDidChange() final;
    
    PlatformSpeechSynthesizer& ensurePlatformSpeechSynthesizer();
    
    RefPtr<PlatformSpeechSynthesizer> m_platformSpeechSynthesizer;
    std::optional<Vector<Ref<SpeechSynthesisVoice>>> m_voiceList;
    std::unique_ptr<SpeechSynthesisUtteranceActivity> m_currentSpeechUtterance;
    Deque<Ref<SpeechSynthesisUtterance>> m_utteranceQueue;
    bool m_isPaused;
    BehaviorRestrictions m_restrictions;
    WeakPtr<SpeechSynthesisClient> m_speechSynthesisClient;
    bool m_hasEventListener { false };
};

} // namespace WebCore

#endif // ENABLE(SPEECH_SYNTHESIS)
