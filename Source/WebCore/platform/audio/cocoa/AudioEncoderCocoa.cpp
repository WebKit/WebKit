/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "AudioEncoderCocoa.h"

#if ENABLE(WEB_CODECS) && USE(AVFOUNDATION)

#include "AudioDecoderCocoa.h"
#include "AudioSampleBufferConverter.h"
#include "AudioSampleFormat.h"
#include "CAAudioStreamDescription.h"
#include "Logging.h"
#include "MediaSampleAVFObjC.h"
#include "MediaUtilities.h"
#include "PlatformRawAudioDataCocoa.h"
#include "WebMAudioUtilitiesCocoa.h"

#include <CoreAudio/CoreAudioTypes.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/MakeString.h>

#include <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(AudioEncoderCocoa);

class InternalAudioEncoderCocoa : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<InternalAudioEncoderCocoa, WTF::DestructionThread::Main> {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(InternalAudioEncoderCocoa);
    WTF_MAKE_NONCOPYABLE(InternalAudioEncoderCocoa);

public:
    using Config = AudioEncoder::Config;
    using EncodePromise = AudioEncoder::EncodePromise;

    struct InternalConfig {
        FourCharCode codec { 0 };
        std::optional<AudioStreamDescription::PCMFormat> pcmFormat;
        std::optional<CAAudioStreamDescription> outputDescription;
        std::optional<MediaTime> frameDuration { };
        std::optional<unsigned> complexity { };
        std::optional<unsigned> packetlossperc { };
        std::optional<bool> useinbandfec { };
    };
    static Expected<InternalConfig, String> checkConfiguration(const String&, const Config&);

    static Ref<InternalAudioEncoderCocoa> create(const Config& config, InternalConfig&& internalConfig, AudioEncoder::DescriptionCallback&& descriptionCallback, AudioEncoder::OutputCallback&& outputCallback)
    {
        return adoptRef(*new InternalAudioEncoderCocoa(config, WTF::move(internalConfig), WTF::move(descriptionCallback), WTF::move(outputCallback)));
    }
    ~InternalAudioEncoderCocoa() = default;

    Ref<AudioEncoder::EncodePromise> encode(AudioEncoder::RawFrame&&);
    Ref<GenericPromise> flush();
    void reset();
    void close();

    static WorkQueue& queueSingleton() { return AudioDecoderCocoa::queueSingleton(); }

private:
    InternalAudioEncoderCocoa(const AudioEncoder::Config&, InternalConfig&&, AudioEncoder::DescriptionCallback&&, AudioEncoder::OutputCallback&&);
    static void compressedAudioOutputBufferCallback(void*, CMBufferQueueTriggerToken);
    Ref<AudioSampleBufferConverter> converter() const { return *m_converter; }
    void processEncodedOutputs();
    AudioEncoder::ActiveConfiguration activeConfiguration(CMSampleBufferRef) const;
    Vector<uint8_t> generateDecoderDescriptionFromSample(CMSampleBufferRef) const;

    AudioEncoder::DescriptionCallback m_descriptionCallback;
    AudioEncoder::OutputCallback m_outputCallback;
    RefPtr<AudioSampleBufferConverter> m_converter;

    const AudioEncoder::Config m_config;
    const InternalConfig m_internalConfig;
    bool m_isClosed WTF_GUARDED_BY_CAPABILITY(queueSingleton()) { false };
    std::optional<CAAudioStreamDescription> m_inputDescription WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    bool m_hasProvidedDecoderConfig WTF_GUARDED_BY_CAPABILITY(queueSingleton()) { false };
    bool m_needsFlushing WTF_GUARDED_BY_CAPABILITY(queueSingleton()) { false };
    OSStatus m_lastError WTF_GUARDED_BY_CAPABILITY(queueSingleton()) { noErr };
};

