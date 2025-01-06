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
#include "VideoEncoder.h"
#include "WebCodecsBase.h"
#include "WebCodecsVideoEncoderConfig.h"
#include <wtf/Vector.h>

namespace WebCore {

class WebCodecsEncodedVideoChunk;
class WebCodecsErrorCallback;
class WebCodecsEncodedVideoChunkOutputCallback;
class WebCodecsVideoFrame;
struct WebCodecsEncodedVideoChunkMetadata;
struct WebCodecsVideoEncoderEncodeOptions;

class WebCodecsVideoEncoder : public WebCodecsBase {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(WebCodecsVideoEncoder);
public:
    ~WebCodecsVideoEncoder();

    struct Init {
        RefPtr<WebCodecsEncodedVideoChunkOutputCallback> output;
        RefPtr<WebCodecsErrorCallback> error;
    };

    static Ref<WebCodecsVideoEncoder> create(ScriptExecutionContext&, Init&&);

    size_t encodeQueueSize() const { return codecQueueSize(); }

    ExceptionOr<void> configure(ScriptExecutionContext&, WebCodecsVideoEncoderConfig&&);
    ExceptionOr<void> encode(Ref<WebCodecsVideoFrame>&&, WebCodecsVideoEncoderEncodeOptions&&);
    void flush(Ref<DeferredPromise>&&);
    ExceptionOr<void> reset();
    ExceptionOr<void> close();

    static void isConfigSupported(ScriptExecutionContext&, WebCodecsVideoEncoderConfig&&, Ref<DeferredPromise>&&);

    WebCodecsEncodedVideoChunkOutputCallback& outputCallbackConcurrently() { return m_output.get(); }
    WebCodecsErrorCallback& errorCallbackConcurrently() { return m_error.get(); }

private:
    WebCodecsVideoEncoder(ScriptExecutionContext&, Init&&);
    size_t maximumCodecOperationsEnqueued() const final { return 4; }

    // ActiveDOMObject.
    void stop() final;
    void suspend(ReasonForSuspension) final;

    // EventTarget
    enum EventTargetInterfaceType eventTargetInterface() const final { return EventTargetInterfaceType::WebCodecsVideoEncoder; }

    ExceptionOr<void> closeEncoder(Exception&&);
    ExceptionOr<void> resetEncoder(const Exception&);
    void setInternalEncoder(Ref<VideoEncoder>&&);

    WebCodecsEncodedVideoChunkMetadata createEncodedChunkMetadata(std::optional<unsigned>);
    void updateRates(const WebCodecsVideoEncoderConfig&);

    Ref<WebCodecsEncodedVideoChunkOutputCallback> m_output;
    Ref<WebCodecsErrorCallback> m_error;
    RefPtr<VideoEncoder> m_internalEncoder;
    Vector<Ref<DeferredPromise>> m_pendingFlushPromises;
    bool m_isKeyChunkRequired { false };
    WebCodecsVideoEncoderConfig m_baseConfiguration;
    VideoEncoder::ActiveConfiguration m_activeConfiguration;
    bool m_hasNewActiveConfiguration { false };
    size_t m_encoderCount { 0 };
};

}

#endif
