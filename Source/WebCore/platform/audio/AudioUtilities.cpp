/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AudioUtilities.h"
#include "BitReader.h"
#include <random>
#include <wtf/HashFunctions.h>
#include <wtf/MathExtras.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>
#include <wtf/WeakRandomNumber.h>
#include <wtf/text/ASCIILiteral.h>

namespace WebCore {

namespace AudioUtilities {

double discreteTimeConstantForSampleRate(double timeConstant, double sampleRate)
{
    return 1 - exp(-1 / (sampleRate * timeConstant));
}

size_t timeToSampleFrame(double time, double sampleRate, SampleFrameRounding rounding)
{
    // To compute the desired frame number, we pretend we're actually running the
    // context at a much higher sample rate (by a factor of |oversampleFactor|).
    // Round this to get the nearest frame number at the higher rate. Then
    // convert back to the original rate to get a new frame number that may not be
    // an integer. Then use the specified |rounding_mode| to round this to the
    // integer frame number that we need.
    //
    // Doing this partially solves the issue where Fs * (k / Fs) != k when doing
    // floating point arithmtic for integer k and Fs is the sample rate. By
    // oversampling and rounding, we'll get k back most of the time.
    //
    // The oversampling factor MUST be a power of two so as not to introduce
    // additional round-off in computing the oversample frame number.
    constexpr double oversampleFactor = 1024;
    double frame = std::round(time * sampleRate * oversampleFactor) / oversampleFactor;

    switch (rounding) {
    case SampleFrameRounding::Nearest:
        frame = std::round(frame);
        break;
    case SampleFrameRounding::Down:
        frame = std::floor(frame);
        break;
    case SampleFrameRounding::Up:
        frame = std::ceil(frame);
        break;
    }

    // Just return the largest possible size_t value if necessary.
    if (frame >= static_cast<double>(std::numeric_limits<size_t>::max()))
        return std::numeric_limits<size_t>::max();

    return static_cast<size_t>(frame);
}

void applyNoise(std::span<float> values, float standardDeviation)
{
    std::random_device device;
    std::mt19937 generator(device());
    std::normal_distribution<float> distribution(1, standardDeviation);

    constexpr auto maximumTableSize = 2048;
    constexpr auto minimumTableSize = 1024;

    size_t unusedCellCount = weakRandomNumber<uint32_t>() % (maximumTableSize - minimumTableSize);
    size_t tableSize = maximumTableSize - unusedCellCount;

    // Avoid heap allocations on the rendering thread.
    std::array<float, maximumTableSize> multipliers;
    multipliers.fill(std::numeric_limits<float>::infinity());

    for (auto& value : values) {
        auto& multiplier = multipliers[DefaultHash<double>::hash(value) % tableSize];
        if (std::isinf(multiplier))
            multiplier = distribution(generator);
        value *= multiplier;
    }
}

Vector<uint8_t> extractAudioSpecificConfigFromESDescriptor(std::span<const uint8_t> esDescriptor)
{
    constexpr uint8_t esDescriptorTag = 0x03;
    constexpr uint8_t decoderConfigDescriptorTag = 0x04;
    constexpr uint8_t decoderSpecificInfoTag = 0x05;

    BitReader reader(esDescriptor);

    // ES_Descriptor: tag (8 bits), size.
    // Then ES_ID (16 bits), streamDependenceFlag (1 bit), URL_Flag (1 bit), OCRstreamFlag (1 bit), streamPriority (5 bits), optional fields, then the DecoderConfigDescriptor.
    if (reader.readBits(8) != esDescriptorTag)
        return { };
    reader.readExpandableSize();
    reader.consumeBits(16); // ES_ID
    bool streamDependenceFlag = reader.readFlag();
    bool urlFlag = reader.readFlag();
    bool ocrStreamFlag = reader.readFlag();
    reader.consumeBits(5); // streamPriority
    if (streamDependenceFlag)
        reader.consumeBits(16); // dependsOn_ES_ID
    if (urlFlag) {
        auto urlLength = reader.readBits(8);
        reader.consumeBits(8 * urlLength); // URLstring
    }
    if (ocrStreamFlag)
        reader.consumeBits(16); // OCR_ES_Id

    // DecoderConfigDescriptor: tag (8 bits) + size.
    // Then objectTypeIndication (8 bits), streamType (6 bits), upStream (1 bit), reserved (1 bit), bufferSizeDB (24 bits), maxBitrate (32 bits), avgBitrate (32 bits)
    // Then the DecoderSpecificInfo.
    if (reader.readBits(8) != decoderConfigDescriptorTag)
        return { };
    reader.readExpandableSize();

    constexpr size_t objectTypeIndicationBits = 8;
    constexpr size_t streamTypeUpStreamdAndReseveredBits = 8;
    constexpr size_t bufferSizeDBBits = 24;
    constexpr size_t maxBitrateBits = 32;
    constexpr size_t avgBitrateBits = 32;
    reader.consumeBits(objectTypeIndicationBits + streamTypeUpStreamdAndReseveredBits + bufferSizeDBBits + maxBitrateBits + avgBitrateBits);

    // DecoderSpecificInfo: tag (8 bits), size.
    // Then the AudioSpecificConfig, whose size is the number of bytes to read.
    if (reader.readBits(8) != decoderSpecificInfoTag)
        return { };
    auto audioSpecificConfig = reader.readSpan(reader.readExpandableSize());

    if (!reader.ok())
        return { };
    return audioSpecificConfig;
}

Vector<uint8_t> opusHeadWithoutPreSkip(std::span<const uint8_t> cookie)
{
    constexpr size_t preSkipOffset = 10;
    Vector<uint8_t> result(cookie);
    if (spanHasPrefix(cookie, "OpusHead"_span8) && result.size() >= preSkipOffset + sizeof(uint16_t))
        zeroSpan(result.mutableSpan().subspan(preSkipOffset, sizeof(uint16_t)));
    return result;
}

} // AudioUtilites

} // WebCore
