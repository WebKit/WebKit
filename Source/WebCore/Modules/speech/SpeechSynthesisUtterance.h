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

#include "ContextDestructionObserver.h"
#include "EventTarget.h"
#include "PlatformSpeechSynthesisUtterance.h"
#include "SpeechSynthesisVoice.h"
#include <wtf/RefCounted.h>

namespace WebCore {

class SpeechSynthesisUtterance final : public PlatformSpeechSynthesisUtteranceClient, public RefCounted<SpeechSynthesisUtterance>, public ContextDestructionObserver, public EventTargetWithInlineData {
    WTF_MAKE_ISO_ALLOCATED(SpeechSynthesisUtterance);
public:
    static Ref<SpeechSynthesisUtterance> create(ScriptExecutionContext&, const String&);

    virtual ~SpeechSynthesisUtterance();

    const String& text() const { return m_platformUtterance->text(); }
    void setText(const String& text) { m_platformUtterance->setText(text); }

    const String& lang() const { return m_platformUtterance->lang(); }
    void setLang(const String& lang) { m_platformUtterance->setLang(lang); }

    SpeechSynthesisVoice* voice() const;
    void setVoice(SpeechSynthesisVoice*);

    float volume() const { return m_platformUtterance->volume(); }
    void setVolume(float volume) { m_platformUtterance->setVolume(volume); }

    float rate() const { return m_platformUtterance->rate(); }
    void setRate(float rate) { m_platformUtterance->setRate(rate); }

    float pitch() const { return m_platformUtterance->pitch(); }
    void setPitch(float pitch) { m_platformUtterance->setPitch(pitch); }

    MonotonicTime startTime() const { return m_platformUtterance->startTime(); }
    void setStartTime(MonotonicTime startTime) { m_platformUtterance->setStartTime(startTime); }

    using RefCounted::ref;
    using RefCounted::deref;

    PlatformSpeechSynthesisUtterance* platformUtterance() const { return m_platformUtterance.get(); }

private:
    SpeechSynthesisUtterance(ScriptExecutionContext&, const String&);

    ScriptExecutionContext* scriptExecutionContext() const final { return ContextDestructionObserver::scriptExecutionContext(); }
    EventTargetInterface eventTargetInterface() const final { return SpeechSynthesisUtteranceEventTargetInterfaceType; }
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    RefPtr<PlatformSpeechSynthesisUtterance> m_platformUtterance;
    RefPtr<SpeechSynthesisVoice> m_voice;
};

} // namespace WebCore

#endif // ENABLE(SPEECH_SYNTHESIS)
