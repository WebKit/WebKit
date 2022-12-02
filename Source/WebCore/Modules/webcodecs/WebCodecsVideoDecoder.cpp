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

#include "config.h"
#include "WebCodecsVideoDecoder.h"

#if ENABLE(WEB_CODECS)

#include "DOMException.h"
#include "Event.h"
#include "EventNames.h"
#include "JSWebCodecsVideoDecoderSupport.h"
#include "ScriptExecutionContext.h"
#include "WebCodecsEncodedVideoChunk.h"
#include "WebCodecsErrorCallback.h"
#include "WebCodecsVideoFrame.h"
#include "WebCodecsVideoFrameOutputCallback.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(WebCodecsVideoDecoder);

Ref<WebCodecsVideoDecoder> WebCodecsVideoDecoder::create(ScriptExecutionContext& context, Init&& init)
{
    auto decoder = adoptRef(*new WebCodecsVideoDecoder(context, WTFMove(init)));
    decoder->suspendIfNeeded();
    return decoder;
}

WebCodecsVideoDecoder::WebCodecsVideoDecoder(ScriptExecutionContext& context, Init&& init)
    : ActiveDOMObject(&context)
    , m_output(init.output.releaseNonNull())
    , m_error(init.error.releaseNonNull())
{
}

WebCodecsVideoDecoder::~WebCodecsVideoDecoder()
{
}

static bool isValidDecoderConfig(const WebCodecsVideoDecoderConfig& config)
{
    // FIXME: Check codec more accurately.
    if (!config.codec.startsWith("vp8"_s) && !config.codec.startsWith("vp09."_s) && !config.codec.startsWith("avc1."_s))
        return false;

    if (!!config.codedWidth != !!config.codedHeight)
        return false;
    if (config.codedWidth && !*config.codedWidth)
        return false;
    if (config.codedHeight && !*config.codedHeight)
        return false;

    if (!!config.displayAspectWidth != !!config.displayAspectHeight)
        return false;
    if (config.displayAspectWidth && !*config.displayAspectWidth)
        return false;
    if (config.displayAspectHeight && !*config.displayAspectHeight)
        return false;

    return true;
}

static VideoDecoder::Config createVideoDecoderConfig(const WebCodecsVideoDecoderConfig& config)
{
    Span<const uint8_t> description;
    if (config.description) {
        auto* data = std::visit([](auto& buffer) -> const uint8_t* {
            return buffer ? static_cast<const uint8_t*>(buffer->data()) : nullptr;
        }, *config.description);
        auto length = std::visit([](auto& buffer) -> size_t {
            return buffer ? buffer->byteLength() : 0;
        }, *config.description);
        if (length)
            description = { data, length };
    }

    return {
        description,
        config.codedWidth.value_or(0),
        config.codedHeight.value_or(0)
    };
}

ExceptionOr<void> WebCodecsVideoDecoder::configure(WebCodecsVideoDecoderConfig&& config)
{
    if (!isValidDecoderConfig(config))
        return Exception { TypeError, "Config is not valid"_s };

    if (m_state == WebCodecsCodecState::Closed || !scriptExecutionContext())
        return Exception { InvalidStateError, "VideoDecoder is closed"_s };

    m_state = WebCodecsCodecState::Configured;
    m_isKeyChunkRequired = true;

    queueControlMessageAndProcess([this, config = WTFMove(config), identifier = scriptExecutionContext()->identifier()]() mutable {
        m_isMessageQueueBlocked = true;
        VideoDecoder::PostTaskCallback postTaskCallback;
        if (isMainThread()) {
            postTaskCallback = [](auto&& task) {
                ensureOnMainThread(WTFMove(task));
            };
        } else {
            postTaskCallback = [identifier](auto&& task) {
                ScriptExecutionContext::postTaskTo(identifier, [task = WTFMove(task)](auto&) mutable {
                    task();
                });
            };
        }

        VideoDecoder::create(config.codec, createVideoDecoderConfig(config), [this, weakedThis = WeakPtr { *this }](auto&& result) {
            if (!weakedThis)
                return;

            if (!result.has_value()) {
                closeDecoder(Exception { NotSupportedError, WTFMove(result.error()) });
                return;
            }
            setInternalDecoder(WTFMove(result.value()));
            m_isMessageQueueBlocked = false;
            processControlMessageQueue();
        }, [this, weakedThis = WeakPtr { *this }](auto&& result) {
            if (!weakedThis || m_state != WebCodecsCodecState::Configured)
                return;

            if (!result.has_value()) {
                closeDecoder(Exception { EncodingError, WTFMove(result).error() });
                return;
            }

            auto decodedResult = WTFMove(result).value();
            WebCodecsVideoFrame::BufferInit init;
            init.codedWidth = decodedResult.frame->presentationSize().width();
            init.codedHeight = decodedResult.frame->presentationSize().height();
            init.timestamp = decodedResult.timestamp;
            init.duration = decodedResult.duration;
            init.colorSpace = decodedResult.frame->colorSpace();

            auto videoFrame = WebCodecsVideoFrame::create(WTFMove(decodedResult.frame), WTFMove(init));
            m_output->handleEvent(WTFMove(videoFrame));
        }, WTFMove(postTaskCallback));
    });
    return { };
}

