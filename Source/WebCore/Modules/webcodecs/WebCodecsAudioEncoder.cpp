/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 * Copyright (C) 2023 Igalia S.L
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
#include "WebCodecsAudioEncoder.h"

#if ENABLE(WEB_CODECS)

#include "AacEncoderConfig.h"
#include "ContextDestructionObserverInlines.h"
#include "DOMException.h"
#include "Event.h"
#include "EventNames.h"
#include "FlacEncoderConfig.h"
#include "JSDOMPromiseDeferred.h"
#include "JSWebCodecsAudioEncoderSupport.h"
#include "Logging.h"
#include "OpusEncoderConfig.h"
#include "SecurityOrigin.h"
#include "WebCodecsAudioData.h"
#include "WebCodecsAudioEncoderConfig.h"
#include "WebCodecsEncodedAudioChunk.h"
#include "WebCodecsEncodedAudioChunkMetadata.h"
#include "WebCodecsEncodedAudioChunkOutputCallback.h"
#include "WebCodecsErrorCallback.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(WebCodecsAudioEncoder);

Ref<WebCodecsAudioEncoder> WebCodecsAudioEncoder::create(ScriptExecutionContext& context, Init&& init)
{
    auto encoder = adoptRef(*new WebCodecsAudioEncoder(context, WTFMove(init)));
    encoder->suspendIfNeeded();
    return encoder;
}


WebCodecsAudioEncoder::WebCodecsAudioEncoder(ScriptExecutionContext& context, Init&& init)
    : ActiveDOMObject(&context)
    , m_output(init.output.releaseNonNull())
    , m_error(init.error.releaseNonNull())
{
}

WebCodecsAudioEncoder::~WebCodecsAudioEncoder()
{
}

static bool isSupportedEncoderCodec(const StringView& codec)
{
    // FIXME: Check codec more accurately.
    bool isMPEG4AAC = codec == "mp4a.40.2"_s || codec == "mp4a.40.02"_s || codec == "mp4a.40.5"_s
        || codec == "mp4a.40.05"_s || codec == "mp4a.40.29"_s || codec == "mp4a.40.42"_s;
    bool isCodecAllowed = isMPEG4AAC || codec == "mp3"_s || codec == "opus"_s
        || codec == "alaw"_s || codec == "ulaw"_s || codec == "flac"_s
        || codec == "vorbis"_s || codec.startsWith("pcm-"_s);

    if (!isCodecAllowed)
        return false;

    return true;
}

static bool isValidEncoderConfig(const WebCodecsAudioEncoderConfig& config)
{
    if (StringView(config.codec).trim(isASCIIWhitespace<UChar>).isEmpty())
        return false;

    // FIXME: The opus and flac checks will probably need to move so that they trigger NotSupported
    // errors in the future.
    if (auto opusConfig = config.opus) {
        if (!opusConfig->isValid())
            return false;
    }

    if (auto flacConfig = config.flac) {
        if (!flacConfig->isValid())
            return false;
    }

    return true;
}

static ExceptionOr<AudioEncoder::Config> createAudioEncoderConfig(const WebCodecsAudioEncoderConfig& config)
{
    std::optional<AudioEncoder::OpusConfig> opusConfig = std::nullopt;
    if (config.opus)
        opusConfig = { config.opus->format == OpusBitstreamFormat::Ogg, config.opus->frameDuration, config.opus->complexity, config.opus->packetlossperc, config.opus->useinbandfec, config.opus->usedtx };

    std::optional<bool> isAacADTS = std::nullopt;
    if (config.aac)
        isAacADTS = config.aac->format == AacBitstreamFormat::Adts;

    std::optional<AudioEncoder::FlacConfig> flacConfig = std::nullopt;
    if (config.flac)
        flacConfig = { config.flac->blockSize, config.flac->compressLevel };

    std::optional<size_t> sampleRate = config.sampleRate;
    std::optional<size_t> numberOfChannels = config.numberOfChannels;
    return AudioEncoder::Config { WTFMove(sampleRate), WTFMove(numberOfChannels), config.bitrate.value_or(0), WTFMove(opusConfig), WTFMove(isAacADTS), WTFMove(flacConfig) };
}

