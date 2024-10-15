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
#include "Event.h"
#include "EventNames.h"
#include "JSDOMPromiseDeferred.h"
#include "JSWebCodecsAudioDecoderSupport.h"
#include "ScriptExecutionContext.h"
#include "WebCodecsAudioData.h"
#include "WebCodecsAudioDataOutputCallback.h"
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
    : ActiveDOMObject(&context)
    , m_output(init.output.releaseNonNull())
    , m_error(init.error.releaseNonNull())
{
}

WebCodecsAudioDecoder::~WebCodecsAudioDecoder() = default;

static bool isValidDecoderConfig(const WebCodecsAudioDecoderConfig& config)
{
    if (StringView(config.codec).trim(isASCIIWhitespace<UChar>).isEmpty())
        return false;

    // FIXME: We might need to check numberOfChannels and sampleRate here. See
    // https://github.com/w3c/webcodecs/issues/714.

    return true;
}

static AudioDecoder::Config createAudioDecoderConfig(const WebCodecsAudioDecoderConfig& config)
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

    return { description, config.sampleRate, config.numberOfChannels };
}

ExceptionOr<void> WebCodecsAudioDecoder::configure(ScriptExecutionContext&, WebCodecsAudioDecoderConfig&& config)
{
    if (!isValidDecoderConfig(config))
        return Exception { ExceptionCode::TypeError, "Config is not valid"_s };

    if (m_state == WebCodecsCodecState::Closed || !scriptExecutionContext())
        return Exception { ExceptionCode::InvalidStateError, "AudioDecoder is closed"_s };

    m_state = WebCodecsCodecState::Configured;
    m_isKeyChunkRequired = true;

    bool isSupportedCodec = AudioDecoder::isCodecSupported(config.codec);
    queueControlMessageAndProcess({ *this, [this, config = WTFMove(config), isSupportedCodec, identifier = scriptExecutionContext()->identifier()]() mutable {
        m_isMessageQueueBlocked = true;

        if (!isSupportedCodec) {
            postTaskToCodec<WebCodecsAudioDecoder>(identifier, *this, [] (auto& decoder) {
                decoder.closeDecoder(Exception { ExceptionCode::NotSupportedError, "Codec is not supported"_s });
            });
            return;
        }

        Ref createDecoderPromise = AudioDecoder::create(config.codec, createAudioDecoderConfig(config), [identifier, weakThis = ThreadSafeWeakPtr { *this }, decoderCount = ++m_decoderCount] (auto&& result) {
            postTaskToCodec<WebCodecsAudioDecoder>(identifier, weakThis, [result = WTFMove(result), decoderCount] (auto& decoder) mutable {
                if (decoder.m_state != WebCodecsCodecState::Configured || decoder.m_decoderCount != decoderCount)
                    return;

                if (!result.has_value()) {
                    decoder.closeDecoder(Exception { ExceptionCode::EncodingError, WTFMove(result).error() });
                    return;
                }

                auto decodedResult = WTFMove(result).value();
                auto audioData = WebCodecsAudioData::create(*decoder.scriptExecutionContext(), WTFMove(decodedResult.data));
                decoder.m_output->handleEvent(WTFMove(audioData));
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
            protectedThis->m_isMessageQueueBlocked = false;
            protectedThis->processControlMessageQueue();
        });
    } });
    return { };
}

ExceptionOr<void> WebCodecsAudioDecoder::decode(Ref<WebCodecsEncodedAudioChunk>&& chunk)
{
    if (m_state != WebCodecsCodecState::Configured)
        return Exception { ExceptionCode::InvalidStateError, "AudioDecoder is not configured"_s };

    if (m_isKeyChunkRequired) {
        if (chunk->type() != WebCodecsEncodedAudioChunkType::Key)
            return Exception { ExceptionCode::DataError, "Key frame is required"_s };
        m_isKeyChunkRequired = false;
    }

    ++m_decodeQueueSize;
    queueControlMessageAndProcess({ *this, [this, chunk = WTFMove(chunk)]() mutable {
        --m_decodeQueueSize;
        scheduleDequeueEvent();

        protectedScriptExecutionContext()->enqueueTaskWhenSettled(m_internalDecoder->decode({ chunk->span(), chunk->type() == WebCodecsEncodedAudioChunkType::Key, chunk->timestamp(), chunk->duration() }), TaskSource::MediaElement, [weakThis = ThreadSafeWeakPtr { *this }, pendingActivity = makePendingActivity(*this)] (auto&& result) {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis || !!result)
                return;

            protectedThis->closeDecoder(Exception { ExceptionCode::EncodingError, WTFMove(result.error()) });
        });
    } });
    return { };
}

ExceptionOr<void> WebCodecsAudioDecoder::flush(Ref<DeferredPromise>&& promise)
{
    if (m_state != WebCodecsCodecState::Configured)
        return Exception { ExceptionCode::InvalidStateError, "AudioDecoder is not configured"_s };

    m_isKeyChunkRequired = true;
    m_pendingFlushPromises.append(WTFMove(promise));
    queueControlMessageAndProcess({ *this, [this, clearFlushPromiseCount = m_clearFlushPromiseCount] {
        protectedScriptExecutionContext()->enqueueTaskWhenSettled(m_internalDecoder->flush(), TaskSource::MediaElement, [weakThis = ThreadSafeWeakPtr { *this }, clearFlushPromiseCount, pendingActivity = makePendingActivity(*this)] (auto&&) {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;

            if (clearFlushPromiseCount != protectedThis->m_clearFlushPromiseCount)
                return;

            protectedThis->m_pendingFlushPromises.takeFirst()->resolve();
        });
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
    m_state = WebCodecsCodecState::Closed;
    m_internalDecoder = nullptr;
    if (exception.code() != ExceptionCode::AbortError)
        m_error->handleEvent(DOMException::create(WTFMove(exception)));

    return { };
}

ExceptionOr<void> WebCodecsAudioDecoder::resetDecoder(const Exception& exception)
{
    if (m_state == WebCodecsCodecState::Closed)
        return Exception { ExceptionCode::InvalidStateError, "AudioDecoder is closed"_s };

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

void WebCodecsAudioDecoder::scheduleDequeueEvent()
{
    if (m_dequeueEventScheduled)
        return;

    m_dequeueEventScheduled = true;
    queueTaskKeepingObjectAlive(*this, TaskSource::MediaElement, [this]() mutable {
        dispatchEvent(Event::create(eventNames().dequeueEvent, Event::CanBubble::No, Event::IsCancelable::No));
        m_dequeueEventScheduled = false;
    });
}

void WebCodecsAudioDecoder::setInternalDecoder(UniqueRef<AudioDecoder>&& internalDecoder)
{
    m_internalDecoder = internalDecoder.moveToUniquePtr();
}

void WebCodecsAudioDecoder::queueControlMessageAndProcess(WebCodecsControlMessage<WebCodecsAudioDecoder>&& message)
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

void WebCodecsAudioDecoder::processControlMessageQueue()
{
    while (!m_isMessageQueueBlocked && !m_controlMessageQueue.isEmpty())
        m_controlMessageQueue.takeFirst()();
}

void WebCore::WebCodecsAudioDecoder::suspend(ReasonForSuspension)
{
}

void WebCodecsAudioDecoder::stop()
{
    m_state = WebCodecsCodecState::Closed;
    m_internalDecoder = nullptr;
    m_controlMessageQueue.clear();
    m_pendingFlushPromises.clear();
}

bool WebCodecsAudioDecoder::virtualHasPendingActivity() const
{
    return m_state == WebCodecsCodecState::Configured && (m_decodeQueueSize || m_isMessageQueueBlocked);
}

} // namespace WebCore

#endif // ENABLE(WEB_CODECS)