ExceptionOr<void> WebCodecsVideoDecoder::decode(Ref<WebCodecsEncodedVideoChunk>&& chunk)
{
    if (m_state != WebCodecsCodecState::Configured)
        return Exception { InvalidStateError, "VideoDecoder is not configured"_s };

    if (m_isKeyChunkRequired) {
        if (chunk->type() != WebCodecsEncodedVideoChunkType::Key)
            return Exception { DataError, "Key frame is required"_s };
        m_isKeyChunkRequired = false;
    }

    ++m_decodeQueueSize;
    queueControlMessageAndProcess([this, chunk = WTFMove(chunk)]() mutable {
        ++m_beingDecodedQueueSize;
        --m_decodeQueueSize;
        scheduleDequeueEvent();

        m_internalDecoder->decode({ { chunk->data(), chunk->byteLength() }, chunk->type() == WebCodecsEncodedVideoChunkType::Key, chunk->timestamp(), chunk->duration() }, [this, weakedThis = WeakPtr { *this }](auto&& result) {
            if (!weakedThis)
                return;

            --m_beingDecodedQueueSize;
            if (!result.isNull()) {
                closeDecoder(Exception { EncodingError, WTFMove(result) });
                return;
            }
        });
    });
    return { };
}

ExceptionOr<void> WebCodecsVideoDecoder::flush(Ref<DeferredPromise>&& promise)
{
    if (m_state != WebCodecsCodecState::Configured)
        return Exception { InvalidStateError, "VideoDecoder is not configured"_s };

    m_isKeyChunkRequired = true;
    m_pendingFlushPromises.append(promise.copyRef());
    m_isFlushing = true;
    queueControlMessageAndProcess([this, clearFlushPromiseCount = m_clearFlushPromiseCount]() mutable {
        m_internalDecoder->flush([this, weakThis = WeakPtr { *this }, clearFlushPromiseCount] {
            if (!weakThis || clearFlushPromiseCount != m_clearFlushPromiseCount)
                return;

            m_pendingFlushPromises.takeFirst()->resolve();
            m_isFlushing = !m_pendingFlushPromises.isEmpty();
        });
    });
    return { };
}

ExceptionOr<void> WebCodecsVideoDecoder::reset()
{
    return resetDecoder(Exception { AbortError, "Reset called"_s });
}

ExceptionOr<void> WebCodecsVideoDecoder::close()
{
    return closeDecoder(Exception { AbortError, "Close called"_s });
}

