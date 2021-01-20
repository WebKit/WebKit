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
#include "RTCRtpSFrameTransform.h"

#if ENABLE(WEB_RTC)

#include "CryptoKeyRaw.h"
#include "JSDOMConvertBufferSource.h"
#include "JSRTCEncodedAudioFrame.h"
#include "JSRTCEncodedVideoFrame.h"
#include "Logging.h"
#include "RTCEncodedAudioFrame.h"
#include "RTCEncodedVideoFrame.h"
#include "RTCRtpSFrameTransformer.h"
#include "RTCRtpTransformBackend.h"
#include "RTCRtpTransformableFrame.h"
#include "ReadableStream.h"
#include "ReadableStreamSource.h"
#include "SharedBuffer.h"
#include "WritableStream.h"
#include "WritableStreamSink.h"

namespace WebCore {

RTCRtpSFrameTransform::RTCRtpSFrameTransform(ScriptExecutionContext& context, Options options)
    : ContextDestructionObserver(&context)
    , m_transformer(RTCRtpSFrameTransformer::create(options.compatibilityMode))
{
    m_transformer->setIsEncrypting(options.role == Role::Encrypt);
    m_transformer->setAuthenticationSize(options.authenticationSize);
}

RTCRtpSFrameTransform::~RTCRtpSFrameTransform()
{
}

void RTCRtpSFrameTransform::setEncryptionKey(CryptoKey& key, Optional<uint64_t> keyId, DOMPromiseDeferred<void>&& promise)
{
    auto algorithm = key.algorithm();
    if (!WTF::holds_alternative<CryptoKeyAlgorithm>(algorithm)) {
        promise.reject(Exception { TypeError, "Invalid key"_s });
        return;
    }

    if (WTF::get<CryptoKeyAlgorithm>(algorithm).name != "HKDF") {
        promise.reject(Exception { TypeError, "Only HKDF is supported"_s });
        return;
    }

    auto& rawKey = downcast<CryptoKeyRaw>(key);
    promise.settle(m_transformer->setEncryptionKey(rawKey.key(), keyId));
}

uint64_t RTCRtpSFrameTransform::counterForTesting() const
{
    return m_transformer->counter();
}

uint64_t RTCRtpSFrameTransform::keyIdForTesting() const
{
    return m_transformer->keyId();
}

bool RTCRtpSFrameTransform::isAttached() const
{
    return m_isAttached || (m_readable && m_readable->isLocked()) || (m_writable && m_writable->isLocked());
}

void RTCRtpSFrameTransform::initializeTransformer(RTCRtpTransformBackend& backend, Side side)
{
    ASSERT(!isAttached());

    m_isAttached = true;
    if (m_readable)
        m_readable->lock();
    if (m_writable)
        m_writable->lock();

    m_transformer->setIsEncrypting(side == Side::Sender);
    m_transformer->setAuthenticationSize(backend.mediaType() == RTCRtpTransformBackend::MediaType::Audio ? 4 : 10);

    backend.setTransformableFrameCallback([transformer = m_transformer, backend = makeRef(backend)](auto&& frame) {
        auto chunk = frame->data();
        auto result = transformer->transform(chunk.data, chunk.size);

        if (result.hasException()) {
            RELEASE_LOG_ERROR(WebRTC, "RTCRtpSFrameTransform failed transforming a frame");
            return;
        }

        frame->setData({ result.returnValue().data(), result.returnValue().size() });

        backend->processTransformedFrame(frame.get());
    });
}

void RTCRtpSFrameTransform::initializeBackendForReceiver(RTCRtpTransformBackend& backend)
{
    initializeTransformer(backend, Side::Receiver);
}

void RTCRtpSFrameTransform::initializeBackendForSender(RTCRtpTransformBackend& backend)
{
    initializeTransformer(backend, Side::Sender);
}

void RTCRtpSFrameTransform::willClearBackend(RTCRtpTransformBackend& backend)
{
    backend.clearTransformableFrameCallback();
}

template<typename Frame>
void transformFrame(Frame& frame, JSDOMGlobalObject& globalObject, RTCRtpSFrameTransformer& transformer, SimpleReadableStreamSource& source)
{
    auto chunk = frame.rtcFrame().data();
    auto result = transformer.transform(chunk.data, chunk.size);
    RELEASE_LOG_ERROR_IF(result.hasException(), WebRTC, "RTCRtpSFrameTransform failed transforming a frame");

    RTCRtpTransformableFrame::Data transformedChunk;
    // In case of error, we just pass along the frame with empty data.
    if (!result.hasException())
        transformedChunk = { result.returnValue().data(), result.returnValue().size() };

    frame.rtcFrame().setData(transformedChunk);
    source.enqueue(toJS(&globalObject, &globalObject, frame));
}

template<>
void transformFrame(JSC::ArrayBuffer& value, JSDOMGlobalObject& globalObject, RTCRtpSFrameTransformer& transformer, SimpleReadableStreamSource& source)
{
    auto result = transformer.transform(static_cast<const uint8_t*>(value.data()), value.byteLength());
    RELEASE_LOG_ERROR_IF(result.hasException(), WebRTC, "RTCRtpSFrameTransform failed transforming a frame");

    auto buffer = result.hasException() ? SharedBuffer::create() : SharedBuffer::create(result.returnValue().data(), result.returnValue().size());
    source.enqueue(toJS(&globalObject, &globalObject, buffer->tryCreateArrayBuffer().get()));
}

void RTCRtpSFrameTransform::createStreams()
{
    auto& globalObject = *scriptExecutionContext()->globalObject();

    m_readableStreamSource = SimpleReadableStreamSource::create();
    auto readable = ReadableStream::create(globalObject, m_readableStreamSource.copyRef());
    if (readable.hasException())
        return;

    auto writable = WritableStream::create(globalObject, SimpleWritableStreamSink::create([transformer = m_transformer, readableStreamSource = m_readableStreamSource](auto& context, auto value) -> ExceptionOr<void> {
        auto& globalObject = *JSC::jsCast<JSDOMGlobalObject*>(context.globalObject());
        auto scope = DECLARE_THROW_SCOPE(globalObject.vm());

        auto frame = convert<IDLUnion<IDLArrayBuffer, IDLInterface<RTCEncodedAudioFrame>, IDLInterface<RTCEncodedVideoFrame>>>(globalObject, value);
        if (scope.exception())
            return Exception { ExistingExceptionError };

        // We do not want to throw any exception in the transform to make sure we do not error the transform.
        WTF::switchOn(frame, [&](RefPtr<RTCEncodedAudioFrame>& value) {
            transformFrame(*value, globalObject, transformer.get(), *readableStreamSource);
        }, [&](RefPtr<RTCEncodedVideoFrame>& value) {
            transformFrame(*value, globalObject, transformer.get(), *readableStreamSource);
        }, [&](RefPtr<ArrayBuffer>& value) {
            transformFrame(*value, globalObject, transformer.get(), *readableStreamSource);
        });
        return { };
    }));
    if (writable.hasException())
        return;

    m_readable = readable.releaseReturnValue();
    m_writable = writable.releaseReturnValue();
    if (m_isAttached) {
        m_readable->lock();
        m_writable->lock();
    }
}

ExceptionOr<RefPtr<ReadableStream>> RTCRtpSFrameTransform::readable()
{
    auto* context = scriptExecutionContext();
    if (!context)
        return Exception { InvalidStateError };

    if (!m_readable)
        createStreams();

    return m_readable.copyRef();
}

ExceptionOr<RefPtr<WritableStream>> RTCRtpSFrameTransform::writable()
{
    auto* context = scriptExecutionContext();
    if (!context)
        return Exception { InvalidStateError };

    if (!m_writable)
        createStreams();

    return m_writable.copyRef();
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