ExceptionOr<void> WebCodecsAudioEncoder::configure(ScriptExecutionContext&, WebCodecsAudioEncoderConfig&& config)
{
    if (!isValidEncoderConfig(config))
        return Exception { ExceptionCode::TypeError, "Config is invalid"_s };

    if (m_state == WebCodecsCodecState::Closed || !scriptExecutionContext())
        return Exception { ExceptionCode::InvalidStateError, "AudioEncoder is closed"_s };

    m_state = WebCodecsCodecState::Configured;
    m_isKeyChunkRequired = true;

    if (m_internalEncoder) {
        queueControlMessageAndProcess([this, config]() mutable {
            m_isMessageQueueBlocked = true;
            m_internalEncoder->flush([weakThis = ThreadSafeWeakPtr { *this }, config = WTFMove(config)]() mutable {
                RefPtr protectedThis = weakThis.get();
                if (!protectedThis)
                    return;

                if (protectedThis->m_state == WebCodecsCodecState::Closed || !protectedThis->scriptExecutionContext())
                    return;

                protectedThis->m_isMessageQueueBlocked = false;
                protectedThis->processControlMessageQueue();
            });
        });
    }

    bool isSupportedCodec = isSupportedEncoderCodec(config.codec);
    queueControlMessageAndProcess([this, config = WTFMove(config), isSupportedCodec, identifier = scriptExecutionContext()->identifier()]() mutable {
        m_isMessageQueueBlocked = true;
        AudioEncoder::PostTaskCallback postTaskCallback = [weakThis = ThreadSafeWeakPtr { *this }, identifier](auto&& task) {
            ScriptExecutionContext::postTaskTo(identifier, [weakThis, task = WTFMove(task)](auto&) mutable {
                RefPtr protectedThis = weakThis.get();
                if (!protectedThis)
                    return;
                protectedThis->queueTaskKeepingObjectAlive(*protectedThis, TaskSource::MediaElement, WTFMove(task));
            });
        };

        if (!isSupportedCodec) {
            postTaskCallback([this] {
                closeEncoder(Exception { ExceptionCode::NotSupportedError, "Codec is not supported"_s });
            });
            return;
        }

        auto encoderConfig = createAudioEncoderConfig(config);
        if (encoderConfig.hasException()) {
            postTaskCallback([this, message = encoderConfig.releaseException().message()]() mutable {
                closeEncoder(Exception { ExceptionCode::NotSupportedError, WTFMove(message) });
            });
            return;
        }

        m_baseConfiguration = config;

        AudioEncoder::create(config.codec, encoderConfig.releaseReturnValue(), [this](auto&& result) {
            if (!result.has_value()) {
                closeEncoder(Exception { ExceptionCode::NotSupportedError, WTFMove(result.error()) });
                return;
            }
            setInternalEncoder(WTFMove(result.value()));
            m_isMessageQueueBlocked = false;
            m_hasNewActiveConfiguration = true;
            processControlMessageQueue();
        }, [this](auto&& configuration) {
            m_activeConfiguration = WTFMove(configuration);
            m_hasNewActiveConfiguration = true;
        }, [this](auto&& result) {
            if (m_state != WebCodecsCodecState::Configured)
                return;

            RefPtr<JSC::ArrayBuffer> buffer = JSC::ArrayBuffer::create(result.data.data(), result.data.size());
            auto chunk = WebCodecsEncodedAudioChunk::create(WebCodecsEncodedAudioChunk::Init {
                result.isKeyFrame ? WebCodecsEncodedAudioChunkType::Key : WebCodecsEncodedAudioChunkType::Delta,
                result.timestamp,
                result.duration,
                BufferSource { WTFMove(buffer) }
            });
            m_output->handleEvent(WTFMove(chunk), createEncodedChunkMetadata());
        }, WTFMove(postTaskCallback));
    });
    return { };
}

WebCodecsEncodedAudioChunkMetadata WebCodecsAudioEncoder::createEncodedChunkMetadata()
{
    WebCodecsEncodedAudioChunkMetadata metadata;

    if (m_hasNewActiveConfiguration) {
        m_hasNewActiveConfiguration = false;
        // FIXME: Provide more accurate decoder configuration...
        auto baseConfigurationSampleRate = m_baseConfiguration.sampleRate.value_or(0);
        auto baseConfigurationNumberOfChannels = m_baseConfiguration.numberOfChannels.value_or(0);
        metadata.decoderConfig = WebCodecsAudioDecoderConfig {
            !m_activeConfiguration.codec.isEmpty() ? WTFMove(m_activeConfiguration.codec) : String { m_baseConfiguration.codec },
            { },
            m_activeConfiguration.sampleRate.value_or(baseConfigurationSampleRate),
            m_activeConfiguration.numberOfChannels.value_or(baseConfigurationNumberOfChannels)
        };

        if (m_activeConfiguration.description && m_activeConfiguration.description->size()) {
            auto arrayBuffer = ArrayBuffer::tryCreateUninitialized(m_activeConfiguration.description->size(), 1);
            RELEASE_LOG_ERROR_IF(!!arrayBuffer, Media, "Cannot create array buffer for WebCodecs encoder description");
            if (arrayBuffer) {
                memcpy(static_cast<uint8_t*>(arrayBuffer->data()), m_activeConfiguration.description->data(), m_activeConfiguration.description->size());
                metadata.decoderConfig->description = WTFMove(arrayBuffer);
            }
        }
    }

    return metadata;
}

