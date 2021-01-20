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
#include "JSRTCRtpScriptTransformerContext.h"
#include "RTCRtpTransformableFrame.h"
#include "ReadableStream.h"
#include "ReadableStreamSource.h"
#include "WritableStream.h"
#include "WritableStreamSink.h"

namespace WebCore {

ExceptionOr<Ref<RTCRtpScriptTransformer>> RTCRtpScriptTransformer::create(ScriptExecutionContext& context)
{
    auto port = downcast<DedicatedWorkerGlobalScope>(context).takePendingRTCTransfomerMessagePort();
    if (!port)
        return Exception { TypeError, "No pending construction data for this RTCRtpScriptTransformer"_s };

    auto transformer = adoptRef(*new RTCRtpScriptTransformer(context, port.releaseNonNull()));
    transformer->suspendIfNeeded();
    return transformer;
}

RTCRtpScriptTransformer::RTCRtpScriptTransformer(ScriptExecutionContext& context, Ref<MessagePort>&& port)
    : ActiveDOMObject(&context)
    , m_port(WTFMove(port))
{
}

RTCRtpScriptTransformer::~RTCRtpScriptTransformer()
{
}

RefPtr<SimpleReadableStreamSource> RTCRtpScriptTransformer::startStreams(RTCRtpTransformBackend& backend)
{
    auto callback = WTFMove(m_callback);
    if (!callback)
        return nullptr;

    auto& context = downcast<WorkerGlobalScope>(*scriptExecutionContext());
    auto& globalObject = *JSC::jsCast<JSDOMGlobalObject*>(context.globalObject());

    auto& vm = globalObject.vm();
    JSC::JSLockHolder lock(vm);

    auto readableStreamSource = SimpleReadableStreamSource::create();
    auto readableStream = ReadableStream::create(globalObject, readableStreamSource.copyRef());
    if (readableStream.hasException())
        return nullptr;

    auto writableStream = WritableStream::create(globalObject, SimpleWritableStreamSink::create([backend = makeRef(backend)](auto& context, auto value) -> ExceptionOr<void> {
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
        backend->processTransformedFrame(rtcFrame.get());
        return { };
    }));
    if (writableStream.hasException())
        return nullptr;

    // Call start callback.
    JSC::MarkedArgumentBuffer args;
    args.append(toJSNewlyCreated(&globalObject, &globalObject, readableStream.releaseReturnValue()));
    args.append(toJSNewlyCreated(&globalObject, &globalObject, writableStream.releaseReturnValue()));
    args.append(toJSNewlyCreated(&globalObject, &globalObject, RTCRtpScriptTransformerContext::create(makeRef(backend))));

    NakedPtr<JSC::Exception> returnedException;
    callback->invokeCallback(JSC::jsUndefined(), args, JSCallbackData::CallbackType::Object, JSC::Identifier::fromString(vm, "start"), returnedException);
    // FIXME: Do something in case of exception? We should at least log errors.

    return readableStreamSource;
}

void RTCRtpScriptTransformer::start(Ref<RTCRtpTransformBackend>&& backend)
{
    auto readableStreamSource = startStreams(backend.get());
    if (!readableStreamSource)
        return;

    m_backend = WTFMove(backend);

    auto& context = downcast<WorkerGlobalScope>(*scriptExecutionContext());
    m_backend->setTransformableFrameCallback([readableStreamSource = makeWeakPtr(readableStreamSource.get()), isAudio = m_backend->mediaType() == RTCRtpTransformBackend::MediaType::Audio, thread = makeRef(context.thread())](auto&& frame) mutable {
        thread->runLoop().postTaskForMode([readableStreamSource, isAudio, frame = WTFMove(frame)](auto& context) mutable {
            if (!readableStreamSource)
                return;

            auto& globalObject = *JSC::jsCast<JSDOMGlobalObject*>(context.globalObject());
            auto& vm = globalObject.vm();
            JSC::JSLockHolder lock(vm);

            auto value = isAudio ? toJS(&globalObject, &globalObject, RTCEncodedAudioFrame::create(WTFMove(frame))) : toJS(&globalObject, &globalObject, RTCEncodedVideoFrame::create(WTFMove(frame)));
            readableStreamSource->enqueue(value);
        }, WorkerRunLoop::defaultMode());
    });
}

void RTCRtpScriptTransformer::clear()
{
    if (m_backend)
        m_backend->clearTransformableFrameCallback();
    m_backend = nullptr;
    stopPendingActivity();
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
