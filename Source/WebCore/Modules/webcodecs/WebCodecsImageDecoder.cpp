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

#include "config.h"
#include "WebCodecsImageDecoder.h"

#if ENABLE(WEB_CODECS)

#include "ContextDestructionObserverInlines.h"
#include "ImageDecoder.h"
#include "JSDOMConvertBoolean.h"
#include "JSDOMConvertDictionary.h"
#include "JSDOMPromiseDeferred.h"
#include "JSWebCodecsImageDecodeResult.h"
#include "MIMETypeRegistry.h"
#include "NativeImage.h"
#include "ReadableStreamToSharedBufferSink.h"
#include "ScriptExecutionContext.h"
#include "ScriptExecutionContextInlines.h"
#include "WebCodecsControlMessage.h"
#include "WebCodecsImageDecodeResult.h"
#include "WebCodecsVideoFrame.h"
#include <wtf/TZoneMallocInlines.h>

#if USE(CG)
#include "UTIRegistry.h"
#include "UTIUtilities.h"
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebCodecsImageDecoder);

Ref<WebCodecsImageDecoder> WebCodecsImageDecoder::create(ScriptExecutionContext& context, Init&& init)
{
    Ref decoder = adoptRef(*new WebCodecsImageDecoder(context, WTF::move(init)));
    decoder->suspendIfNeeded();
    return decoder;
}

WebCodecsImageDecoder::WebCodecsImageDecoder(ScriptExecutionContext& context, Init&& init)
    : WebCodecsControlMessageQueue(context)
    , m_completedPromise(makeUniqueRef<CompletedPromise>())
    , m_tracks(WebCodecsImageTrackList::create())
{
    RefPtr<SharedBuffer> buffer;

    // FIXME: Support SharedArrayBuffer.
    WTF::switchOn(init.data,
        [&](const Ref<JSC::ArrayBuffer>& data) {
            if (RefPtr buffer = SharedBuffer::create(data->span()))
                setInternalDecoderData(*buffer, init.type, true);
        },
        [&](const Ref<JSC::ArrayBufferView>& data) {
            if (RefPtr buffer = SharedBuffer::create(data->span()))
                setInternalDecoderData(*buffer, init.type, true);
        },
        [&](const Ref<ReadableStream>& stream) {
            sinkStreamToInternalDecoder(stream, init.type);
        }
    );
}

WebCodecsImageDecoder::~WebCodecsImageDecoder() = default;

void WebCodecsImageDecoder::sinkStreamToInternalDecoder(const Ref<ReadableStream>& stream, const String& type)
{
    m_sink = ReadableStreamToSharedBufferSink::create([weakThis = ThreadSafeWeakPtr { *this }, type](auto&& result) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        WTF::switchOn(WTF::move(result),
            [&](std::nullptr_t) {
                if (RefPtr buffer = protectedThis->m_bufferBuilder.copyBuffer())
                    protectedThis->setInternalDecoderData(*buffer, type, true);
            },
            [&](std::span<const uint8_t>&& chunk) {
                protectedThis->m_bufferBuilder.append(chunk);
                if (RefPtr buffer = protectedThis->m_bufferBuilder.copyBuffer())
                    protectedThis->setInternalDecoderData(*buffer, type, false);
            },
            [&](JSC::JSValue) {
                protectedThis->closeDecoder(Exception { ExceptionCode::AbortError, "ReadableStream cancelled"_s });
            },
            [&](Exception&& exception) {
                protectedThis->closeDecoder(WTF::move(exception));
            }
        );
    });
    protect(m_sink)->pipeFrom(stream);
}

void WebCodecsImageDecoder::setInternalDecoderData(FragmentedSharedBuffer& buffer, const String& type, bool allDataReceived)
{
    if (state() == WebCodecsCodecState::Closed)
        return;

    if (!m_internalDecoder)
        m_internalDecoder = ImageDecoder::create(buffer, type, AlphaOption::Premultiplied, GammaAndColorProfileOption::Applied);

    if (!m_internalDecoder)
        return;

    // Set all accumulated encoded data in m_internalDecoder.
    protect(m_internalDecoder)->setData(buffer, allDataReceived);
    if (!allDataReceived)
        return;

    // Now m_internalDecoder has received all data, finish configuring
    // the state of WebCodecsImageDecoder.
    establishTrackList();
    setState(WebCodecsCodecState::Configured);
    m_completedPromise->resolve();

    // Begin queuing the pending decode requests which were received while
    // the encoded data was being received.
    queuePendingDecodeRequests();
}

