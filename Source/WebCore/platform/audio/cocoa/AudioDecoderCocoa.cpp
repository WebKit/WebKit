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
#include "AudioDecoderCocoa.h"

#if ENABLE(WEB_CODECS) && USE(AVFOUNDATION)

#include "AudioSampleBufferConverter.h"
#include "AudioSampleFormat.h"
#include "CAAudioStreamDescription.h"
#include "MediaSampleAVFObjC.h"
#include "MediaUtilities.h"
#include "PlatformRawAudioData.h"
#include "SharedBuffer.h"
#include <CoreAudio/CoreAudioTypes.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/UniqueRef.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/MakeString.h>

#include <pal/cf/AudioToolboxSoftLink.h>
#include <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(AudioDecoderCocoa);

static WorkQueue& queueSingleton()
{
    static std::once_flag onceKey;
    static LazyNeverDestroyed<Ref<WorkQueue>> workQueue;
    std::call_once(onceKey, [] {
        workQueue.construct(WorkQueue::create("AudioDecoder queue"_s));
    });
    return workQueue.get();
}

class InternalAudioDecoderCocoa : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<InternalAudioDecoderCocoa> {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(InternalAudioDecoderCocoa);

public:
    static Ref<InternalAudioDecoderCocoa> create(AudioDecoder::OutputCallback&& outputCallback)
    {
        return adoptRef(*new InternalAudioDecoderCocoa(WTFMove(outputCallback)));
    }
    virtual ~InternalAudioDecoderCocoa() = default;

    String initialize(const String& codecName, const AudioDecoder::Config&);

    Ref<AudioDecoder::DecodePromise> decode(Ref<SharedBuffer>&&, bool isKeyFrame, int64_t timestamp, std::optional<uint64_t> duration);
    Ref<GenericPromise> flush();
    void close();
    bool isConfigured() const { return !!m_converter; }

private:
    InternalAudioDecoderCocoa(AudioDecoder::OutputCallback&&);

    static void decompressedAudioOutputBufferCallback(void*, CMBufferQueueTriggerToken);
    Ref<AudioSampleBufferConverter> converter() { return *m_converter; }
    void processedDecodedOutputs();

    AudioDecoder::OutputCallback m_outputCallback;
    RefPtr<AudioSampleBufferConverter> m_converter;

    Ref<SharedBuffer> stripADTSHeader(SharedBuffer&);

    const bool m_configured { false };
    bool m_isClosed { false };
    Vector<uint8_t> m_codecDescription;
    std::optional<CAAudioStreamDescription> m_inputDescription;
    RetainPtr<CMFormatDescriptionRef> m_inputFormatDescription;
    std::optional<CAAudioStreamDescription> m_outputDescription;
    bool mIsAAC { false }; // indicate if we need to strip the ADTS header.
};

Ref<AudioDecoder::CreatePromise> AudioDecoderCocoa::create(const String& codecName, const Config& config, OutputCallback&& outputCallback)
{
    auto decoder = adoptRef(*new AudioDecoderCocoa(WTFMove(outputCallback)));
    auto internalDecoder = decoder->m_internalDecoder;
    if (auto error = internalDecoder->initialize(codecName, config); !error.isEmpty())
        return CreatePromise::createAndReject(makeString("Internal audio decoder failed to configure for codec "_s, codecName, " with error "_s, error));
    return CreatePromise::createAndResolve(WTFMove(decoder));
}

AudioDecoderCocoa::AudioDecoderCocoa(OutputCallback&& outputCallback)
    : m_internalDecoder(InternalAudioDecoderCocoa::create(WTFMove(outputCallback)))
{
}

AudioDecoderCocoa::~AudioDecoderCocoa() = default;

