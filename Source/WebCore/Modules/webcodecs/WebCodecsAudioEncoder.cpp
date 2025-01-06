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
#include "FlacEncoderConfig.h"
#include "JSDOMPromiseDeferred.h"
#include "JSWebCodecsAudioEncoderSupport.h"
#include "Logging.h"
#include "OpusEncoderConfig.h"
#include "SecurityOrigin.h"
#include "WebCodecsAudioData.h"
#include "WebCodecsAudioEncoderConfig.h"
#include "WebCodecsControlMessage.h"
#include "WebCodecsEncodedAudioChunk.h"
#include "WebCodecsEncodedAudioChunkMetadata.h"
#include "WebCodecsEncodedAudioChunkOutputCallback.h"
#include "WebCodecsErrorCallback.h"
#include "WebCodecsUtilities.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <wtf/StdLibExtras.h>
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
    : WebCodecsBase(context)
    , m_output(init.output.releaseNonNull())
    , m_error(init.error.releaseNonNull())
{
}

WebCodecsAudioEncoder::~WebCodecsAudioEncoder() = default;

static bool isSupportedEncoderCodec(const WebCodecsAudioEncoderConfig& config)
{
    // FIXME: Check codec more accurately.
    const auto& codec = config.codec;
    bool isMPEG4AAC = codec == "mp4a.40.2"_s || codec == "mp4a.40.02"_s || codec == "mp4a.40.5"_s
        || codec == "mp4a.40.05"_s || codec == "mp4a.40.29"_s || codec == "mp4a.40.42"_s;
    bool isCodecAllowed = isMPEG4AAC || codec == "mp3"_s || codec == "opus"_s
        || codec == "alaw"_s || codec == "ulaw"_s || codec == "flac"_s
        || codec == "vorbis"_s || codec.startsWith("pcm-"_s);

    if (!isCodecAllowed)
        return false;

    // FIXME: https://github.com/web-platform-tests/wpt/issues/49635
    // WPT audio-encoder-config.https.any.html checks for the samplingRate is between "supported" values.
    if (config.sampleRate < 3000 || config.sampleRate > 384000)
        return false;

    return true;
}

static bool isValidEncoderConfig(const WebCodecsAudioEncoderConfig& config)
{
    if (StringView(config.codec).trim(isASCIIWhitespace<UChar>).isEmpty())
        return false;

    if (!config.sampleRate || !config.numberOfChannels)
        return false;

    // FIXME: This isn't per spec, but both Chrome and Firefox checks that the bitrate is not greater than INT_MAX
    // Even though the spec made it a `long long`
    // https://github.com/web-platform-tests/wpt/issues/49634
    if (config.bitrate && *config.bitrate > std::numeric_limits<int>::max())
        return false;

    // FIXME: https://github.com/w3c/webcodecs/issues/860
    // Not per spec yet, but tested by w3c/web-platform-tests/webcodecs/audio-encoder-config.https.any.js 'Bit rate present but equal to zero'
    if (config.bitrate && !*config.bitrate)
        return false;

    // FIXME: New WPT requires this to reject as non valid. For now we just state that it's not supported (webkit.org/b/283900)
    // https://w3c.github.io/webcodecs/opus_codec_registration.html#opus-encoder-config
    if (config.codec == "opus"_s && config.bitrate && (*config.bitrate < 6000 || *config.bitrate > 510000))
        return false;

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
    if (config.opus) {
        opusConfig = {
            .isOggBitStream = config.opus->format == OpusBitstreamFormat::Ogg,
            .frameDuration = config.opus->frameDuration,
            .complexity = config.opus->complexity,
            .packetlossperc = config.opus->packetlossperc,
            .useinbandfec = config.opus->useinbandfec,
            .usedtx = config.opus->usedtx
        };
    }

    std::optional<bool> isAacADTS = std::nullopt;
    if (config.aac)
        isAacADTS = config.aac->format == AacBitstreamFormat::Adts;

    std::optional<AudioEncoder::FlacConfig> flacConfig = std::nullopt;
    if (config.flac)
        flacConfig = { config.flac->blockSize, config.flac->compressLevel };

    return AudioEncoder::Config {
        .sampleRate = config.sampleRate,
        .numberOfChannels = config.numberOfChannels,
        .bitRate = config.bitrate.value_or(0),
        .bitRateMode = config.bitrateMode,
        .opusConfig = WTFMove(opusConfig),
        .isAacADTS = isAacADTS,
        .flacConfig = WTFMove(flacConfig)
    };
}

