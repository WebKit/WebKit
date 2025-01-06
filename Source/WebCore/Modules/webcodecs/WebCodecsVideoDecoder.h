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

#include "JSDOMPromiseDeferredForward.h"
#include "VideoDecoder.h"
#include "WebCodecsBase.h"
#include "WebCodecsEncodedVideoChunkType.h"
#include "WebCodecsVideoDecoderSupport.h"
#include <wtf/Vector.h>

namespace WebCore {

class WebCodecsEncodedVideoChunk;
class WebCodecsErrorCallback;
class WebCodecsVideoFrameOutputCallback;

class WebCodecsVideoDecoder : public WebCodecsBase {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(WebCodecsVideoDecoder);
public:
    ~WebCodecsVideoDecoder();

    struct Init {
        RefPtr<WebCodecsVideoFrameOutputCallback> output;
        RefPtr<WebCodecsErrorCallback> error;
    };

    static Ref<WebCodecsVideoDecoder> create(ScriptExecutionContext&, Init&&);

    size_t decodeQueueSize() const { return codecQueueSize(); }

    WebCodecsVideoFrameOutputCallback& outputCallbackConcurrently() { return m_output.get(); }
    WebCodecsErrorCallback& errorCallbackConcurrently() { return m_error.get(); }

    ExceptionOr<void> configure(ScriptExecutionContext&, WebCodecsVideoDecoderConfig&&);
    ExceptionOr<void> decode(Ref<WebCodecsEncodedVideoChunk>&&);
    ExceptionOr<void> flush(Ref<DeferredPromise>&&);
    ExceptionOr<void> reset();
    ExceptionOr<void> close();

    static void isConfigSupported(ScriptExecutionContext&, WebCodecsVideoDecoderConfig&&, Ref<DeferredPromise>&&);

private:
    WebCodecsVideoDecoder(ScriptExecutionContext&, Init&&);
    size_t maximumCodecOperationsEnqueued() const final { return 4; }

    // ActiveDOMObject.
    void stop() final;
    void suspend(ReasonForSuspension) final;

    // EventTarget.
    enum EventTargetInterfaceType eventTargetInterface() const final { return EventTargetInterfaceType::WebCodecsVideoDecoder; }

    ExceptionOr<void> closeDecoder(Exception&&);
    ExceptionOr<void> resetDecoder(const Exception&);
    void setInternalDecoder(Ref<VideoDecoder>&&);

    Ref<WebCodecsVideoFrameOutputCallback> m_output;
    Ref<WebCodecsErrorCallback> m_error;
    RefPtr<VideoDecoder> m_internalDecoder;
    Vector<Ref<DeferredPromise>> m_pendingFlushPromises;
    bool m_isKeyChunkRequired { false };
    size_t m_decoderCount { 0 };
};

}

#endif
