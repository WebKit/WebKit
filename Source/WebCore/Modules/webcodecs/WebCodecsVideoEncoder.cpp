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
#include "WebCodecsVideoEncoder.h"

#if ENABLE(WEB_CODECS)

#include "DOMException.h"
#include "Event.h"
#include "EventNames.h"
#include "JSWebCodecsVideoEncoderSupport.h"
#include "Logging.h"
#include "WebCodecsEncodedVideoChunk.h"
#include "WebCodecsEncodedVideoChunkMetadata.h"
#include "WebCodecsEncodedVideoChunkOutputCallback.h"
#include "WebCodecsErrorCallback.h"
#include "WebCodecsVideoEncoderConfig.h"
#include "WebCodecsVideoEncoderEncodeOptions.h"
#include "WebCodecsVideoFrame.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(WebCodecsVideoEncoder);

Ref<WebCodecsVideoEncoder> WebCodecsVideoEncoder::create(ScriptExecutionContext& context, Init&& init)
{
    auto encoder = adoptRef(*new WebCodecsVideoEncoder(context, WTFMove(init)));
    encoder->suspendIfNeeded();
    return encoder;
}


WebCodecsVideoEncoder::WebCodecsVideoEncoder(ScriptExecutionContext& context, Init&& init)
    : ActiveDOMObject(&context)
    , m_output(init.output.releaseNonNull())
    , m_error(init.error.releaseNonNull())
{
}

WebCodecsVideoEncoder::~WebCodecsVideoEncoder()
{
}

static bool isValidEncoderConfig(const WebCodecsVideoEncoderConfig& config, const Settings::Values& settings)
{
    // FIXME: Check codec more accurately.
    if (!config.codec.startsWith("vp8"_s) && !config.codec.startsWith("vp09."_s) && !config.codec.startsWith("avc1."_s) && !config.codec.startsWith("hev1."_s) && (!config.codec.startsWith("av01."_s) || !settings.webCodecsAV1Enabled))
        return false;

    if (!config.width || !config.height)
        return false;
    if (!config.displayWidth.value_or(config.width) || !config.displayHeight.value_or(config.height))
        return false;

    return true;
}

static VideoEncoder::Config createVideoEncoderConfig(const WebCodecsVideoEncoderConfig& config)
{
    bool useAnnexB = config.avc && config.avc->format == AvcBitstreamFormat::Annexb;
    return { config.width, config.height, useAnnexB, config.bitrate.value_or(0), config.framerate.value_or(0), config.latencyMode == LatencyMode::Realtime };
}

ExceptionOr<void> WebCodecsVideoEncoder::configure(ScriptExecutionContext& context, WebCodecsVideoEncoderConfig&& config)
{
    if (!isValidEncoderConfig(config, context.settingsValues()))
        return Exception { TypeError, "Config is invalid"_s };

    if (m_state == WebCodecsCodecState::Closed || !scriptExecutionContext())
        return Exception { InvalidStateError, "VideoEncoder is closed"_s };

    m_state = WebCodecsCodecState::Configured;
    m_isKeyChunkRequired = true;

    if (m_internalEncoder) {
        queueControlMessageAndProcess([this, config]() mutable {
            m_isMessageQueueBlocked = true;
            m_internalEncoder->flush([this, weakedThis = WeakPtr { *this }, config = WTFMove(config)]() mutable {
                if (!weakedThis)
                    return;

                if (m_state == WebCodecsCodecState::Closed || !scriptExecutionContext())
                    return;

                m_isMessageQueueBlocked = false;
                processControlMessageQueue();
            });
        });
    }

    queueControlMessageAndProcess([this, config = WTFMove(config), identifier = scriptExecutionContext()->identifier()]() mutable {
        m_isMessageQueueBlocked = true;
        m_baseConfiguration = config;

        VideoEncoder::PostTaskCallback postTaskCallback;
        if (isMainThread()) {
            postTaskCallback = [](auto&& task) {
                callOnMainThread(WTFMove(task));
            };
        } else {
            postTaskCallback = [identifier](auto&& task) {
                ScriptExecutionContext::postTaskTo(identifier, [task = WTFMove(task)](auto&) mutable {
                    task();
                });
            };
        }

        VideoEncoder::create(config.codec, createVideoEncoderConfig(config), [this, weakThis = WeakPtr { *this }](auto&& result) {
            if (!weakThis)
                return;

            if (!result.has_value()) {
                closeEncoder(Exception { NotSupportedError, WTFMove(result.error()) });
                return;
            }
            setInternalEncoder(WTFMove(result.value()));
            m_isMessageQueueBlocked = false;
            m_hasNewActiveConfiguration = true;
            processControlMessageQueue();
        }, [this, weakThis = WeakPtr { *this }](auto&& configuration) {
            if (!weakThis)
                return;

            m_activeConfiguration = WTFMove(configuration);
            m_hasNewActiveConfiguration = true;
        }, [this, weakThis = WeakPtr { *this }](auto&& result) {
            if (!weakThis || m_state != WebCodecsCodecState::Configured)
                return;

            RefPtr<JSC::ArrayBuffer> buffer = JSC::ArrayBuffer::create(result.data.data(), result.data.size());
            auto chunk = WebCodecsEncodedVideoChunk::create(WebCodecsEncodedVideoChunk::Init {
                result.isKeyFrame ? WebCodecsEncodedVideoChunkType::Key : WebCodecsEncodedVideoChunkType::Delta,
                result.timestamp,
                result.duration,
                BufferSource { WTFMove(buffer) }
            });
            m_output->handleEvent(WTFMove(chunk), createEncodedChunkMetadata());
        }, WTFMove(postTaskCallback));
    });
    return { };
}

