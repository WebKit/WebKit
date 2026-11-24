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

#include <WebCore/PlanarImageConverter.h>
#include <limits>
#include <wtf/Vector.h>

namespace TestWebKitAPI {

using namespace WebCore::PlanarImageConverter;

namespace {

class BackendScope {
    WTF_MAKE_NONCOPYABLE(BackendScope);
public:
    explicit BackendScope(Backend backend) { setBackendForTesting(backend); }
    ~BackendScope() { setBackendForTesting(std::nullopt); }
};

}

static Vector<Backend> conversionBackends()
{
    Vector<Backend> backends;
    for (auto backend : { Backend::PlatformFrameworks, Backend::LibYUV }) {
        if (isBackendAvailable(backend))
            backends.append(backend);
    }
    return backends;
}

static constexpr uint8_t guardByte = 0xEE;

static DestinationPlane allocatePlane(Vector<uint8_t>& storage, size_t width, size_t height, SampleFormat format, uint8_t channels = 1, size_t offset = 0, size_t padding = 0)
{
    size_t stride = width * channels * format.sampleSize() + padding;
    storage = Vector<uint8_t>(FillWith { }, offset + stride * height, guardByte);
    return *makePlane(storage.mutableSpan(), offset, stride, width, height, format, channels);
}

static uint16_t sampleAt(const SourcePlane& plane, size_t x, size_t y, size_t channel = 0)
{
    auto row = plane.row(y);
    size_t index = (x * plane.channels + channel) * plane.format.sampleSize();
    if (plane.format.sampleSize() == 1)
        return row[index];
    return row[index] | (row[index + 1] << 8);
}

static void setSampleAt(const DestinationPlane& plane, size_t x, size_t y, uint16_t value, size_t channel = 0)
{
    auto row = plane.row(y);
    size_t index = (x * plane.channels + channel) * plane.format.sampleSize();
    row[index] = value & 0xff;
    if (plane.format.sampleSize() > 1)
        row[index + 1] = value >> 8;
}

static void fill(const DestinationPlane& plane, uint16_t value)
{
    for (size_t y = 0; y < plane.height; ++y) {
        for (size_t x = 0; x < plane.width; ++x) {
            for (size_t channel = 0; channel < plane.channels; ++channel)
                setSampleAt(plane, x, y, value, channel);
        }
    }
}

// Bytes outside a plane's rows keep the guard byte.
static size_t countModifiedBytesOutsideRows(std::span<const uint8_t> storage, size_t offset, const SourcePlane& plane)
{
    size_t count = 0;
    for (size_t index = 0; index < storage.size(); ++index) {
        bool isInRows = index >= offset && index < offset + plane.rows.size() && (index - offset) % plane.stride < plane.rowBytes();
        if (!isInRows && storage[index] != guardByte)
            ++count;
    }
    return count;
}

TEST(PlanarImageConverter, MakePlaneChecksBounds)
{
    std::array<uint8_t, 64> buffer { };
    std::span<uint8_t> bytes { buffer };

    // The last row ends exactly at the end of the buffer.
    auto plane = makePlane(bytes, 8, 20, 8, 3, tenBitLSB);
    ASSERT_TRUE(plane);
    EXPECT_EQ(plane->rows.size(), 56u);
    EXPECT_EQ(plane->rowBytes(), 16u);

    EXPECT_FALSE(makePlane(bytes, 9, 20, 8, 3, tenBitLSB));
    EXPECT_FALSE(makePlane(bytes, 0, 15, 8, 1, tenBitLSB));
    EXPECT_FALSE(makePlane(bytes, 0, 16, 0, 1, eightBit));
    EXPECT_FALSE(makePlane(bytes, 0, 16, 1, 0, eightBit));
    EXPECT_FALSE(makePlane(bytes, 0, 16, 4, 1, eightBit, 0));
    EXPECT_FALSE(makePlane(bytes, 0, 16, 4, 1, SampleFormat { 9, false }));
    EXPECT_FALSE(makePlane(bytes, 64, 1, 1, 1, eightBit));
    EXPECT_FALSE(makePlane(bytes, std::numeric_limits<size_t>::max(), 1, 1, 1, eightBit));
    EXPECT_FALSE(makePlane(bytes, 0, std::numeric_limits<size_t>::max(), 1, 2, eightBit));
    EXPECT_FALSE(makePlane(bytes, 0, 1, std::numeric_limits<size_t>::max(), 1, sixteenBit));
    EXPECT_FALSE(makePlane(bytes, 0, static_cast<size_t>(std::numeric_limits<int>::max()) + 1, 1, 1, eightBit));
}

TEST(PlanarImageConverter, MakePlaneNormalizesSampleFormats)
{
    std::array<uint8_t, 16> buffer { };
    std::span<uint8_t> bytes { buffer };
    EXPECT_EQ(makePlane(bytes, 0, 4, 4, 1, SampleFormat { 8, true })->format, eightBit);
    EXPECT_EQ(makePlane(bytes, 0, 8, 4, 1, SampleFormat { 16, false })->format, sixteenBit);
    EXPECT_EQ(makePlane(bytes, 0, 8, 4, 1, tenBitLSB)->format, tenBitLSB);
}

TEST(PlanarImageConverter, MakePlaneFromSixteenBitSamples)
{
    std::array<uint16_t, 32> buffer { };
    std::span<uint16_t> samples { buffer };

    // The last row ends 2 samples before the end of the buffer.
    auto plane = makePlane(samples, 2, 10, 8, 3, tenBitLSB);
    ASSERT_TRUE(plane);
    EXPECT_EQ(plane->rows.data(), asByteSpan(samples).subspan(4).data());
    EXPECT_EQ(plane->stride, 20u);
    EXPECT_EQ(plane->rowBytes(), 16u);

    EXPECT_TRUE(makePlane(std::span<const uint16_t> { buffer }, 0, 8, 8, 4, sixteenBit));
    EXPECT_FALSE(makePlane(samples, 5, 10, 8, 3, tenBitLSB));
    EXPECT_FALSE(makePlane(samples, 0, 8, 8, 1, eightBit));
    EXPECT_FALSE(makePlane(samples, std::numeric_limits<size_t>::max(), 1, 1, 1, tenBitLSB));
    EXPECT_FALSE(makePlane(samples, 0, std::numeric_limits<size_t>::max(), 1, 1, tenBitLSB));
}

TEST(PlanarImageConverter, ChromaSize)
{
    EXPECT_EQ(chromaWidth(ChromaSubsampling::HorizontalAndVertical, 33), 17u);
    EXPECT_EQ(chromaHeight(ChromaSubsampling::HorizontalAndVertical, 17), 9u);
    EXPECT_EQ(chromaWidth(ChromaSubsampling::Horizontal, 33), 17u);
    EXPECT_EQ(chromaHeight(ChromaSubsampling::Horizontal, 17), 17u);
    EXPECT_EQ(chromaWidth(ChromaSubsampling::None, 33), 33u);
    EXPECT_EQ(chromaHeight(ChromaSubsampling::None, 17), 17u);
    EXPECT_EQ(chromaWidth(ChromaSubsampling::HorizontalAndVertical, 64), 32u);
    EXPECT_EQ(chromaHeight(ChromaSubsampling::HorizontalAndVertical, 48), 24u);
}

TEST(PlanarImageConverter, Subplane)
{
    Vector<uint8_t> storage;
    auto plane = allocatePlane(storage, 6, 4, tenBitLSB, 2, 3, 5);
    for (size_t y = 0; y < plane.height; ++y) {
        for (size_t x = 0; x < plane.width; ++x) {
            setSampleAt(plane, x, y, y * 100 + x * 10, 0);
            setSampleAt(plane, x, y, y * 100 + x * 10 + 1, 1);
        }
    }

    auto rect = subplane(asSource(plane), 2, 1, 3, 2);
    ASSERT_TRUE(rect);
    EXPECT_EQ(rect->width, 3u);
    EXPECT_EQ(rect->height, 2u);
    EXPECT_EQ(sampleAt(*rect, 0, 0, 0), 120);
    EXPECT_EQ(sampleAt(*rect, 2, 1, 1), 241);

    EXPECT_TRUE(subplane(asSource(plane), 0, 0, 6, 4));
    EXPECT_FALSE(subplane(asSource(plane), 4, 0, 3, 1));
    EXPECT_FALSE(subplane(asSource(plane), 0, 3, 1, 2));
    EXPECT_FALSE(subplane(asSource(plane), 0, 0, 0, 1));
    EXPECT_FALSE(subplane(asSource(plane), std::numeric_limits<size_t>::max(), 0, 2, 1));
}

struct ConversionCase {
    SampleFormat from;
    SampleFormat to;
    uint16_t input;
    uint16_t expected;
};

static constexpr std::array conversionCases {
    ConversionCase { eightBit, eightBit, 77, 77 },
    ConversionCase { tenBitLSB, tenBitLSB, 513, 513 },
    ConversionCase { tenBitMSB, twelveBitMSB, 0x1234, 0x1234 },
    ConversionCase { sixteenBit, tenBitMSB, 0xFFFF, 0xFFFF },
    ConversionCase { eightBit, tenBitLSB, 255, 1023 },
    ConversionCase { eightBit, tenBitLSB, 128, 514 },
    ConversionCase { eightBit, twelveBitLSB, 255, 4095 },
    ConversionCase { eightBit, twelveBitLSB, 1, 16 },
    ConversionCase { eightBit, tenBitMSB, 255, 0xFFFF },
    ConversionCase { eightBit, sixteenBit, 1, 0x0101 },
    ConversionCase { tenBitLSB, eightBit, 1023, 255 },
    ConversionCase { tenBitLSB, eightBit, 7, 1 },
    ConversionCase { twelveBitLSB, eightBit, 4095, 255 },
    ConversionCase { twelveBitLSB, eightBit, 31, 1 },
    ConversionCase { tenBitMSB, eightBit, 0xABCD, 0xAB },
    ConversionCase { tenBitLSB, tenBitMSB, 1023, 0xFFC0 },
    ConversionCase { tenBitLSB, tenBitMSB, 1, 0x0040 },
    ConversionCase { twelveBitLSB, twelveBitMSB, 4095, 0xFFF0 },
    ConversionCase { tenBitLSB, sixteenBit, 1023, 0xFFFF },
    ConversionCase { tenBitLSB, sixteenBit, 512, 0x8020 },
    ConversionCase { tenBitLSB, sixteenBit, 1, 0x0040 },
    ConversionCase { twelveBitLSB, sixteenBit, 4095, 0xFFFF },
    ConversionCase { twelveBitLSB, sixteenBit, 2048, 0x8008 },
    ConversionCase { twelveBitLSB, sixteenBit, 1, 0x0010 },
    ConversionCase { tenBitMSB, tenBitLSB, 0xFFC0, 1023 },
    ConversionCase { twelveBitMSB, twelveBitLSB, 0xFFF0, 4095 },
    ConversionCase { sixteenBit, twelveBitLSB, 0x0010, 1 },
    ConversionCase { sixteenBit, tenBitLSB, 0xFFFF, 1023 },
    ConversionCase { sixteenBit, twelveBitLSB, 0xFFFF, 4095 },
};

TEST(PlanarImageConverter, CopyPlaneConvertsBitDepthAndAlignment)
{
    for (auto backend : conversionBackends()) {
        BackendScope scope { backend };
        for (auto& testCase : conversionCases) {
            for (uint8_t channels : { 1, 2 }) {
                Vector<uint8_t> sourceStorage;
                Vector<uint8_t> destinationStorage;
                auto source = allocatePlane(sourceStorage, 3, 2, testCase.from, channels);
                auto destination = allocatePlane(destinationStorage, 3, 2, testCase.to, channels);
                fill(source, testCase.input);
                ASSERT_TRUE(copyPlane(asSource(source), destination)) << "backend " << static_cast<int>(backend) << " from " << static_cast<int>(testCase.from.bitDepth) << " to " << static_cast<int>(testCase.to.bitDepth);
                for (size_t y = 0; y < 2; ++y) {
                    for (size_t x = 0; x < 3; ++x) {
                        for (size_t channel = 0; channel < channels; ++channel)
                            EXPECT_EQ(sampleAt(asSource(destination), x, y, channel), testCase.expected) << "backend " << static_cast<int>(backend) << " input " << testCase.input;
                    }
                }
            }
        }
    }
}

TEST(PlanarImageConverter, CopyPlaneRejectsUnsupportedConversions)
{
    Vector<uint8_t> sourceStorage;
    Vector<uint8_t> destinationStorage;
    auto tenBit = allocatePlane(sourceStorage, 2, 2, tenBitLSB);
    auto twelveBit = allocatePlane(destinationStorage, 2, 2, twelveBitLSB);
    EXPECT_FALSE(copyPlane(asSource(tenBit), twelveBit));

    Vector<uint8_t> smallStorage;
    auto small = allocatePlane(smallStorage, 1, 2, tenBitLSB);
    EXPECT_FALSE(copyPlane(asSource(tenBit), small));
}

TEST(PlanarImageConverter, CopyBackendOnlyCopies)
{
    BackendScope scope { Backend::Copy };
    Vector<uint8_t> sourceStorage;
    Vector<uint8_t> destinationStorage;
    auto source = allocatePlane(sourceStorage, 3, 2, tenBitLSB, 1, 1, 3);
    auto destination = allocatePlane(destinationStorage, 3, 2, tenBitLSB, 1, 3, 1);
    fill(source, 700);
    ASSERT_TRUE(copyPlane(asSource(source), destination));
    EXPECT_EQ(sampleAt(asSource(destination), 2, 1), 700);
    EXPECT_EQ(countModifiedBytesOutsideRows(destinationStorage.span(), 3, asSource(destination)), 0u);

    Vector<uint8_t> msbStorage;
    auto msb = allocatePlane(msbStorage, 3, 2, tenBitMSB);
    EXPECT_FALSE(copyPlane(asSource(source), msb));
}

// WebCodecs layouts can put 16-bit samples at odd offsets and strides.
TEST(PlanarImageConverter, CopyPlaneHandlesUnalignedSixteenBitPlanes)
{
    for (auto backend : conversionBackends()) {
        BackendScope scope { backend };
        Vector<uint8_t> sourceStorage;
        Vector<uint8_t> destinationStorage;
        auto source = allocatePlane(sourceStorage, 3, 3, tenBitLSB, 1, 1, 1);
        auto destination = allocatePlane(destinationStorage, 3, 3, tenBitMSB, 1, 3, 3);
        for (size_t y = 0; y < 3; ++y) {
            for (size_t x = 0; x < 3; ++x)
                setSampleAt(source, x, y, 100 * y + x);
        }
        ASSERT_TRUE(copyPlane(asSource(source), destination)) << "backend " << static_cast<int>(backend);
        for (size_t y = 0; y < 3; ++y) {
            for (size_t x = 0; x < 3; ++x)
                EXPECT_EQ(sampleAt(asSource(destination), x, y), (100 * y + x) << 6);
        }
        EXPECT_EQ(countModifiedBytesOutsideRows(destinationStorage.span(), 3, asSource(destination)), 0u);
    }
}

TEST(PlanarImageConverter, InterleaveAndDeinterleaveEightBit)
{
    for (auto backend : conversionBackends()) {
        BackendScope scope { backend };
        Vector<uint8_t> uStorage;
        Vector<uint8_t> vStorage;
        Vector<uint8_t> uvStorage;
        auto u = allocatePlane(uStorage, 3, 3, eightBit, 1, 0, 2);
        auto v = allocatePlane(vStorage, 3, 3, eightBit, 1, 1, 0);
        auto uv = allocatePlane(uvStorage, 3, 3, eightBit, 2, 2, 3);
        for (size_t y = 0; y < 3; ++y) {
            for (size_t x = 0; x < 3; ++x) {
                setSampleAt(u, x, y, 10 * y + x);
                setSampleAt(v, x, y, 100 + 10 * y + x);
            }
        }
        ASSERT_TRUE(interleave(asSource(u), asSource(v), uv)) << "backend " << static_cast<int>(backend);
        EXPECT_EQ(sampleAt(asSource(uv), 2, 1, 0), 12);
        EXPECT_EQ(sampleAt(asSource(uv), 2, 1, 1), 112);
        EXPECT_EQ(countModifiedBytesOutsideRows(uvStorage.span(), 2, asSource(uv)), 0u);

        fill(u, 0);
        fill(v, 0);
        ASSERT_TRUE(deinterleave(asSource(uv), u, v));
        for (size_t y = 0; y < 3; ++y) {
            for (size_t x = 0; x < 3; ++x) {
                EXPECT_EQ(sampleAt(asSource(u), x, y), 10 * y + x);
                EXPECT_EQ(sampleAt(asSource(v), x, y), 100 + 10 * y + x);
            }
        }
    }
}

TEST(PlanarImageConverter, InterleaveAndDeinterleaveSixteenBit)
{
    if (!isBackendAvailable(Backend::LibYUV))
        return;

    for (auto format : { tenBitLSB, twelveBitLSB }) {
        Vector<uint8_t> uStorage;
        Vector<uint8_t> vStorage;
        Vector<uint8_t> uvStorage;
        auto u = allocatePlane(uStorage, 3, 2, format, 1, 1, 1);
        auto v = allocatePlane(vStorage, 3, 2, format);
        auto uv = allocatePlane(uvStorage, 3, 2, tenBitMSB, 2, 1, 3);
        for (size_t y = 0; y < 2; ++y) {
            for (size_t x = 0; x < 3; ++x) {
                setSampleAt(u, x, y, 300 + 10 * y + x);
                setSampleAt(v, x, y, 600 + 10 * y + x);
            }
        }
        ASSERT_TRUE(interleave(asSource(u), asSource(v), uv));
        unsigned shift = 16 - format.bitDepth;
        EXPECT_EQ(sampleAt(asSource(uv), 1, 1, 0), 311 << shift);
        EXPECT_EQ(sampleAt(asSource(uv), 1, 1, 1), 611 << shift);
        EXPECT_EQ(countModifiedBytesOutsideRows(uvStorage.span(), 1, asSource(uv)), 0u);

        fill(u, 0);
        fill(v, 0);
        ASSERT_TRUE(deinterleave(asSource(uv), u, v));
        for (size_t y = 0; y < 2; ++y) {
            for (size_t x = 0; x < 3; ++x) {
                EXPECT_EQ(sampleAt(asSource(u), x, y), 300 + 10 * y + x);
                EXPECT_EQ(sampleAt(asSource(v), x, y), 600 + 10 * y + x);
            }
        }
    }
}

TEST(PlanarImageConverter, DownsampleWithBoxFilter)
{
    if (!isBackendAvailable(Backend::LibYUV))
        return;

    Vector<uint8_t> sourceStorage;
    auto source = allocatePlane(sourceStorage, 4, 4, tenBitMSB);
    for (size_t y = 0; y < 4; ++y) {
        for (size_t x = 0; x < 4; ++x)
            setSampleAt(source, x, y, 1000 * (y + 1) + 100 * x);
    }

    // 4:4:4 to 4:2:0.
    Vector<uint8_t> quarterStorage;
    auto quarter = allocatePlane(quarterStorage, 2, 2, tenBitMSB);
    ASSERT_TRUE(downsample(asSource(source), quarter));
    EXPECT_EQ(sampleAt(asSource(quarter), 0, 0), 1550);
    EXPECT_EQ(sampleAt(asSource(quarter), 1, 1), 3750);

    // 4:2:2 to 4:2:0.
    Vector<uint8_t> halfStorage;
    auto half = allocatePlane(halfStorage, 4, 2, tenBitMSB);
    ASSERT_TRUE(downsample(asSource(source), half));
    EXPECT_EQ(sampleAt(asSource(half), 0, 0), 1500);
    EXPECT_EQ(sampleAt(asSource(half), 3, 1), 3800);

    Vector<uint8_t> wrongStorage;
    auto wrong = allocatePlane(wrongStorage, 3, 2, tenBitMSB);
    EXPECT_FALSE(downsample(asSource(source), wrong));

    Vector<uint8_t> eightBitStorage;
    auto eightBitSource = allocatePlane(eightBitStorage, 4, 2, eightBit, 1, 1, 3);
    for (size_t y = 0; y < 2; ++y) {
        for (size_t x = 0; x < 4; ++x)
            setSampleAt(eightBitSource, x, y, 100 * y + 20 * x);
    }
    Vector<uint8_t> eightBitQuarterStorage;
    auto eightBitQuarter = allocatePlane(eightBitQuarterStorage, 2, 1, eightBit);
    ASSERT_TRUE(downsample(asSource(eightBitSource), eightBitQuarter));
    EXPECT_EQ(sampleAt(asSource(eightBitQuarter), 0, 0), 60);
    EXPECT_EQ(sampleAt(asSource(eightBitQuarter), 1, 0), 100);
}

#if USE(ACCELERATE)

TEST(PlanarImageConverter, FullToVideoRange)
{
    Vector<uint8_t> sourceStorage;
    Vector<uint8_t> destinationStorage;
    auto source = allocatePlane(sourceStorage, 3, 1, eightBit);
    auto destination = allocatePlane(destinationStorage, 3, 1, tenBitMSB);
    setSampleAt(source, 0, 0, 0);
    setSampleAt(source, 1, 0, 128);
    setSampleAt(source, 2, 0, 255);

    ASSERT_TRUE(copyPlane(asSource(source), destination, RangeConversion::FullToVideo, Component::Luma));
    EXPECT_EQ(sampleAt(asSource(destination), 0, 0), 64 << 6);
    EXPECT_EQ(sampleAt(asSource(destination), 1, 0), 504 << 6);
    EXPECT_EQ(sampleAt(asSource(destination), 2, 0), 940 << 6);

    ASSERT_TRUE(copyPlane(asSource(source), destination, RangeConversion::FullToVideo, Component::Chroma));
    EXPECT_EQ(sampleAt(asSource(destination), 0, 0), 62 << 6);
    EXPECT_EQ(sampleAt(asSource(destination), 1, 0), 512 << 6);
    EXPECT_EQ(sampleAt(asSource(destination), 2, 0), 958 << 6);

    Vector<uint8_t> tenBitStorage;
    auto tenBitSource = allocatePlane(tenBitStorage, 3, 1, tenBitMSB);
    setSampleAt(tenBitSource, 0, 0, 0);
    setSampleAt(tenBitSource, 1, 0, 512 << 6);
    setSampleAt(tenBitSource, 2, 0, 1023 << 6);
    ASSERT_TRUE(copyPlane(asSource(tenBitSource), destination, RangeConversion::FullToVideo, Component::Luma));
    EXPECT_EQ(sampleAt(asSource(destination), 0, 0), 64 << 6);
    EXPECT_EQ(sampleAt(asSource(destination), 2, 0), 940 << 6);
}

// Every 8-bit full range level survives widening to 10-bit video range and narrowing back.
TEST(PlanarImageConverter, FullRangeRoundTripIsExact)
{
    Vector<uint8_t> levelsStorage;
    Vector<uint8_t> widenedStorage;
    Vector<uint8_t> narrowedStorage;
    auto levels = allocatePlane(levelsStorage, 256, 1, eightBit);
    auto widened = allocatePlane(widenedStorage, 256, 1, tenBitMSB);
    auto narrowed = allocatePlane(narrowedStorage, 256, 1, eightBit);
    for (size_t x = 0; x < 256; ++x)
        setSampleAt(levels, x, 0, x);

    for (auto component : { Component::Luma, Component::Chroma }) {
        ASSERT_TRUE(copyPlane(asSource(levels), widened, RangeConversion::FullToVideo, component));
        ASSERT_TRUE(copyPlane(asSource(widened), narrowed, RangeConversion::VideoToFull, component));
        for (size_t x = 0; x < 256; ++x)
            EXPECT_EQ(sampleAt(asSource(narrowed), x, 0), x);
    }

    Vector<uint8_t> uStorage;
    Vector<uint8_t> vStorage;
    Vector<uint8_t> uvStorage;
    auto u = allocatePlane(uStorage, 256, 1, eightBit);
    auto v = allocatePlane(vStorage, 256, 1, eightBit);
    auto uv = allocatePlane(uvStorage, 256, 1, tenBitMSB, 2);
    for (size_t x = 0; x < 256; ++x) {
        setSampleAt(u, x, 0, x);
        setSampleAt(v, x, 0, 255 - x);
    }
    ASSERT_TRUE(interleave(asSource(u), asSource(v), uv, RangeConversion::FullToVideo));
    EXPECT_EQ(sampleAt(asSource(uv), 128, 0, 0), 512 << 6);
    fill(u, 0);
    fill(v, 0);
    ASSERT_TRUE(deinterleave(asSource(uv), u, v, RangeConversion::VideoToFull));
    for (size_t x = 0; x < 256; ++x) {
        EXPECT_EQ(sampleAt(asSource(u), x, 0), x);
        EXPECT_EQ(sampleAt(asSource(v), x, 0), 255 - x);
    }
}

// 10-bit LSB samples convert to the same values as their MSB aligned equivalents, and samples above 1023 as 1023.
TEST(PlanarImageConverter, FullToVideoRangeFromTenBitLSB)
{
    // Every 10-bit level, once each.
    constexpr size_t width = 64;
    constexpr size_t height = 16;
    auto level = [](size_t x, size_t y) {
        return static_cast<uint16_t>(y * width + x);
    };

    Vector<uint8_t> msbStorage;
    auto msb = allocatePlane(msbStorage, width, height, tenBitMSB);
    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x)
            setSampleAt(msb, x, y, level(x, y) << 6);
    }

    for (auto component : { Component::Luma, Component::Chroma }) {
        Vector<uint8_t> fromMSBStorage;
        auto fromMSB = allocatePlane(fromMSBStorage, width, height, tenBitMSB);
        ASSERT_TRUE(copyPlane(asSource(msb), fromMSB, RangeConversion::FullToVideo, component));

        // A padded stride, then rows that aren't 2-byte aligned.
        for (auto [offset, padding] : { std::pair<size_t, size_t> { 0, 6 }, std::pair<size_t, size_t> { 1, 3 } }) {
            Vector<uint8_t> lsbStorage;
            Vector<uint8_t> fromLSBStorage;
            auto lsb = allocatePlane(lsbStorage, width, height, tenBitLSB, 1, offset, padding);
            auto fromLSB = allocatePlane(fromLSBStorage, width, height, tenBitMSB, 1, offset, padding);
            for (size_t y = 0; y < height; ++y) {
                for (size_t x = 0; x < width; ++x)
                    setSampleAt(lsb, x, y, level(x, y));
            }
            ASSERT_TRUE(copyPlane(asSource(lsb), fromLSB, RangeConversion::FullToVideo, component));
            for (size_t y = 0; y < height; ++y) {
                for (size_t x = 0; x < width; ++x)
                    EXPECT_EQ(sampleAt(asSource(fromLSB), x, y), sampleAt(asSource(fromMSB), x, y)) << "at " << x << "," << y;
            }
            EXPECT_EQ(countModifiedBytesOutsideRows(fromLSBStorage.span(), offset, asSource(fromLSB)), 0u);
        }

        Vector<uint8_t> strayStorage;
        Vector<uint8_t> clampedStorage;
        auto stray = allocatePlane(strayStorage, 3, 1, tenBitLSB);
        auto clamped = allocatePlane(clampedStorage, 3, 1, tenBitMSB);
        setSampleAt(stray, 0, 0, 1023);
        setSampleAt(stray, 1, 0, 1024);
        setSampleAt(stray, 2, 0, 0xFFFF);
        ASSERT_TRUE(copyPlane(asSource(stray), clamped, RangeConversion::FullToVideo, component));
        EXPECT_EQ(sampleAt(asSource(clamped), 0, 0), sampleAt(asSource(fromMSB), width - 1, height - 1));
        EXPECT_EQ(sampleAt(asSource(clamped), 1, 0), sampleAt(asSource(clamped), 0, 0));
        EXPECT_EQ(sampleAt(asSource(clamped), 2, 0), sampleAt(asSource(clamped), 0, 0));
    }

    if (!isBackendAvailable(Backend::LibYUV))
        return;

    Vector<uint8_t> chromaStorage;
    auto chroma = allocatePlane(chromaStorage, width, height, tenBitMSB);
    ASSERT_TRUE(copyPlane(asSource(msb), chroma, RangeConversion::FullToVideo, Component::Chroma));
    auto expectedChroma = [&](uint16_t value) {
        return sampleAt(asSource(chroma), value % width, value / width);
    };

    Vector<uint8_t> uStorage;
    Vector<uint8_t> vStorage;
    Vector<uint8_t> uvStorage;
    auto u = allocatePlane(uStorage, width, height, tenBitLSB, 1, 1, 1);
    auto v = allocatePlane(vStorage, width, height, tenBitLSB);
    auto uv = allocatePlane(uvStorage, width, height, tenBitMSB, 2, 1, 3);
    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x) {
            setSampleAt(u, x, y, level(x, y));
            setSampleAt(v, x, y, 1023 - level(x, y));
        }
    }
    ASSERT_TRUE(interleave(asSource(u), asSource(v), uv, RangeConversion::FullToVideo));
    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x) {
            EXPECT_EQ(sampleAt(asSource(uv), x, y, 0), expectedChroma(level(x, y))) << "at " << x << "," << y;
            EXPECT_EQ(sampleAt(asSource(uv), x, y, 1), expectedChroma(1023 - level(x, y))) << "at " << x << "," << y;
        }
    }
    EXPECT_EQ(countModifiedBytesOutsideRows(uvStorage.span(), 1, asSource(uv)), 0u);
}

