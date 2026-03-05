/*
 * Copyright (C) 2020-2025 Apple Inc. All rights reserved.
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

#include "ContextDestructionObserverInlines.h"
#include "DedicatedWorkerGlobalScope.h"
#include "EventLoop.h"
#include "JSDOMPromiseDeferred.h"
#include "MessageWithMessagePorts.h"
#include "RTCEncodedStreamProducer.h"
#include "ScriptExecutionContextInlines.h"
#include "WorkerThread.h"

namespace WebCore {

ExceptionOr<Ref<RTCRtpScriptTransformer>> RTCRtpScriptTransformer::create(ScriptExecutionContext& context, MessageWithMessagePorts&& options)
{
    if (!context.globalObject())
        return Exception { ExceptionCode::InvalidStateError };

    auto& globalObject = *JSC::jsCast<JSDOMGlobalObject*>(context.globalObject());
    JSC::JSLockHolder lock(globalObject.vm());

    auto producerOrException = RTCEncodedStreamProducer::create(context);
    if (producerOrException.hasException())
        return producerOrException.releaseException();
    if (!options.message)
        return Exception { ExceptionCode::InvalidStateError };

    auto ports = MessagePort::entanglePorts(context, WTF::move(options.transferredPorts));
    auto transformer = adoptRef(*new RTCRtpScriptTransformer(context, options.message.releaseNonNull(), WTF::move(ports), producerOrException.releaseReturnValue()));
    transformer->suspendIfNeeded();
    return transformer;
}

RTCRtpScriptTransformer::RTCRtpScriptTransformer(ScriptExecutionContext& context, Ref<SerializedScriptValue>&& options, Vector<Ref<MessagePort>>&& ports, Ref<RTCEncodedStreamProducer>&& streamProducer)
    : ActiveDOMObject(&context)
    , m_options(WTF::move(options))
    , m_ports(WTF::move(ports))
    , m_streamProducer(WTF::move(streamProducer))
{
}

RTCRtpScriptTransformer::~RTCRtpScriptTransformer() = default;

ReadableStream& RTCRtpScriptTransformer::readable()
{
    return m_streamProducer->readable();
}

WritableStream& RTCRtpScriptTransformer::writable()
{
    return m_streamProducer->writable();
}

void RTCRtpScriptTransformer::start(Ref<RTCRtpTransformBackend>&& backend)
{
    m_isSender = backend->side() == RTCRtpTransformBackend::Side::Sender;
    m_streamProducer->start(WTF::move(backend), backend->mediaType() == RTCRtpTransformBackend::MediaType::Video);
}

void RTCRtpScriptTransformer::clear(ClearCallback clearCallback)
{
    m_streamProducer->clear(clearCallback == ClearCallback::Yes);
    stopPendingActivity();
}

static std::optional<Exception> validateRid(const String& rid)
{
    if (rid.isNull())
        return { };

    if (rid.isEmpty())
        return Exception { ExceptionCode::NotAllowedError, "rid is empty"_s };

    constexpr unsigned maxRidLength = 255;
    if (rid.length() > maxRidLength)
        return Exception { ExceptionCode::NotAllowedError, "rid is too long"_s };

    auto foundBadCharacters = rid.find([](auto character) -> bool {
        return !isASCIIDigit(character) && !isASCIIAlpha(character);
    });
    if (foundBadCharacters != notFound)
        return Exception { ExceptionCode::NotAllowedError, "rid has a character that is not alpha numeric"_s };

    return { };
}

void RTCRtpScriptTransformer::generateKeyFrame(const String& rid, Ref<DeferredPromise>&& promise)
{
    RefPtr context = scriptExecutionContext();
    if (!context || !m_streamProducer->isVideo() || !m_isSender) {
        promise->reject(Exception { ExceptionCode::InvalidStateError, "Not attached to a valid video sender"_s });
        return;
    }

    if (auto exception = validateRid(rid)) {
        promise->reject(WTF::move(*exception));
        return;
    }

    m_streamProducer->generateKeyFrame(*context, rid, WTF::move(promise));
}

void RTCRtpScriptTransformer::sendKeyFrameRequest(Ref<DeferredPromise>&& promise)
{
    RefPtr context = scriptExecutionContext();
    if (!context || !m_streamProducer->isVideo() || m_isSender) {
        promise->reject(Exception { ExceptionCode::InvalidStateError, "Not attached to a valid video receiver"_s });
        return;
    }

    m_streamProducer->sendKeyFrameRequest();

    // FIXME: We should be able to know when the FIR request is sent to resolve the promise at this exact time.
    protect(context->eventLoop())->queueTask(TaskSource::Networking, [promise = WTF::move(promise)]() mutable {
        promise->resolve();
    });
}

JSC::JSValue RTCRtpScriptTransformer::options(JSC::JSGlobalObject& globalObject)
{
    return m_options->deserialize(globalObject, &globalObject, m_ports);
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
