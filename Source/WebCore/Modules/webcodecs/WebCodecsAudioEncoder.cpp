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
#include "WebCodecsUtilities.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(WebCodecsAudioEncoder);

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

WebCodecsAudioEncoder::~WebCodecsAudioEncoder() = default;

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

    return AudioEncoder::Config { config.sampleRate, config.numberOfChannels, config.bitrate.value_or(0), WTFMove(opusConfig), WTFMove(isAacADTS), WTFMove(flacConfig) };
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
        queueControlMessageAndProcess({ *this, [this, config]() mutable {
            m_isMessageQueueBlocked = true;

            protectedScriptExecutionContext()->enqueueTaskWhenSettled(m_internalEncoder->flush(), TaskSource::MediaElement, [weakThis = ThreadSafeWeakPtr { *this }, config = WTFMove(config)] (auto&&) mutable {
                RefPtr protectedThis = weakThis.get();
                if (!protectedThis)
                    return;

                if (protectedThis->m_state == WebCodecsCodecState::Closed || !protectedThis->scriptExecutionContext())
                    return;

                protectedThis->m_isMessageQueueBlocked = false;
                protectedThis->processControlMessageQueue();
            });
        } });
    }

    bool isSupportedCodec = isSupportedEncoderCodec(config.codec);
    queueControlMessageAndProcess({ *this, [this, config = WTFMove(config), isSupportedCodec, identifier = scriptExecutionContext()->identifier()]() mutable {
        RefPtr context = scriptExecutionContext();

        m_isMessageQueueBlocked = true;
        if (!isSupportedCodec) {
            postTaskToCodec<WebCodecsAudioEncoder>(identifier, *this, [] (auto& encoder) {
                encoder.closeEncoder(Exception { ExceptionCode::NotSupportedError, "Codec is not supported"_s });
            });
            return;
        }

        auto encoderConfig = createAudioEncoderConfig(config);
        if (encoderConfig.hasException()) {
            postTaskToCodec<WebCodecsAudioEncoder>(identifier, *this, [message = encoderConfig.releaseException().message()] (auto& encoder) mutable {
                encoder.closeEncoder(Exception { ExceptionCode::NotSupportedError, WTFMove(message) });
            });
            return;
        }

        m_baseConfiguration = config;

        Ref createEncoderPromise = AudioEncoder::create(config.codec, encoderConfig.releaseReturnValue(), [identifier, weakThis = ThreadSafeWeakPtr { *this }] (auto&& configuration) {
            postTaskToCodec<WebCodecsAudioEncoder>(identifier, weakThis, [configuration = WTFMove(configuration)] (auto& encoder) mutable {
                encoder.m_activeConfiguration = WTFMove(configuration);
                encoder.m_hasNewActiveConfiguration = true;
            });
        }, [identifier, weakThis = ThreadSafeWeakPtr { *this }, encoderCount = ++m_encoderCount] (auto&& result) {
            postTaskToCodec<WebCodecsAudioEncoder>(identifier, weakThis, [result = WTFMove(result), encoderCount] (auto& encoder) mutable {
                if (encoder.m_state != WebCodecsCodecState::Configured || encoder.m_encoderCount != encoderCount)
                    return;

                RefPtr buffer = JSC::ArrayBuffer::create(result.data);
                auto chunk = WebCodecsEncodedAudioChunk::create(WebCodecsEncodedAudioChunk::Init {
                    result.isKeyFrame ? WebCodecsEncodedAudioChunkType::Key : WebCodecsEncodedAudioChunkType::Delta,
                    result.timestamp,
                    result.duration,
                    BufferSource { WTFMove(buffer) }
                });
                encoder.m_output->handleEvent(WTFMove(chunk), encoder.createEncodedChunkMetadata());
            });
        });

        context->enqueueTaskWhenSettled(WTFMove(createEncoderPromise), TaskSource::MediaElement, [weakThis = ThreadSafeWeakPtr { *this }] (auto&& result) mutable {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;

            if (!result) {
                protectedThis->closeEncoder(Exception { ExceptionCode::NotSupportedError, WTFMove(result.error()) });
                return;
            }
            protectedThis->setInternalEncoder(WTFMove(*result));
            protectedThis->m_isMessageQueueBlocked = false;
            protectedThis->m_hasNewActiveConfiguration = true;
            protectedThis->processControlMessageQueue();
        });
    } });
    return { };
}