void WebCodecsImageDecoder::establishTrackList()
{
    ASSERT(m_internalDecoder);
    ASSERT(!m_tracks->isEstablished());

    // FIXME: Can images can have more than one track?
    auto frameCount = protect(m_internalDecoder)->frameCount();
    auto repetitionCount = protect(m_internalDecoder)->repetitionCount();
    Ref track = WebCodecsImageTrack::create(repetitionCount, frameCount, frameCount > 1, true);

    protect(m_tracks)->setTrackList({ WTF::move(track) });
}

WorkQueue& WebCodecsImageDecoder::queueSingleton()
{
    static NeverDestroyed<Ref<WorkQueue>> workQueue = WorkQueue::create("WebCodecsImageDecoder Work Queue"_s);
    return workQueue.get();
}

Ref<WebCodecsImageDecoder::DecodePromise> WebCodecsImageDecoder::createNativeImageAtIndex(size_t frameIndex)
{
    if (state() == WebCodecsCodecState::Closed)
        return DecodePromise::createAndReject();

    ASSERT(m_internalDecoder);
    return invokeAsync(WebCodecsImageDecoder::queueSingleton(), [decoder = m_internalDecoder, frameIndex] {
        if (auto result = decoder->createNativeImageAtIndex(frameIndex, SubsamplingLevel::Default, { }))
            return DecodePromise::createAndResolve(WTF::move(std::get<Ref<NativeImage>>(*result)));
        return DecodePromise::createAndReject();
    });
}

void WebCodecsImageDecoder::fulfillPendingDecodePromises(size_t frameIndex, RefPtr<NativeImage>&& nativeImage)
{
    if (state() == WebCodecsCodecState::Closed) {
        rejectPendingDecodePromises(frameIndex, Exception { ExceptionCode::DataError, "ImageDecoder is closed"_s });
        return;
    }

    if (!nativeImage) {
        rejectPendingDecodePromises(frameIndex, Exception { ExceptionCode::DataError, "Decoding error"_s });
        return;
    }

    ASSERT(m_internalDecoder);
    Ref internalDecoder = *m_internalDecoder;

    IntSize frameSize = internalDecoder->frameSizeAtIndex(frameIndex);

    WebCodecsVideoFrame::Init init {
        .duration = static_cast<uint64_t>(internalDecoder->frameDurationAtIndex(frameIndex).value()),
        .timestamp = std::nullopt,
        .alpha = WebCodecsAlphaOption::Keep,
        .visibleRect = std::nullopt,
        .displayWidth = static_cast<size_t>(frameSize.width()),
        .displayHeight = static_cast<size_t>(frameSize.height())
    };

    // FIXME: nativeImage should be rotated based on the frame ImageOrientation.
    auto videoFrame = WebCodecsVideoFrame::create(*protect(scriptExecutionContext()), nativeImage.releaseNonNull(), WTF::move(init));
    if (videoFrame.hasException()) {
        rejectPendingDecodePromises(frameIndex, videoFrame.releaseException());
        return;
    }

    auto decodeResult = WebCodecsImageDecodeResult { videoFrame.releaseReturnValue(), true };
    for (auto& promise : m_pendingDecodePromises.take(frameIndex + 1))
        promise->resolve<IDLDictionary<WebCodecsImageDecodeResult>>(decodeResult);
}

void WebCodecsImageDecoder::rejectPendingDecodePromises(size_t frameIndex, const Exception& exception)
{
    for (auto& promise : m_pendingDecodePromises.take(frameIndex + 1))
        promise->reject(exception);
}