Expected<InternalAudioEncoderCocoa::InternalConfig, String> InternalAudioEncoderCocoa::checkConfiguration(const String& codecName, const AudioEncoder::Config& config)
{
    auto result = AudioDecoderCocoa::isCodecSupported(codecName);
    if (!result)
        return makeUnexpected(result.error());

    InternalConfig internalConfig;

    std::tie(internalConfig.codec, internalConfig.pcmFormat) = *result;
    if (internalConfig.codec == 'vorb')
        return makeUnexpected("Vorbis encoding is not supported"_s);
    if (internalConfig.codec == kAudioFormatFLAC)
        return makeUnexpected("FLAC encoding is not supported"_s);
    if (internalConfig.codec == kAudioFormatMPEGLayer3)
        return makeUnexpected("MP3 encoding is not supported"_s);

    if (internalConfig.codec == kAudioFormatOpus) {
        if (config.numberOfChannels > 2)
            return makeUnexpected("Opus with more than two channels is not supported"_s);

        if (config.opusConfig && config.opusConfig->isOggBitStream)
            return makeUnexpected("Opus Ogg format is unsupported"_s);

        auto opusConfig = config.opusConfig.value_or(AudioEncoder::OpusConfig { });
        internalConfig.frameDuration = MediaTime(opusConfig.frameDuration, 1000000);
        internalConfig.complexity = opusConfig.complexity;
        internalConfig.packetlossperc = opusConfig.packetlossperc;
        internalConfig.useinbandfec = opusConfig.useinbandfec;
    } else if ((internalConfig.codec == kAudioFormatMPEG4AAC || internalConfig.codec == kAudioFormatMPEG4AAC_HE || internalConfig.codec == kAudioFormatMPEG4AAC_LD || internalConfig.codec == kAudioFormatMPEG4AAC_HE_V2 || internalConfig.codec == kAudioFormatMPEG4AAC_ELD)) {
        if (config.isAacADTS.value_or(false))
            return makeUnexpected("AAC ADTS format is unsupported"_s);
    }

    if (internalConfig.pcmFormat)
        internalConfig.outputDescription = CAAudioStreamDescription { double(config.sampleRate), uint32_t(config.numberOfChannels), *internalConfig.pcmFormat, CAAudioStreamDescription::IsInterleaved::Yes };
    else {
        AudioStreamBasicDescription asbd { };
        asbd.mFormatID = internalConfig.codec;
        asbd.mSampleRate = double(config.sampleRate);
        asbd.mChannelsPerFrame = uint32_t(config.numberOfChannels);
        if (internalConfig.frameDuration)
            asbd.mFramesPerPacket = config.sampleRate * internalConfig.frameDuration->toDouble();
        internalConfig.outputDescription = asbd;
    }

    return internalConfig;
}

Ref<AudioEncoder::CreatePromise> AudioEncoderCocoa::create(const String& codecName, const AudioEncoder::Config& config, DescriptionCallback&& descriptionCallback, OutputCallback&& outputCallback)
{
    auto result = InternalAudioEncoderCocoa::checkConfiguration(codecName, config);
    if (!result)
        return CreatePromise::createAndReject(makeString("AudioEncoder initialization failed with error: "_s, result.error()));
    Ref internalEncoder = InternalAudioEncoderCocoa::create(config, WTF::move(*result), WTF::move(descriptionCallback), WTF::move(outputCallback));
    Ref encoder = adoptRef(*new AudioEncoderCocoa(WTF::move(internalEncoder)));
    return CreatePromise::createAndResolve(WTF::move(encoder));
}

AudioEncoderCocoa::AudioEncoderCocoa(Ref<InternalAudioEncoderCocoa>&& internalEncoder)
    : m_internalEncoder(WTF::move(internalEncoder))
{
}

AudioEncoderCocoa::~AudioEncoderCocoa()
{
    // We need to ensure the internal decoder is closed and the audio converter finished.
    InternalAudioEncoderCocoa::queueSingleton().dispatch([encoder = m_internalEncoder] {
        encoder->close();
    });
}

Ref<AudioEncoder::EncodePromise> AudioEncoderCocoa::encode(RawFrame&& frame)
{
    return invokeAsync(InternalAudioEncoderCocoa::queueSingleton(), [frame = WTF::move(frame), encoder = m_internalEncoder]() mutable {
        return encoder->encode(WTF::move(frame));
    });
}

Ref<GenericPromise> AudioEncoderCocoa::flush()
{
    return invokeAsync(InternalAudioEncoderCocoa::queueSingleton(), [encoder = m_internalEncoder] {
        return encoder->flush();
    });
}

void AudioEncoderCocoa::reset()
{
    InternalAudioEncoderCocoa::queueSingleton().dispatch([encoder = m_internalEncoder] {
        encoder->close();
    });
}

void AudioEncoderCocoa::close()
{
    InternalAudioEncoderCocoa::queueSingleton().dispatch([encoder = m_internalEncoder] {
        encoder->close();
    });
}