WebCodecsEncodedAudioChunkMetadata WebCodecsAudioEncoder::createEncodedChunkMetadata()
{
    WebCodecsEncodedAudioChunkMetadata metadata;

    if (m_hasNewActiveConfiguration) {
        m_hasNewActiveConfiguration = false;
        // FIXME: Provide more accurate decoder configuration...
        auto baseConfigurationSampleRate = m_baseConfiguration.sampleRate;
        auto baseConfigurationNumberOfChannels = m_baseConfiguration.numberOfChannels;
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
    queueControlMessageAndProcess({ *this, [this, audioData = WTFMove(audioData), timestamp = frame->timestamp(), duration = frame->duration()]() mutable {
        --m_encodeQueueSize;
        scheduleDequeueEvent();

        protectedScriptExecutionContext()->enqueueTaskWhenSettled(m_internalEncoder->encode({ WTFMove(audioData), timestamp, duration }), TaskSource::MediaElement, [weakThis = ThreadSafeWeakPtr { *this }, pendingActivity = makePendingActivity(*this)] (auto&& result) {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis || !!result)
                return;

            if (auto context = protectedThis->protectedScriptExecutionContext())
                context->addConsoleMessage(MessageSource::JS, MessageLevel::Error, makeString("AudioEncoder encode failed: "_s, result.error()));
            protectedThis->closeEncoder(Exception { ExceptionCode::EncodingError, WTFMove(result.error()) });
        });
    } });
    return { };
}

void WebCodecsAudioEncoder::flush(Ref<DeferredPromise>&& promise)
{
    if (m_state != WebCodecsCodecState::Configured) {
        promise->reject(Exception { ExceptionCode::InvalidStateError, "AudioEncoder is not configured"_s });
        return;
    }

    m_pendingFlushPromises.append(WTFMove(promise));
    queueControlMessageAndProcess({ *this, [this, clearFlushPromiseCount = m_clearFlushPromiseCount]() mutable {
        protectedScriptExecutionContext()->enqueueTaskWhenSettled(m_internalEncoder->flush(), TaskSource::MediaElement, [weakThis = ThreadSafeWeakPtr { *this }, clearFlushPromiseCount, pendingActivity = makePendingActivity(*this)] (auto&&) {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis || clearFlushPromiseCount != protectedThis->m_clearFlushPromiseCount)
                return;

            protectedThis->m_pendingFlushPromises.takeFirst()->resolve();
        });
    } });
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

    auto createEncoderPromise = AudioEncoder::create(config.codec, encoderConfig.releaseReturnValue(), [](auto&&) { }, [](auto&&) { });
    context.enqueueTaskWhenSettled(WTFMove(createEncoderPromise), TaskSource::MediaElement, [config, promise = WTFMove(promise)](auto&& result) mutable {
        promise->template resolve<IDLDictionary<WebCodecsAudioEncoderSupport>>(WebCodecsAudioEncoderSupport { !!result, WTFMove(config) });
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

void WebCodecsAudioEncoder::queueControlMessageAndProcess(WebCodecsControlMessage<WebCodecsAudioEncoder>&& message)
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
}

void WebCodecsAudioEncoder::stop()
{
    m_state = WebCodecsCodecState::Closed;
    m_internalEncoder = nullptr;
    m_controlMessageQueue.clear();
    m_pendingFlushPromises.clear();
}

bool WebCodecsAudioEncoder::virtualHasPendingActivity() const
{
    return m_state == WebCodecsCodecState::Configured && (m_encodeQueueSize || m_isMessageQueueBlocked);
}

} // namespace WebCore

#endif // ENABLE(WEB_CODECS)
