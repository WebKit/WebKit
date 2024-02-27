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

#include "ContextDestructionObserverInlines.h"
#include "DOMException.h"
#include "Event.h"
#include "EventNames.h"
#include "JSDOMPromiseDeferred.h"
#include "JSWebCodecsVideoDecoderSupport.h"
#include "ScriptExecutionContext.h"
#include "WebCodecsEncodedVideoChunk.h"
#include "WebCodecsErrorCallback.h"
#include "WebCodecsVideoFrame.h"
#include "WebCodecsVideoFrameOutputCallback.h"
#include <wtf/ASCIICType.h>
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

static bool isSupportedDecoderCodec(const String& codec, const Settings::Values& settings)
{
    return codec.startsWith("vp8"_s) || codec.startsWith("vp09.00"_s) || codec.startsWith("avc1."_s)
#if ENABLE(WEB_RTC)
        || (codec.startsWith("vp09.02"_s) && settings.webRTCVP9Profile2CodecEnabled)
#endif
        || (codec.startsWith("hev1."_s) && settings.webCodecsHEVCEnabled)
        || (codec.startsWith("hvc1."_s) && settings.webCodecsHEVCEnabled)
        || (codec.startsWith("av01.0"_s) && settings.webCodecsAV1Enabled);
}

static bool isValidDecoderConfig(const WebCodecsVideoDecoderConfig& config)
{
    if (StringView(config.codec).trim(isASCIIWhitespace<UChar>).isEmpty())
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
    std::span<const uint8_t> description;
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
        config.codedHeight.value_or(0),
        config.hardwareAcceleration == HardwareAcceleration::PreferSoftware ? VideoDecoder::HardwareAcceleration::No : VideoDecoder::HardwareAcceleration::Yes
    };
}

ExceptionOr<void> WebCodecsVideoDecoder::configure(ScriptExecutionContext& context, WebCodecsVideoDecoderConfig&& config)
{
    if (!isValidDecoderConfig(config))
        return Exception { ExceptionCode::TypeError, "Config is not valid"_s };

    if (m_state == WebCodecsCodecState::Closed || !scriptExecutionContext())
        return Exception { ExceptionCode::InvalidStateError, "VideoDecoder is closed"_s };

    m_state = WebCodecsCodecState::Configured;
    m_isKeyChunkRequired = true;

    bool isSupportedCodec = isSupportedDecoderCodec(config.codec, context.settingsValues());
    queueControlMessageAndProcess([this, config = WTFMove(config), isSupportedCodec, identifier = scriptExecutionContext()->identifier()]() mutable {
        m_isMessageQueueBlocked = true;
        VideoDecoder::PostTaskCallback postTaskCallback = [identifier, weakThis = ThreadSafeWeakPtr { *this }](auto&& task) {
            ScriptExecutionContext::postTaskTo(identifier, [weakThis, task = WTFMove(task)](auto&) mutable {
                RefPtr protectedThis = weakThis.get();
                if (!protectedThis)
                    return;
                protectedThis->queueTaskKeepingObjectAlive(*protectedThis, TaskSource::MediaElement, [task = WTFMove(task)]() mutable {
                    task();
                });
            });
        };

        if (!isSupportedCodec) {
            postTaskCallback([this] {
                closeDecoder(Exception { ExceptionCode::NotSupportedError, "Codec is not supported"_s });
            });
            return;
        }

        VideoDecoder::create(config.codec, createVideoDecoderConfig(config), [this](auto&& result) {
            if (!result.has_value()) {
                closeDecoder(Exception { ExceptionCode::NotSupportedError, WTFMove(result.error()) });
                return;
            }
            setInternalDecoder(WTFMove(result.value()));
            m_isMessageQueueBlocked = false;
            processControlMessageQueue();
        }, [this](auto&& result) {
            if (m_state != WebCodecsCodecState::Configured)
                return;

            if (!result.has_value()) {
                closeDecoder(Exception { ExceptionCode::EncodingError, WTFMove(result).error() });
                return;
            }

            auto decodedResult = WTFMove(result).value();
            WebCodecsVideoFrame::BufferInit init;
            init.codedWidth = decodedResult.frame->presentationSize().width();
            init.codedHeight = decodedResult.frame->presentationSize().height();
            init.timestamp = decodedResult.timestamp;
            init.duration = decodedResult.duration;
            init.colorSpace = decodedResult.frame->colorSpace();

            auto videoFrame = WebCodecsVideoFrame::create(*scriptExecutionContext(), WTFMove(decodedResult.frame), WTFMove(init));
            m_output->handleEvent(WTFMove(videoFrame));
        }, WTFMove(postTaskCallback));
    });
    return { };
}

