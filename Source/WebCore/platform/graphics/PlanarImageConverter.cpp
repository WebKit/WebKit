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

#include "config.h"
#include "PlanarImageConverter.h"

#include "libYUV.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <limits>
#include <wtf/CheckedArithmetic.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Noncopyable.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>

#if USE(ACCELERATE)
#include <Accelerate/Accelerate.h>
#endif

#if USE(LIBWEBRTC)
#include "WebRTCProvider.h"
#endif

namespace WebCore::PlanarImageConverter {

static std::atomic<int> s_backendForTesting { -1 };

void setBackendForTesting(std::optional<Backend> backend)
{
    s_backendForTesting = backend ? static_cast<int>(*backend) : -1;
}

bool isBackendAvailable(Backend backend)
{
    switch (backend) {
    case Backend::PlatformFrameworks:
#if USE(ACCELERATE)
        return true;
#else
        return false;
#endif
    case Backend::LibYUV:
#if USE(LIBWEBRTC)
        // libyuv is part of libwebrtc, which may not be loaded.
        return WebRTCProvider::webRTCAvailable();
#else
        return false;
#endif
    case Backend::Copy:
        return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

static bool shouldUse(Backend backend)
{
    int backendForTesting = s_backendForTesting;
    return isBackendAvailable(backend) && (backendForTesting < 0 || backendForTesting == static_cast<int>(backend));
}

enum class PreferredBackend : bool { PlatformFrameworks, LibYUV };

// Runs the first available backend that supports the operation, trying the preferred one of the platform frameworks and libyuv first.
template<typename PlatformFrameworks, typename LibYUV, typename Copy>
static bool perform(NOESCAPE const PlatformFrameworks& platformFrameworks, NOESCAPE const LibYUV& libYUV, NOESCAPE const Copy& copy, PreferredBackend preferredBackend = PreferredBackend::PlatformFrameworks)
{
    auto usePlatformFrameworks = [&] {
        return shouldUse(Backend::PlatformFrameworks) && platformFrameworks();
    };
    auto useLibYUV = [&] {
        return shouldUse(Backend::LibYUV) && libYUV();
    };
    bool succeeded = preferredBackend == PreferredBackend::LibYUV ? useLibYUV() || usePlatformFrameworks() : usePlatformFrameworks() || useLibYUV();
    return succeeded || (shouldUse(Backend::Copy) && copy());
}

static constexpr auto unsupported = [] {
    return false;
};

static bool fitsInInt(size_t value)
{
    return value <= static_cast<size_t>(std::numeric_limits<int>::max());
}

// 8-bit samples have no alignment, and 16-bit samples have no unused bits, so both have a single representation.
static std::optional<SampleFormat> normalizedSampleFormat(SampleFormat format)
{
    switch (format.bitDepth) {
    case 8:
        return eightBit;
    case 10:
    case 12:
        return format;
    case 16:
        return sixteenBit;
    default:
        return { };
    }
}

template<typename Byte>
static std::optional<Plane<Byte>> makePlaneImpl(std::span<Byte> buffer, size_t offset, size_t stride, size_t width, size_t height, SampleFormat format, uint8_t channels)
{
    auto normalizedFormat = normalizedSampleFormat(format);
    if (!normalizedFormat || !width || !height || !channels)
        return { };

    size_t rowBytes;
    size_t size;
    size_t end;
    if (!WTF::safeMultiply(width, channels * normalizedFormat->sampleSize(), rowBytes) || stride < rowBytes || !fitsInInt(stride) || !fitsInInt(height)
        || !WTF::safeMultiply(stride, height - 1, size) || !WTF::safeAdd(size, rowBytes, size) || !WTF::safeAdd(offset, size, end) || end > buffer.size())
        return { };
    return Plane<Byte> { buffer.subspan(offset, size), stride, width, height, *normalizedFormat, channels };
}

std::optional<SourcePlane> makePlane(std::span<const uint8_t> buffer, size_t offset, size_t stride, size_t width, size_t height, SampleFormat format, uint8_t channels)
{
    return makePlaneImpl(buffer, offset, stride, width, height, format, channels);
}

std::optional<DestinationPlane> makePlane(std::span<uint8_t> buffer, size_t offset, size_t stride, size_t width, size_t height, SampleFormat format, uint8_t channels)
{
    return makePlaneImpl(buffer, offset, stride, width, height, format, channels);
}

template<typename Byte>
static std::optional<Plane<Byte>> makePlaneFromSamples(std::span<Byte> buffer, size_t offset, size_t stride, size_t width, size_t height, SampleFormat format, uint8_t channels)
{
    size_t byteOffset;
    size_t byteStride;
    if (format.sampleSize() != sizeof(uint16_t) || !WTF::safeMultiply(offset, sizeof(uint16_t), byteOffset) || !WTF::safeMultiply(stride, sizeof(uint16_t), byteStride))
        return { };
    return makePlaneImpl(buffer, byteOffset, byteStride, width, height, format, channels);
}

std::optional<SourcePlane> makePlane(std::span<const uint16_t> samples, size_t offset, size_t stride, size_t width, size_t height, SampleFormat format, uint8_t channels)
{
    return makePlaneFromSamples(asByteSpan(samples), offset, stride, width, height, format, channels);
}

std::optional<DestinationPlane> makePlane(std::span<uint16_t> samples, size_t offset, size_t stride, size_t width, size_t height, SampleFormat format, uint8_t channels)
{
    return makePlaneFromSamples(asMutableByteSpan(samples), offset, stride, width, height, format, channels);
}

template<typename Byte>
static std::optional<Plane<Byte>> subplaneImpl(const Plane<Byte>& plane, size_t left, size_t top, size_t width, size_t height)
{
    size_t right;
    size_t bottom;
    if (!WTF::safeAdd(left, width, right) || right > plane.width || !WTF::safeAdd(top, height, bottom) || bottom > plane.height)
        return { };
    size_t offset = top * plane.stride + left * plane.channels * plane.format.sampleSize();
    return makePlaneImpl(plane.rows, offset, plane.stride, width, height, plane.format, plane.channels);
}

std::optional<SourcePlane> subplane(const SourcePlane& plane, size_t left, size_t top, size_t width, size_t height)
{
    return subplaneImpl(plane, left, top, width, height);
}

std::optional<DestinationPlane> subplane(const DestinationPlane& plane, size_t left, size_t top, size_t width, size_t height)
{
    return subplaneImpl(plane, left, top, width, height);
}

SourcePlane asSource(const DestinationPlane& plane)
{
    return { plane.rows, plane.stride, plane.width, plane.height, plane.format, plane.channels };
}

static bool haveSameSize(const auto& a, const auto& b)
{
    return a.width == b.width && a.height == b.height && a.channels == b.channels;
}

static bool isTwoByteAddressable(const auto& plane)
{
    return plane.format.sampleSize() == 1 || (!(reinterpret_cast<uintptr_t>(plane.rows.data()) % alignof(uint16_t)) && !(plane.stride % sizeof(uint16_t)));
}

static DestinationPlane packedPlane(Vector<uint16_t>& storage, size_t width, size_t height, SampleFormat format, uint8_t channels)
{
    size_t rowBytes = width * channels * format.sampleSize();
    storage = Vector<uint16_t>((rowBytes * height + 1) / sizeof(uint16_t));
    return { asMutableByteSpan(storage.mutableSpan()).first(rowBytes * height), rowBytes, width, height, format, channels };
}

static void copyRows(const SourcePlane& source, const DestinationPlane& destination)
{
    for (size_t row = 0; row < source.height; ++row)
        memcpySpan(destination.row(row), source.row(row));
}

// The backends read 16-bit samples as uint16_t. Planes that aren't 2-byte aligned go through aligned storage.
class AlignedSource {
    WTF_MAKE_NONCOPYABLE(AlignedSource);
public:
    explicit AlignedSource(const SourcePlane& plane)
        : m_plane(plane)
    {
        if (isTwoByteAddressable(plane))
            return;
        auto packed = packedPlane(m_storage, plane.width, plane.height, plane.format, plane.channels);
        copyRows(plane, packed);
        m_plane = asSource(packed);
    }

    const SourcePlane& plane() const { return m_plane; }

private:
    Vector<uint16_t> m_storage;
    SourcePlane m_plane;
};

class AlignedDestination {
    WTF_MAKE_NONCOPYABLE(AlignedDestination);
public:
    explicit AlignedDestination(const DestinationPlane& plane)
        : m_destination(plane)
        , m_plane(plane)
    {
        if (!isTwoByteAddressable(plane))
            m_plane = packedPlane(m_storage, plane.width, plane.height, plane.format, plane.channels);
    }

    const DestinationPlane& plane() const { return m_plane; }

    void commit() const
    {
        if (!m_storage.isEmpty())
            copyRows(asSource(m_plane), m_destination);
    }

private:
    DestinationPlane m_destination;
    Vector<uint16_t> m_storage;
    DestinationPlane m_plane;
};

// MSB aligned samples of any bit depth share one representation.
static bool haveSameRepresentation(SampleFormat a, SampleFormat b)
{
    return a == b || (a.isMSBAligned && b.isMSBAligned);
}

static bool copyPlaneWithCopy(const SourcePlane& source, const DestinationPlane& destination, RangeConversion conversion)
{
    if (conversion != RangeConversion::None || !haveSameRepresentation(source.format, destination.format))
        return false;
    copyRows(source, destination);
    return true;
}

#if USE(ACCELERATE)

using EightBitLookupTable = std::array<uint16_t, 256>;
using SixteenBitLookupTable = Vector<uint16_t>;
static constexpr size_t sixteenBitLookupTableSize = 65536;

// ITU-T H.273 full to video range scaling, to MSB aligned 10-bit samples.
template<size_t levels>
static constexpr std::array<uint16_t, levels> fullToTenBitVideoRangeTable(Component component)
{
    std::array<uint16_t, levels> table { };
    constexpr int maximum = levels - 1;
    constexpr int center = levels / 2;
    for (int value = 0; value < static_cast<int>(levels); ++value) {
        int mapped;
        if (component == Component::Chroma) {
            int scaled = (value - center) * 896;
            mapped = 512 + (scaled >= 0 ? (scaled + maximum / 2) / maximum : (scaled - maximum / 2) / maximum);
        } else
            mapped = 64 + (value * 876 + maximum / 2) / maximum;
        table[value] = static_cast<uint16_t>(std::clamp(mapped, 0, 1023) << 6);
    }
    return table;
}

static constexpr auto eightBitFullToTenBitVideoLuma = fullToTenBitVideoRangeTable<256>(Component::Luma);
static constexpr auto eightBitFullToTenBitVideoChroma = fullToTenBitVideoRangeTable<256>(Component::Chroma);
static constexpr auto tenBitFullToTenBitVideoLuma = fullToTenBitVideoRangeTable<1024>(Component::Luma);
static constexpr auto tenBitFullToTenBitVideoChroma = fullToTenBitVideoRangeTable<1024>(Component::Chroma);

// The inverse of fullToTenBitVideoRangeTable<256>, for MSB aligned 10-bit samples.
static uint8_t narrowTenBitVideoRangeSample(uint16_t sample, Component component)
{
    int value = sample >> 6;
    if (component == Component::Chroma) {
        int scaled = (value - 512) * 255;
        return std::clamp(128 + (scaled >= 0 ? (scaled + 448) / 896 : (scaled - 448) / 896), 0, 255);
    }
    return ((std::clamp(value, 64, 940) - 64) * 255 + 438) / 876;
}

// Bit replication: 255 maps to the largest bitDepth value.
static constexpr EightBitLookupTable eightBitToLSBTable(unsigned bitDepth)
{
    EightBitLookupTable table { };
    for (unsigned value = 0; value < table.size(); ++value)
        table[value] = (value * 0x0101) >> (16 - bitDepth);
    return table;
}

static constexpr auto eightBitToTenBitLSB = eightBitToLSBTable(10);
static constexpr auto eightBitToTwelveBitLSB = eightBitToLSBTable(12);

enum class SixteenBitTable : uint8_t {
    TenBitLSBToMSB,
    TwelveBitLSBToMSB,
    TenBitLSBToSixteenBit,
    TwelveBitLSBToSixteenBit,
    MSBToTenBitLSB,
    MSBToTwelveBitLSB,
    TenBitLSBToEightBit,
    TwelveBitLSBToEightBit,
    FullToVideoLuma,
    FullToVideoChroma,
    TenBitLSBFullToVideoLuma,
    TenBitLSBFullToVideoChroma,
    VideoToFullLuma,
    VideoToFullChroma,
};

static uint16_t sixteenBitTableEntry(SixteenBitTable table, uint16_t value)
{
    switch (table) {
    case SixteenBitTable::TenBitLSBToMSB:
        return static_cast<uint16_t>(value << 6);
    case SixteenBitTable::TwelveBitLSBToMSB:
        return static_cast<uint16_t>(value << 4);
    case SixteenBitTable::TenBitLSBToSixteenBit:
        value &= 0x3FF;
        return static_cast<uint16_t>(value << 6 | value >> 4);
    case SixteenBitTable::TwelveBitLSBToSixteenBit:
        value &= 0xFFF;
        return static_cast<uint16_t>(value << 4 | value >> 8);
    case SixteenBitTable::MSBToTenBitLSB:
        return value >> 6;
    case SixteenBitTable::MSBToTwelveBitLSB:
        return value >> 4;
    case SixteenBitTable::TenBitLSBToEightBit:
        return std::min<uint16_t>(value >> 2, 255);
    case SixteenBitTable::TwelveBitLSBToEightBit:
        return std::min<uint16_t>(value >> 4, 255);
    case SixteenBitTable::FullToVideoLuma:
        return tenBitFullToTenBitVideoLuma[value >> 6];
    case SixteenBitTable::FullToVideoChroma:
        return tenBitFullToTenBitVideoChroma[value >> 6];
    case SixteenBitTable::TenBitLSBFullToVideoLuma:
        return tenBitFullToTenBitVideoLuma[std::min<uint16_t>(value, 1023)];
    case SixteenBitTable::TenBitLSBFullToVideoChroma:
        return tenBitFullToTenBitVideoChroma[std::min<uint16_t>(value, 1023)];
    case SixteenBitTable::VideoToFullLuma:
        return narrowTenBitVideoRangeSample(value, Component::Luma);
    case SixteenBitTable::VideoToFullChroma:
        return narrowTenBitVideoRangeSample(value, Component::Chroma);
    }
    ASSERT_NOT_REACHED();
    return 0;
}

// Each table is 128 KB, so they are built on first use.
template<SixteenBitTable table>
static const SixteenBitLookupTable& sixteenBitLookupTableSingleton()
{
    static NeverDestroyed lookupTable = [] {
        SixteenBitLookupTable lookupTable(sixteenBitLookupTableSize);
        for (size_t value = 0; value < sixteenBitLookupTableSize; ++value)
            lookupTable[value] = sixteenBitTableEntry(table, static_cast<uint16_t>(value));
        return lookupTable;
    }();
    return lookupTable.get();
}

template<SixteenBitTable lumaTable, SixteenBitTable chromaTable>
static std::span<const uint16_t> sixteenBitLookupTable(Component component)
{
    if (component == Component::Chroma)
        return sixteenBitLookupTableSingleton<chromaTable>().span();
    return sixteenBitLookupTableSingleton<lumaTable>().span();
}

static vImage_Buffer vImageBuffer(const auto& plane, size_t samplesPerRow)
{
    return { const_cast<uint8_t*>(plane.rows.data()), plane.height, samplesPerRow, plane.stride };
}

static vImage_Buffer vImageBuffer(const auto& plane)
{
    return vImageBuffer(plane, plane.width * plane.channels);
}

static bool lookUp(const SourcePlane& source, const DestinationPlane& destination, const EightBitLookupTable& table)
{
    auto sourceBuffer = vImageBuffer(source);
    auto destinationBuffer = vImageBuffer(destination);
    return vImageLookupTable_Planar8toPlanar16(&sourceBuffer, &destinationBuffer, table.data(), kvImageNoFlags) == kvImageNoError;
}

static bool lookUp(const SourcePlane& source, const DestinationPlane& destination, std::span<const uint16_t> table)
{
    auto sourceBuffer = vImageBuffer(source);
    auto destinationBuffer = vImageBuffer(destination);
    return vImageLookupTable_Planar16(&sourceBuffer, &destinationBuffer, table.data(), kvImageNoFlags) == kvImageNoError;
}

// Every 16-bit sample of the source becomes its low (byteIndex 0) or high (byteIndex 1) byte.
static bool extractByte(const SourcePlane& source, size_t byteIndex, const DestinationPlane& destination)
{
    std::array<const void*, 1> sourceChannels { source.rows.subspan(byteIndex).data() };
    auto destinationBuffer = vImageBuffer(destination);
    std::array<const vImage_Buffer*, 1> destinationBuffers { &destinationBuffer };
    return vImageConvert_ChunkyToPlanar8(sourceChannels.data(), destinationBuffers.data(), 1, sizeof(uint16_t), destination.width * destination.channels, destination.height, source.stride, kvImageNoFlags) == kvImageNoError;
}

static bool lookUpToEightBit(const SourcePlane& source, const DestinationPlane& destination, std::span<const uint16_t> table)
{
    Vector<uint16_t> storage;
    auto scratch = packedPlane(storage, source.width, source.height, sixteenBit, source.channels);
    return lookUp(source, scratch, table) && extractByte(asSource(scratch), 0, destination);
}

static bool copyPlaneWithPlatformFrameworks(const SourcePlane& source, const DestinationPlane& destination, RangeConversion conversion, Component component)
{
    auto from = source.format;
    auto to = destination.format;
    switch (conversion) {
    case RangeConversion::None:
        if (haveSameRepresentation(from, to)) {
            auto sourceBuffer = vImageBuffer(source, source.rowBytes());
            auto destinationBuffer = vImageBuffer(destination, destination.rowBytes());
            return vImageCopyBuffer(&sourceBuffer, &destinationBuffer, 1, kvImageNoFlags) == kvImageNoError;
        }
        if (from == eightBit) {
            if (to.isMSBAligned) {
                auto sourceBuffer = vImageBuffer(source);
                auto destinationBuffer = vImageBuffer(destination);
                return vImageConvert_Planar8To16U(&sourceBuffer, &destinationBuffer, kvImageNoFlags) == kvImageNoError;
            }
            return lookUp(source, destination, to.bitDepth == 10 ? eightBitToTenBitLSB : eightBitToTwelveBitLSB);
        }
        if (to == eightBit) {
            if (from.isMSBAligned)
                return extractByte(source, 1, destination);
            if (from.bitDepth == 10)
                return lookUpToEightBit(source, destination, sixteenBitLookupTableSingleton<SixteenBitTable::TenBitLSBToEightBit>().span());
            return lookUpToEightBit(source, destination, sixteenBitLookupTableSingleton<SixteenBitTable::TwelveBitLSBToEightBit>().span());
        }
        if (to == sixteenBit) {
            if (from.bitDepth == 10)
                return lookUp(source, destination, sixteenBitLookupTableSingleton<SixteenBitTable::TenBitLSBToSixteenBit>().span());
            return lookUp(source, destination, sixteenBitLookupTableSingleton<SixteenBitTable::TwelveBitLSBToSixteenBit>().span());
        }
        if (to.isMSBAligned) {
            if (from.bitDepth == 10)
                return lookUp(source, destination, sixteenBitLookupTableSingleton<SixteenBitTable::TenBitLSBToMSB>().span());
            return lookUp(source, destination, sixteenBitLookupTableSingleton<SixteenBitTable::TwelveBitLSBToMSB>().span());
        }
        if (from.isMSBAligned) {
            if (to.bitDepth == 10)
                return lookUp(source, destination, sixteenBitLookupTableSingleton<SixteenBitTable::MSBToTenBitLSB>().span());
            return lookUp(source, destination, sixteenBitLookupTableSingleton<SixteenBitTable::MSBToTwelveBitLSB>().span());
        }
        return false;
    case RangeConversion::FullToVideo:
        if (to != tenBitMSB)
            return false;
        if (from == eightBit)
            return lookUp(source, destination, component == Component::Chroma ? eightBitFullToTenBitVideoChroma : eightBitFullToTenBitVideoLuma);
        if (from == tenBitMSB)
            return lookUp(source, destination, sixteenBitLookupTable<SixteenBitTable::FullToVideoLuma, SixteenBitTable::FullToVideoChroma>(component));
        if (from == tenBitLSB)
            return lookUp(source, destination, sixteenBitLookupTable<SixteenBitTable::TenBitLSBFullToVideoLuma, SixteenBitTable::TenBitLSBFullToVideoChroma>(component));
        return false;
    case RangeConversion::VideoToFull:
        if (from != tenBitMSB || to != eightBit)
            return false;
        return lookUpToEightBit(source, destination, sixteenBitLookupTable<SixteenBitTable::VideoToFullLuma, SixteenBitTable::VideoToFullChroma>(component));
    }
    ASSERT_NOT_REACHED();
    return false;
}

static bool interleaveEightBit(const SourcePlane& u, const SourcePlane& v, const DestinationPlane& uv)
{
    auto uBuffer = vImageBuffer(u);
    auto vBuffer = vImageBuffer(v);
    std::array<const vImage_Buffer*, 2> sourceBuffers { &uBuffer, &vBuffer };
    std::array<void*, 2> destinationChannels { uv.rows.data(), uv.rows.subspan(1).data() };
    return vImageConvert_PlanarToChunky8(sourceBuffers.data(), destinationChannels.data(), 2, 2, uv.width, uv.height, uv.stride, kvImageNoFlags) == kvImageNoError;
}

static bool interleaveWithPlatformFrameworks(const SourcePlane& u, const SourcePlane& v, const DestinationPlane& uv, RangeConversion conversion)
{
    if (u.format != eightBit)
        return false;
    if (conversion == RangeConversion::None && uv.format == eightBit)
        return interleaveEightBit(u, v, uv);
    return false;
}

static bool deinterleaveEightBit(const SourcePlane& uv, size_t sampleSize, const DestinationPlane& u, const DestinationPlane& v)
{
    std::array<const void*, 2> sourceChannels { uv.rows.data(), uv.rows.subspan(sampleSize).data() };
    auto uBuffer = vImageBuffer(u);
    auto vBuffer = vImageBuffer(v);
    std::array<const vImage_Buffer*, 2> destinationBuffers { &uBuffer, &vBuffer };
    return vImageConvert_ChunkyToPlanar8(sourceChannels.data(), destinationBuffers.data(), 2, 2 * sampleSize, u.width, u.height, uv.stride, kvImageNoFlags) == kvImageNoError;
}

static bool deinterleaveWithPlatformFrameworks(const SourcePlane& uv, const DestinationPlane& u, const DestinationPlane& v, RangeConversion conversion)
{
    if (u.format != eightBit)
        return false;
    if (conversion == RangeConversion::None && uv.format == eightBit)
        return deinterleaveEightBit(uv, 1, u, v);
    if (conversion == RangeConversion::VideoToFull && uv.format == tenBitMSB) {
        // The table leaves 8-bit values in the low byte of each sample.
        Vector<uint16_t> storage;
        auto scratch = packedPlane(storage, uv.width, uv.height, sixteenBit, 2);
        return lookUp(uv, scratch, sixteenBitLookupTableSingleton<SixteenBitTable::VideoToFullChroma>().span()) && deinterleaveEightBit(asSource(scratch), sizeof(uint16_t), u, v);
    }
    return false;
}

static bool copyPlaneToChannelWithPlatformFrameworks(const SourcePlane& source, const DestinationPlane& destination, uint8_t channel)
{
    auto sourceBuffer = vImageBuffer(source);
    auto destinationBuffer = vImageBuffer(destination, destination.width);
    // The mask names the channels in memory order, from 0x8 for the first to 0x1 for the last.
    uint8_t channelMask = 0x8 >> channel;
    return vImageOverwriteChannels_ARGB8888(&sourceBuffer, &destinationBuffer, &destinationBuffer, channelMask, kvImageNoFlags) == kvImageNoError;
}

static bool permuteChannelsWithPlatformFrameworks(const SourcePlane& source, const DestinationPlane& destination, std::array<uint8_t, 4> channelMap, std::optional<uint8_t> opaqueChannel)
{
    auto sourceBuffer = vImageBuffer(source, source.width);
    auto destinationBuffer = vImageBuffer(destination, destination.width);
    if (!opaqueChannel)
        return vImagePermuteChannels_ARGB8888(&sourceBuffer, &destinationBuffer, channelMap.data(), kvImageNoFlags) == kvImageNoError;

    // The mask names the channels in memory order, from 0x8 for the first to 0x1 for the last.
    uint8_t channelMask = 0x8 >> *opaqueChannel;
    const Pixel_8888 opaque { 255, 255, 255, 255 };
    return vImagePermuteChannelsWithMaskedInsert_ARGB8888(&sourceBuffer, &destinationBuffer, channelMap.data(), channelMask, opaque, kvImageNoFlags) == kvImageNoError;
}

#else

static bool copyPlaneWithPlatformFrameworks(const SourcePlane&, const DestinationPlane&, RangeConversion, Component)
{
    return false;
}

static bool interleaveWithPlatformFrameworks(const SourcePlane&, const SourcePlane&, const DestinationPlane&, RangeConversion)
{
    return false;
}

static bool deinterleaveWithPlatformFrameworks(const SourcePlane&, const DestinationPlane&, const DestinationPlane&, RangeConversion)
{
    return false;
}

static bool copyPlaneToChannelWithPlatformFrameworks(const SourcePlane&, const DestinationPlane&, uint8_t)
{
    return false;
}

static bool permuteChannelsWithPlatformFrameworks(const SourcePlane&, const DestinationPlane&, std::array<uint8_t, 4>, std::optional<uint8_t>)
{
    return false;
}

#endif // USE(ACCELERATE)

#if USE(LIBWEBRTC)

static const uint16_t* sixteenBitSamples(const SourcePlane& plane)
{
    return spanReinterpretCast<const uint16_t>(plane.rows.first(plane.rows.size() & ~1)).data();
}

static uint16_t* sixteenBitSamples(const DestinationPlane& plane)
{
    return spanReinterpretCast<uint16_t>(plane.rows.first(plane.rows.size() & ~1)).data();
}

// makePlane() checked that the sizes fit in an int.
static int sampleStride(const auto& plane)
{
    return static_cast<int>(plane.stride / plane.format.sampleSize());
}

static int samplesPerRow(const auto& plane)
{
    return static_cast<int>(plane.width * plane.channels);
}

// Fills the unused low bits of MSB aligned samples with their top bits, so the largest sourceBitDepth value becomes 0xFFFF.
static void replicateTopBits(const DestinationPlane& plane, unsigned sourceBitDepth)
{
    for (size_t row = 0; row < plane.height; ++row) {
        for (auto& sample : spanReinterpretCast<uint16_t>(plane.row(row)))
            sample |= sample >> sourceBitDepth;
    }
}

static bool copyPlaneWithLibYUV(const SourcePlane& source, const DestinationPlane& destination, RangeConversion conversion)
{
    if (conversion != RangeConversion::None)
        return false;

    auto from = source.format;
    auto to = destination.format;
    int width = samplesPerRow(source);
    int height = static_cast<int>(source.height);
    if (haveSameRepresentation(from, to)) {
        libyuv::CopyPlane(source.rows.data(), static_cast<int>(source.stride), destination.rows.data(), static_cast<int>(destination.stride), static_cast<int>(source.rowBytes()), height);
        return true;
    }
    if (from == eightBit) {
        libyuv::Convert8To16Plane(source.rows.data(), static_cast<int>(source.stride), sixteenBitSamples(destination), sampleStride(destination), to.isMSBAligned ? 16 : to.bitDepth, width, height);
        return true;
    }
    if (to == eightBit) {
        libyuv::Convert16To8Plane(sixteenBitSamples(source), sampleStride(source), destination.rows.data(), static_cast<int>(destination.stride), from.isMSBAligned ? 256 : 1 << (24 - from.bitDepth), width, height);
        return true;
    }
    if (to.isMSBAligned) {
        libyuv::ConvertToMSBPlane_16(sixteenBitSamples(source), sampleStride(source), sixteenBitSamples(destination), sampleStride(destination), width, height, from.bitDepth);
        if (to == sixteenBit)
            replicateTopBits(destination, from.bitDepth);
        return true;
    }
    if (from.isMSBAligned) {
        libyuv::ConvertToLSBPlane_16(sixteenBitSamples(source), sampleStride(source), sixteenBitSamples(destination), sampleStride(destination), width, height, to.bitDepth);
        return true;
    }
    return false;
}

static bool interleaveWithLibYUV(const SourcePlane& u, const SourcePlane& v, const DestinationPlane& uv, RangeConversion conversion)
{
    if (conversion != RangeConversion::None)
        return false;
    int width = static_cast<int>(u.width);
    int height = static_cast<int>(u.height);
    if (u.format == eightBit && uv.format == eightBit) {
        libyuv::MergeUVPlane(u.rows.data(), static_cast<int>(u.stride), v.rows.data(), static_cast<int>(v.stride), uv.rows.data(), static_cast<int>(uv.stride), width, height);
        return true;
    }
    if (u.format.sampleSize() > 1 && !u.format.isMSBAligned && uv.format.isMSBAligned) {
        libyuv::MergeUVPlane_16(sixteenBitSamples(u), sampleStride(u), sixteenBitSamples(v), sampleStride(v), sixteenBitSamples(uv), sampleStride(uv), width, height, u.format.bitDepth);
        return true;
    }
    if (u.format.sampleSize() > 1 && haveSameRepresentation(u.format, uv.format)) {
        libyuv::MergeUVPlane_16(sixteenBitSamples(u), sampleStride(u), sixteenBitSamples(v), sampleStride(v), sixteenBitSamples(uv), sampleStride(uv), width, height, 16);
        return true;
    }
    return false;
}

static bool deinterleaveWithLibYUV(const SourcePlane& uv, const DestinationPlane& u, const DestinationPlane& v, RangeConversion conversion)
{
    if (conversion != RangeConversion::None)
        return false;
    int width = static_cast<int>(u.width);
    int height = static_cast<int>(u.height);
    if (u.format == eightBit && uv.format == eightBit) {
        libyuv::SplitUVPlane(uv.rows.data(), static_cast<int>(uv.stride), u.rows.data(), static_cast<int>(u.stride), v.rows.data(), static_cast<int>(v.stride), width, height);
        return true;
    }
    if (u.format.sampleSize() > 1 && !u.format.isMSBAligned && uv.format.isMSBAligned) {
        libyuv::SplitUVPlane_16(sixteenBitSamples(uv), sampleStride(uv), sixteenBitSamples(u), sampleStride(u), sixteenBitSamples(v), sampleStride(v), width, height, u.format.bitDepth);
        return true;
    }
    return false;
}

static bool downsampleWithLibYUV(const SourcePlane& source, const DestinationPlane& destination)
{
    int sourceWidth = static_cast<int>(source.width);
    int sourceHeight = static_cast<int>(source.height);
    int destinationWidth = static_cast<int>(destination.width);
    int destinationHeight = static_cast<int>(destination.height);
    if (source.format.sampleSize() == 1)
        return !libyuv::ScalePlane(source.rows.data(), static_cast<int>(source.stride), sourceWidth, sourceHeight, destination.rows.data(), static_cast<int>(destination.stride), destinationWidth, destinationHeight, libyuv::kFilterBox);
    return !libyuv::ScalePlane_16(sixteenBitSamples(source), sampleStride(source), sourceWidth, sourceHeight, sixteenBitSamples(destination), sampleStride(destination), destinationWidth, destinationHeight, libyuv::kFilterBox);
}

// libyuv only writes the last channel.
static bool copyPlaneToChannelWithLibYUV(const SourcePlane& source, const DestinationPlane& destination, uint8_t channel)
{
    if (channel != 3)
        return false;
    return !libyuv::ARGBCopyYToAlpha(source.rows.data(), static_cast<int>(source.stride), destination.rows.data(), static_cast<int>(destination.stride), static_cast<int>(source.width), static_cast<int>(source.height));
}

#else

static bool copyPlaneWithLibYUV(const SourcePlane&, const DestinationPlane&, RangeConversion)
{
    return false;
}

static bool interleaveWithLibYUV(const SourcePlane&, const SourcePlane&, const DestinationPlane&, RangeConversion)
{
    return false;
}

static bool deinterleaveWithLibYUV(const SourcePlane&, const DestinationPlane&, const DestinationPlane&, RangeConversion)
{
    return false;
}

static bool downsampleWithLibYUV(const SourcePlane&, const DestinationPlane&)
{
    return false;
}

static bool copyPlaneToChannelWithLibYUV(const SourcePlane&, const DestinationPlane&, uint8_t)
{
    return false;
}

#endif // USE(LIBWEBRTC)

// libyuv shifts 16-bit samples directly; vImage needs a 64K-entry lookup table, and a scratch plane for 8-bit results.
static bool isSixteenBitShift(SampleFormat from, SampleFormat to, RangeConversion conversion)
{
    return conversion == RangeConversion::None && from.sampleSize() > 1 && !haveSameRepresentation(from, to) && !(from.isMSBAligned && to == eightBit);
}

bool copyPlane(const SourcePlane& source, const DestinationPlane& destination, RangeConversion conversion, Component component)
{
    if (!haveSameSize(source, destination))
        return false;

    AlignedSource alignedSource { source };
    AlignedDestination alignedDestination { destination };
    auto copy = [&] {
        return copyPlaneWithCopy(alignedSource.plane(), alignedDestination.plane(), conversion);
    };
    auto preferredBackend = isSixteenBitShift(source.format, destination.format, conversion) ? PreferredBackend::LibYUV : PreferredBackend::PlatformFrameworks;
    bool succeeded = perform([&] { return copyPlaneWithPlatformFrameworks(alignedSource.plane(), alignedDestination.plane(), conversion, component); },
        [&] { return copyPlaneWithLibYUV(alignedSource.plane(), alignedDestination.plane(), conversion); },
        copy, preferredBackend);
    if (!succeeded)
        return false;
    alignedDestination.commit();
    return true;
}

bool interleave(const SourcePlane& u, const SourcePlane& v, const DestinationPlane& uv, RangeConversion conversion)
{
    if (!haveSameSize(u, v) || u.format != v.format || u.channels != 1 || uv.channels != 2 || u.width != uv.width || u.height != uv.height)
        return false;

    // Cb and Cr share a table, so the interleaved samples are converted in one pass.
    if (conversion == RangeConversion::FullToVideo && (u.format == eightBit || u.format == tenBitLSB) && uv.format == tenBitMSB) {
        Vector<uint16_t> storage;
        auto scratch = packedPlane(storage, uv.width, uv.height, u.format, 2);
        return interleave(u, v, scratch, RangeConversion::None) && copyPlane(asSource(scratch), uv, conversion, Component::Chroma);
    }

    AlignedSource alignedU { u };
    AlignedSource alignedV { v };
    AlignedDestination alignedUV { uv };
    bool succeeded = perform([&] { return interleaveWithPlatformFrameworks(alignedU.plane(), alignedV.plane(), alignedUV.plane(), conversion); },
        [&] { return interleaveWithLibYUV(alignedU.plane(), alignedV.plane(), alignedUV.plane(), conversion); },
        unsupported);
    if (!succeeded)
        return false;
    alignedUV.commit();
    return true;
}

bool deinterleave(const SourcePlane& uv, const DestinationPlane& u, const DestinationPlane& v, RangeConversion conversion)
{
    if (!haveSameSize(u, v) || u.format != v.format || u.channels != 1 || uv.channels != 2 || u.width != uv.width || u.height != uv.height)
        return false;

    AlignedSource alignedUV { uv };
    AlignedDestination alignedU { u };
    AlignedDestination alignedV { v };
    bool succeeded = perform([&] { return deinterleaveWithPlatformFrameworks(alignedUV.plane(), alignedU.plane(), alignedV.plane(), conversion); },
        [&] { return deinterleaveWithLibYUV(alignedUV.plane(), alignedU.plane(), alignedV.plane(), conversion); },
        unsupported);
    if (!succeeded)
        return false;
    alignedU.commit();
    alignedV.commit();
    return true;
}

static bool isHalvedOrEqual(size_t source, size_t destination)
{
    return destination == source || destination == (source + 1) / 2;
}

bool downsample(const SourcePlane& source, const DestinationPlane& destination)
{
    if (source.format != destination.format || source.channels != 1 || destination.channels != 1
        || !isHalvedOrEqual(source.width, destination.width) || !isHalvedOrEqual(source.height, destination.height))
        return false;

    AlignedSource alignedSource { source };
    AlignedDestination alignedDestination { destination };
    if (!perform(unsupported, [&] { return downsampleWithLibYUV(alignedSource.plane(), alignedDestination.plane()); }, unsupported))
        return false;
    alignedDestination.commit();
    return true;
}

bool copyPlaneToChannel(const SourcePlane& source, const DestinationPlane& destination, uint8_t channel)
{
    if (source.format != eightBit || destination.format != eightBit || source.channels != 1 || destination.channels != 4 || channel > 3
        || source.width != destination.width || source.height != destination.height)
        return false;

    auto libYUV = [&] {
        return copyPlaneToChannelWithLibYUV(source, destination, channel);
    };
    return perform([&] { return copyPlaneToChannelWithPlatformFrameworks(source, destination, channel); }, libYUV, unsupported);
}

static bool isIdentity(std::array<uint8_t, 4> channelMap)
{
    return channelMap == std::array<uint8_t, 4> { 0, 1, 2, 3 };
}

bool permuteChannels(const SourcePlane& source, const DestinationPlane& destination, std::array<uint8_t, 4> channelMap, std::optional<uint8_t> opaqueChannel)
{
    if (source.format != eightBit || destination.format != eightBit || source.channels != 4 || !haveSameSize(source, destination)
        || std::ranges::any_of(channelMap, [](auto channel) { return channel > 3; }) || (opaqueChannel && *opaqueChannel > 3))
        return false;

    auto copy = [&] {
        if (!isIdentity(channelMap) || opaqueChannel)
            return false;
        copyRows(source, destination);
        return true;
    };
    return perform([&] { return permuteChannelsWithPlatformFrameworks(source, destination, channelMap, opaqueChannel); }, unsupported, copy);
}

} // namespace WebCore::PlanarImageConverter
