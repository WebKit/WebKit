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
#include "WebCodecsAudioDecoder.h"

#if ENABLE(WEB_CODECS)

#include "ContextDestructionObserverInlines.h"
#include "DOMException.h"
#include "ExceptionOr.h"
#include "JSDOMPromiseDeferred.h"
#include "JSWebCodecsAudioDecoderSupport.h"
#include "ScriptExecutionContextInlines.h"
#include "WebCodecsAudioData.h"
#include "WebCodecsAudioDataOutputCallback.h"
#include "WebCodecsControlMessage.h"
#include "WebCodecsEncodedAudioChunk.h"
#include "WebCodecsErrorCallback.h"
#include "WebCodecsUtilities.h"

#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(WebCodecsAudioDecoder);

Ref<WebCodecsAudioDecoder> WebCodecsAudioDecoder::create(ScriptExecutionContext& context, Init&& init)
{
    auto decoder = adoptRef(*new WebCodecsAudioDecoder(context, WTFMove(init)));
    decoder->suspendIfNeeded();
    return decoder;
}

WebCodecsAudioDecoder::WebCodecsAudioDecoder(ScriptExecutionContext& context, Init&& init)
    : WebCodecsBase(context)
    , m_output(init.output.releaseNonNull())
    , m_error(init.error.releaseNonNull())
{
}

WebCodecsAudioDecoder::~WebCodecsAudioDecoder() = default;

static AudioDecoder::Config createAudioDecoderConfig(const WebCodecsAudioDecoderConfig& config)
{
    Vector<uint8_t> description;
    if (config.description) {
        auto data = WTF::visit([](auto& buffer) {
            return buffer ? buffer->span() : std::span<const uint8_t> { };
        }, *config.description);
        if (!data.empty())
            description = data;
    }

    return {
        .description = WTFMove(description),
        .sampleRate = config.sampleRate,
        .numberOfChannels = config.numberOfChannels
    };
}

static bool isValidDecoderConfig(const WebCodecsAudioDecoderConfig& config)
{
    // https://w3c.github.io/webcodecs/#valid-audiodecoderconfig
    // 1. If codec is empty after stripping leading and trailing ASCII whitespace, return false.
    if (StringView(config.codec).trim(isASCIIWhitespace<char16_t>).isEmpty())
        return false;

    // 2. If description is [detached], return false.
    if (config.description && WTF::visit([](auto& view) { return view->isDetached(); }, *config.description))
        return false;

    // FIXME: Not yet per spec https://github.com/w3c/webcodecs/issues/878
    if (!config.numberOfChannels || !config.sampleRate)
        return false;

    auto descriptionSize = createAudioDecoderConfig(config).description.size();

    // FIXME: Not yet per spec, but being tested and tracked by https://github.com/w3c/webcodecs/issues/826
    // And handled that way by all other UAs.
    // If we have a config.description, ensures it's not an empty one.
    if (config.description && !descriptionSize)
        return false;

    // FIXME: WPT issue: https://github.com/web-platform-tests/wpt/issues/49636 is causing a test to fail when it shouldn't
    // Opus with more than two channels require a description
    // Extra spec issue: https://github.com/w3c/webcodecs/issues/861 : configure shouldn't throw on invalid config.
    if (config.codec == "opus"_s && config.numberOfChannels > 2 && descriptionSize < 10)
        return false;

    return true;
}