#endif // USE(ACCELERATE)

TEST(PlanarImageConverter, CopyPlaneToChannel)
{
    for (auto backend : conversionBackends()) {
        BackendScope scope { backend };
        for (uint8_t channel = 0; channel < 4; ++channel) {
            Vector<uint8_t> sourceStorage;
            Vector<uint8_t> destinationStorage;
            auto source = allocatePlane(sourceStorage, 3, 2, eightBit, 1, 1, 2);
            auto destination = allocatePlane(destinationStorage, 3, 2, eightBit, 4, 2, 3);
            for (size_t y = 0; y < 2; ++y) {
                for (size_t x = 0; x < 3; ++x) {
                    setSampleAt(source, x, y, 200 + 10 * y + x);
                    for (size_t index = 0; index < 4; ++index)
                        setSampleAt(destination, x, y, index + 1, index);
                }
            }

            bool succeeded = copyPlaneToChannel(asSource(source), destination, channel);
            // libyuv only writes the last channel.
            if (backend == Backend::LibYUV && channel != 3) {
                EXPECT_FALSE(succeeded);
                continue;
            }
            ASSERT_TRUE(succeeded) << "backend " << static_cast<int>(backend) << " channel " << static_cast<int>(channel);
            for (size_t y = 0; y < 2; ++y) {
                for (size_t x = 0; x < 3; ++x) {
                    for (size_t index = 0; index < 4; ++index)
                        EXPECT_EQ(sampleAt(asSource(destination), x, y, index), index == channel ? 200 + 10 * y + x : index + 1);
                }
            }
            EXPECT_EQ(countModifiedBytesOutsideRows(destinationStorage.span(), 2, asSource(destination)), 0u);
        }
    }

    Vector<uint8_t> sourceStorage;
    Vector<uint8_t> destinationStorage;
    auto source = allocatePlane(sourceStorage, 3, 2, eightBit);
    auto destination = allocatePlane(destinationStorage, 3, 2, eightBit, 4);
    EXPECT_FALSE(copyPlaneToChannel(asSource(source), destination, 4));
    Vector<uint8_t> threeChannelStorage;
    auto threeChannels = allocatePlane(threeChannelStorage, 3, 2, eightBit, 3);
    EXPECT_FALSE(copyPlaneToChannel(asSource(source), threeChannels, 0));
}