InternalAudioEncoderCocoa::InternalAudioEncoderCocoa(const Config& config, InternalConfig&& internalConfig, AudioEncoder::DescriptionCallback&& descriptionCallback, AudioEncoder::OutputCallback&& outputCallback)
    : m_descriptionCallback(WTF::move(descriptionCallback))
    , m_outputCallback(WTF::move(outputCallback))
    , m_config(config)
    , m_internalConfig(WTF::move(internalConfig))
{
}

void InternalAudioEncoderCocoa::compressedAudioOutputBufferCallback(void* object, CMBufferQueueTriggerToken)
{
    // We can only be called from the CoreMedia callback if we are still alive.
    RefPtr encoder = static_cast<class InternalAudioEncoderCocoa*>(object);

    InternalAudioEncoderCocoa::queueSingleton().dispatch([weakEncoder = ThreadSafeWeakPtr { *encoder }] {
        if (auto strongEncoder = weakEncoder.get())
            strongEncoder->processEncodedOutputs();
    });
}

Vector<uint8_t> InternalAudioEncoderCocoa::generateDecoderDescriptionFromSample(CMSampleBufferRef sample) const
{
    RetainPtr formatDescription = PAL::CMSampleBufferGetFormatDescription(sample);
    ASSERT(formatDescription);
    const AudioStreamBasicDescription* const asbd = PAL::CMAudioFormatDescriptionGetStreamBasicDescription(formatDescription.get());
    if (!asbd)
        return { };

    if (asbd->mFormatID == kAudioFormatOpus)
        return createOpusPrivateData(*asbd, m_converter->preSkip());

    size_t cookieSize = 0;
    auto* cookie = PAL::CMAudioFormatDescriptionGetMagicCookie(formatDescription.get(), &cookieSize);
    if (!cookieSize)
        return { };
    return unsafeMakeSpan(static_cast<const uint8_t*>(cookie), cookieSize);
}

AudioEncoder::ActiveConfiguration InternalAudioEncoderCocoa::activeConfiguration(CMSampleBufferRef sample) const
{
    assertIsCurrent(queueSingleton());
    ASSERT(!m_isClosed && m_converter);

    RetainPtr formatDescription = PAL::CMSampleBufferGetFormatDescription(sample);
    ASSERT(formatDescription);
    const AudioStreamBasicDescription* const asbd = PAL::CMAudioFormatDescriptionGetStreamBasicDescription(formatDescription.get());
    if (!asbd)
        return { };

    return AudioEncoder::ActiveConfiguration {
        .sampleRate = asbd->mSampleRate,
        .numberOfChannels = asbd->mChannelsPerFrame,
        .bitrate = converter()->bitRate(),
        .description = generateDecoderDescriptionFromSample(sample)
    };
}

void InternalAudioEncoderCocoa::processEncodedOutputs()
{
    assertIsCurrent(queueSingleton());

    if (m_isClosed)
        return;

    while (RetainPtr cmSample = converter()->takeOutputSampleBuffer()) {
        Ref sample = MediaSampleAVFObjC::create(cmSample.get(), 0);
        RetainPtr rawBuffer = PAL::CMSampleBufferGetDataBuffer(cmSample.get());
        ASSERT(rawBuffer);
        // Make sure block buffer is contiguous.
        if (!PAL::CMBlockBufferIsRangeContiguous(rawBuffer.get(), 0, 0)) {
            CMBlockBufferRef contiguousBuffer;
            if (auto error = PAL::CMBlockBufferCreateContiguous(nullptr, rawBuffer.get(), nullptr, nullptr, 0, 0, 0, &contiguousBuffer)) {
                RELEASE_LOG_ERROR(MediaStream, "Couldn't create buffer with error %d", error);
                m_lastError = error;
                continue;
            }
            rawBuffer = adoptCF(contiguousBuffer);
        }
        auto size = PAL::CMBlockBufferGetDataLength(rawBuffer.get());
        char* data = nullptr;
        if (auto error = PAL::CMBlockBufferGetDataPointer(rawBuffer.get(), 0, nullptr, nullptr, &data)) {
            RELEASE_LOG_ERROR(MediaStream, "Couldn't create buffer with error %d", error);
            m_lastError = error;
            continue;
        }

        std::optional<uint64_t> duration;
        if (auto sampleDuration = sample->duration(); sampleDuration.isValid())
            duration = sampleDuration.toMicroseconds();

        AudioEncoder::EncodedFrame encodedFrame {
            .data = unsafeMakeSpan(byteCast<uint8_t>(data), size),
            .isKeyFrame = sample->isSync(),
            .timestamp = sample->presentationTime().toMicroseconds(),
            .duration = duration
        };

        if (!m_hasProvidedDecoderConfig) {
            m_hasProvidedDecoderConfig = true;
            m_descriptionCallback(activeConfiguration(cmSample.get()));
        }
        m_outputCallback({ WTF::move(encodedFrame) });
    }
}