ExceptionOr<void> WebCodecsAudioDecoder::configure(ScriptExecutionContext&, WebCodecsAudioDecoderConfig&& config)
{
    if (!isValidDecoderConfig(config))
        return Exception { ExceptionCode::TypeError, "Config is not valid"_s };

    if (state() == WebCodecsCodecState::Closed || !scriptExecutionContext())
        return Exception { ExceptionCode::InvalidStateError, "AudioDecoder is closed"_s };

    setState(WebCodecsCodecState::Configured);
    m_isKeyChunkRequired = true;

    bool isSupportedCodec = AudioDecoder::isCodecSupported(config.codec);
    queueControlMessageAndProcess({ *this, [this, codec = config.codec, config = createAudioDecoderConfig(config), isSupportedCodec, identifier = scriptExecutionContext()->identifier()]() mutable {
        blockControlMessageQueue();

        if (!isSupportedCodec) {
            postTaskToCodec<WebCodecsAudioDecoder>(identifier, *this, [] (auto& decoder) {
                decoder.closeDecoder(Exception { ExceptionCode::NotSupportedError, "Codec is not supported"_s });
            });
            return WebCodecsControlMessageOutcome::Processed;
        }

        Ref createDecoderPromise = AudioDecoder::create(codec, config, [identifier, weakThis = ThreadSafeWeakPtr { *this }, decoderCount = ++m_decoderCount] (auto&& result) {
            postTaskToCodec<WebCodecsAudioDecoder>(identifier, weakThis, [result = WTFMove(result), decoderCount] (auto& decoder) mutable {
                if (decoder.state() != WebCodecsCodecState::Configured || decoder.m_decoderCount != decoderCount)
                    return;

                if (!result.has_value()) {
                    decoder.closeDecoder(Exception { ExceptionCode::EncodingError, WTFMove(result).error() });
                    return;
                }

                auto decodedResult = WTFMove(result).value();
                auto audioData = WebCodecsAudioData::create(*decoder.scriptExecutionContext(), WTFMove(decodedResult.data));
                decoder.m_output->invoke(WTFMove(audioData));
            });
        });

        protectedScriptExecutionContext()->enqueueTaskWhenSettled(WTFMove(createDecoderPromise), TaskSource::MediaElement, [weakThis = ThreadSafeWeakPtr { *this }] (AudioDecoder::CreateResult&& result) mutable {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;

            if (!result) {
                protectedThis->closeDecoder(Exception { ExceptionCode::NotSupportedError, WTFMove(result.error()) });
                return;
            }

            protectedThis->setInternalDecoder(WTFMove(*result));
            protectedThis->unblockControlMessageQueue();
        });
        return WebCodecsControlMessageOutcome::Processed;
    } });
    return { };
}

ExceptionOr<void> WebCodecsAudioDecoder::decode(Ref<WebCodecsEncodedAudioChunk>&& chunk)
{
    if (state() != WebCodecsCodecState::Configured)
        return Exception { ExceptionCode::InvalidStateError, "AudioDecoder is not configured"_s };

    if (m_isKeyChunkRequired) {
        if (chunk->type() != WebCodecsEncodedAudioChunkType::Key)
            return Exception { ExceptionCode::DataError, "Key frame is required"_s };
        m_isKeyChunkRequired = false;
    }

    queueCodecControlMessageAndProcess({ *this, [this, chunk = WTFMove(chunk)]() mutable {
        incrementCodecOperationCount();
        protectedScriptExecutionContext()->enqueueTaskWhenSettled(Ref { *m_internalDecoder }->decode({ chunk->span(), chunk->type() == WebCodecsEncodedAudioChunkType::Key, chunk->timestamp(), chunk->duration() }), TaskSource::MediaElement, [weakThis = ThreadSafeWeakPtr { *this }, pendingActivity = makePendingActivity(*this)] (auto&& result) {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;

            if (!result) {
                protectedThis->closeDecoder(Exception { ExceptionCode::EncodingError, WTFMove(result.error()) });
                return;
            }
            protectedThis->decrementCodecOperationCountAndMaybeProcessControlMessageQueue();
        });

        return WebCodecsControlMessageOutcome::Processed;
    } });
    return { };
}

