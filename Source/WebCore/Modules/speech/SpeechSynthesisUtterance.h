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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SpeechSynthesisUtterance_h
#define SpeechSynthesisUtterance_h

#if ENABLE(SPEECH_SYNTHESIS)

#include "ContextDestructionObserver.h"
#include "EventTarget.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {
    
class SpeechSynthesisUtterance : public RefCounted<SpeechSynthesisUtterance>, public ContextDestructionObserver, public EventTarget {
public:
    static PassRefPtr<SpeechSynthesisUtterance> create(ScriptExecutionContext*, const String&);
    
    const String& text() const { return m_text; }
    void setText(const String& text) { m_text = text; }

    const String& lang() const { return m_lang; }
    void setLang(const String& lang) { m_lang = lang; }

    const String& voiceURI() const { return m_voiceURI; }
    void setVoiceURI(const String& voiceURI) { m_voiceURI = voiceURI; }

    float volume() const { return m_volume; }
    void setVolume(float volume) { m_volume = volume; }

    float rate() const { return m_rate; }
    void setRate(float rate) { m_rate = rate; }

    float pitch() const { return m_pitch; }
    void setPitch(float pitch) { m_pitch = pitch; }

    DEFINE_ATTRIBUTE_EVENT_LISTENER(start);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(end);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(error);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(pause);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(resume);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(mark);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(boundary);

    using RefCounted<SpeechSynthesisUtterance>::ref;
    using RefCounted<SpeechSynthesisUtterance>::deref;

    virtual ScriptExecutionContext* scriptExecutionContext() const;

private:
    SpeechSynthesisUtterance(ScriptExecutionContext*, const String&);

    String m_text;
    String m_lang;
    String m_voiceURI;
    float m_volume;
    float m_rate;
    float m_pitch;
    
    // EventTarget
    EventTargetData m_eventTargetData;

    virtual const AtomicString& interfaceName() const OVERRIDE;
    virtual void refEventTarget() OVERRIDE { ref(); }
    virtual void derefEventTarget() OVERRIDE { deref(); }
    virtual EventTargetData* eventTargetData() OVERRIDE { return &m_eventTargetData; }
    virtual EventTargetData* ensureEventTargetData() OVERRIDE { return &m_eventTargetData; }
};
    
} // namespace WebCore

#endif // ENABLE(SPEECH_SYNTHESIS)

#endif // SpeechSynthesisUtterance_h