void WebCodecsVideoDecoder::isConfigSupported(ScriptExecutionContext& context, WebCodecsVideoDecoderConfig&& config, Ref<DeferredPromise>&& promise)
{
    if (!isValidDecoderConfig(config)) {
        promise->reject(Exception { TypeError, "Config is not valid"_s });
        return;
    }

    auto* promisePtr = promise.ptr();
    context.addDeferredPromise(WTFMove(promise));

    auto videoDecoderConfig = createVideoDecoderConfig(config);
    Vector<uint8_t> description { videoDecoderConfig.description };
    VideoDecoder::create(config.codec, createVideoDecoderConfig(config), [identifier = context.identifier(), config = config.isolatedCopyWithoutDescription(), description = WTFMove(description), promisePtr](auto&& result) mutable {
        ScriptExecutionContext::postTaskTo(identifier, [success = result.has_value(), config = WTFMove(config).isolatedCopyWithoutDescription(), description = WTFMove(description), promisePtr](auto& context) mutable {
            if (auto promise = context.takeDeferredPromise(promisePtr)) {
                if (description.size())
                    config.description = RefPtr { JSC::ArrayBuffer::create(description.data(), description.size()) };
                promise->template resolve<IDLDictionary<WebCodecsVideoDecoderSupport>>(WebCodecsVideoDecoderSupport { success, WTFMove(config) });
            }
        });
    }, [](auto&&) {
    }, [] (auto&& task) {
        task();
    });
}

ExceptionOr<void> WebCodecsVideoDecoder::closeDecoder(Exception&& exception)
{
    auto result = resetDecoder(exception);
    if (result.hasException())
        return result;
    m_state = WebCodecsCodecState::Closed;
    m_internalDecoder = nullptr;
    if (exception.code() != AbortError) {
        queueTaskKeepingObjectAlive(*this, TaskSource::MediaElement, [this, exception = WTFMove(exception)]() mutable {
            m_error->handleEvent(DOMException::create(WTFMove(exception)));
        });
    }
    return { };
}

ExceptionOr<void> WebCodecsVideoDecoder::resetDecoder(const Exception&)
{
    if (m_state == WebCodecsCodecState::Closed)
        return Exception { InvalidStateError, "VideoDecoder is closed"_s };

    m_state = WebCodecsCodecState::Unconfigured;
    if (m_internalDecoder)
        m_internalDecoder->reset();
    m_controlMessageQueue.clear();
    if (m_decodeQueueSize) {
        m_decodeQueueSize = 0;
        scheduleDequeueEvent();
    }
    ++m_clearFlushPromiseCount;
    // FIXME: Check whether aligning with the spec or with WPT tests.
    while (!m_pendingFlushPromises.isEmpty())
        m_pendingFlushPromises.takeFirst()->reject(Exception { AbortError, "aborting flush as decoder is reset"_s });

    return { };
}

void WebCodecsVideoDecoder::scheduleDequeueEvent()
{
    if (m_dequeueEventScheduled)
        return;

    m_dequeueEventScheduled = true;
    queueTaskKeepingObjectAlive(*this, TaskSource::MediaElement, [this]() mutable {
        dispatchEvent(Event::create(eventNames().dequeueEvent, Event::CanBubble::No, Event::IsCancelable::No));
        m_dequeueEventScheduled = false;
    });
}

void WebCodecsVideoDecoder::setInternalDecoder(UniqueRef<VideoDecoder>&& internalDecoder)
{
    m_internalDecoder = internalDecoder.moveToUniquePtr();
}

void WebCodecsVideoDecoder::queueControlMessageAndProcess(Function<void()>&& message)
{
    if (m_isMessageQueueBlocked) {
        m_controlMessageQueue.append(WTFMove(message));
        return;
    }
    if (m_controlMessageQueue.isEmpty()) {
        message();
        return;
    }

    m_controlMessageQueue.append(WTFMove(message));
    processControlMessageQueue();
}

void WebCodecsVideoDecoder::processControlMessageQueue()
{
    while (!m_isMessageQueueBlocked && !m_controlMessageQueue.isEmpty())
        m_controlMessageQueue.takeFirst()();
}

void WebCore::WebCodecsVideoDecoder::suspend(ReasonForSuspension)
{
    // FIXME: Implement.
}

void WebCodecsVideoDecoder::stop()
{
    m_state = WebCodecsCodecState::Closed;
    m_internalDecoder = nullptr;
}

const char* WebCodecsVideoDecoder::activeDOMObjectName() const
{
    return "VideoDecoder";
}

bool WebCodecsVideoDecoder::virtualHasPendingActivity() const
{
    return m_state == WebCodecsCodecState::Configured && (m_decodeQueueSize || m_beingDecodedQueueSize || m_isFlushing);
}

} // namespace WebCore

#endif // ENABLE(WEB_CODECS)