ExceptionOr<void> WebCodecsAudioDecoder::flush(Ref<DeferredPromise>&& promise)
{
    if (state() != WebCodecsCodecState::Configured)
        return Exception { ExceptionCode::InvalidStateError, "AudioDecoder is not configured"_s };

    m_isKeyChunkRequired = true;
    m_pendingFlushPromises.append(promise);
    queueControlMessageAndProcess({ *this, [this, promise = WTFMove(promise)]() mutable {
        protectedScriptExecutionContext()->enqueueTaskWhenSettled(Ref { *m_internalDecoder }->flush(), TaskSource::MediaElement, [weakThis = ThreadSafeWeakPtr { *this }, pendingActivity = makePendingActivity(*this), promise = WTFMove(promise)] (auto&&) {
            promise->resolve();
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->m_pendingFlushPromises.removeFirstMatching([&](auto& flushPromise) { return promise.ptr() == flushPromise.ptr(); });
        });
        return WebCodecsControlMessageOutcome::Processed;
    } });
    return { };
}

ExceptionOr<void> WebCodecsAudioDecoder::reset()
{
    return resetDecoder(Exception { ExceptionCode::AbortError, "Reset called"_s });
}

ExceptionOr<void> WebCodecsAudioDecoder::close()
{
    return closeDecoder(Exception { ExceptionCode::AbortError, "Close called"_s });
}

void WebCodecsAudioDecoder::isConfigSupported(ScriptExecutionContext& context, WebCodecsAudioDecoderConfig&& config, Ref<DeferredPromise>&& promise)
{
    if (!isValidDecoderConfig(config)) {
        promise->reject(Exception { ExceptionCode::TypeError, "Config is not valid"_s });
        return;
    }

    if (!AudioDecoder::isCodecSupported(config.codec)) {
        promise->template resolve<IDLDictionary<WebCodecsAudioDecoderSupport>>(WebCodecsAudioDecoderSupport { false, WTFMove(config) });
        return;
    }

    Ref createDecoderPromise = AudioDecoder::create(config.codec, createAudioDecoderConfig(config), [](auto&&) { });
    context.enqueueTaskWhenSettled(WTFMove(createDecoderPromise), TaskSource::MediaElement, [config = WTFMove(config), promise = WTFMove(promise)](auto&& result) mutable {
        promise->template resolve<IDLDictionary<WebCodecsAudioDecoderSupport>>(WebCodecsAudioDecoderSupport { !!result, WTFMove(config) });
    });
}

ExceptionOr<void> WebCodecsAudioDecoder::closeDecoder(Exception&& exception)
{
    auto result = resetDecoder(exception);
    if (result.hasException())
        return result;
    setState(WebCodecsCodecState::Closed);
    m_internalDecoder = nullptr;
    if (exception.code() != ExceptionCode::AbortError)
        m_error->invoke(DOMException::create(WTFMove(exception)));

    return { };
}

ExceptionOr<void> WebCodecsAudioDecoder::resetDecoder(const Exception& exception)
{
    if (state() == WebCodecsCodecState::Closed)
        return Exception { ExceptionCode::InvalidStateError, "AudioDecoder is closed"_s };

    setState(WebCodecsCodecState::Unconfigured);
    if (RefPtr internalDecoder = std::exchange(m_internalDecoder, { }))
        internalDecoder->reset();
    clearControlMessageQueueAndMaybeScheduleDequeueEvent();

    auto promises = std::exchange(m_pendingFlushPromises, { });
    for (auto& promise : promises)
        promise->reject(exception);

    return { };
}

void WebCodecsAudioDecoder::setInternalDecoder(Ref<AudioDecoder>&& internalDecoder)
{
    m_internalDecoder = WTFMove(internalDecoder);
}

void WebCore::WebCodecsAudioDecoder::suspend(ReasonForSuspension)
{
}

void WebCodecsAudioDecoder::stop()
{
    setState(WebCodecsCodecState::Closed);
    m_internalDecoder = nullptr;
    clearControlMessageQueue();
    m_pendingFlushPromises.clear();
}

} // namespace WebCore

#endif // ENABLE(WEB_CODECS)
