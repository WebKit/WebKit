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

// Returns the payload of the descriptor at the start of `data` (ISO/IEC 14496-1) and advances past it.
static std::optional<std::pair<uint8_t, std::span<const uint8_t>>> nextDescriptor(std::span<const uint8_t>& data)
{
    if (data.size() < 2)
        return { };

    uint8_t tag = data[0];
    size_t offset = 1;
    size_t length = 0;
    bool isLengthComplete = false;
    for (unsigned i = 0; i < 4 && offset < data.size() && !isLengthComplete; ++i) {
        uint8_t byte = data[offset++];
        length = (length << 7) | (byte & 0x7f);
        isLengthComplete = !(byte & 0x80);
    }
    if (!isLengthComplete || length > data.size() - offset)
        return { };

    auto payload = data.subspan(offset, length);
    data = data.subspan(offset + length);
    return std::pair { tag, payload };
}

static std::optional<std::span<const uint8_t>> audioSpecificConfigFromESDescriptor(std::span<const uint8_t> data)
{
    constexpr uint8_t esDescriptorTag = 0x03;
    constexpr uint8_t decoderConfigDescriptorTag = 0x04;
    constexpr uint8_t decoderSpecificInfoTag = 0x05;
    constexpr size_t decoderConfigFixedSize = 13; // objectTypeIndication, streamType, bufferSizeDB, maxBitrate, avgBitrate.

    auto esDescriptor = nextDescriptor(data);
    if (!esDescriptor || esDescriptor->first != esDescriptorTag || esDescriptor->second.size() < 3)
        return { };

    auto esPayload = esDescriptor->second;
    uint8_t flags = esPayload[2];
    esPayload = esPayload.subspan(3);
    auto skip = [&](size_t size) {
        if (size > esPayload.size())
            return false;
        esPayload = esPayload.subspan(size);
        return true;
    };
    // streamDependenceFlag: dependsOn_ES_ID.
    if ((flags & 0x80) && !skip(2))
        return { };
    // URL_Flag: URLlength, URLstring.
    if (flags & 0x40) {
        if (esPayload.empty() || !skip(1 + esPayload[0]))
            return { };
    }
    // OCRstreamFlag: OCR_ES_Id.
    if ((flags & 0x20) && !skip(2))
        return { };

    auto decoderConfig = nextDescriptor(esPayload);
    if (!decoderConfig || decoderConfig->first != decoderConfigDescriptorTag || decoderConfig->second.size() < decoderConfigFixedSize)
        return { };

    auto decoderConfigPayload = decoderConfig->second.subspan(decoderConfigFixedSize);
    while (auto descriptor = nextDescriptor(decoderConfigPayload)) {
        if (descriptor->first == decoderSpecificInfoTag)
            return descriptor->second;
    }
    return { };
}

std::optional<Vector<uint8_t>> audioSpecificConfigFromAACCookie(std::span<const uint8_t> cookie)
{
    // An AudioSpecificConfig never starts with a byte below 0x08 (its audio object type would be 0).
    if (cookie.empty() || cookie[0] >= 0x08)
        return Vector<uint8_t>(cookie);
    auto audioSpecificConfig = audioSpecificConfigFromESDescriptor(cookie);
    if (!audioSpecificConfig)
        return { };
    return Vector<uint8_t>(*audioSpecificConfig);
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
