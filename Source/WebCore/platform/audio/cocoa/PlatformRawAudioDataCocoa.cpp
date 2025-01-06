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
#include "PlatformRawAudioDataCocoa.h"

#if ENABLE(WEB_CODECS) && USE(AVFOUNDATION)

#include "AudioSampleFormat.h"
#include "CAAudioStreamDescription.h"
#include "CMUtilities.h"
#include "MediaSampleAVFObjC.h"
#include "NotImplemented.h"
#include "SharedBuffer.h"
#include "WebAudioBufferList.h"
#include "WebCodecsAudioDataAlgorithms.h"

#include <CoreAudio/CoreAudioTypes.h>
#include <wtf/MediaTime.h>

#include <pal/cf/CoreMediaSoftLink.h>

using namespace WTF;

namespace WebCore {

Ref<PlatformRawAudioData> PlatformRawAudioData::create(Ref<MediaSample>&& sample)
{
    return PlatformRawAudioDataCocoa::create(downcast<MediaSampleAVFObjC>(WTFMove(sample)));
}

static AudioStreamDescription::PCMFormat audioSampleFormatToPCMFormat(AudioSampleFormat format)
{
    switch (format) {
    case AudioSampleFormat::U8:
        return AudioStreamDescription::Uint8;
    case AudioSampleFormat::S16:
        return AudioStreamDescription::Int16;
    case AudioSampleFormat::S32:
        return AudioStreamDescription::Int32;
    case AudioSampleFormat::F32:
        return AudioStreamDescription::Float32;
    case AudioSampleFormat::U8Planar:
        return AudioStreamDescription::Uint8;
    case AudioSampleFormat::S16Planar:
        return AudioStreamDescription::Int16;
    case AudioSampleFormat::S32Planar:
        return AudioStreamDescription::Int32;
    case AudioSampleFormat::F32Planar:
        return AudioStreamDescription::Float32;
    }
}

static CAAudioStreamDescription::IsInterleaved interleavedFormat(AudioSampleFormat format)
{
    return isAudioSampleFormatInterleaved(format) ? CAAudioStreamDescription::IsInterleaved::Yes : CAAudioStreamDescription::IsInterleaved::No;
}

static RetainPtr<CMSampleBufferRef> createSampleBuffer(const CAAudioStreamDescription& description, const CMTime& time, size_t numberOfFrames, const WebAudioBufferList& list)
{
    CMAudioFormatDescriptionRef rawFormatDescription;
    if (PAL::CMAudioFormatDescriptionCreate(kCFAllocatorDefault, &description.streamDescription(), 0, nullptr, 0, nullptr, nullptr, &rawFormatDescription))
        return nullptr;
    RetainPtr formatDescription = adoptCF(rawFormatDescription);

    CMSampleBufferRef rawSampleBuffer;
    if (PAL::CMAudioSampleBufferCreateWithPacketDescriptions(kCFAllocatorDefault, nullptr, false, nullptr, nullptr, rawFormatDescription, numberOfFrames, time, nullptr, &rawSampleBuffer))
        return nullptr;
    auto sampleBuffer = adoptCF(rawSampleBuffer);

    if (auto error = PAL::CMSampleBufferSetDataBufferFromAudioBufferList(sampleBuffer.get(), kCFAllocatorDefault, kCFAllocatorDefault, kCMSampleBufferFlag_AudioBufferList_Assure16ByteAlignment, list.list())) {
        RELEASE_LOG_ERROR(MediaStream, "PlatformRawAudioData::create createSampleBuffer couldn't allocate memory with error %d", error);
        return nullptr;
    }
    return sampleBuffer;

    auto result = WebAudioBufferList::createWebAudioBufferListWithBlockBuffer(description, numberOfFrames);
    if (!result)
        return nullptr;

    auto [newList, blockBuffer] = WTFMove(*result);
    if (PAL::CMSampleBufferSetDataBuffer(rawSampleBuffer, blockBuffer.get()))
        return nullptr;
    return sampleBuffer;
}

RefPtr<PlatformRawAudioData> PlatformRawAudioData::create(std::span<const uint8_t> sourceData, AudioSampleFormat format, float sampleRate, int64_t timestamp, size_t numberOfFrames, size_t numberOfChannels)
{
    // WebCodecsAudioDataAlgorithms's isValidAudioDataInit performs the checks ensuring all of the input parameters are valid (numberOfChannels != 0, numberOfFrames != 0 etc)
    CAAudioStreamDescription inputDescription(sampleRate, numberOfChannels, audioSampleFormatToPCMFormat(format), interleavedFormat(format));
    if (!WebAudioBufferList::isSupportedDescription(inputDescription, numberOfFrames)) {
        RELEASE_LOG_ERROR(MediaStream, "PlatformRawAudioData::create not supported configuration");
        return nullptr;
    }

    size_t sizePlane;
    if (!WTF::safeMultiply(numberOfFrames, inputDescription.bytesPerFrame(), sizePlane)) {
        RELEASE_LOG_ERROR(MediaStream, "PlatformRawAudioData::create overflow");
        return nullptr;
    }
    WebAudioBufferList inputList = inputDescription;
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    uint8_t* data = const_cast<uint8_t*>(sourceData.data());
    for (auto& buffer : inputList.buffers()) {
        buffer.mData = data;
        buffer.mNumberChannels = inputDescription.numberOfInterleavedChannels();
        buffer.mDataByteSize = sizePlane;
        data += sizePlane;
    }
    if (data > sourceData.data() + sourceData.size()) {
        RELEASE_LOG_ERROR(MediaStream, "PlatformRawAudioData::create nonsensical format data");
        return nullptr;
    }
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

    RetainPtr sample = createSampleBuffer(inputDescription, PAL::CMTimeMake(timestamp, 1000000), numberOfFrames, inputList);
    if (!sample) {
        RELEASE_LOG_ERROR(MediaStream, "PlatformRawAudioData::create failed");
        return nullptr;
    }

    return PlatformRawAudioDataCocoa::create(MediaSampleAVFObjC::create(sample.get(), 0));
}

PlatformRawAudioDataCocoa::PlatformRawAudioDataCocoa(Ref<MediaSampleAVFObjC>&& sample)
    : m_sample(WTFMove(sample))
    , m_description(asbd())
{
}

const AudioStreamBasicDescription& PlatformRawAudioDataCocoa::asbd() const
{
    auto description = PAL::CMSampleBufferGetFormatDescription(m_sample->sampleBuffer());
    ASSERT(description);
    const AudioStreamBasicDescription* const asbd = PAL::CMAudioFormatDescriptionGetStreamBasicDescription(description);
    ASSERT(asbd);
    return *asbd;
}

AudioSampleFormat PlatformRawAudioDataCocoa::format() const
{
    switch (m_description.format()) {
    case AudioStreamDescription::Uint8:
        return m_description.isInterleaved() ? AudioSampleFormat::U8 : AudioSampleFormat::U8Planar;
    case AudioStreamDescription::Int16:
        return m_description.isInterleaved() ? AudioSampleFormat::S16 : AudioSampleFormat::S16Planar;
    case AudioStreamDescription::Int32:
        return m_description.isInterleaved() ? AudioSampleFormat::S32 : AudioSampleFormat::S32Planar;
    case AudioStreamDescription::Float32:
        return m_description.isInterleaved() ? AudioSampleFormat::F32 : AudioSampleFormat::F32Planar;
    default:
        ASSERT_NOT_REACHED();
        return AudioSampleFormat::F32;
    }
}

size_t PlatformRawAudioDataCocoa::sampleRate() const
{
    return m_description.sampleRate();
}

size_t PlatformRawAudioDataCocoa::numberOfChannels() const
{
    return m_description.numberOfChannels();
}

size_t PlatformRawAudioDataCocoa::numberOfFrames() const
{
    return PAL::CMSampleBufferGetNumSamples(m_sample->sampleBuffer());
}

std::optional<uint64_t> PlatformRawAudioDataCocoa::duration() const
{
    return MediaTime(numberOfFrames(), sampleRate()).toMicroseconds();
}

int64_t PlatformRawAudioDataCocoa::timestamp() const
{
    return m_sample->presentationTime().toMicroseconds();
}

size_t PlatformRawAudioDataCocoa::memoryCost() const
{
    return m_sample->sizeInBytes();
}

bool PlatformRawAudioDataCocoa::isInterleaved() const
{
    return m_description.isInterleaved();
}

static std::variant<Vector<std::span<uint8_t>>, Vector<std::span<int16_t>>, Vector<std::span<int32_t>>, Vector<std::span<float>>> planesOfSamples(AudioSampleFormat format, const WebAudioBufferList& list, size_t samplesOffset)
{
    auto subspan = [samplesOffset](auto span) {
        RELEASE_ASSERT(samplesOffset <= span.size());
        return span.subspan(samplesOffset, span.size() - samplesOffset);
    };
    switch (format) {
    case AudioSampleFormat::U8:
    case AudioSampleFormat::U8Planar:
        return Vector<std::span<uint8_t>> { list.bufferCount(), [&](auto index) {
            return subspan(list.bufferAsSpan<uint8_t>(index));
        } };
    case AudioSampleFormat::S16:
    case AudioSampleFormat::S16Planar:
        return Vector<std::span<int16_t>> { list.bufferCount(), [&](auto index) {
            return subspan(list.bufferAsSpan<int16_t>(index));
        } };
    case AudioSampleFormat::S32:
    case AudioSampleFormat::S32Planar:
        return Vector<std::span<int32_t>> { list.bufferCount(), [&](auto index) {
            return subspan(list.bufferAsSpan<int32_t>(index));
        } };
    case AudioSampleFormat::F32:
    case AudioSampleFormat::F32Planar:
        return Vector<std::span<float>> { list.bufferCount(), [&](auto index) {
            return subspan(list.bufferAsSpan<float>(index));
        } };
    }
    RELEASE_ASSERT_NOT_REACHED();
    return Vector<std::span<uint8_t>> { };
}

void PlatformRawAudioData::copyTo(std::span<uint8_t> destination, AudioSampleFormat destinationFormat, size_t planeIndex, std::optional<size_t> frameOffset, std::optional<size_t>, unsigned long copyElementCount)
{
    // WebCodecsAudioDataAlgorithms's computeCopyElementCount ensures that all parameters are correct.
    auto& audioData = downcast<PlatformRawAudioDataCocoa>(*this);

    auto sourceFormat = format();
    WebAudioBufferList sourceList(audioData.m_description, audioData.sampleBuffer());
    bool destinationIsInterleaved = isAudioSampleFormatInterleaved(destinationFormat);

    if (audioSampleElementFormat(sourceFormat) == audioSampleElementFormat(destinationFormat)) {
        if (numberOfChannels() == 1 || (audioData.isInterleaved() && destinationIsInterleaved)) {
            // Simplest case.
            ASSERT(!planeIndex);
            auto source = sourceList.bufferAsSpan(0);
            size_t frameOffsetInBytes = frameOffset.value_or(0) * audioData.m_description.bytesPerFrame();
            RELEASE_ASSERT(frameOffsetInBytes <= source.size());
            auto subSource = source.subspan(frameOffsetInBytes, source.size() - frameOffsetInBytes);
            memcpySpan(destination, subSource);
            return;
        }
    }

    auto source = planesOfSamples(sourceFormat, sourceList, frameOffset.value_or(0) * (audioData.isInterleaved() ? numberOfChannels() : 1));

    if (!audioData.isInterleaved() && destinationIsInterleaved) {
        // Copy of all channels of the source into the destination buffer and deinterleave.
        // Ideally we would use an AudioToolbox's AudioConverter but it performs incorrect rounding during sample conversion in a way that makes us fail the W3C's AudioData tests.
        ASSERT(!planeIndex);
        ASSERT(!(copyElementCount % numberOfChannels()));

        auto copyElements = [numberOfChannels = numberOfChannels()]<typename T>(std::span<T> destination, auto& source, size_t frames) {
            RELEASE_ASSERT(destination.size() >= frames * numberOfChannels);
            RELEASE_ASSERT(source[0].size() >= frames); // All planes have the exact same size.
            size_t index = 0;
            for (size_t frame = 0; frame < frames; frame++) {
                for (size_t channel = 0; channel < source.size(); channel++)
                    destination[index++] = convertAudioSample<T>(source[channel][frame]);
            }
        };

        switchOn(audioElementSpan(destinationFormat, destination), [&](auto dst) {
            switchOn(source, [&](auto& src) {
                size_t numberOfFrames = copyElementCount / numberOfChannels();
                copyElements(dst, src, numberOfFrames);
            });
        });
        return;
    }

    // interleaved -> interleaved
    // planar -> planar
    // interleaved -> planar

    // Interleaved to planar, only copy samples of the correct channel (plane) to the destination.
    // If destination is interleaved, copy of all channels of the source into the destination buffer.
    size_t sampleOffset = audioData.isInterleaved() ? planeIndex : 0;
    size_t sampleIncrement = audioData.isInterleaved() && !destinationIsInterleaved ? numberOfChannels() : 1;
    size_t sourcePlane = audioData.isInterleaved() ? 0 : planeIndex;

    auto copyElements = []<typename T>(std::span<T> destination, auto sourcePlane, size_t sampleOffset, size_t sampleIndexIncrement, size_t samples) {
        RELEASE_ASSERT(destination.size() >= samples);
        RELEASE_ASSERT(sourcePlane.size() >= sampleIndexIncrement * samples + sampleOffset - 1);
        size_t sourceSampleIndex = sampleOffset;
        for (size_t sample = 0; sample < samples; sample++) {
            destination[sample] = convertAudioSample<T>(sourcePlane[sourceSampleIndex]);
            sourceSampleIndex += sampleIndexIncrement;
        }
    };
    switchOn(audioElementSpan(destinationFormat, destination), [&](auto dst) {
        switchOn(source, [&](auto& src) {
            copyElements(dst, src[sourcePlane], sampleOffset, sampleIncrement, copyElementCount);
        });
    });
}

CMSampleBufferRef PlatformRawAudioDataCocoa::sampleBuffer() const
{
    return m_sample->sampleBuffer();
}

} // namespace WebCore

#undef GST_CAT_DEFAULT

#endif // ENABLE(WEB_CODECS) && USE(GSTREAMER)
