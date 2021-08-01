/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "AudioBus.h"

#include "DenormalDisabler.h"
#include "SincResampler.h"
#include "VectorMath.h"
#include <algorithm>
#include <assert.h>
#include <math.h>

namespace WebCore {

constexpr unsigned MaxBusChannels = 32;

RefPtr<AudioBus> AudioBus::create(unsigned numberOfChannels, size_t length, bool allocate)
{
    ASSERT(numberOfChannels <= MaxBusChannels);
    if (numberOfChannels > MaxBusChannels)
        return nullptr;

    return adoptRef(*new AudioBus(numberOfChannels, length, allocate));
}

AudioBus::AudioBus(unsigned numberOfChannels, size_t length, bool allocate)
    : m_length(length)
{
    m_channels.reserveInitialCapacity(numberOfChannels);

    for (unsigned i = 0; i < numberOfChannels; ++i) {
        auto channel = allocate ? makeUnique<AudioChannel>(length) : makeUnique<AudioChannel>(nullptr, length);
        m_channels.uncheckedAppend(WTFMove(channel));
    }

    m_layout = LayoutCanonical; // for now this is the only layout we define
}

void AudioBus::setChannelMemory(unsigned channelIndex, float* storage, size_t length)
{
    if (channelIndex < m_channels.size()) {
        channel(channelIndex)->set(storage, length);
        m_length = length; // FIXME: verify that this length matches all the other channel lengths
    }
}

void AudioBus::setLength(size_t newLength)
{
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(newLength <= m_length);
    if (newLength == m_length)
        return;
    for (unsigned i = 0; i < m_channels.size(); ++i)
        m_channels[i]->setLength(newLength);
    m_length = newLength;
}

void AudioBus::zero()
{
    for (unsigned i = 0; i < m_channels.size(); ++i)
        m_channels[i]->zero();
}

AudioChannel* AudioBus::channelByType(unsigned channelType)
{
    // For now we only support canonical channel layouts...
    if (m_layout != LayoutCanonical)
        return 0;

    switch (numberOfChannels()) {
    case 1: // mono
        if (channelType == ChannelMono || channelType == ChannelLeft)
            return channel(0);
        return 0;

    case 2: // stereo
        switch (channelType) {
        case ChannelLeft: return channel(0);
        case ChannelRight: return channel(1);
        default: return 0;
        }

    case 4: // quad
        switch (channelType) {
        case ChannelLeft: return channel(0);
        case ChannelRight: return channel(1);
        case ChannelSurroundLeft: return channel(2);
        case ChannelSurroundRight: return channel(3);
        default: return 0;
        }

    case 5: // 5.0
        switch (channelType) {
        case ChannelLeft: return channel(0);
        case ChannelRight: return channel(1);
        case ChannelCenter: return channel(2);
        case ChannelSurroundLeft: return channel(3);
        case ChannelSurroundRight: return channel(4);
        default: return 0;
        }

    case 6: // 5.1
        switch (channelType) {
        case ChannelLeft: return channel(0);
        case ChannelRight: return channel(1);
        case ChannelCenter: return channel(2);
        case ChannelLFE: return channel(3);
        case ChannelSurroundLeft: return channel(4);
        case ChannelSurroundRight: return channel(5);
        default: return 0;
        }
    }
    
    ASSERT_NOT_REACHED();
    return 0;
}

const AudioChannel* AudioBus::channelByType(unsigned type) const
{
    return const_cast<AudioBus*>(this)->channelByType(type);
}

// Returns true if the channel count and frame-size match.
bool AudioBus::topologyMatches(const AudioBus& bus) const
{
    if (numberOfChannels() != bus.numberOfChannels())
        return false; // channel mismatch

    // Make sure source bus has enough frames.
    if (length() > bus.length())
        return false; // frame-size mismatch

    return true;
}

RefPtr<AudioBus> AudioBus::createBufferFromRange(const AudioBus* sourceBuffer, unsigned startFrame, unsigned endFrame)
{
    size_t numberOfSourceFrames = sourceBuffer->length();
    unsigned numberOfChannels = sourceBuffer->numberOfChannels();

    // Sanity checking
    bool isRangeSafe = startFrame < endFrame && endFrame <= numberOfSourceFrames;
    ASSERT(isRangeSafe);
    if (!isRangeSafe)
        return nullptr;

    size_t rangeLength = endFrame - startFrame;

    RefPtr<AudioBus> audioBus = create(numberOfChannels, rangeLength);
    audioBus->setSampleRate(sourceBuffer->sampleRate());

    for (unsigned i = 0; i < numberOfChannels; ++i)
        audioBus->channel(i)->copyFromRange(sourceBuffer->channel(i), startFrame, endFrame);

    return audioBus;
}

float AudioBus::maxAbsValue() const
{
    float max = 0.0f;
    for (unsigned i = 0; i < numberOfChannels(); ++i) {
        const AudioChannel* channel = this->channel(i);
        max = std::max(max, channel->maxAbsValue());
    }

    return max;
}

void AudioBus::normalize()
{
    float max = maxAbsValue();
    if (max)
        scale(1.0f / max);
}

void AudioBus::scale(float scale)
{
    for (unsigned i = 0; i < numberOfChannels(); ++i)
        channel(i)->scale(scale);
}

void AudioBus::copyFromRange(const AudioBus& sourceBus, unsigned startFrame, unsigned endFrame)
{
    if (!topologyMatches(sourceBus)) {
        ASSERT_NOT_REACHED();
        zero();
        return;
    }

    size_t numberOfSourceFrames = sourceBus.length();
    bool isRangeSafe = startFrame < endFrame && endFrame <= numberOfSourceFrames;
    ASSERT(isRangeSafe);
    if (!isRangeSafe) {
        zero();
        return;
    }

    unsigned numberOfChannels = this->numberOfChannels();
    ASSERT(numberOfChannels <= MaxBusChannels);
    if (numberOfChannels > MaxBusChannels) {
        zero();
        return;
    }

    for (unsigned i = 0; i < numberOfChannels; ++i)
        channel(i)->copyFromRange(sourceBus.channel(i), startFrame, endFrame);
}

void AudioBus::copyFrom(const AudioBus& sourceBus, ChannelInterpretation channelInterpretation)
{
    if (&sourceBus == this)
        return;

    // Copying bus is equivalent to zeroing and then summing.
    zero();
    sumFrom(sourceBus, channelInterpretation);
}

void AudioBus::sumFrom(const AudioBus& sourceBus, ChannelInterpretation channelInterpretation)
{
    if (&sourceBus == this)
        return;

    unsigned numberOfSourceChannels = sourceBus.numberOfChannels();
    unsigned numberOfDestinationChannels = numberOfChannels();

    if (numberOfDestinationChannels == numberOfSourceChannels) {
        for (unsigned i = 0; i < numberOfSourceChannels; ++i)
            channel(i)->sumFrom(sourceBus.channel(i));
    } else {
        switch (channelInterpretation) {
        case ChannelInterpretation::Speakers:
            if (numberOfSourceChannels < numberOfDestinationChannels)
                speakersSumFromByUpMixing(sourceBus);
            else
                speakersSumFromByDownMixing(sourceBus);
            break;
        case ChannelInterpretation::Discrete:
            discreteSumFrom(sourceBus);
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    }
}

void AudioBus::speakersSumFromByUpMixing(const AudioBus& sourceBus)
{
    unsigned numberOfSourceChannels = sourceBus.numberOfChannels();
    unsigned numberOfDestinationChannels = numberOfChannels();

    if ((numberOfSourceChannels == 1 && numberOfDestinationChannels == 2)
        || (numberOfSourceChannels == 1 && numberOfDestinationChannels == 4)) {
        // Handle mono -> stereo case (summing mono channel into both left and right).
        auto* sourceL = sourceBus.channelByType(ChannelLeft);
        channelByType(ChannelLeft)->sumFrom(sourceL);
        channelByType(ChannelRight)->sumFrom(sourceL);
    } else if (numberOfSourceChannels == 1 && numberOfDestinationChannels == 6) {
        // Handle mono -> 5.1 case, sum mono channel into center.
        channelByType(ChannelCenter)->sumFrom(sourceBus.channelByType(ChannelLeft));
    } else if ((numberOfSourceChannels == 2 && numberOfDestinationChannels == 4)
        || (numberOfSourceChannels == 2 && numberOfDestinationChannels == 6)) {
        // Up-mixing: 2 -> 4, 2 -> 5.1
        channelByType(ChannelLeft)->sumFrom(sourceBus.channelByType(ChannelLeft));
        channelByType(ChannelRight)->sumFrom(sourceBus.channelByType(ChannelRight));
    } else if (numberOfSourceChannels == 4 && numberOfDestinationChannels == 6) {
        // Up-mixing: 4 -> 5.1
        channelByType(ChannelLeft)->sumFrom(sourceBus.channelByType(ChannelLeft));
        channelByType(ChannelRight)->sumFrom(sourceBus.channelByType(ChannelRight));
        channelByType(ChannelSurroundLeft)->sumFrom(sourceBus.channelByType(ChannelSurroundLeft));
        channelByType(ChannelSurroundRight)->sumFrom(sourceBus.channelByType(ChannelSurroundRight));
    } else {
        // Fallback for unknown combinations.
        discreteSumFrom(sourceBus);
    }
}

void AudioBus::speakersSumFromByDownMixing(const AudioBus& sourceBus)
{
    unsigned numberOfSourceChannels = sourceBus.numberOfChannels();
    unsigned numberOfDestinationChannels = numberOfChannels();

    if (numberOfSourceChannels == 2 && numberOfDestinationChannels == 1) {
        // Handle stereo -> mono case. output += 0.5 * (input.L + input.R).
        AudioBus& sourceBusSafe = const_cast<AudioBus&>(sourceBus);

        const float* sourceL = sourceBusSafe.channelByType(ChannelLeft)->data();
        const float* sourceR = sourceBusSafe.channelByType(ChannelRight)->data();

        float* destination = channelByType(ChannelLeft)->mutableData();
        VectorMath::multiplyByScalarThenAddToOutput(sourceL, 0.5, destination, length());
        VectorMath::multiplyByScalarThenAddToOutput(sourceR, 0.5, destination, length());
    } else if (numberOfSourceChannels == 4 && numberOfDestinationChannels == 1) {
        // Down-mixing: 4 -> 1
        // output = 0.25 * (input.L + input.R + input.SL + input.SR)
        auto* sourceL = sourceBus.channelByType(ChannelLeft)->data();
        auto* sourceR = sourceBus.channelByType(ChannelRight)->data();
        auto* sourceSL = sourceBus.channelByType(ChannelSurroundLeft)->data();
        auto* sourceSR = sourceBus.channelByType(ChannelSurroundRight)->data();

        auto* destination = channelByType(ChannelLeft)->mutableData();

        VectorMath::multiplyByScalarThenAddToOutput(sourceL, 0.25, destination, length());
        VectorMath::multiplyByScalarThenAddToOutput(sourceR, 0.25, destination, length());
        VectorMath::multiplyByScalarThenAddToOutput(sourceSL, 0.25, destination, length());
        VectorMath::multiplyByScalarThenAddToOutput(sourceSR, 0.25, destination, length());
    } else if (numberOfSourceChannels == 6 && numberOfDestinationChannels == 1) {
        // Down-mixing: 5.1 -> 1
        // output = sqrt(1/2) * (input.L + input.R) + input.C + 0.5 * (input.SL + input.SR)
        auto* sourceL = sourceBus.channelByType(ChannelLeft)->data();
        auto* sourceR = sourceBus.channelByType(ChannelRight)->data();
        auto* sourceC = sourceBus.channelByType(ChannelCenter)->data();
        auto* sourceSL = sourceBus.channelByType(ChannelSurroundLeft)->data();
        auto* sourceSR = sourceBus.channelByType(ChannelSurroundRight)->data();

        auto* destination = channelByType(ChannelLeft)->mutableData();
        float scaleSqrtHalf = sqrtf(0.5);

        VectorMath::multiplyByScalarThenAddToOutput(sourceL, scaleSqrtHalf, destination, length());
        VectorMath::multiplyByScalarThenAddToOutput(sourceR, scaleSqrtHalf, destination, length());
        VectorMath::add(sourceC, destination, destination, length());
        VectorMath::multiplyByScalarThenAddToOutput(sourceSL, 0.5, destination, length());
        VectorMath::multiplyByScalarThenAddToOutput(sourceSR, 0.5, destination, length());
    } else if (numberOfSourceChannels == 4 && numberOfDestinationChannels == 2) {
        // Down-mixing: 4 -> 2
        // output.L = 0.5 * (input.L + input.SL)
        // output.R = 0.5 * (input.R + input.SR)
        auto* sourceL = sourceBus.channelByType(ChannelLeft)->data();
        auto* sourceR = sourceBus.channelByType(ChannelRight)->data();
        auto* sourceSL = sourceBus.channelByType(ChannelSurroundLeft)->data();
        auto* sourceSR = sourceBus.channelByType(ChannelSurroundRight)->data();

        auto* destinationL = channelByType(ChannelLeft)->mutableData();
        auto* destinationR = channelByType(ChannelRight)->mutableData();

        VectorMath::multiplyByScalarThenAddToOutput(sourceL, 0.5, destinationL, length());
        VectorMath::multiplyByScalarThenAddToOutput(sourceSL, 0.5, destinationL, length());
        VectorMath::multiplyByScalarThenAddToOutput(sourceR, 0.5, destinationR, length());
        VectorMath::multiplyByScalarThenAddToOutput(sourceSR, 0.5, destinationR, length());
    } else if (numberOfSourceChannels == 6 && numberOfDestinationChannels == 2) {
        // Down-mixing: 5.1 -> 2
        // output.L = input.L + sqrt(1/2) * (input.C + input.SL)
        // output.R = input.R + sqrt(1/2) * (input.C + input.SR)
        auto* sourceL = sourceBus.channelByType(ChannelLeft)->data();
        auto* sourceR = sourceBus.channelByType(ChannelRight)->data();
        auto* sourceC = sourceBus.channelByType(ChannelCenter)->data();
        auto* sourceSL = sourceBus.channelByType(ChannelSurroundLeft)->data();
        auto* sourceSR = sourceBus.channelByType(ChannelSurroundRight)->data();

        float* destinationL = channelByType(ChannelLeft)->mutableData();
        float* destinationR = channelByType(ChannelRight)->mutableData();
        float scaleSqrtHalf = sqrtf(0.5);

        VectorMath::add(sourceL, destinationL, destinationL, length());
        VectorMath::multiplyByScalarThenAddToOutput(sourceC, scaleSqrtHalf, destinationL, length());
        VectorMath::multiplyByScalarThenAddToOutput(sourceSL, scaleSqrtHalf, destinationL, length());
        VectorMath::add(sourceR, destinationR, destinationR, length());
        VectorMath::multiplyByScalarThenAddToOutput(sourceC, scaleSqrtHalf, destinationR, length());
        VectorMath::multiplyByScalarThenAddToOutput(sourceSR, scaleSqrtHalf, destinationR, length());
    } else if (numberOfSourceChannels == 6 && numberOfDestinationChannels == 4) {
        // Down-mixing: 5.1 -> 4
        // output.L = input.L + sqrt(1/2) * input.C
        // output.R = input.R + sqrt(1/2) * input.C
        // output.SL = input.SL
        // output.SR = input.SR
        auto* sourceL = sourceBus.channelByType(ChannelLeft)->data();
        auto* sourceR = sourceBus.channelByType(ChannelRight)->data();
        auto* sourceC = sourceBus.channelByType(ChannelCenter)->data();

        auto* destinationL = channelByType(ChannelLeft)->mutableData();
        auto* destinationR = channelByType(ChannelRight)->mutableData();
        auto scaleSqrtHalf = sqrtf(0.5);

        VectorMath::add(sourceL, destinationL, destinationL, length());
        VectorMath::multiplyByScalarThenAddToOutput(sourceC, scaleSqrtHalf, destinationL, length());
        VectorMath::add(sourceR, destinationR, destinationR, length());
        VectorMath::multiplyByScalarThenAddToOutput(sourceC, scaleSqrtHalf, destinationR, length());
        channel(2)->sumFrom(sourceBus.channel(4));
        channel(3)->sumFrom(sourceBus.channel(5));
    } else {
        // Fallback for unknown combinations.
        discreteSumFrom(sourceBus);
    }
}

void AudioBus::discreteSumFrom(const AudioBus& sourceBus)
{
    unsigned numberOfSourceChannels = sourceBus.numberOfChannels();
    unsigned numberOfDestinationChannels = numberOfChannels();

    if (numberOfDestinationChannels < numberOfSourceChannels) {
        // Down-mix by summing channels and dropping the remaining.
        for (unsigned i = 0; i < numberOfDestinationChannels; ++i) 
            channel(i)->sumFrom(sourceBus.channel(i));
    } else if (numberOfDestinationChannels > numberOfSourceChannels) {
        // Up-mix by summing as many channels as we have.
        for (unsigned i = 0; i < numberOfSourceChannels; ++i) 
            channel(i)->sumFrom(sourceBus.channel(i));
    }
}

void AudioBus::copyWithGainFrom(const AudioBus& sourceBus, float gain)
{
    if (!topologyMatches(sourceBus)) {
        ASSERT_NOT_REACHED();
        zero();
        return;
    }

    if (sourceBus.isSilent()) {
        zero();
        return;
    }

    unsigned numberOfChannels = this->numberOfChannels();
    ASSERT(numberOfChannels <= MaxBusChannels);
    if (numberOfChannels > MaxBusChannels)
        return;

    // If it is copying from the same bus and no need to change gain, just return.
    if (this == &sourceBus && gain == 1)
        return;

    AudioBus& sourceBusSafe = const_cast<AudioBus&>(sourceBus);
    const float* sources[MaxBusChannels];
    float* destinations[MaxBusChannels];

    for (unsigned i = 0; i < numberOfChannels; ++i) {
        sources[i] = sourceBusSafe.channel(i)->data();
        destinations[i] = channel(i)->mutableData();
    }

    unsigned framesToProcess = length();

    // Handle gains of 0 and 1 (exactly) specially.
    if (gain == 1) {
        for (unsigned channelIndex = 0; channelIndex < numberOfChannels; ++channelIndex)
            memcpy(destinations[channelIndex], sources[channelIndex], framesToProcess * sizeof(*destinations[channelIndex]));
    } else if (!gain) {
        for (unsigned channelIndex = 0; channelIndex < numberOfChannels; ++channelIndex)
            memset(destinations[channelIndex], 0, framesToProcess * sizeof(*destinations[channelIndex]));
    } else {
        for (unsigned channelIndex = 0; channelIndex < numberOfChannels; ++channelIndex)
            VectorMath::multiplyByScalar(sources[channelIndex], gain, destinations[channelIndex], framesToProcess);
    }
}

void AudioBus::copyWithSampleAccurateGainValuesFrom(const AudioBus &sourceBus, float* gainValues, unsigned numberOfGainValues)
{
    // Make sure we're processing from the same type of bus.
    // We *are* able to process from mono -> stereo
    if (sourceBus.numberOfChannels() != 1 && !topologyMatches(sourceBus)) {
        ASSERT_NOT_REACHED();
        return;
    }

    if (!gainValues || numberOfGainValues > sourceBus.length()) {
        ASSERT_NOT_REACHED();
        return;
    }

    if (sourceBus.length() == numberOfGainValues && sourceBus.length() == length() && sourceBus.isSilent()) {
        zero();
        return;
    }

    // We handle both the 1 -> N and N -> N case here.
    const float* source = sourceBus.channel(0)->data();
    for (unsigned channelIndex = 0; channelIndex < numberOfChannels(); ++channelIndex) {
        if (sourceBus.numberOfChannels() == numberOfChannels())
            source = sourceBus.channel(channelIndex)->data();
        float* destination = channel(channelIndex)->mutableData();
        VectorMath::multiply(source, gainValues, destination, numberOfGainValues);
    }
}

RefPtr<AudioBus> AudioBus::createBySampleRateConverting(const AudioBus* sourceBus, bool mixToMono, double newSampleRate)
{
    // sourceBus's sample-rate must be known.
    ASSERT(sourceBus && sourceBus->sampleRate());
    if (!sourceBus || !sourceBus->sampleRate())
        return nullptr;

    double sourceSampleRate = sourceBus->sampleRate();
    double destinationSampleRate = newSampleRate;
    double sampleRateRatio = sourceSampleRate / destinationSampleRate;
    unsigned numberOfSourceChannels = sourceBus->numberOfChannels();

    if (numberOfSourceChannels == 1)
        mixToMono = false; // already mono
        
    if (sourceSampleRate == destinationSampleRate) {
        // No sample-rate conversion is necessary.
        if (mixToMono)
            return AudioBus::createByMixingToMono(sourceBus);

        // Return exact copy.
        return AudioBus::createBufferFromRange(sourceBus, 0, sourceBus->length());
    }

    if (sourceBus->isSilent()) {
        RefPtr<AudioBus> silentBus = create(numberOfSourceChannels, sourceBus->length() / sampleRateRatio);
        silentBus->setSampleRate(newSampleRate);
        return silentBus;
    }

    // First, mix to mono (if necessary) then sample-rate convert.
    const AudioBus* resamplerSourceBus;
    RefPtr<AudioBus> mixedMonoBus;
    if (mixToMono) {
        mixedMonoBus = AudioBus::createByMixingToMono(sourceBus);
        resamplerSourceBus = mixedMonoBus.get();
    } else {
        // Directly resample without down-mixing.
        resamplerSourceBus = sourceBus;
    }

    // Calculate destination length based on the sample-rates.
    int sourceLength = resamplerSourceBus->length();
    int destinationLength = sourceLength / sampleRateRatio;

    // Create destination bus with same number of channels.
    unsigned numberOfDestinationChannels = resamplerSourceBus->numberOfChannels();
    RefPtr<AudioBus> destinationBus = create(numberOfDestinationChannels, destinationLength);

    // Sample-rate convert each channel.
    for (unsigned i = 0; i < numberOfDestinationChannels; ++i) {
        const float* source = resamplerSourceBus->channel(i)->data();
        float* destination = destinationBus->channel(i)->mutableData();

        SincResampler::processBuffer(source, destination, sourceLength, sampleRateRatio);
    }

    destinationBus->clearSilentFlag();
    destinationBus->setSampleRate(newSampleRate);    
    return destinationBus;
}

RefPtr<AudioBus> AudioBus::createByMixingToMono(const AudioBus* sourceBus)
{
    if (sourceBus->isSilent())
        return create(1, sourceBus->length());

    switch (sourceBus->numberOfChannels()) {
    case 1:
        // Simply create an exact copy.
        return AudioBus::createBufferFromRange(sourceBus, 0, sourceBus->length());
    case 2:
        {
            unsigned n = sourceBus->length();
            RefPtr<AudioBus> destinationBus = create(1, n);

            const float* sourceL = sourceBus->channel(0)->data();
            const float* sourceR = sourceBus->channel(1)->data();
            float* destination = destinationBus->channel(0)->mutableData();
        
            // Do the mono mixdown.
            VectorMath::addVectorsThenMultiplyByScalar(sourceL, sourceR, 0.5, destination, n);

            destinationBus->clearSilentFlag();
            destinationBus->setSampleRate(sourceBus->sampleRate());    
            return destinationBus;
        }
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

bool AudioBus::isSilent() const
{
    for (size_t i = 0; i < m_channels.size(); ++i) {
        if (!m_channels[i]->isSilent())
            return false;
    }
    return true;
}

void AudioBus::clearSilentFlag()
{
    for (size_t i = 0; i < m_channels.size(); ++i)
        m_channels[i]->clearSilentFlag();
}

} // WebCore

#endif // ENABLE(WEB_AUDIO)