ExceptionOr<void> WebCodecsAudioEncoder::encode(Ref<WebCodecsAudioData>&& frame)
{
    auto audioData = frame->data().audioData;
    if (!audioData) {
        ASSERT(frame->isDetached());
        return Exception { ExceptionCode::TypeError, "AudioData is detached"_s };
    }
    ASSERT(!frame->isDetached());

    if (m_state != WebCodecsCodecState::Configured)
        return Exception { ExceptionCode::InvalidStateError, "AudioEncoder is not configured"_s };

    // FIXME: These checks are not yet spec-compliant. See also https://github.com/w3c/webcodecs/issues/716
    if (m_activeConfiguration.numberOfChannels && *m_activeConfiguration.numberOfChannels != audioData->numberOfChannels()) {
        frame->close();
        queueTaskKeepingObjectAlive(*this, TaskSource::MediaElement, [this]() mutable {
            m_error->handleEvent(DOMException::create(Exception { ExceptionCode::EncodingError, "Input audio buffer is incompatible with codec parameters"_s }));
        });
        return { };
    }
    if (m_activeConfiguration.sampleRate && *m_activeConfiguration.sampleRate != audioData->sampleRate()) {
        frame->close();
        queueTaskKeepingObjectAlive(*this, TaskSource::MediaElement, [this]() mutable {
            m_error->handleEvent(DOMException::create(Exception { ExceptionCode::EncodingError, "Input audio buffer is incompatible with codec parameters"_s }));
        });
        return { };
    }

    ++m_encodeQueueSize;
    queueControlMessageAndProcess([this, audioData = WTFMove(audioData), timestamp = frame->timestamp(), duration = frame->duration()]() mutable {
        ++m_beingEncodedQueueSize;
        --m_encodeQueueSize;
        scheduleDequeueEvent();
        m_internalEncoder->encode({ WTFMove(audioData), timestamp, duration }, [this, weakThis = ThreadSafeWeakPtr { *this }](auto&& result) {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;

            --m_beingEncodedQueueSize;
            if (!result.isNull()) {
                if (auto* context = scriptExecutionContext())
                    context->addConsoleMessage(MessageSource::JS, MessageLevel::Error, makeString("AudioEncoder encode failed: ", result));
                closeEncoder(Exception { ExceptionCode::EncodingError, WTFMove(result) });
                return;
            }
        });
    });
    return { };
}

void WebCodecsAudioEncoder::flush(Ref<DeferredPromise>&& promise)
{
    if (m_state != WebCodecsCodecState::Configured) {
        promise->reject(Exception { ExceptionCode::InvalidStateError, "AudioEncoder is not configured"_s });
        return;
    }

    m_pendingFlushPromises.append(WTFMove(promise));
    m_isFlushing = true;
    queueControlMessageAndProcess([this, clearFlushPromiseCount = m_clearFlushPromiseCount]() mutable {
        m_internalEncoder->flush([this, weakThis = ThreadSafeWeakPtr { *this }, clearFlushPromiseCount] {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis || clearFlushPromiseCount != m_clearFlushPromiseCount)
                return;

            m_pendingFlushPromises.takeFirst()->resolve();
            m_isFlushing = !m_pendingFlushPromises.isEmpty();
        });
    });
}

ExceptionOr<void> WebCodecsAudioEncoder::reset()
{
    return resetEncoder(Exception { ExceptionCode::AbortError, "Reset called"_s });
}

ExceptionOr<void> WebCodecsAudioEncoder::close()
{
    return closeEncoder(Exception { ExceptionCode::AbortError, "Close called"_s });
}