ExceptionOr<void> WebCodecsVideoDecoder::decode(Ref<WebCodecsEncodedVideoChunk>&& chunk)
{
    if (m_state != WebCodecsCodecState::Configured)
        return Exception { ExceptionCode::InvalidStateError, "VideoDecoder is not configured"_s };

    if (m_isKeyChunkRequired) {
        if (chunk->type() != WebCodecsEncodedVideoChunkType::Key)
            return Exception { ExceptionCode::DataError, "Key frame is required"_s };
        m_isKeyChunkRequired = false;
    }

    ++m_decodeQueueSize;
    queueControlMessageAndProcess([this, chunk = WTFMove(chunk)]() mutable {
        ++m_beingDecodedQueueSize;
        --m_decodeQueueSize;
        scheduleDequeueEvent();

        m_internalDecoder->decode({ { chunk->data(), chunk->byteLength() }, chunk->type() == WebCodecsEncodedVideoChunkType::Key, chunk->timestamp(), chunk->duration() }, [this](auto&& result) {
            --m_beingDecodedQueueSize;
            if (!result.isNull())
                closeDecoder(Exception { ExceptionCode::EncodingError, WTFMove(result) });
        });
    });
    return { };
}

ExceptionOr<void> WebCodecsVideoDecoder::flush(Ref<DeferredPromise>&& promise)
{
    if (m_state != WebCodecsCodecState::Configured)
        return Exception { ExceptionCode::InvalidStateError, "VideoDecoder is not configured"_s };

    m_isKeyChunkRequired = true;
    m_pendingFlushPromises.append(promise.copyRef());
    m_isFlushing = true;
    queueControlMessageAndProcess([this, clearFlushPromiseCount = m_clearFlushPromiseCount] {
        m_internalDecoder->flush([this, clearFlushPromiseCount] {
            if (clearFlushPromiseCount != m_clearFlushPromiseCount)
                return;

            m_pendingFlushPromises.takeFirst()->resolve();
            m_isFlushing = !m_pendingFlushPromises.isEmpty();
        });
    });
    return { };
}

ExceptionOr<void> WebCodecsVideoDecoder::reset()
{
    return resetDecoder(Exception { ExceptionCode::AbortError, "Reset called"_s });
}

ExceptionOr<void> WebCodecsVideoDecoder::close()
{
    return closeDecoder(Exception { ExceptionCode::AbortError, "Close called"_s });
}

void WebCodecsVideoDecoder::isConfigSupported(ScriptExecutionContext& context, WebCodecsVideoDecoderConfig&& config, Ref<DeferredPromise>&& promise)
{
    if (!isValidDecoderConfig(config)) {
        promise->reject(Exception { ExceptionCode::TypeError, "Config is not valid"_s });
        return;
    }

    if (!isSupportedDecoderCodec(config.codec, context.settingsValues())) {
        promise->template resolve<IDLDictionary<WebCodecsVideoDecoderSupport>>(WebCodecsVideoDecoderSupport { false, WTFMove(config) });
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
    if (exception.code() != ExceptionCode::AbortError)
        m_error->handleEvent(DOMException::create(WTFMove(exception)));

    return { };
}

ExceptionOr<void> WebCodecsVideoDecoder::resetDecoder(const Exception& exception)
{
    if (m_state == WebCodecsCodecState::Closed)
        return Exception { ExceptionCode::InvalidStateError, "VideoDecoder is closed"_s };

    m_state = WebCodecsCodecState::Unconfigured;
    if (m_internalDecoder)
        m_internalDecoder->reset();
    m_controlMessageQueue.clear();
    if (m_decodeQueueSize) {
        m_decodeQueueSize = 0;
        scheduleDequeueEvent();
    }
    ++m_clearFlushPromiseCount;

    while (!m_pendingFlushPromises.isEmpty())
        m_pendingFlushPromises.takeFirst()->reject(exception);

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
