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

#include "AudioDecoder.h"
#include "JSDOMPromiseDeferredForward.h"
#include "WebCodecsAudioDecoderConfig.h"
#include "WebCodecsAudioDecoderSupport.h"
#include "WebCodecsBase.h"
#include "WebCodecsEncodedAudioChunkType.h"
#include <wtf/Vector.h>

namespace WebCore {

class WebCodecsEncodedAudioChunk;
class WebCodecsErrorCallback;
class WebCodecsAudioDataOutputCallback;

class WebCodecsAudioDecoder : public WebCodecsBase {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(WebCodecsAudioDecoder);
public:
    ~WebCodecsAudioDecoder();

    struct Init {
        RefPtr<WebCodecsAudioDataOutputCallback> output;
        RefPtr<WebCodecsErrorCallback> error;
    };

    static Ref<WebCodecsAudioDecoder> create(ScriptExecutionContext&, Init&&);

    size_t decodeQueueSize() const { return codecQueueSize(); }

    ExceptionOr<void> configure(ScriptExecutionContext&, WebCodecsAudioDecoderConfig&&);
    ExceptionOr<void> decode(Ref<WebCodecsEncodedAudioChunk>&&);
    ExceptionOr<void> flush(Ref<DeferredPromise>&&);
    ExceptionOr<void> reset();
    ExceptionOr<void> close();

    static void isConfigSupported(ScriptExecutionContext&, WebCodecsAudioDecoderConfig&&, Ref<DeferredPromise>&&);

    WebCodecsAudioDataOutputCallback& outputCallbackConcurrently() { return m_output.get(); }
    WebCodecsErrorCallback& errorCallbackConcurrently() { return m_error.get(); }

private:
    WebCodecsAudioDecoder(ScriptExecutionContext&, Init&&);

    // ActiveDOMObject.
    void stop() final;
    void suspend(ReasonForSuspension) final;

    // EventTarget
    enum EventTargetInterfaceType eventTargetInterface() const final { return EventTargetInterfaceType::WebCodecsAudioDecoder; }

    ExceptionOr<void> closeDecoder(Exception&&);
    ExceptionOr<void> resetDecoder(const Exception&);
    void setInternalDecoder(Ref<AudioDecoder>&&);

    Ref<WebCodecsAudioDataOutputCallback> m_output;
    Ref<WebCodecsErrorCallback> m_error;
    RefPtr<AudioDecoder> m_internalDecoder;
    Vector<Ref<DeferredPromise>> m_pendingFlushPromises;
    bool m_isKeyChunkRequired { false };
    size_t m_decoderCount { 0 };
};

}

#endif
