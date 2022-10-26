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

#if ENABLE(WEB_CODECS)

#include "ActiveDOMObject.h"
#include "EventTarget.h"
#include "JSDOMPromiseDeferred.h"
#include "VideoDecoder.h"
#include "WebCodecsCodecState.h"
#include "WebCodecsEncodedVideoChunkType.h"
#include "WebCodecsVideoDecoderSupport.h"
#include <wtf/Deque.h>
#include <wtf/Vector.h>

namespace WebCore {

class WebCodecsEncodedVideoChunk;
class WebCodecsErrorCallback;
class WebCodecsVideoFrameOutputCallback;

class WebCodecsVideoDecoder
    : public RefCounted<WebCodecsVideoDecoder>
    , public ActiveDOMObject
    , public EventTarget {
    WTF_MAKE_ISO_ALLOCATED(WebCodecsVideoDecoder);
public:
    ~WebCodecsVideoDecoder();

    struct Init {
        RefPtr<WebCodecsVideoFrameOutputCallback> output;
        RefPtr<WebCodecsErrorCallback> error;
    };

    static Ref<WebCodecsVideoDecoder> create(ScriptExecutionContext&, Init&&);

    WebCodecsCodecState state() const { return m_state; }
    size_t decodeQueueSize() const { return m_decodeQueueSize; }

    ExceptionOr<void> configure(WebCodecsVideoDecoderConfig&&);
    ExceptionOr<void> decode(Ref<WebCodecsEncodedVideoChunk>&&);
    ExceptionOr<void> flush(Ref<DeferredPromise>&&);
    ExceptionOr<void> reset();
    ExceptionOr<void> close();

    static void isConfigSupported(ScriptExecutionContext&, WebCodecsVideoDecoderConfig&&, Ref<DeferredPromise>&&);

    using RefCounted::ref;
    using RefCounted::deref;

private:
    WebCodecsVideoDecoder(ScriptExecutionContext&, Init&&);

    // ActiveDOMObject API.
    void stop() final;
    const char* activeDOMObjectName() const final;
    void suspend(ReasonForSuspension) final;
    bool virtualHasPendingActivity() const final;

    // EventTarget
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }
    EventTargetInterface eventTargetInterface() const final { return WebCodecsVideoDecoderEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }

    ExceptionOr<void> closeDecoder(Exception&&);
    ExceptionOr<void> resetDecoder(const Exception&);
    void setInternalDecoder(UniqueRef<VideoDecoder>&&);
    void scheduleDequeueEvent();

    void queueControlMessageAndProcess(Function<void()>&&);
    void processControlMessageQueue();

    WebCodecsCodecState m_state { WebCodecsCodecState::Unconfigured };
    size_t m_decodeQueueSize { 0 };
    size_t m_beingDecodedQueueSize { 0 };
    Ref<WebCodecsVideoFrameOutputCallback> m_output;
    Ref<WebCodecsErrorCallback> m_error;
    std::unique_ptr<VideoDecoder> m_internalDecoder;
    bool m_dequeueEventScheduled { false };
    Deque<Ref<DeferredPromise>> m_pendingFlushPromises;
    size_t m_clearFlushPromiseCount { 0 };
    bool m_isKeyChunkRequired { false };
    Deque<Function<void()>> m_controlMessageQueue;
    bool m_isMessageQueueBlocked { false };
};

}

#endif
