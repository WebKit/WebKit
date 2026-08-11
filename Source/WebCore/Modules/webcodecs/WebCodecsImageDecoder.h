/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "DOMPromiseProxy.h"
#include "EventTargetInterfaces.h"
#include "ExceptionOr.h"
#include "IDLTypes.h"
#include "JSDOMPromiseDeferredForward.h"
#include "WebCodecsBase.h"
#include "WebCodecsImageTrackList.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <JavaScriptCore/ArrayBufferView.h>
#include <wtf/UniqueRef.h>

namespace WebCore {

class ReadableStream;
class WebCodecsVideoFrame;

using ImageBufferSource = Variant
    < Ref<JSC::ArrayBuffer>
    , Ref<JSC::ArrayBufferView>
    , Ref<ReadableStream>
>;

class WebCodecsImageDecoder final : public WebCodecsBase {
    WTF_MAKE_TZONE_ALLOCATED(WebCodecsImageDecoder);
public:
    ~WebCodecsImageDecoder();

    enum class ColorSpaceConversion : bool { None, Default };

    struct Init {
        String type;
        ImageBufferSource data;
        ColorSpaceConversion colorSpaceConversion { ColorSpaceConversion::Default };
        std::optional<size_t> desiredWidth;
        std::optional<size_t> desiredHeight;
        std::optional<bool> preferAnimation;
    };

    struct DecodeOptions {
        size_t frameIndex { 0 };
        bool completeFramesOnly { true };
    };

    static Ref<WebCodecsImageDecoder> create(ScriptExecutionContext&, Init&&);

    String type() const { return m_type; }
    bool complete() const { return true; }

    using CompletedPromise = DOMPromiseProxy<IDLUndefined>;
    CompletedPromise& completed() { return m_completedPromise.get(); }

    Ref<WebCodecsImageTrackList> tracks() const;

    ExceptionOr<void> decode(std::optional<DecodeOptions>&&, Ref<DeferredPromise>&&);
    ExceptionOr<void> reset();
    ExceptionOr<void> close();

    static void isTypeSupported(ScriptExecutionContext&, String&& type, Ref<DeferredPromise>&&);

private:
    WebCodecsImageDecoder(ScriptExecutionContext&, Init&&);

    // ActiveDOMObject.
    void stop() final;
    void NODELETE suspend(ReasonForSuspension) final;

    // EventTarget.
    enum EventTargetInterfaceType eventTargetInterface() const final { return EventTargetInterfaceType::WebCodecsImageDecoder; }

    String m_type;
    UniqueRef<CompletedPromise> m_completedPromise;
    mutable RefPtr<WebCodecsImageTrackList> m_tracks;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_EVENTTARGET(WebCodecsImageDecoder)

#endif
