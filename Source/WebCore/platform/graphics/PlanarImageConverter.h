/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <wtf/Assertions.h>

// Plane conversions for planar and bi-planar Y'CbCr images. Operations use the platform frameworks
// when they can, fall back to libyuv where it is available, and otherwise only copy planes unchanged.
// Shifts between 16-bit sample formats use libyuv first, which needs no lookup table.
namespace WebCore::PlanarImageConverter {

// Samples above 8 bits are stored little endian in 16 bits, in the low (LSB) or high (MSB) bits.
struct SampleFormat {
    uint8_t bitDepth { 8 };
    bool isMSBAligned { false };

    constexpr size_t sampleSize() const { return bitDepth > 8 ? 2 : 1; }
    friend constexpr bool operator==(const SampleFormat&, const SampleFormat&) = default;
};

inline constexpr SampleFormat eightBit { };
inline constexpr SampleFormat tenBitLSB { 10, false };
inline constexpr SampleFormat twelveBitLSB { 12, false };
inline constexpr SampleFormat tenBitMSB { 10, true };
inline constexpr SampleFormat twelveBitMSB { 12, true };
inline constexpr SampleFormat sixteenBit { 16, true };

enum class ChromaSubsampling : uint8_t {
    None, // 4:4:4
    Horizontal, // 4:2:2
    HorizontalAndVertical, // 4:2:0
};

// The number of luma samples per chroma sample in each direction.
constexpr size_t horizontalChromaFactor(ChromaSubsampling subsampling) { return subsampling == ChromaSubsampling::None ? 1 : 2; }
constexpr size_t verticalChromaFactor(ChromaSubsampling subsampling) { return subsampling == ChromaSubsampling::HorizontalAndVertical ? 2 : 1; }

// The size of the chroma planes of an image of width x height samples, rounded up.
constexpr size_t chromaWidth(ChromaSubsampling subsampling, size_t width) { return (width + horizontalChromaFactor(subsampling) - 1) / horizontalChromaFactor(subsampling); }
constexpr size_t chromaHeight(ChromaSubsampling subsampling, size_t height) { return (height + verticalChromaFactor(subsampling) - 1) / verticalChromaFactor(subsampling); }

// A validated view of the rows of a plane. width counts samples of one channel.
template<typename Byte>
struct Plane {
    std::span<Byte> rows;
    size_t stride { 0 };
    size_t width { 0 };
    size_t height { 0 };
    SampleFormat format;
    uint8_t channels { 1 };

    size_t rowBytes() const { return width * channels * format.sampleSize(); }
    std::span<Byte> row(size_t index) const
    {
        RELEASE_ASSERT(index < height);
        return rows.subspan(index * stride, rowBytes());
    }
};

using SourcePlane = Plane<const uint8_t>;
using DestinationPlane = Plane<uint8_t>;

// Return nothing unless every row fits in the buffer and the sizes fit in an int.
WEBCORE_EXPORT std::optional<SourcePlane> makePlane(std::span<const uint8_t>, size_t offset, size_t stride, size_t width, size_t height, SampleFormat, uint8_t channels = 1);
WEBCORE_EXPORT std::optional<DestinationPlane> makePlane(std::span<uint8_t>, size_t offset, size_t stride, size_t width, size_t height, SampleFormat, uint8_t channels = 1);

// For samples stored in 16 bits: offset and stride count samples.
WEBCORE_EXPORT std::optional<SourcePlane> makePlane(std::span<const uint16_t>, size_t offset, size_t stride, size_t width, size_t height, SampleFormat, uint8_t channels = 1);
WEBCORE_EXPORT std::optional<DestinationPlane> makePlane(std::span<uint16_t>, size_t offset, size_t stride, size_t width, size_t height, SampleFormat, uint8_t channels = 1);

WEBCORE_EXPORT std::optional<SourcePlane> subplane(const SourcePlane&, size_t left, size_t top, size_t width, size_t height);
WEBCORE_EXPORT std::optional<DestinationPlane> subplane(const DestinationPlane&, size_t left, size_t top, size_t width, size_t height);

WEBCORE_EXPORT SourcePlane asSource(const DestinationPlane&);

enum class RangeConversion : uint8_t { None, FullToVideo, VideoToFull };
enum class Component : bool { Luma, Chroma };

// Planes must have the same size and channel count.
// RangeConversion::None: between 8 bits and LSB or MSB storage, and between LSB and MSB storage. From 8 bits the top bits are
// replicated, so 255 becomes the largest value. LSB samples are shifted to and from the MSBs (to 16 bits, the top bits are also
// replicated into the low bits), and MSB storage of any bit depth is copied unchanged.
// RangeConversion::FullToVideo: 8-bit or 10-bit LSB to 10-bit MSB, and 10-bit MSB to 10-bit MSB.
// RangeConversion::VideoToFull: 10-bit MSB to 8-bit.
WEBCORE_EXPORT bool copyPlane(const SourcePlane&, const DestinationPlane&, RangeConversion = RangeConversion::None, Component = Component::Luma);

// Single channel u and v planes to and from a two channel plane.
// interleave(): 8-bit to 8-bit, 10 or 12-bit LSB to MSB, and between 16-bit formats with the same representation unchanged.
// RangeConversion::FullToVideo: 8-bit or 10-bit LSB to 10-bit MSB.
// deinterleave(): 8-bit to 8-bit, and MSB to 10 or 12-bit LSB. RangeConversion::VideoToFull: 10-bit MSB to 8-bit.
WEBCORE_EXPORT bool interleave(const SourcePlane& uPlane, const SourcePlane& vPlane, const DestinationPlane& uvPlane, RangeConversion = RangeConversion::None);
WEBCORE_EXPORT bool deinterleave(const SourcePlane& uvPlane, const DestinationPlane& uPlane, const DestinationPlane& vPlane, RangeConversion = RangeConversion::None);

// The destination is half the source size, rounded up, in either or both directions. Even sizes average each pair of
// samples; odd sizes are resampled bilinearly (libyuv's kFilterBox).
WEBCORE_EXPORT bool downsample(const SourcePlane&, const DestinationPlane&);

// Writes a single channel 8-bit plane into one channel, 0 to 3 in memory order, of a 4-channel 8-bit plane of the same size.
// The other channels are unchanged.
WEBCORE_EXPORT bool copyPlaneToChannel(const SourcePlane&, const DestinationPlane&, uint8_t channel);

// Between 4-channel 8-bit planes of the same size: channel i of the destination is channel channelMap[i] of the source.
// opaqueChannel, when set, is filled with 255 instead.
WEBCORE_EXPORT bool permuteChannels(const SourcePlane&, const DestinationPlane&, std::array<uint8_t, 4> channelMap, std::optional<uint8_t> opaqueChannel = { });

enum class Backend : uint8_t { PlatformFrameworks, LibYUV, Copy };
// Restricts the operations to one backend, for testing.
WEBCORE_EXPORT void setBackendForTesting(std::optional<Backend>);
WEBCORE_EXPORT bool isBackendAvailable(Backend);

} // namespace WebCore::PlanarImageConverter
