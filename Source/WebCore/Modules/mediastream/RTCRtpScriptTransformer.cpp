/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RTCRtpScriptTransformer.h"

#if ENABLE(WEB_RTC)

#include "DedicatedWorkerGlobalScope.h"
#include "JSRTCEncodedAudioFrame.h"
#include "JSRTCEncodedVideoFrame.h"
#include "MessageWithMessagePorts.h"
#include "RTCRtpTransformableFrame.h"
#include "ReadableStream.h"
#include "ReadableStreamSource.h"
#include "SerializedScriptValue.h"
#include "WritableStream.h"
#include "WritableStreamSink.h"

namespace WebCore {

ExceptionOr<Ref<RTCRtpScriptTransformer>> RTCRtpScriptTransformer::create(ScriptExecutionContext& context, MessageWithMessagePorts&& options)
{
    if (!context.globalObject())
        return Exception { InvalidStateError };

    auto& globalObject = *JSC::jsCast<JSDOMGlobalObject*>(context.globalObject());
    JSC::JSLockHolder lock(globalObject.vm());
    auto readableSource = SimpleReadableStreamSource::create();
    auto readable = ReadableStream::create(globalObject, readableSource.copyRef());
    if (readable.hasException())
        return readable.releaseException();
    if (!options.message)
        return Exception { InvalidStateError };

    auto ports = MessagePort::entanglePorts(context, WTFMove(options.transferredPorts));
    auto transformer = adoptRef(*new RTCRtpScriptTransformer(context, options.message.releaseNonNull(), WTFMove(ports), readable.releaseReturnValue(), WTFMove(readableSource)));
    transformer->suspendIfNeeded();
    return transformer;
}

RTCRtpScriptTransformer::RTCRtpScriptTransformer(ScriptExecutionContext& context, Ref<SerializedScriptValue>&& options, Vector<RefPtr<MessagePort>>&& ports, Ref<ReadableStream>&& readable, Ref<SimpleReadableStreamSource>&& readableSource)
    : ActiveDOMObject(&context)
    , m_options(WTFMove(options))
    , m_ports(WTFMove(ports))
    , m_readableSource(WTFMove(readableSource))
    , m_readable(WTFMove(readable))
{
}

RTCRtpScriptTransformer::~RTCRtpScriptTransformer()
{
}

ReadableStream& RTCRtpScriptTransformer::readable()
{
    return m_readable.get();
}

ExceptionOr<Ref<WritableStream>> RTCRtpScriptTransformer::writable()
{
    if (!m_writable) {
        auto* context = downcast<WorkerGlobalScope>(scriptExecutionContext());
        if (!context || !context->globalObject())
            return Exception { InvalidStateError };

        auto& globalObject = *JSC::jsCast<JSDOMGlobalObject*>(context->globalObject());
        auto writableOrException = WritableStream::create(globalObject, SimpleWritableStreamSink::create([transformer = makeRef(*this)](auto& context, auto value) -> ExceptionOr<void> {
            if (!transformer->m_backend)
                return Exception { InvalidStateError };

            auto& globalObject = *context.globalObject();

            auto scope = DECLARE_THROW_SCOPE(globalObject.vm());
            auto frame = convert<IDLUnion<IDLInterface<RTCEncodedAudioFrame>, IDLInterface<RTCEncodedVideoFrame>>>(globalObject, value);

            if (scope.exception())
                return Exception { ExistingExceptionError };

            auto rtcFrame = WTF::switchOn(frame, [&](RefPtr<RTCEncodedAudioFrame>& value) {
                return makeRef(value->rtcFrame());
            }, [&](RefPtr<RTCEncodedVideoFrame>& value) {
                return makeRef(value->rtcFrame());
            });

            // If no data, skip the frame since there is nothing to packetize or decode.
            if (rtcFrame->data().data())
                transformer->m_backend->processTransformedFrame(rtcFrame.get());
            return { };
        }));
        if (writableOrException.hasException())
            return writableOrException;
        m_writable = writableOrException.releaseReturnValue();
    }
    return makeRef(*m_writable);
}

void RTCRtpScriptTransformer::start(Ref<RTCRtpTransformBackend>&& backend)
{
    m_backend = WTFMove(backend);

    auto& context = downcast<WorkerGlobalScope>(*scriptExecutionContext());
    m_backend->setTransformableFrameCallback([readableSource = makeWeakPtr(m_readableSource.get()), isAudio = m_backend->mediaType() == RTCRtpTransformBackend::MediaType::Audio, thread = makeRef(context.thread())](auto&& frame) mutable {
        thread->runLoop().postTaskForMode([readableSource, isAudio, frame = WTFMove(frame)](auto& context) mutable {
            if (!readableSource)
                return;

            auto& globalObject = *JSC::jsCast<JSDOMGlobalObject*>(context.globalObject());
            auto& vm = globalObject.vm();
            JSC::JSLockHolder lock(vm);

            auto value = isAudio ? toJS(&globalObject, &globalObject, RTCEncodedAudioFrame::create(WTFMove(frame))) : toJS(&globalObject, &globalObject, RTCEncodedVideoFrame::create(WTFMove(frame)));
            readableSource->enqueue(value);
        }, WorkerRunLoop::defaultMode());
    });
}

void RTCRtpScriptTransformer::clear(ClearCallback clearCallback)
{
    if (m_backend && clearCallback == ClearCallback::Yes)
        m_backend->clearTransformableFrameCallback();
    m_backend = nullptr;
    stopPendingActivity();
}

ExceptionOr<void> RTCRtpScriptTransformer::requestKeyFrame()
{
    if (!m_backend)
        return Exception { InvalidStateError, "Not attached to a receiver or sender"_s };

    if (m_backend->mediaType() != RTCRtpTransformBackend::MediaType::Video)
        return Exception { InvalidStateError, "Cannot request key frame on audio sender"_s };

    m_backend->requestKeyFrame();
    return { };
}

JSC::JSValue RTCRtpScriptTransformer::options(JSC::JSGlobalObject& globalObject)
{
    return m_options->deserialize(globalObject, &globalObject, m_ports);
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
