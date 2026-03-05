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

#include "EventLoop.h"
#include "FrameRateMonitor.h"
#include "JSRTCEncodedAudioFrame.h"
#include "JSRTCEncodedVideoFrame.h"
#include "Logging.h"
#include "ReadableStreamSource.h"
#include "ScriptExecutionContextInlines.h"
#include "Settings.h"
#include "WritableStreamSink.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RTCEncodedStreamProducer);

RTCEncodedStreamProducer::~RTCEncodedStreamProducer() = default;

ExceptionOr<Ref<RTCEncodedStreamProducer>> RTCEncodedStreamProducer::create(ScriptExecutionContext& context)
{
    auto* globalObject = JSC::jsCast<JSDOMGlobalObject*>(context.globalObject());
    if (!globalObject)
        return Exception { ExceptionCode::InvalidStateError };

    Ref readableSource = SimpleReadableStreamSource::create();
    auto readable = ReadableStream::create(*globalObject, readableSource.copyRef());
    if (readable.hasException())
        return readable.releaseException();

    Ref producer = adoptRef(*new RTCEncodedStreamProducer(context, readable.releaseReturnValue(), WTF::move(readableSource)));

    if (auto exception = producer->initialize(*globalObject))
        return { WTF::move(*exception) };

    return producer;
}

RTCEncodedStreamProducer::RTCEncodedStreamProducer(ScriptExecutionContext& context, Ref<ReadableStream>&& readable, Ref<SimpleReadableStreamSource>&& readableSource)
    : m_context({ context })
    , m_contextIdentifier(context.identifier())
    , m_readable(WTF::move(readable))
    , m_readableSource(WTF::move(readableSource))
#if !RELEASE_LOG_DISABLED
    , m_enableAdditionalLogging(context.settingsValues().webRTCMediaPipelineAdditionalLoggingEnabled)
    , m_identifier(RTCEncodedStreamProducerIdentifier::generate())
#endif
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
    return { };
}

void RTCEncodedStreamProducer::start(Ref<RTCRtpTransformBackend>&& transformBackend, bool isVideo)
{
    transformBackend->setTransformableFrameCallback([weakThis = WeakPtr { *this }, contextIdentifer = m_contextIdentifier](Ref<RTCRtpTransformableFrame>&& frame) mutable {
        ScriptExecutionContext::postTaskTo(contextIdentifer, [weakThis, frame = WTF::move(frame)](auto&) mutable {
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->enqueueFrame(WTF::move(frame));
        });
    });
    m_transformBackend = WTF::move(transformBackend);
    m_isVideo = isVideo;
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

    if (m_isVideo && !m_pendingKeyFramePromises.isEmpty() && frame->isKeyFrame()) {
        // FIXME: We should take into account rids to resolve promises.
        for (Ref promise : std::exchange(m_pendingKeyFramePromises, { }))
            promise->resolve<IDLUnsignedLongLong>(frame->timestamp());
    }

#if !RELEASE_LOG_DISABLED
    if (m_enableAdditionalLogging && m_isVideo) {
        if (!m_readableFrameRateMonitor) {
            m_readableFrameRateMonitor = makeUnique<FrameRateMonitor>([identifier = m_identifier](auto info) {
                RELEASE_LOG(WebRTC, "RTCEncodedStreamProducer readable %" PRIu64 ", frame at %f, previous frame was at %f, observed frame rate is %f, delay since last frame is %f ms, frame count is %lu", identifier.toUInt64(), info.frameTime.secondsSinceEpoch().value(), info.lastFrameTime.secondsSinceEpoch().value(), info.observedFrameRate, ((info.frameTime - info.lastFrameTime) * 1000).value(), info.frameCount);
            });
        }
        m_readableFrameRateMonitor->update();
    }
#endif

    auto value = m_isVideo ? toJS(globalObject, globalObject, RTCEncodedVideoFrame::create(WTF::move(frame))) : toJS(globalObject, globalObject, RTCEncodedAudioFrame::create(WTF::move(frame)));

    m_readableSource->enqueue(value);
}

ExceptionOr<void> RTCEncodedStreamProducer::writeFrame(ScriptExecutionContext& context, JSC::JSValue value)
{
    RefPtr transformBackend = m_transformBackend;
    if (!transformBackend)
        return { };

    auto* globalObject = context.globalObject();
    if (!globalObject)
        return { };

    Ref vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto frameConversionResult = convert<IDLUnion<IDLInterface<RTCEncodedAudioFrame>, IDLInterface<RTCEncodedVideoFrame>>>(*globalObject, value);
    if (frameConversionResult.hasException(scope)) [[unlikely]]
        return Exception { ExceptionCode::ExistingExceptionError };

    auto frame = frameConversionResult.releaseReturnValue();
    auto rtcFrame = WTF::switchOn(frame,
        [&](Ref<RTCEncodedAudioFrame>& value) {
            return value->rtcFrame(vm);
        },
        [&](Ref<RTCEncodedVideoFrame>& value) {
            return value->rtcFrame(vm);
        }
    );

    // If no data, skip the frame since there is nothing to packetize or decode.
    if (rtcFrame->data().data())
        transformBackend->processTransformedFrame(rtcFrame.get());

    return { };
}

void RTCEncodedStreamProducer::generateKeyFrame(ScriptExecutionContext& context, const String& rid, Ref<DeferredPromise>&& promise)
{
    ASSERT(m_isVideo);

    RefPtr backend = m_transformBackend;
    if (!backend)
        return;

    if (!backend->requestKeyFrame(rid)) {
        protect(context.eventLoop())->queueTask(TaskSource::Networking, [promise = WTF::move(promise)]() mutable {
            promise->reject(Exception { ExceptionCode::NotFoundError, "rid was not found or is empty"_s });
        });
        return;
    }

    m_pendingKeyFramePromises.append(WTF::move(promise));
}

void RTCEncodedStreamProducer::sendKeyFrameRequest()
{
    ASSERT(m_isVideo);
    if (RefPtr backend = m_transformBackend)
        backend->requestKeyFrame({ });
}

void RTCEncodedStreamProducer::clear(bool shouldClearCallback)
{
    RefPtr backend = std::exchange(m_transformBackend, { });
    if (backend && shouldClearCallback)
        backend->clearTransformableFrameCallback();
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