void WebCodecsImageDecoder::queueDecodeRequest(std::optional<DecodeOptions>&& options)
{
    queueControlMessageAndProcess({ *this, [this, protectedThis = Ref { *this }, options = WTF::move(options)]() mutable {
        protect(scriptExecutionContext())->enqueueTaskWhenSettled(
            createNativeImageAtIndex(options->frameIndex),
            TaskSource::MediaElement,
            [weakThis = ThreadSafeWeakPtr { *this }, pendingActivity = makePendingActivity(*this), options = WTF::move(options)](auto&& result) {
                if (RefPtr protectedThis = weakThis.get()) {
                    if (result.has_value())
                        protectedThis->fulfillPendingDecodePromises(options->frameIndex, WTF::move(result.value()));
                    else
                        protectedThis->fulfillPendingDecodePromises(options->frameIndex, nullptr);
                }
        });
        return WebCodecsControlMessageOutcome::Processed;
    } });
}

void WebCodecsImageDecoder::queuePendingDecodeRequests()
{
    for (auto frameIndex : m_pendingDecodePromises.keys())
        queueDecodeRequest(DecodeOptions { frameIndex - 1, true });
}

void WebCodecsImageDecoder::decode(std::optional<DecodeOptions>&& options, Ref<DeferredPromise>&& promise)
{
    // If state is closed, return a Promise rejected.
    if (state() == WebCodecsCodecState::Closed) {
        promise->reject(Exception { ExceptionCode::InvalidStateError, "ImageDecoder is closed"_s });
        return;
    }

    // If options is undefined, assign a new ImageDecodeOptions to options.
    if (!options)
        options = DecodeOptions { };

    // Append promise to the pendingDecodePromises.
    auto addResult = m_pendingDecodePromises.ensure(options->frameIndex + 1, [] -> Vector<Ref<DeferredPromise>> {
        return { };
    });

    addResult.iterator->value.append(WTF::move(promise));

    if (!addResult.isNewEntry)
        return;

    // Wait for the tracks to be established.
    if (!m_tracks->isEstablished())
        return;

    queueDecodeRequest(WTF::move(options));
}

ExceptionOr<void> WebCodecsImageDecoder::resetDecoder(const Exception& exception)
{
    if (state() == WebCodecsCodecState::Closed)
        return Exception { ExceptionCode::InvalidStateError, "ImageDecoder is closed"_s };

    // Abort any active decoding operation.
    clearControlMessageQueue();

    // Reject pendding decoding promises with exception.
    auto pendingDecodePromises = std::exchange(m_pendingDecodePromises, { });
    for (auto& framePromises : pendingDecodePromises.values()) {
        for (auto& promise : framePromises)
            promise->reject(exception);
    }

    return { };
}

ExceptionOr<void> WebCodecsImageDecoder::closeDecoder(const Exception& exception)
{
    // Run Reset ImageDecoder algorithm with exception
    auto result = resetDecoder(exception);
    if (result.hasException())
        return result;

    // Set state to closed.
    setState(WebCodecsCodecState::Closed);

    // Clear the internal decoder and release associated system resources.
    m_internalDecoder = nullptr;
    m_sink = nullptr;

    // Clear the ImageTrackList;
    protect(m_tracks)->clearTrackList(exception);

    // If completed is not resolved, reject it with exception.
    if (!m_completedPromise->isFulfilled())
        m_completedPromise->reject(exception);
    return { };
}

ExceptionOr<void> WebCodecsImageDecoder::reset()
{
    return resetDecoder(Exception { ExceptionCode::AbortError, "Reset called"_s });
}

ExceptionOr<void> WebCodecsImageDecoder::close()
{
    return closeDecoder(Exception { ExceptionCode::AbortError, "Close called"_s });
}

void WebCodecsImageDecoder::isTypeSupported(ScriptExecutionContext&, String&& mimeType, DOMPromiseDeferred<IDLBoolean>&& promise)
{
#if USE(CG)
    bool isTypeSupported = isSupportedImageType(UTIFromMIMEType(mimeType));
#else
    bool isTypeSupported = MIMETypeRegistry::isSupportedImageMIMEType(mimeType);
#endif

    if (isTypeSupported)
        promise.resolve(true);
    else
        promise.reject(Exception { ExceptionCode::TypeError, "Image type is not supported"_s });
}

void WebCodecsImageDecoder::suspend(ReasonForSuspension)
{
}

void WebCodecsImageDecoder::stop()
{
    setState(WebCodecsCodecState::Closed);
    m_internalDecoder = nullptr;
    m_sink = nullptr;
    clearControlMessageQueue();
    m_pendingDecodePromises.clear();
}

} // namespace WebCore

#endif // ENABLE(WEB_CODECS)
