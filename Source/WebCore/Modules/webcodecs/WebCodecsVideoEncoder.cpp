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

#include "ContextDestructionObserverInlines.h"
#include "DOMException.h"
#include "JSDOMPromiseDeferred.h"
#include "JSWebCodecsVideoEncoderSupport.h"
#include "Logging.h"
#include "WebCodecsControlMessage.h"
#include "WebCodecsEncodedVideoChunk.h"
#include "WebCodecsEncodedVideoChunkMetadata.h"
#include "WebCodecsEncodedVideoChunkOutputCallback.h"
#include "WebCodecsErrorCallback.h"
#include "WebCodecsUtilities.h"
#include "WebCodecsVideoEncoderConfig.h"
#include "WebCodecsVideoEncoderEncodeOptions.h"
#include "WebCodecsVideoFrame.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(WebCodecsVideoEncoder);

Ref<WebCodecsVideoEncoder> WebCodecsVideoEncoder::create(ScriptExecutionContext& context, Init&& init)
{
    auto encoder = adoptRef(*new WebCodecsVideoEncoder(context, WTFMove(init)));
    encoder->suspendIfNeeded();
    return encoder;
}


WebCodecsVideoEncoder::WebCodecsVideoEncoder(ScriptExecutionContext& context, Init&& init)
    : WebCodecsBase(context)
    , m_output(init.output.releaseNonNull())
    , m_error(init.error.releaseNonNull())
{
}

WebCodecsVideoEncoder::~WebCodecsVideoEncoder() = default;

static bool isSupportedEncoderCodec(const String& codec, const Settings::Values& settings)
{
    return codec.startsWith("vp8"_s) || codec.startsWith("vp09.00"_s) || codec.startsWith("avc1."_s)
#if ENABLE(WEB_RTC)
        || (codec.startsWith("vp09.02"_s) && settings.webRTCVP9Profile2CodecEnabled)
#endif
        || (codec.startsWith("hev1."_s) && settings.webCodecsHEVCEnabled)
        || (codec.startsWith("hvc1."_s) && settings.webCodecsHEVCEnabled)
        || (codec.startsWith("av01.0"_s) && settings.webCodecsAV1Enabled);
}

static bool isValidEncoderConfig(const WebCodecsVideoEncoderConfig& config)
{
    if (StringView(config.codec).trim(isASCIIWhitespace<UChar>).isEmpty())
        return false;

    if (!config.width || !config.height)
        return false;
    if (!config.displayWidth.value_or(config.width) || !config.displayHeight.value_or(config.height))
        return false;

    return true;
}

static ExceptionOr<VideoEncoder::Config> createVideoEncoderConfig(const WebCodecsVideoEncoderConfig& config)
{
    if (config.alpha == WebCodecsAlphaOption::Keep)
        return Exception { ExceptionCode::NotSupportedError, "Alpha keep is not supported"_s };

    VideoEncoder::ScalabilityMode scalabilityMode = VideoEncoder::ScalabilityMode::L1T1;
    if (!config.scalabilityMode.isNull()) {
        if (config.scalabilityMode == "L1T3"_s)
            scalabilityMode = VideoEncoder::ScalabilityMode::L1T3;
        else if (config.scalabilityMode == "L1T2"_s)
            scalabilityMode = VideoEncoder::ScalabilityMode::L1T2;
        else if (config.scalabilityMode != "L1T1"_s)
            return Exception { ExceptionCode::TypeError, "Scalabilty mode is not supported"_s };
    }

    if (config.codec.startsWith("avc1."_s) && (!!(config.width % 2) || !!(config.height % 2)))
        return Exception { ExceptionCode::TypeError, "H264 only supports even sized frames"_s };

    bool useAnnexB = config.avc && config.avc->format == AvcBitstreamFormat::Annexb;
    return VideoEncoder::Config { config.width, config.height, useAnnexB, config.bitrate.value_or(0), config.framerate.value_or(0), config.latencyMode == LatencyMode::Realtime, scalabilityMode };
}