Ref<AudioEncoder::EncodePromise> InternalAudioEncoderCocoa::encode(AudioEncoder::RawFrame&& rawFrame)
{
    assertIsCurrent(queueSingleton());

    RetainPtr cmSample = downcast<PlatformRawAudioDataCocoa>(rawFrame.frame)->sampleBuffer();
    ASSERT(cmSample);
    if (auto error = PAL::CMSampleBufferSetOutputPresentationTimeStamp(cmSample.get(), PAL::CMTimeMake(rawFrame.timestamp, 1000000)))
        RELEASE_LOG_ERROR(MediaStream, "AudioSampleBufferConverter CMSampleBufferSetOutputPresentationTimeStamp failed with %d", error);

    RetainPtr formatDescription = PAL::CMSampleBufferGetFormatDescription(cmSample.get());
    if (!formatDescription)
        return EncodePromise::createAndReject("Couldn't retrieve AudioData's format description"_s);
    const AudioStreamBasicDescription* const asbd = PAL::CMAudioFormatDescriptionGetStreamBasicDescription(formatDescription.get());
    if (!asbd)
        return EncodePromise::createAndReject("Couldn't retrieve AudioData's basic description"_s);

    if (*asbd != m_inputDescription) {
        if (RefPtr converter = std::exchange(m_converter, { }))
            converter->finish();
        m_inputDescription = *asbd;

        AudioSampleBufferConverter::Options options = {
            .format = m_internalConfig.codec,
            .description = m_internalConfig.outputDescription->streamDescription(),
            .outputBitRate = m_config.bitRate ? std::optional { m_config.bitRate } : std::nullopt,
            .generateTimestamp = false,
            .bitrateMode = m_config.bitRateMode,
            .complexity = m_internalConfig.complexity,
            .packetlossperc = m_internalConfig.packetlossperc,
            .useinbandfec = m_internalConfig.useinbandfec
        };
        m_converter = AudioSampleBufferConverter::create(compressedAudioOutputBufferCallback, this, options);
        if (!m_converter) {
            RELEASE_LOG_ERROR(MediaStream, "InternalAudioEncoderCocoa::encode: creation of converter failed");
            return EncodePromise::createAndReject("InternalAudioEncoderCocoa::encode: creation of converter failed"_s);
        }
    }

    m_needsFlushing = true;

    ASSERT(m_converter);
    AudioEncoder::EncodePromise::Producer producer;
    Ref promise = producer.promise();
    converter()->addSampleBuffer(cmSample.get())->whenSettled(queueSingleton(), [weakThis = ThreadSafeWeakPtr { *this }, producer = WTF::move(producer)](auto result) mutable {
        assertIsCurrent(queueSingleton());
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || !result || protectedThis->m_lastError) {
            producer.reject("InternalAudioEncoderCocoa encoding failed"_s);
            return;
        }
        protectedThis->processEncodedOutputs();
        producer.resolve();
    });
    return promise;
}

Ref<GenericPromise> InternalAudioEncoderCocoa::flush()
{
    assertIsCurrent(queueSingleton());

    if (!m_converter || !m_needsFlushing)
        return GenericPromise::createAndResolve(); // No frame encoded yet, nothing to flush;
    return converter()->drain()->whenSettled(queueSingleton(), [protectedThis = Ref { *this }](auto&& result) {
        assertIsCurrent(queueSingleton());
        protectedThis->processEncodedOutputs();
        protectedThis->m_needsFlushing = false;
        return (protectedThis->m_lastError || !result) ? GenericPromise::createAndReject() : GenericPromise::createAndResolve();
    });
}

void InternalAudioEncoderCocoa::close()
{
    assertIsCurrent(queueSingleton());

    m_isClosed = true;
    RefPtr converter = std::exchange(m_converter, { });
    if (!converter)
        return;
    // We keep a reference to ourselves until the converter has been marked as finished. This guarantees that no
    // callback will occur after we are destructed.
    converter->finish()->whenSettled(queueSingleton(), [protectedThis = Ref { *this }] { });
}

} // namespace WebCore

#endif // ENABLE(WEB_CODECS) && USE(AVFOUNDATION)