WebCodecsEncodedVideoChunkMetadata WebCodecsVideoEncoder::createEncodedChunkMetadata()
{
    WebCodecsVideoDecoderConfig decoderConfig;
    if (!m_hasNewActiveConfiguration)
        return { };

    m_hasNewActiveConfiguration = false;

    // FIXME: Provide more accurate decoder configuration
    WebCodecsVideoDecoderConfig config {
        !m_activeConfiguration.codec.isEmpty() ? WTFMove(m_activeConfiguration.codec) : String { m_baseConfiguration.codec },
        { },
        m_activeConfiguration.visibleWidth ? m_activeConfiguration.visibleWidth : m_baseConfiguration.width,
        m_activeConfiguration.visibleHeight ? m_activeConfiguration.visibleHeight : m_baseConfiguration.height,
        m_activeConfiguration.displayWidth ? m_activeConfiguration.displayWidth : m_baseConfiguration.displayWidth,
        m_activeConfiguration.displayHeight ? m_activeConfiguration.displayHeight : m_baseConfiguration.displayHeight,
        m_activeConfiguration.colorSpace,
        HardwareAcceleration::NoPreference,
        { }
    };

    RefPtr<ArrayBuffer> arrayBuffer;
    if (m_activeConfiguration.description && m_activeConfiguration.description->size()) {
        auto arrayBuffer = ArrayBuffer::tryCreateUninitialized(m_activeConfiguration.description->size(), 1);
        RELEASE_LOG_ERROR_IF(!!arrayBuffer, Media, "Cannot create array buffer for WebCodecs encoder description");
        if (arrayBuffer) {
            memcpy(static_cast<uint8_t*>(arrayBuffer->data()), m_activeConfiguration.description->data(), m_activeConfiguration.description->size());
            config.description = WTFMove(arrayBuffer);
        }
    }

    return WebCodecsEncodedVideoChunkMetadata { WTFMove(config) };
}

ExceptionOr<void> WebCodecsVideoEncoder::encode(Ref<WebCodecsVideoFrame>&& frame, WebCodecsVideoEncoderEncodeOptions&& options)
{
    auto internalFrame = frame->internalFrame();
    if (!internalFrame) {
        ASSERT(frame->isDetached());
        return Exception { TypeError, "VideoFrame is detached"_s };
    }
    ASSERT(!frame->isDetached());

    if (m_state != WebCodecsCodecState::Configured)
        return Exception { InvalidStateError, "VideoEncoder is not configured"_s };

    ++m_encodeQueueSize;
    queueControlMessageAndProcess([this, internalFrame = internalFrame.releaseNonNull(), timestamp = frame->timestamp(), duration = frame->duration(), options = WTFMove(options)]() mutable {
        ++m_beingEncodedQueueSize;
        --m_encodeQueueSize;
        scheduleDequeueEvent();
        m_internalEncoder->encode({ WTFMove(internalFrame), timestamp, duration }, options.keyFrame, [this, weakedThis = WeakPtr { *this }](auto&& result) {
            if (!weakedThis)
                return;

            --m_beingEncodedQueueSize;
            if (!result.isNull()) {
                if (auto* context = scriptExecutionContext())
                    context->addConsoleMessage(MessageSource::JS, MessageLevel::Error, makeString("VideoEncoder encode failed: ", result));
                closeEncoder(Exception { EncodingError, WTFMove(result) });
                return;
            }
        });
    });
    return { };
}

void WebCodecsVideoEncoder::flush(Ref<DeferredPromise>&& promise)
{
    if (m_state != WebCodecsCodecState::Configured) {
        promise->reject(Exception { InvalidStateError, "VideoEncoder is not configured"_s });
        return;
    }

    m_pendingFlushPromises.append(promise.copyRef());
    m_isFlushing = true;
    queueControlMessageAndProcess([this, clearFlushPromiseCount = m_clearFlushPromiseCount]() mutable {
        m_internalEncoder->flush([this, weakThis = WeakPtr { *this }, clearFlushPromiseCount] {
            if (!weakThis || clearFlushPromiseCount != m_clearFlushPromiseCount)
                return;

            m_pendingFlushPromises.takeFirst()->resolve();
            m_isFlushing = !m_pendingFlushPromises.isEmpty();
        });
    });
}