Expected<std::pair<FourCharCode, std::optional<AudioStreamDescription::PCMFormat>>, String> AudioDecoderCocoa::isCodecSupported(const StringView& codecName)
{
    FourCharCode codec = 0;
    std::optional<AudioStreamDescription::PCMFormat> format;

    if (codecName.startsWith("mp4a"_s)) {
        if (codecName.startsWith("mp4a.40.2"_s))
            codec = kAudioFormatMPEG4AAC;
        else if (codecName.startsWith("mp4a.40.5"_s))
            codec = kAudioFormatMPEG4AAC_HE;
        else if (codecName.startsWith("mp4a.40.23"_s))
            codec = kAudioFormatMPEG4AAC_LD;
        else if (codecName.startsWith("mp4a.40.29"_s))
            codec = kAudioFormatMPEG4AAC_HE_V2;
        else if (codecName.startsWith("mp4a.40.39"_s))
            codec = kAudioFormatMPEG4AAC_ELD;
        else
            codec = kAudioFormatMPEG4AAC;
    } else if (codecName == "mp3"_s)
        codec = kAudioFormatMPEGLayer3;
    else if (codecName == "opus"_s) {
        codec = kAudioFormatOpus;
    } else if (codecName == "alaw"_s)
        codec = kAudioFormatALaw;
    else if (codecName == "ulaw"_s)
        codec = kAudioFormatULaw;
    else if (codecName == "flac"_s) {
        codec = kAudioFormatFLAC;
    } else if (codecName == "vorbis"_s) {
        codec = 'vorb';
    } else if (codecName.startsWith("pcm-"_s)) {
        auto components = codecName.toString().split('-');
        if (components.size() != 2)
            return makeUnexpected(makeString("Invalid LPCM codec string:"_s, codecName));
        auto pcmFormat = components[1].convertToASCIILowercase();
        codec = kAudioFormatLinearPCM;
        if (pcmFormat == "u8"_s)
            format = AudioStreamDescription::Uint8;
        else if (pcmFormat == "s16"_s)
            format = AudioStreamDescription::Int16;
        else if (pcmFormat == "s24"_s)
            format = AudioStreamDescription::Int24;
        else if (pcmFormat == "s32"_s)
            format = AudioStreamDescription::Int32;
        else if (pcmFormat == "f32"_s)
            format = AudioStreamDescription::Float32;
        else
            return makeUnexpected(makeString("Invalid LPCM codec format:"_s, pcmFormat));
    } else
        return makeUnexpected("not supported codec"_s);

    return std::make_pair(codec, format);
}

Ref<AudioDecoder::DecodePromise> AudioDecoderCocoa::decode(EncodedData&& data)
{
    return invokeAsync(queueSingleton(), [data = SharedBuffer::create(data.data), isKeyFrame = data.isKeyFrame, timestamp = data.timestamp, duration = data.duration, decoder = m_internalDecoder]() mutable {
        return decoder->decode(WTFMove(data), isKeyFrame, timestamp, duration);
    });
}

Ref<GenericPromise> AudioDecoderCocoa::flush()
{
    return invokeAsync(queueSingleton(), [decoder = m_internalDecoder] {
        return decoder->flush();
    });
}

void AudioDecoderCocoa::reset()
{
    // FIXME: that can't be correct
    m_internalDecoder->close();
}

void AudioDecoderCocoa::close()
{
    queueSingleton().dispatch([decoder = m_internalDecoder] {
        decoder->close();
    });
}

void InternalAudioDecoderCocoa::decompressedAudioOutputBufferCallback(void* object, CMBufferQueueTriggerToken)
{
    // We can only be called from the CoreMedia callback if we are still alive.
    RefPtr decoder = static_cast<class InternalAudioDecoderCocoa*>(object);

    queueSingleton().dispatch([weakDecoder = ThreadSafeWeakPtr { *decoder }] {
        if (auto strongDecoder = weakDecoder.get())
            strongDecoder->processedDecodedOutputs();
    });
}

void InternalAudioDecoderCocoa::processedDecodedOutputs()
{
    if (m_isClosed)
        return;
    while (RetainPtr sample = converter()->takeOutputSampleBuffer()) {
        Ref audioData = PlatformRawAudioData::create(MediaSampleAVFObjC::create(sample.get(), 0));
        m_outputCallback(AudioDecoder::DecodedData { WTFMove(audioData) });
    }
}

InternalAudioDecoderCocoa::InternalAudioDecoderCocoa(AudioDecoder::OutputCallback&& outputCallback)
    : m_outputCallback(WTFMove(outputCallback))
{
}

String InternalAudioDecoderCocoa::initialize(const String& codecName, const AudioDecoder::Config& config)
{
    auto result = AudioDecoderCocoa::isCodecSupported(codecName);
    if (!result)
        return result.error();

    auto [codec, format] = *result;
    if (!format) {
        if (codec == kAudioFormatOpus && config.numberOfChannels >= 2)
            return "Opus with more than 2 channels isn't supported"_s;

        if (codec == kAudioFormatFLAC && config.description.empty())
            return "Decoder config description for flac codec is mandatory"_s;

        if (codec == 'vorb' && config.description.empty())
            return "Decoder config description for vorbis codec is mandatory"_s;

        mIsAAC = codec == kAudioFormatMPEG4AAC || codec == kAudioFormatMPEG4AAC_HE || codec == kAudioFormatMPEG4AAC_LD || codec == kAudioFormatMPEG4AAC_HE_V2 || codec == kAudioFormatMPEG4AAC_ELD;

        AudioStreamBasicDescription absd { };
        absd.mFormatID = codec;
        // Attempt to create description with provided cookie.
        bool succeeded = false;
        if (config.description.size()) {
            UInt32 size = sizeof(absd);
            succeeded = PAL::AudioFormatGetProperty(kAudioFormatProperty_FormatInfo, config.description.size(), config.description.data(), &size, &absd) == noErr;
        }
        if (!succeeded) {
            absd.mSampleRate = double(config.sampleRate);
            absd.mChannelsPerFrame = uint32_t(config.numberOfChannels);
        }
        m_inputDescription = absd;
    } else
        m_inputDescription = CAAudioStreamDescription { double(config.sampleRate), uint32_t(config.numberOfChannels), *format, CAAudioStreamDescription::IsInterleaved::Yes };

    m_inputFormatDescription = createAudioFormatDescription(*m_inputDescription, m_codecDescription.span());

    m_outputDescription = CAAudioStreamDescription { double(config.sampleRate), uint32_t(config.numberOfChannels), AudioStreamDescription::Float32, CAAudioStreamDescription::IsInterleaved::No };

    AudioSampleBufferConverter::Options options = {
        .format = kAudioFormatLinearPCM,
        .description = m_outputDescription->streamDescription(),
        .generateTimestamp = false
    };

    m_converter = AudioSampleBufferConverter::create(decompressedAudioOutputBufferCallback, this, options);
    if (!m_converter)
        return "Couldn't create AudioSampleBufferConverter"_s;

    return { };
}

