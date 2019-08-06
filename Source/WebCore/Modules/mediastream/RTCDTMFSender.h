/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEB_RTC)

#include "ActiveDOMObject.h"
#include "EventTarget.h"
#include "ExceptionOr.h"
#include "ScriptWrappable.h"
#include "Timer.h"

namespace WebCore {

class MediaStreamTrack;
class RTCDTMFSenderBackend;
class RTCRtpSender;

class RTCDTMFSender final : public RefCounted<RTCDTMFSender>, public EventTargetWithInlineData, public ActiveDOMObject {
    WTF_MAKE_ISO_ALLOCATED(RTCDTMFSender);
public:
    static Ref<RTCDTMFSender> create(ScriptExecutionContext& context, RTCRtpSender& sender, std::unique_ptr<RTCDTMFSenderBackend>&& backend) { return adoptRef(* new RTCDTMFSender(context, sender, WTFMove(backend))); }
    virtual ~RTCDTMFSender();

    bool canInsertDTMF() const;
    String toneBuffer() const;

    ExceptionOr<void> insertDTMF(const String& tones, size_t duration, size_t interToneGap);

    using RefCounted::ref;
    using RefCounted::deref;

private:
    RTCDTMFSender(ScriptExecutionContext&, RTCRtpSender&, std::unique_ptr<RTCDTMFSenderBackend>&&);

    void stop() final;
    const char* activeDOMObjectName() const final;
    bool canSuspendForDocumentSuspension() const final;

    EventTargetInterface eventTargetInterface() const final { return RTCDTMFSenderEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }

    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    bool isStopped() const { return !m_sender; }

    void playNextTone();
    void onTonePlayed();
    void toneTimerFired();

    Timer m_toneTimer;
    WeakPtr<RTCRtpSender> m_sender;
    std::unique_ptr<RTCDTMFSenderBackend> m_backend;
    String m_tones;
    size_t m_duration;
    size_t m_interToneGap;
    bool m_isPendingPlayoutTask { false };
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