ExceptionOr<void> WebCodecsVideoEncoder::reset()
{
    return resetEncoder(Exception { AbortError, "Reset called"_s });
}

ExceptionOr<void> WebCodecsVideoEncoder::close()
{
    return closeEncoder(Exception { AbortError, "Close called"_s });
}

void WebCodecsVideoEncoder::isConfigSupported(ScriptExecutionContext& context, WebCodecsVideoEncoderConfig&& config, Ref<DeferredPromise>&& promise)
{
    if (!isValidEncoderConfig(config, context.settingsValues())) {
        promise->reject(Exception { TypeError, "Config is not valid"_s });
        return;
    }
    if (config.alpha == WebCodecsAlphaOption::Keep) {
        promise->reject(Exception { NotSupportedError, "Alpha keep is not supported"_s });
        return;
    }

    auto* promisePtr = promise.ptr();
    context.addDeferredPromise(WTFMove(promise));

    VideoEncoder::create(config.codec, createVideoEncoderConfig(config), [identifier = context.identifier(), config = config.isolatedCopy(), promisePtr](auto&& result) mutable {
        ScriptExecutionContext::postTaskTo(identifier, [success = result.has_value(), config = WTFMove(config).isolatedCopy(), promisePtr](auto& context) mutable {
            if (auto promise = context.takeDeferredPromise(promisePtr))
                promise->template resolve<IDLDictionary<WebCodecsVideoEncoderSupport>>(WebCodecsVideoEncoderSupport { success, WTFMove(config) });
        });
    }, [](auto&&) {
    }, [](auto&&) {
    }, [] (auto&& task) {
        task();
    });
}

ExceptionOr<void> WebCodecsVideoEncoder::closeEncoder(Exception&& exception)
{
    auto result = resetEncoder(exception);
    if (result.hasException())
        return result;
    m_state = WebCodecsCodecState::Closed;
    m_internalEncoder = nullptr;
    if (exception.code() != AbortError) {
        queueTaskKeepingObjectAlive(*this, TaskSource::MediaElement, [this, exception = WTFMove(exception)]() mutable {
            m_error->handleEvent(DOMException::create(WTFMove(exception)));
        });
    }
    return { };
}

ExceptionOr<void> WebCodecsVideoEncoder::resetEncoder(const Exception& exception)
{
    if (m_state == WebCodecsCodecState::Closed)
        return Exception { InvalidStateError, "VideoEncoder is closed"_s };

    m_state = WebCodecsCodecState::Unconfigured;
    if (m_internalEncoder)
        m_internalEncoder->reset();
    m_controlMessageQueue.clear();
    if (m_encodeQueueSize) {
        m_encodeQueueSize = 0;
        scheduleDequeueEvent();
        queueTaskKeepingObjectAlive(*this, TaskSource::MediaElement, [this, exception = Exception { exception }]() mutable {
            m_error->handleEvent(DOMException::create(WTFMove(exception)));
        });
    }
    ++m_clearFlushPromiseCount;
    while (!m_pendingFlushPromises.isEmpty())
        m_pendingFlushPromises.takeFirst()->reject(exception);

    return { };
}

void WebCodecsVideoEncoder::scheduleDequeueEvent()
{
    if (m_dequeueEventScheduled)
        return;

    m_dequeueEventScheduled = true;
    queueTaskKeepingObjectAlive(*this, TaskSource::MediaElement, [this]() mutable {
        dispatchEvent(Event::create(eventNames().dequeueEvent, Event::CanBubble::No, Event::IsCancelable::No));
        m_dequeueEventScheduled = false;
    });
}

void WebCodecsVideoEncoder::setInternalEncoder(UniqueRef<VideoEncoder>&& internalEncoder)
{
    m_internalEncoder = internalEncoder.moveToUniquePtr();
}

void WebCodecsVideoEncoder::queueControlMessageAndProcess(Function<void()>&& message)
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

void WebCodecsVideoEncoder::processControlMessageQueue()
{
    while (!m_isMessageQueueBlocked && !m_controlMessageQueue.isEmpty())
        m_controlMessageQueue.takeFirst()();
}

void WebCore::WebCodecsVideoEncoder::suspend(ReasonForSuspension)
{
    // FIXME: Implement.
}

void WebCodecsVideoEncoder::stop()
{
    // FIXME: Implement.
}

const char* WebCodecsVideoEncoder::activeDOMObjectName() const
{
    return "VideoEncoder";
}

bool WebCodecsVideoEncoder::virtualHasPendingActivity() const
{
    return m_state == WebCodecsCodecState::Configured && (m_encodeQueueSize || m_beingEncodedQueueSize || m_isFlushing);
}

} // namespace WebCore

#endif // ENABLE(WEB_CODECS)
