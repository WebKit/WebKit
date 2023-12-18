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
#include "JSDOMPromiseDeferredForward.h"
#include "VideoEncoder.h"
#include "WebCodecsCodecState.h"
#include "WebCodecsVideoEncoderConfig.h"
#include <wtf/Vector.h>

namespace WebCore {

class WebCodecsEncodedVideoChunk;
class WebCodecsErrorCallback;
class WebCodecsEncodedVideoChunkOutputCallback;
class WebCodecsVideoFrame;
struct WebCodecsEncodedVideoChunkMetadata;
struct WebCodecsVideoEncoderEncodeOptions;

class WebCodecsVideoEncoder
    : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<WebCodecsVideoEncoder>
    , public ActiveDOMObject
    , public EventTarget {
    WTF_MAKE_ISO_ALLOCATED(WebCodecsVideoEncoder);
public:
    ~WebCodecsVideoEncoder();

    struct Init {
        RefPtr<WebCodecsEncodedVideoChunkOutputCallback> output;
        RefPtr<WebCodecsErrorCallback> error;
    };

    static Ref<WebCodecsVideoEncoder> create(ScriptExecutionContext&, Init&&);

    WebCodecsCodecState state() const { return m_state; }
    size_t encodeQueueSize() const { return m_encodeQueueSize; }

    ExceptionOr<void> configure(ScriptExecutionContext&, WebCodecsVideoEncoderConfig&&);
    ExceptionOr<void> encode(Ref<WebCodecsVideoFrame>&&, WebCodecsVideoEncoderEncodeOptions&&);
    void flush(Ref<DeferredPromise>&&);
    ExceptionOr<void> reset();
    ExceptionOr<void> close();

    static void isConfigSupported(ScriptExecutionContext&, WebCodecsVideoEncoderConfig&&, Ref<DeferredPromise>&&);

    using ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::ref;
    using ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::deref;

private:
    WebCodecsVideoEncoder(ScriptExecutionContext&, Init&&);

    // ActiveDOMObject API.
    void stop() final;
    const char* activeDOMObjectName() const final;
    void suspend(ReasonForSuspension) final;
    bool virtualHasPendingActivity() const final;

    // EventTarget
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }
    EventTargetInterface eventTargetInterface() const final { return WebCodecsVideoEncoderEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }

    ExceptionOr<void> closeEncoder(Exception&&);
    ExceptionOr<void> resetEncoder(const Exception&);
    void setInternalEncoder(UniqueRef<VideoEncoder>&&);
    void scheduleDequeueEvent();

    void queueControlMessageAndProcess(Function<void()>&&);
    void processControlMessageQueue();
    WebCodecsEncodedVideoChunkMetadata createEncodedChunkMetadata(std::optional<unsigned>);

    WebCodecsCodecState m_state { WebCodecsCodecState::Unconfigured };
    size_t m_encodeQueueSize { 0 };
    size_t m_beingEncodedQueueSize { 0 };
    Ref<WebCodecsEncodedVideoChunkOutputCallback> m_output;
    Ref<WebCodecsErrorCallback> m_error;
    std::unique_ptr<VideoEncoder> m_internalEncoder;
    bool m_dequeueEventScheduled { false };
    Deque<Ref<DeferredPromise>> m_pendingFlushPromises;
    size_t m_clearFlushPromiseCount { 0 };
    bool m_isKeyChunkRequired { false };
    Deque<Function<void()>> m_controlMessageQueue;
    bool m_isMessageQueueBlocked { false };
    WebCodecsVideoEncoderConfig m_baseConfiguration;
    VideoEncoder::ActiveConfiguration m_activeConfiguration;
    bool m_hasNewActiveConfiguration { false };
    bool m_isFlushing { false };
};

}

#endif
