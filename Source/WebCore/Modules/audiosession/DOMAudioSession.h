/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if ENABLE(DOM_AUDIO_SESSION)

#include "ActiveDOMObject.h"
#include "AudioSession.h"
#include "EventTarget.h"
#include "ExceptionOr.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class DOMAudioSession final : public RefCounted<DOMAudioSession>, public ActiveDOMObject, public EventTarget, public AudioSession::InterruptionObserver {
    WTF_MAKE_ISO_ALLOCATED(DOMAudioSession);
public:
    static Ref<DOMAudioSession> create(ScriptExecutionContext*);
    ~DOMAudioSession();

    enum class Type : uint8_t { Auto, Playback, Transient, TransientSolo, Ambient, PlayAndRecord };
    enum class State : uint8_t { Inactive, Active, Interrupted };

    ExceptionOr<void> setType(Type);
    Type type() const;
    State state() const;

    using RefCounted<DOMAudioSession>::ref;
    using RefCounted<DOMAudioSession>::deref;

private:
    explicit DOMAudioSession(ScriptExecutionContext*);

    // EventTarget
    EventTargetInterface eventTargetInterface() const final { return DOMAudioSessionEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const final { return ContextDestructionObserver::scriptExecutionContext(); }
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    // ActiveDOMObject
    void stop() final;
    const char* activeDOMObjectName() const final;
    bool virtualHasPendingActivity() const final;

    // InterruptionObserver
    void beginAudioSessionInterruption() final;
    void endAudioSessionInterruption(AudioSession::MayResume) final;
    void activeStateChanged() final;

    void scheduleStateChangeEvent();

    bool m_hasScheduleStateChangeEvent { false };
    mutable std::optional<State> m_state;
};

} // namespace WebCore

#endif // ENABLE(DOM_AUDIO_SESSION)