void WebCodecsAudioEncoder::isConfigSupported(ScriptExecutionContext& context, WebCodecsAudioEncoderConfig&& config, Ref<DeferredPromise>&& promise)
{
    if (!isValidEncoderConfig(config)) {
        promise->reject(Exception { ExceptionCode::TypeError, "Config is not valid"_s });
        return;
    }

    if (!isSupportedEncoderCodec(config.codec)) {
        promise->template resolve<IDLDictionary<WebCodecsAudioEncoderSupport>>(WebCodecsAudioEncoderSupport { false, WTFMove(config) });
        return;
    }

    auto encoderConfig = createAudioEncoderConfig(config);
    if (encoderConfig.hasException()) {
        promise->template resolve<IDLDictionary<WebCodecsAudioEncoderSupport>>(WebCodecsAudioEncoderSupport { false, WTFMove(config) });
        return;
    }

    auto* promisePtr = promise.ptr();
    context.addDeferredPromise(WTFMove(promise));

    AudioEncoder::create(config.codec, encoderConfig.releaseReturnValue(), [identifier = context.identifier(), config = config.isolatedCopy(), promisePtr](auto&& result) mutable {
        ScriptExecutionContext::postTaskTo(identifier, [success = result.has_value(), config = WTFMove(config).isolatedCopy(), promisePtr](auto& context) mutable {
            if (auto promise = context.takeDeferredPromise(promisePtr))
                promise->template resolve<IDLDictionary<WebCodecsAudioEncoderSupport>>(WebCodecsAudioEncoderSupport { success, WTFMove(config) });
        });
    }, [](auto&&) {
    }, [](auto&&) {
    }, [](auto&& task) {
        task();
    });
}

ExceptionOr<void> WebCodecsAudioEncoder::closeEncoder(Exception&& exception)
{
    auto result = resetEncoder(exception);
    if (result.hasException())
        return result;
    m_state = WebCodecsCodecState::Closed;
    m_internalEncoder = nullptr;
    if (exception.code() != ExceptionCode::AbortError)
        m_error->handleEvent(DOMException::create(WTFMove(exception)));

    return { };
}

ExceptionOr<void> WebCodecsAudioEncoder::resetEncoder(const Exception& exception)
{
    if (m_state == WebCodecsCodecState::Closed)
        return Exception { ExceptionCode::InvalidStateError, "AudioEncoder is closed"_s };

    m_state = WebCodecsCodecState::Unconfigured;
    if (m_internalEncoder)
        m_internalEncoder->reset();
    m_controlMessageQueue.clear();
    if (m_encodeQueueSize) {
        m_encodeQueueSize = 0;
        scheduleDequeueEvent();
    }
    ++m_clearFlushPromiseCount;
    while (!m_pendingFlushPromises.isEmpty())
        m_pendingFlushPromises.takeFirst()->reject(exception);

    return { };
}

void WebCodecsAudioEncoder::scheduleDequeueEvent()
{
    if (m_dequeueEventScheduled)
        return;

    m_dequeueEventScheduled = true;
    queueTaskKeepingObjectAlive(*this, TaskSource::MediaElement, [this]() mutable {
        dispatchEvent(Event::create(eventNames().dequeueEvent, Event::CanBubble::No, Event::IsCancelable::No));
        m_dequeueEventScheduled = false;
    });
}

void WebCodecsAudioEncoder::setInternalEncoder(UniqueRef<AudioEncoder>&& internalEncoder)
{
    m_internalEncoder = internalEncoder.moveToUniquePtr();
}

void WebCodecsAudioEncoder::queueControlMessageAndProcess(Function<void()>&& message)
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

void WebCodecsAudioEncoder::processControlMessageQueue()
{
    while (!m_isMessageQueueBlocked && !m_controlMessageQueue.isEmpty())
        m_controlMessageQueue.takeFirst()();
}

void WebCore::WebCodecsAudioEncoder::suspend(ReasonForSuspension)
{
    // FIXME: Implement.
}

void WebCodecsAudioEncoder::stop()
{
    // FIXME: Implement.
}

const char* WebCodecsAudioEncoder::activeDOMObjectName() const
{
    return "AudioEncoder";
}

bool WebCodecsAudioEncoder::virtualHasPendingActivity() const
{
    return m_state == WebCodecsCodecState::Configured && (m_encodeQueueSize || m_beingEncodedQueueSize || m_isFlushing);
}

} // namespace WebCore

#endif // ENABLE(WEB_CODECS)