ExceptionOr<void> WebCodecsAudioEncoder::configure(ScriptExecutionContext&, WebCodecsAudioEncoderConfig&& config)
{
    if (!isValidEncoderConfig(config))
        return Exception { ExceptionCode::TypeError, "Config is invalid"_s };

    if (state() == WebCodecsCodecState::Closed || !scriptExecutionContext())
        return Exception { ExceptionCode::InvalidStateError, "AudioEncoder is closed"_s };

    setState(WebCodecsCodecState::Configured);
    m_isKeyChunkRequired = true;

    if (m_internalEncoder) {
        queueControlMessageAndProcess({ *this, [this, config]() mutable {
            blockControlMessageQueue();

            protectedScriptExecutionContext()->enqueueTaskWhenSettled(Ref { *m_internalEncoder }->flush(), TaskSource::MediaElement, [weakThis = ThreadSafeWeakPtr { *this }, config = WTFMove(config)] (auto&&) mutable {
                RefPtr protectedThis = weakThis.get();
                if (!protectedThis)
                    return;

                if (protectedThis->state() == WebCodecsCodecState::Closed || !protectedThis->scriptExecutionContext())
                    return;

                protectedThis->unblockControlMessageQueue();
            });
            return WebCodecsControlMessageOutcome::Processed;
        } });
    }

    bool isSupportedCodec = isSupportedEncoderCodec(config);
    queueControlMessageAndProcess({ *this, [this, config = WTFMove(config), isSupportedCodec, identifier = scriptExecutionContext()->identifier()]() mutable {
        RefPtr context = scriptExecutionContext();

        blockControlMessageQueue();
        if (!isSupportedCodec) {
            postTaskToCodec<WebCodecsAudioEncoder>(identifier, *this, [] (auto& encoder) {
                encoder.closeEncoder(Exception { ExceptionCode::NotSupportedError, "Codec is not supported"_s });
            });
            return WebCodecsControlMessageOutcome::Processed;
        }

        auto encoderConfig = createAudioEncoderConfig(config);
        if (encoderConfig.hasException()) {
            postTaskToCodec<WebCodecsAudioEncoder>(identifier, *this, [message = encoderConfig.releaseException().message()] (auto& encoder) mutable {
                encoder.closeEncoder(Exception { ExceptionCode::NotSupportedError, WTFMove(message) });
            });
            return WebCodecsControlMessageOutcome::Processed;
        }

        m_baseConfiguration = config;

        Ref createEncoderPromise = AudioEncoder::create(config.codec, encoderConfig.releaseReturnValue(), [identifier, weakThis = ThreadSafeWeakPtr { *this }] (auto&& configuration) {
            postTaskToCodec<WebCodecsAudioEncoder>(identifier, weakThis, [configuration = WTFMove(configuration)] (auto& encoder) mutable {
                encoder.m_activeConfiguration = WTFMove(configuration);
                encoder.m_hasNewActiveConfiguration = true;
            });
        }, [identifier, weakThis = ThreadSafeWeakPtr { *this }, encoderCount = ++m_encoderCount] (auto&& result) {
            postTaskToCodec<WebCodecsAudioEncoder>(identifier, weakThis, [result = WTFMove(result), encoderCount] (auto& encoder) mutable {
                if (encoder.state() != WebCodecsCodecState::Configured || encoder.m_encoderCount != encoderCount)
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
            protectedThis->m_hasNewActiveConfiguration = true;
            protectedThis->unblockControlMessageQueue();
        });

        return WebCodecsControlMessageOutcome::Processed;
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
                memcpySpan(arrayBuffer->mutableSpan(), m_activeConfiguration.description->span());
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

    if (state() != WebCodecsCodecState::Configured)
        return Exception { ExceptionCode::InvalidStateError, "AudioEncoder is not configured"_s };

    queueCodecControlMessageAndProcess({ *this, [this, audioData = WTFMove(audioData), timestamp = frame->timestamp(), duration = frame->duration()]() mutable {
        // FIXME: These checks are not yet spec-compliant. See also https://github.com/w3c/webcodecs/issues/716
        if (m_baseConfiguration.numberOfChannels != audioData->numberOfChannels()
            || m_baseConfiguration.sampleRate != audioData->sampleRate()) {
            queueTaskKeepingObjectAlive(*this, TaskSource::MediaElement, [this]() mutable {
                closeEncoder(Exception { ExceptionCode::EncodingError, "Input audio buffer is incompatible with codec parameters"_s });
            });
            return WebCodecsControlMessageOutcome::Processed;
        }

        incrementCodecOperationCount();
        protectedScriptExecutionContext()->enqueueTaskWhenSettled(Ref { *m_internalEncoder }->encode({ WTFMove(audioData), timestamp, duration }), TaskSource::MediaElement, [weakThis = ThreadSafeWeakPtr { *this }, pendingActivity = makePendingActivity(*this)] (auto&& result) {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;

            if (!result) {
                if (auto context = protectedThis->protectedScriptExecutionContext())
                    context->addConsoleMessage(MessageSource::JS, MessageLevel::Error, makeString("AudioEncoder encode failed: "_s, result.error()));
                protectedThis->closeEncoder(Exception { ExceptionCode::EncodingError, WTFMove(result.error()) });
                return;
            }
            protectedThis->decrementCodecOperationCountAndMaybeProcessControlMessageQueue();
        });
        return WebCodecsControlMessageOutcome::Processed;
    } });
    return { };
}

void WebCodecsAudioEncoder::flush(Ref<DeferredPromise>&& promise)
{
    if (state() != WebCodecsCodecState::Configured) {
        promise->reject(Exception { ExceptionCode::InvalidStateError, "AudioEncoder is not configured"_s });
        return;
    }

    m_pendingFlushPromises.append(promise);
    queueControlMessageAndProcess({ *this, [this, promise = WTFMove(promise)]() mutable {
        protectedScriptExecutionContext()->enqueueTaskWhenSettled(Ref { *m_internalEncoder }->flush(), TaskSource::MediaElement, [weakThis = ThreadSafeWeakPtr { *this }, pendingActivity = makePendingActivity(*this), promise = WTFMove(promise)] (auto&&) {
            promise->resolve();
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->m_pendingFlushPromises.removeFirstMatching([&](auto& flushPromise) { return promise.ptr() == flushPromise.ptr(); });
        });
        return WebCodecsControlMessageOutcome::Processed;
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

    if (!isSupportedEncoderCodec(config)) {
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
    setState(WebCodecsCodecState::Closed);
    m_internalEncoder = nullptr;
    if (exception.code() != ExceptionCode::AbortError)
        m_error->handleEvent(DOMException::create(WTFMove(exception)));

    return { };
}

ExceptionOr<void> WebCodecsAudioEncoder::resetEncoder(const Exception& exception)
{
    if (state() == WebCodecsCodecState::Closed)
        return Exception { ExceptionCode::InvalidStateError, "AudioEncoder is closed"_s };

    setState(WebCodecsCodecState::Unconfigured);
    if (RefPtr internalEncoder = std::exchange(m_internalEncoder, { }))
        internalEncoder->reset();
    clearControlMessageQueueAndMaybeScheduleDequeueEvent();

    auto promises = std::exchange(m_pendingFlushPromises, { });
    for (auto& promise : promises)
        promise->reject(exception);

    return { };
}

void WebCodecsAudioEncoder::setInternalEncoder(Ref<AudioEncoder>&& internalEncoder)
{
    m_internalEncoder = WTFMove(internalEncoder);
}

void WebCore::WebCodecsAudioEncoder::suspend(ReasonForSuspension)
{
}

void WebCodecsAudioEncoder::stop()
{
    setState(WebCodecsCodecState::Closed);
    m_internalEncoder = nullptr;
    clearControlMessageQueue();
    m_pendingFlushPromises.clear();
}

} // namespace WebCore

#endif // ENABLE(WEB_CODECS)