void WebCodecsVideoEncoder::updateRates(const WebCodecsVideoEncoderConfig& config)
{
    auto bitrate = config.bitrate.value_or(0);
    auto framerate = config.framerate.value_or(0);

    blockControlMessageQueue();
    protectedScriptExecutionContext()->enqueueTaskWhenSettled(Ref { *m_internalEncoder }->setRates(bitrate, framerate), TaskSource::MediaElement, [weakThis = ThreadSafeWeakPtr { *this }, bitrate, framerate] (auto&&) mutable {
        auto protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        if (protectedThis->state() == WebCodecsCodecState::Closed || !protectedThis->scriptExecutionContext())
            return;

        if (bitrate)
            protectedThis->m_baseConfiguration.bitrate = bitrate;
        if (framerate)
            protectedThis->m_baseConfiguration.framerate = framerate;
        protectedThis->unblockControlMessageQueue();
    });
}

ExceptionOr<void> WebCodecsVideoEncoder::configure(ScriptExecutionContext& context, WebCodecsVideoEncoderConfig&& config)
{
    if (!isValidEncoderConfig(config))
        return Exception { ExceptionCode::TypeError, "Config is invalid"_s };

    if (state() == WebCodecsCodecState::Closed || !scriptExecutionContext())
        return Exception { ExceptionCode::InvalidStateError, "VideoEncoder is closed"_s };

    setState(WebCodecsCodecState::Configured);
    m_isKeyChunkRequired = true;

    if (m_internalEncoder) {
        queueControlMessageAndProcess({ *this, [this, config]() mutable {
            if (isSameConfigurationExceptBitrateAndFramerate(m_baseConfiguration, config)) {
                updateRates(config);
                return WebCodecsControlMessageOutcome::Processed;
            }

            blockControlMessageQueue();
            protectedScriptExecutionContext()->enqueueTaskWhenSettled(Ref { *m_internalEncoder }->flush(), TaskSource::MediaElement, [weakThis = ThreadSafeWeakPtr { *this }, config = WTFMove(config), pendingActivity = makePendingActivity(*this)] (auto&&) mutable {
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

    bool isSupportedCodec = isSupportedEncoderCodec(config.codec, context.settingsValues());
    queueControlMessageAndProcess({ *this, [this, config = WTFMove(config), isSupportedCodec]() mutable {
        if (isSupportedCodec && isSameConfigurationExceptBitrateAndFramerate(m_baseConfiguration, config)) {
            updateRates(config);
            return WebCodecsControlMessageOutcome::Processed;
        }

        auto identifier = scriptExecutionContext()->identifier();

        blockControlMessageQueue();

        if (!isSupportedCodec) {
            postTaskToCodec<WebCodecsVideoEncoder>(identifier, *this, [] (auto& encoder) {
                encoder.closeEncoder(Exception { ExceptionCode::NotSupportedError, "Codec is not supported"_s });
            });
            return WebCodecsControlMessageOutcome::Processed;
        }

        auto encoderConfig = createVideoEncoderConfig(config);
        if (encoderConfig.hasException()) {
            postTaskToCodec<WebCodecsVideoEncoder>(identifier, *this, [message = encoderConfig.releaseException().message()] (auto& encoder) mutable {
                encoder.closeEncoder(Exception { ExceptionCode::NotSupportedError, WTFMove(message) });
            });
            return WebCodecsControlMessageOutcome::Processed;
        }

        m_baseConfiguration = config;

        Ref createEncoderPromise = VideoEncoder::create(config.codec, encoderConfig.releaseReturnValue(), [identifier, weakThis = ThreadSafeWeakPtr { *this }] (auto&& configuration) {
            postTaskToCodec<WebCodecsVideoEncoder>(identifier, weakThis, [configuration = WTFMove(configuration)] (auto& encoder) mutable {
                encoder.m_activeConfiguration = WTFMove(configuration);
                encoder.m_hasNewActiveConfiguration = true;
            });
        }, [identifier, weakThis = ThreadSafeWeakPtr { *this }, encoderCount = ++m_encoderCount](auto&& result) {
            postTaskToCodec<WebCodecsVideoEncoder>(identifier, weakThis, [result = WTFMove(result), encoderCount] (auto& encoder) mutable {
                if (encoder.state() != WebCodecsCodecState::Configured || encoder.m_encoderCount != encoderCount)
                    return;

                RefPtr<JSC::ArrayBuffer> buffer = JSC::ArrayBuffer::create(result.data);
                auto chunk = WebCodecsEncodedVideoChunk::create(WebCodecsEncodedVideoChunk::Init {
                    result.isKeyFrame ? WebCodecsEncodedVideoChunkType::Key : WebCodecsEncodedVideoChunkType::Delta,
                    result.timestamp,
                    result.duration,
                    BufferSource { WTFMove(buffer) }
                });
                encoder.m_output->handleEvent(WTFMove(chunk), encoder.createEncodedChunkMetadata(result.temporalIndex));
            });
        });

        protectedScriptExecutionContext()->enqueueTaskWhenSettled(WTFMove(createEncoderPromise), TaskSource::MediaElement, [weakThis = ThreadSafeWeakPtr { *this }](auto&& result) mutable {
            auto protectedThis = weakThis.get();
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

WebCodecsEncodedVideoChunkMetadata WebCodecsVideoEncoder::createEncodedChunkMetadata(std::optional<unsigned> temporalIndex)
{
    WebCodecsEncodedVideoChunkMetadata metadata;

    if (m_hasNewActiveConfiguration) {
        m_hasNewActiveConfiguration = false;
        // FIXME: Provide more accurate decoder configuration
        metadata.decoderConfig = WebCodecsVideoDecoderConfig {
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

        if (m_activeConfiguration.description && m_activeConfiguration.description->size()) {
            auto arrayBuffer = ArrayBuffer::tryCreateUninitialized(m_activeConfiguration.description->size(), 1);
            RELEASE_LOG_ERROR_IF(!!arrayBuffer, Media, "Cannot create array buffer for WebCodecs encoder description");
            if (arrayBuffer) {
                memcpySpan(arrayBuffer->mutableSpan(), m_activeConfiguration.description->span());
                metadata.decoderConfig->description = WTFMove(arrayBuffer);
            }
        }

    }

    if (temporalIndex)
        metadata.svc = WebCodecsSvcOutputMetadata { *temporalIndex };

    return metadata;
}

ExceptionOr<void> WebCodecsVideoEncoder::encode(Ref<WebCodecsVideoFrame>&& frame, WebCodecsVideoEncoderEncodeOptions&& options)
{
    auto internalFrame = frame->internalFrame();
    if (!internalFrame) {
        ASSERT(frame->isDetached());
        return Exception { ExceptionCode::TypeError, "VideoFrame is detached"_s };
    }
    ASSERT(!frame->isDetached());

    if (state() != WebCodecsCodecState::Configured)
        return Exception { ExceptionCode::InvalidStateError, "VideoEncoder is not configured"_s };

    queueCodecControlMessageAndProcess({ *this, [this, internalFrame = internalFrame.releaseNonNull(), timestamp = frame->timestamp(), duration = frame->duration(), options = WTFMove(options)]() mutable {
        incrementCodecOperationCount();
        protectedScriptExecutionContext()->enqueueTaskWhenSettled(Ref { *m_internalEncoder }->encode({ WTFMove(internalFrame), timestamp, duration }, options.keyFrame), TaskSource::MediaElement, [weakThis = ThreadSafeWeakPtr { *this }, pendingActivity = makePendingActivity(*this)] (auto&& result) {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;

            if (!result) {
                if (RefPtr context = protectedThis->scriptExecutionContext())
                    context->addConsoleMessage(MessageSource::JS, MessageLevel::Error, makeString("VideoEncoder encode failed: "_s, result.error()));
                protectedThis->closeEncoder(Exception { ExceptionCode::EncodingError, WTFMove(result.error()) });
                return;
            }
            protectedThis->decrementCodecOperationCountAndMaybeProcessControlMessageQueue();
        });
        return WebCodecsControlMessageOutcome::Processed;
    } });
    return { };
}

void WebCodecsVideoEncoder::flush(Ref<DeferredPromise>&& promise)
{
    if (state() != WebCodecsCodecState::Configured) {
        promise->reject(Exception { ExceptionCode::InvalidStateError, "VideoEncoder is not configured"_s });
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

ExceptionOr<void> WebCodecsVideoEncoder::reset()
{
    return resetEncoder(Exception { ExceptionCode::AbortError, "Reset called"_s });
}

ExceptionOr<void> WebCodecsVideoEncoder::close()
{
    return closeEncoder(Exception { ExceptionCode::AbortError, "Close called"_s });
}

void WebCodecsVideoEncoder::isConfigSupported(ScriptExecutionContext& context, WebCodecsVideoEncoderConfig&& config, Ref<DeferredPromise>&& promise)
{
    if (!isValidEncoderConfig(config)) {
        promise->reject(Exception { ExceptionCode::TypeError, "Config is not valid"_s });
        return;
    }

    if (!isSupportedEncoderCodec(config.codec, context.settingsValues())) {
        promise->template resolve<IDLDictionary<WebCodecsVideoEncoderSupport>>(WebCodecsVideoEncoderSupport { false, WTFMove(config) });
        return;
    }

    auto encoderConfig = createVideoEncoderConfig(config);
    if (encoderConfig.hasException()) {
        promise->template resolve<IDLDictionary<WebCodecsVideoEncoderSupport>>(WebCodecsVideoEncoderSupport { false, WTFMove(config) });
        return;
    }

    Ref createEncoderPromise = VideoEncoder::create(config.codec, encoderConfig.releaseReturnValue(), [] (auto&&) { }, [] (auto&&) { });
    context.enqueueTaskWhenSettled(WTFMove(createEncoderPromise), TaskSource::MediaElement, [config, promise = WTFMove(promise)](auto&& result) mutable {
        promise->template resolve<IDLDictionary<WebCodecsVideoEncoderSupport>>(WebCodecsVideoEncoderSupport { !!result, WTFMove(config) });
    });
}

ExceptionOr<void> WebCodecsVideoEncoder::closeEncoder(Exception&& exception)
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

ExceptionOr<void> WebCodecsVideoEncoder::resetEncoder(const Exception& exception)
{
    if (state() == WebCodecsCodecState::Closed)
        return Exception { ExceptionCode::InvalidStateError, "VideoEncoder is closed"_s };

    setState(WebCodecsCodecState::Unconfigured);
    if (RefPtr internalEncoder = std::exchange(m_internalEncoder, { }))
        internalEncoder->reset();
    clearControlMessageQueueAndMaybeScheduleDequeueEvent();

    auto promises = std::exchange(m_pendingFlushPromises, { });
    for (auto& promise : promises)
        promise->reject(exception);

    return { };
}

void WebCodecsVideoEncoder::setInternalEncoder(Ref<VideoEncoder>&& internalEncoder)
{
    m_internalEncoder = WTFMove(internalEncoder);
}

void WebCore::WebCodecsVideoEncoder::suspend(ReasonForSuspension)
{
}

void WebCodecsVideoEncoder::stop()
{
    setState(WebCodecsCodecState::Closed);
    m_internalEncoder = nullptr;
    clearControlMessageQueue();
    m_pendingFlushPromises.clear();
}

} // namespace WebCore

#endif // ENABLE(WEB_CODECS)
