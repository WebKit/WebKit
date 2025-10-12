/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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


#include "config.h"
#include "RTCEncodedStreamProducer.h"

#if ENABLE(WEB_RTC)

#include "JSRTCEncodedAudioFrame.h"
#include "JSRTCEncodedVideoFrame.h"
#include "ReadableStreamSource.h"
#include "WritableStreamSink.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(RTCEncodedStreamProducer);

RTCEncodedStreamProducer::~RTCEncodedStreamProducer() = default;

ExceptionOr<Ref<RTCEncodedStreamProducer>> RTCEncodedStreamProducer::create(ScriptExecutionContext& context, Ref<RTCRtpTransformBackend>&& transformBackend, bool isVideo)
{
    auto* globalObject = JSC::jsCast<JSDOMGlobalObject*>(context.globalObject());
    if (!globalObject)
        return Exception { ExceptionCode::InvalidStateError };

    Ref readableSource = SimpleReadableStreamSource::create();
    auto readable = ReadableStream::create(*globalObject, readableSource.copyRef());
    if (readable.hasException())
        return readable.releaseException();

    Ref producer = adoptRef(*new RTCEncodedStreamProducer(context, readable.releaseReturnValue(), WTFMove(readableSource), WTFMove(transformBackend), isVideo));

    if (auto exception = producer->initialize(*globalObject))
        return { WTFMove(*exception) };

    return producer;
}

RTCEncodedStreamProducer::RTCEncodedStreamProducer(ScriptExecutionContext& context, Ref<ReadableStream>&& readable, Ref<SimpleReadableStreamSource>&& readableSource, Ref<RTCRtpTransformBackend>&& transformBackend, bool isVideo)
    : m_context({ context })
    , m_readable(WTFMove(readable))
    , m_readableSource(WTFMove(readableSource))
    , m_transformBackend(WTFMove(transformBackend))
    , m_isVideo(isVideo)
{
}

std::optional<Exception> RTCEncodedStreamProducer::initialize(JSDOMGlobalObject& globalObject)
{
    auto writable = WritableStream::create(globalObject, SimpleWritableStreamSink::create([weakThis = WeakPtr { *this }](auto& context, auto value) -> ExceptionOr<void> {
        RefPtr protectedThis = weakThis.get();
        return protectedThis ? protectedThis->writeFrame(context, value) : Exception { ExceptionCode::InvalidStateError };
    }));
    if (writable.hasException())
        return writable.releaseException();

    lazyInitialize(m_writable, writable.releaseReturnValue());

    m_transformBackend->setTransformableFrameCallback([weakThis = WeakPtr { *this }](Ref<RTCRtpTransformableFrame>&& frame) mutable {
        callOnMainThread([weakThis, frame = WTFMove(frame)]() mutable {
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->enqueueFrame(WTFMove(frame));
        });
    });
    return { };
}

void RTCEncodedStreamProducer::enqueueFrame(Ref<RTCRtpTransformableFrame>&& frame)
{
    RefPtr context = m_context.get();
    if (!context)
        return;

    auto* globalObject = JSC::jsCast<JSDOMGlobalObject*>(context->globalObject());
    if (!globalObject)
        return;

    Ref vm = globalObject->vm();
    JSC::JSLockHolder lock(vm);

    auto value = m_isVideo ? toJS(globalObject, globalObject, RTCEncodedVideoFrame::create(WTFMove(frame))) : toJS(globalObject, globalObject, RTCEncodedAudioFrame::create(WTFMove(frame)));

    m_readableSource->enqueue(value);
}

ExceptionOr<void> RTCEncodedStreamProducer::writeFrame(ScriptExecutionContext& context, JSC::JSValue value)
{
    auto* globalObject = context.globalObject();
    if (!globalObject)
        return { };

    Ref vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto frameConversionResult = convert<IDLUnion<IDLInterface<RTCEncodedAudioFrame>, IDLInterface<RTCEncodedVideoFrame>>>(*globalObject, value);
    if (frameConversionResult.hasException(scope)) [[unlikely]]
        return Exception { ExceptionCode::ExistingExceptionError };

    auto frame = frameConversionResult.releaseReturnValue();
    auto rtcFrame = WTF::switchOn(frame, [&](RefPtr<RTCEncodedAudioFrame>& value) {
        return value->rtcFrame(vm);
    }, [&](RefPtr<RTCEncodedVideoFrame>& value) {
        return value->rtcFrame(vm);
    });

    // If no data, skip the frame since there is nothing to packetize or decode.
    if (rtcFrame->data().data())
        m_transformBackend->processTransformedFrame(rtcFrame.get());

    return { };
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