Ref<SharedBuffer> InternalAudioDecoderCocoa::stripADTSHeader(SharedBuffer& buffer)
{
    static constexpr int kADTSHeaderSize = 7;
    if (buffer.size() < kADTSHeaderSize)
        return buffer;

    auto matchesSync = [](std::span<const uint8_t> data) {
        return data.size() >= 2 && data[0] == 0xff && (data[1] & 0xf6) == 0xf0;
    };

    if (!matchesSync(buffer.span()))
        return buffer;

    return buffer.getContiguousData(kADTSHeaderSize, buffer.size() - kADTSHeaderSize);
}

Ref<AudioDecoder::DecodePromise> InternalAudioDecoderCocoa::decode(Ref<SharedBuffer>&& frameData, [[maybe_unused]] bool isKeyFrame, int64_t timestamp, std::optional<uint64_t> duration)
{
    LOG(Media, "Decoding%s frame", isKeyFrame ? " key" : "");

    if (mIsAAC)
        frameData = stripADTSHeader(frameData);

    if (m_isClosed)
        return AudioDecoder::DecodePromise::createAndReject("Decoder is closed"_s);

    RetainPtr<CMBlockBufferRef> completeBlockBuffers;

    RetainPtr blockBuffer = frameData->createCMBlockBuffer();
    if (!blockBuffer)
        return AudioDecoder::DecodePromise::createAndReject("Couldn't create CMBlockBuffer"_s);

    CMSampleTimingInfo packetTiming = {
        .duration = PAL::CMTimeMake(duration.value_or(0), 1000000), // CoreMedia does not deal with a CMSampleBuffer with a duration set to either invalid or indefinite. So use 0 instead if no duration has been provided.
        .presentationTimeStamp = PAL::CMTimeMake(timestamp, 1000000),
        .decodeTimeStamp = PAL::CMTimeMake(timestamp, 1000000)
    };
    size_t packetSize = frameData->size();

    CMSampleBufferRef rawSampleBuffer = nullptr;
    if (auto err = PAL::CMSampleBufferCreateReady(kCFAllocatorDefault, blockBuffer.get(), m_inputFormatDescription.get(), 1, 1, &packetTiming, 1, &packetSize, &rawSampleBuffer))
        return AudioDecoder::DecodePromise::createAndReject("CMSampleBufferCreateReady failed: OOM"_s);

    AudioDecoder::DecodePromise::Producer producer;
    Ref promise = producer.promise();
    m_converter->addSampleBuffer(rawSampleBuffer)->whenSettled(queueSingleton(), [producer = WTFMove(producer)](auto result) mutable {
        if (result)
            producer.resolve();
        else
            producer.reject("InternalAudioDecoderCocoa decoding failed"_s);
    });
    return promise;
}

Ref<GenericPromise> InternalAudioDecoderCocoa::flush()
{
    if (m_isClosed) {
        LOG(Media, "Decoder closed, nothing to flush");
        return GenericPromise::createAndResolve();
    }
    return m_converter->flush()->whenSettled(queueSingleton(), [protectedThis = Ref { *this }] {
        protectedThis->processedDecodedOutputs();
        return GenericPromise::createAndResolve();
    });
}

void InternalAudioDecoderCocoa::close()
{
    m_isClosed = true;
    if (!m_converter)
        return;
    // Flush to ensure no callback calls us after we're done.
    m_converter->drain()->whenSettled(queueSingleton(), [protectedThis = Ref { *this }] { });
}

} // namespace WebCore

#endif // ENABLE(WEB_CODECS) && USE(AVFOUNDATION)
