/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 * Copyright (C) 2023 Igalia S.L
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

#if ENABLE(WEB_CODECS)

#include "ActiveDOMObject.h"
#include "AudioDecoder.h"
#include "EventTarget.h"
#include "JSDOMPromiseDeferredForward.h"
#include "WebCodecsAudioDecoderConfig.h"
#include "WebCodecsAudioDecoderSupport.h"
#include "WebCodecsCodecState.h"
#include "WebCodecsEncodedAudioChunkType.h"
#include <wtf/Deque.h>
#include <wtf/Vector.h>

namespace WebCore {

class WebCodecsEncodedAudioChunk;
class WebCodecsErrorCallback;
class WebCodecsAudioDataOutputCallback;

class WebCodecsAudioDecoder
    : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<WebCodecsAudioDecoder>
    , public ActiveDOMObject
    , public EventTarget {
    WTF_MAKE_ISO_ALLOCATED(WebCodecsAudioDecoder);
public:
    ~WebCodecsAudioDecoder();

    struct Init {
        RefPtr<WebCodecsAudioDataOutputCallback> output;
        RefPtr<WebCodecsErrorCallback> error;
    };

    static Ref<WebCodecsAudioDecoder> create(ScriptExecutionContext&, Init&&);

    WebCodecsCodecState state() const { return m_state; }
    size_t decodeQueueSize() const { return m_decodeQueueSize; }

    ExceptionOr<void> configure(ScriptExecutionContext&, WebCodecsAudioDecoderConfig&&);
    ExceptionOr<void> decode(Ref<WebCodecsEncodedAudioChunk>&&);
    ExceptionOr<void> flush(Ref<DeferredPromise>&&);
    ExceptionOr<void> reset();
    ExceptionOr<void> close();

    static void isConfigSupported(ScriptExecutionContext&, WebCodecsAudioDecoderConfig&&, Ref<DeferredPromise>&&);

    using ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::ref;
    using ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::deref;

private:
    WebCodecsAudioDecoder(ScriptExecutionContext&, Init&&);

    // ActiveDOMObject API.
    void stop() final;
    const char* activeDOMObjectName() const final;
    void suspend(ReasonForSuspension) final;
    bool virtualHasPendingActivity() const final;

    // EventTarget
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }
    EventTargetInterface eventTargetInterface() const final { return WebCodecsAudioDecoderEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }

    ExceptionOr<void> closeDecoder(Exception&&);
    ExceptionOr<void> resetDecoder(const Exception&);
    void setInternalDecoder(UniqueRef<AudioDecoder>&&);
    void scheduleDequeueEvent();

    void queueControlMessageAndProcess(Function<void()>&&);
    void processControlMessageQueue();

    WebCodecsCodecState m_state { WebCodecsCodecState::Unconfigured };
    size_t m_decodeQueueSize { 0 };
    size_t m_beingDecodedQueueSize { 0 };
    Ref<WebCodecsAudioDataOutputCallback> m_output;
    Ref<WebCodecsErrorCallback> m_error;
    std::unique_ptr<AudioDecoder> m_internalDecoder;
    bool m_dequeueEventScheduled { false };
    Deque<Ref<DeferredPromise>> m_pendingFlushPromises;
    size_t m_clearFlushPromiseCount { 0 };
    bool m_isKeyChunkRequired { false };
    Deque<Function<void()>> m_controlMessageQueue;
    bool m_isMessageQueueBlocked { false };
    bool m_isFlushing { false };
};

}

#endif