TEST(PlanarImageConverter, PermuteChannels)
{
    struct Case {
        std::array<uint8_t, 4> channelMap;
        std::optional<uint8_t> opaqueChannel;
    };
    static constexpr std::array cases {
        Case { { 3, 0, 1, 2 }, { } },
        Case { { 1, 2, 3, 0 }, { } },
        Case { { 1, 2, 3, 0 }, 3 },
        Case { { 0, 1, 2, 3 }, 3 },
        Case { { 0, 1, 2, 3 }, { } },
    };

    for (auto backend : { Backend::PlatformFrameworks, Backend::LibYUV, Backend::Copy }) {
        if (!isBackendAvailable(backend))
            continue;
        BackendScope scope { backend };
        for (auto& testCase : cases) {
            Vector<uint8_t> sourceStorage;
            Vector<uint8_t> destinationStorage;
            auto source = allocatePlane(sourceStorage, 3, 2, eightBit, 4, 1, 2);
            auto destination = allocatePlane(destinationStorage, 3, 2, eightBit, 4, 3, 1);
            for (size_t y = 0; y < 2; ++y) {
                for (size_t x = 0; x < 3; ++x) {
                    for (size_t channel = 0; channel < 4; ++channel)
                        setSampleAt(source, x, y, 10 * channel + 3 * y + x, channel);
                }
            }

            bool succeeded = permuteChannels(asSource(source), destination, testCase.channelMap, testCase.opaqueChannel);
            // Only the platform frameworks reorder channels; the copy backend copies them unchanged.
            bool isUnchangedCopy = testCase.channelMap == std::array<uint8_t, 4> { 0, 1, 2, 3 } && !testCase.opaqueChannel;
            if (backend == Backend::LibYUV || (backend == Backend::Copy && !isUnchangedCopy)) {
                EXPECT_FALSE(succeeded);
                continue;
            }
            ASSERT_TRUE(succeeded) << "backend " << static_cast<int>(backend);
            for (size_t y = 0; y < 2; ++y) {
                for (size_t x = 0; x < 3; ++x) {
                    for (size_t channel = 0; channel < 4; ++channel) {
                        uint16_t expected = testCase.opaqueChannel == channel ? 255 : 10 * testCase.channelMap[channel] + 3 * y + x;
                        EXPECT_EQ(sampleAt(asSource(destination), x, y, channel), expected);
                    }
                }
            }
            EXPECT_EQ(countModifiedBytesOutsideRows(destinationStorage.span(), 3, asSource(destination)), 0u);
        }
    }

    Vector<uint8_t> sourceStorage;
    Vector<uint8_t> destinationStorage;
    auto source = allocatePlane(sourceStorage, 3, 2, eightBit, 4);
    auto destination = allocatePlane(destinationStorage, 3, 2, eightBit, 4);
    EXPECT_FALSE(permuteChannels(asSource(source), destination, { 0, 1, 2, 4 }));
    EXPECT_FALSE(permuteChannels(asSource(source), destination, { 0, 1, 2, 3 }, 4));
    Vector<uint8_t> smallerStorage;
    auto smaller = allocatePlane(smallerStorage, 2, 2, eightBit, 4);
    EXPECT_FALSE(permuteChannels(asSource(source), smaller, { 0, 1, 2, 3 }));
}

TEST(PlanarImageConverter, RangeConversionsNeedPlatformFrameworks)
{
    if (!isBackendAvailable(Backend::LibYUV))
        return;

    BackendScope scope { Backend::LibYUV };
    Vector<uint8_t> sourceStorage;
    Vector<uint8_t> destinationStorage;
    auto source = allocatePlane(sourceStorage, 2, 1, eightBit);
    auto destination = allocatePlane(destinationStorage, 2, 1, tenBitMSB);
    EXPECT_FALSE(copyPlane(asSource(source), destination, RangeConversion::FullToVideo, Component::Luma));
}

} // namespace TestWebKitAPI
