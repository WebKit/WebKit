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

#import "config.h"

#if PLATFORM(COCOA)

#import <CoreVideo/CoreVideo.h>
#import <WebCore/PlanarImageConverterCV.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

namespace TestWebKitAPI {

using namespace WebCore;
using namespace WebCore::PlanarImageConverter;

static constexpr size_t testWidth = 33;
static constexpr size_t testHeight = 17;

static constexpr std::array<OSType, 20> knownPixelFormats {
    kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange,
    kCVPixelFormatType_420YpCbCr8BiPlanarFullRange,
    kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange,
    kCVPixelFormatType_420YpCbCr10BiPlanarFullRange,
    kCVPixelFormatType_422YpCbCr8BiPlanarVideoRange,
    kCVPixelFormatType_422YpCbCr8BiPlanarFullRange,
    kCVPixelFormatType_422YpCbCr10BiPlanarVideoRange,
    kCVPixelFormatType_422YpCbCr10BiPlanarFullRange,
    kCVPixelFormatType_444YpCbCr8BiPlanarVideoRange,
    kCVPixelFormatType_444YpCbCr8BiPlanarFullRange,
    kCVPixelFormatType_444YpCbCr10BiPlanarVideoRange,
    kCVPixelFormatType_444YpCbCr10BiPlanarFullRange,
    kCVPixelFormatType_420YpCbCr8VideoRange_8A_TriPlanar,
    'v2a8',
    'v4a8',
    'x0a8',
    'x2a8',
    'x4a8',
    'x2as',
    'x4as',
};

static RetainPtr<CVPixelBufferRef> createPixelBuffer(OSType format, size_t width = testWidth, size_t height = testHeight)
{
    CVPixelBufferRef pixelBuffer = nullptr;
    if (CVPixelBufferCreate(kCFAllocatorDefault, width, height, format, nullptr, &pixelBuffer) != kCVReturnSuccess)
        return nullptr;
    return adoptCF(pixelBuffer);
}

static DestinationPlane allocatePlane(Vector<uint8_t>& storage, size_t width, size_t height, SampleFormat format, uint8_t channels = 1)
{
    size_t stride = width * channels * format.sampleSize();
    storage = Vector<uint8_t>(FillWith { }, stride * height, uint8_t(0));
    return *makePlane(storage.mutableSpan(), 0, stride, width, height, format, channels);
}

static uint16_t sampleAt(const SourcePlane& plane, size_t x, size_t y, size_t channel = 0)
{
    auto row = plane.row(y);
    size_t index = (x * plane.channels + channel) * plane.format.sampleSize();
    if (plane.format.sampleSize() == 1)
        return row[index];
    return row[index] | (row[index + 1] << 8);
}

static void setSampleAt(const DestinationPlane& plane, size_t x, size_t y, uint16_t value)
{
    auto row = plane.row(y);
    size_t index = x * plane.format.sampleSize();
    row[index] = value & 0xff;
    if (plane.format.sampleSize() > 1)
        row[index + 1] = value >> 8;
}

struct FrameStorage {
    Vector<uint8_t> y;
    Vector<uint8_t> u;
    Vector<uint8_t> v;
    Vector<uint8_t> alpha;
};

static DestinationFrame allocateFrame(FrameStorage& storage, ChromaSubsampling subsampling, SampleFormat format, bool isFullRange, std::optional<SampleFormat> alphaFormat, size_t width = testWidth, size_t height = testHeight)
{
    size_t uvWidth = chromaWidth(subsampling, width);
    size_t uvHeight = chromaHeight(subsampling, height);
    std::optional<DestinationPlane> alpha;
    if (alphaFormat)
        alpha = allocatePlane(storage.alpha, width, height, *alphaFormat);
    return {
        subsampling,
        isFullRange,
        allocatePlane(storage.y, width, height, format),
        allocatePlane(storage.u, uvWidth, uvHeight, format),
        allocatePlane(storage.v, uvWidth, uvHeight, format),
        alpha,
    };
}

static SourceFrame asSource(const DestinationFrame& frame)
{
    std::optional<SourcePlane> alpha;
    if (frame.alpha)
        alpha = PlanarImageConverter::asSource(*frame.alpha);
    return { frame.subsampling, frame.isFullRange, PlanarImageConverter::asSource(frame.y), PlanarImageConverter::asSource(frame.u), PlanarImageConverter::asSource(frame.v), alpha };
}

static void fillPlane(const DestinationPlane& plane, unsigned seed)
{
    unsigned levels = 1u << plane.format.bitDepth;
    for (size_t y = 0; y < plane.height; ++y) {
        for (size_t x = 0; x < plane.width; ++x)
            setSampleAt(plane, x, y, (x * 7 + y * 13 + seed * 31) % levels);
    }
}

static void fillFrame(const DestinationFrame& frame)
{
    fillPlane(frame.y, 0);
    fillPlane(frame.u, 1);
    fillPlane(frame.v, 2);
    if (frame.alpha)
        fillPlane(*frame.alpha, 3);
}

static void expectEqualPlanes(const SourcePlane& expected, const SourcePlane& actual, const char* name)
{
    ASSERT_EQ(expected.width, actual.width) << name;
    ASSERT_EQ(expected.height, actual.height) << name;
    for (size_t y = 0; y < expected.height; ++y) {
        for (size_t x = 0; x < expected.width; ++x)
            ASSERT_EQ(sampleAt(expected, x, y), sampleAt(actual, x, y)) << name << " at " << x << "," << y;
    }
}

TEST(PlanarImageConverterCV, PixelFormatDescriptionsRoundTrip)
{
    for (auto format : knownPixelFormats) {
        auto description = pixelFormatDescription(format);
        ASSERT_TRUE(description);
        EXPECT_EQ(description->pixelFormat, format);
        EXPECT_EQ(PlanarImageConverter::pixelFormat(description->subsampling, description->color, description->isFullRange, description->alpha), format);
    }
    EXPECT_FALSE(pixelFormatDescription(kCVPixelFormatType_32BGRA));
    EXPECT_FALSE(PlanarImageConverter::pixelFormat(ChromaSubsampling::HorizontalAndVertical, tenBitMSB, false, sixteenBit));
}

TEST(PlanarImageConverterCV, PixelBufferSampleFormat)
{
    EXPECT_EQ(pixelBufferSampleFormat(eightBit), eightBit);
    EXPECT_EQ(pixelBufferSampleFormat(tenBitLSB), tenBitMSB);
    EXPECT_EQ(pixelBufferSampleFormat(twelveBitLSB), tenBitMSB);
    EXPECT_EQ(pixelBufferSampleFormat(tenBitMSB), tenBitMSB);
    EXPECT_EQ(pixelBufferSampleFormat(twelveBitMSB), tenBitMSB);
    EXPECT_FALSE(pixelBufferSampleFormat(sixteenBit));
}

TEST(PlanarImageConverterCV, PixelBufferPlanesMatchCoreVideo)
{
    for (auto format : knownPixelFormats) {
        RetainPtr pixelBuffer = createPixelBuffer(format);
        ASSERT_TRUE(pixelBuffer);
        EXPECT_TRUE(WritableYUVPixelBufferPlanes { pixelBuffer.get() });
        ReadOnlyYUVPixelBufferPlanes lockedPlanes { pixelBuffer.get() };
        ASSERT_TRUE(lockedPlanes);
        Borrow planes = lockedPlanes;
        auto subsampling = planes->description().subsampling;
        EXPECT_EQ(planes->y().width, testWidth);
        EXPECT_EQ(planes->y().height, testHeight);
        EXPECT_EQ(planes->y().format, planes->description().color);
        EXPECT_EQ(planes->uv().channels, 2);
        EXPECT_EQ(planes->uv().width, chromaWidth(subsampling, testWidth));
        EXPECT_EQ(planes->uv().height, chromaHeight(subsampling, testHeight));
        EXPECT_EQ(planes->alpha().has_value(), planes->description().alpha.has_value());
        if (planes->alpha())
            EXPECT_EQ(planes->alpha()->format, *planes->description().alpha);
    }

    RetainPtr bgra = createPixelBuffer(kCVPixelFormatType_32BGRA);
    EXPECT_FALSE(ReadOnlyYUVPixelBufferPlanes { bgra.get() });
}

struct FrameCase {
    OSType pixelFormat;
    SampleFormat format;
    bool isFullRange;
    std::optional<SampleFormat> alpha;
};

static constexpr std::array frameCases {
    FrameCase { kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange, eightBit, false, { } },
    FrameCase { kCVPixelFormatType_420YpCbCr8BiPlanarFullRange, eightBit, true, { } },
    FrameCase { kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange, tenBitLSB, false, { } },
    FrameCase { kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange, twelveBitLSB, false, { } },
    FrameCase { kCVPixelFormatType_422YpCbCr8BiPlanarVideoRange, eightBit, false, { } },
    FrameCase { kCVPixelFormatType_422YpCbCr10BiPlanarFullRange, tenBitLSB, true, { } },
    FrameCase { kCVPixelFormatType_444YpCbCr8BiPlanarFullRange, eightBit, true, { } },
    FrameCase { kCVPixelFormatType_444YpCbCr10BiPlanarVideoRange, twelveBitLSB, false, { } },
    FrameCase { kCVPixelFormatType_420YpCbCr8VideoRange_8A_TriPlanar, eightBit, false, eightBit },
    FrameCase { 'v2a8', eightBit, false, eightBit },
    FrameCase { 'v4a8', eightBit, false, eightBit },
    FrameCase { 'x2as', tenBitLSB, false, tenBitLSB },
    FrameCase { 'x4as', twelveBitLSB, false, twelveBitLSB },
    // 8-bit full range, widened to 10-bit video range.
    FrameCase { 'x0a8', eightBit, true, eightBit },
    FrameCase { 'x2a8', eightBit, true, eightBit },
    FrameCase { 'x4a8', eightBit, true, eightBit },
    FrameCase { kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange, eightBit, true, { } },
};

TEST(PlanarImageConverterCV, CopyFrameRoundTrips)
{
    for (auto& frameCase : frameCases) {
        auto description = pixelFormatDescription(frameCase.pixelFormat);
        ASSERT_TRUE(description);
        RetainPtr pixelBuffer = createPixelBuffer(frameCase.pixelFormat);
        ASSERT_TRUE(pixelBuffer);

        FrameStorage sourceStorage;
        auto source = allocateFrame(sourceStorage, description->subsampling, frameCase.format, frameCase.isFullRange, frameCase.alpha);
        fillFrame(source);
        ASSERT_TRUE(copyFrame(asSource(source), pixelBuffer.get())) << "pixel format " << frameCase.pixelFormat;

        FrameStorage resultStorage;
        auto result = allocateFrame(resultStorage, description->subsampling, frameCase.format, frameCase.isFullRange, frameCase.alpha);
        ASSERT_TRUE(copyFrame(pixelBuffer.get(), 0, 0, result)) << "pixel format " << frameCase.pixelFormat;

        expectEqualPlanes(PlanarImageConverter::asSource(source.y), PlanarImageConverter::asSource(result.y), "y");
        expectEqualPlanes(PlanarImageConverter::asSource(source.u), PlanarImageConverter::asSource(result.u), "u");
        expectEqualPlanes(PlanarImageConverter::asSource(source.v), PlanarImageConverter::asSource(result.v), "v");
        if (source.alpha)
            expectEqualPlanes(PlanarImageConverter::asSource(*source.alpha), PlanarImageConverter::asSource(*result.alpha), "alpha");
    }
}

TEST(PlanarImageConverterCV, CopyFrameStoresMSBAlignedSamples)
{
    RetainPtr pixelBuffer = createPixelBuffer('x2as');
    FrameStorage storage;
    auto frame = allocateFrame(storage, ChromaSubsampling::Horizontal, twelveBitLSB, false, twelveBitLSB);
    fillFrame(frame);
    setSampleAt(frame.y, 1, 0, 4095);
    setSampleAt(frame.u, 0, 0, 2048);
    setSampleAt(*frame.alpha, 0, 0, 1);
    ASSERT_TRUE(copyFrame(asSource(frame), pixelBuffer.get()));

    ReadOnlyYUVPixelBufferPlanes lockedPlanes { pixelBuffer.get() };
    ASSERT_TRUE(lockedPlanes);
    Borrow planes = lockedPlanes;
    EXPECT_EQ(sampleAt(planes->y(), 1, 0), 0xFFF0);
    EXPECT_EQ(sampleAt(planes->uv(), 0, 0, 0), 0x8000);
    EXPECT_EQ(sampleAt(*planes->alpha(), 0, 0), 0x0010);
}

TEST(PlanarImageConverterCV, CopyFrameWidensFullRange)
{
    RetainPtr pixelBuffer = createPixelBuffer('x0a8');
    FrameStorage storage;
    auto frame = allocateFrame(storage, ChromaSubsampling::HorizontalAndVertical, eightBit, true, eightBit);
    fillFrame(frame);
    setSampleAt(frame.y, 0, 0, 0);
    setSampleAt(frame.y, 1, 0, 255);
    setSampleAt(frame.u, 0, 0, 128);
    setSampleAt(*frame.alpha, 0, 0, 255);
    ASSERT_TRUE(copyFrame(asSource(frame), pixelBuffer.get()));

    ReadOnlyYUVPixelBufferPlanes lockedPlanes { pixelBuffer.get() };
    ASSERT_TRUE(lockedPlanes);
    Borrow planes = lockedPlanes;
    EXPECT_EQ(sampleAt(planes->y(), 0, 0), 64 << 6);
    EXPECT_EQ(sampleAt(planes->y(), 1, 0), 940 << 6);
    EXPECT_EQ(sampleAt(planes->uv(), 0, 0, 0), 512 << 6);
    EXPECT_EQ(sampleAt(*planes->alpha(), 0, 0), 255);
}

TEST(PlanarImageConverterCV, CopyFrameRect)
{
    RetainPtr pixelBuffer = createPixelBuffer(kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange);
    FrameStorage sourceStorage;
    auto source = allocateFrame(sourceStorage, ChromaSubsampling::HorizontalAndVertical, tenBitLSB, false, { });
    fillFrame(source);
    ASSERT_TRUE(copyFrame(asSource(source), pixelBuffer.get()));

    FrameStorage rectStorage;
    auto rect = allocateFrame(rectStorage, ChromaSubsampling::HorizontalAndVertical, tenBitLSB, false, { }, 9, 5);
    ASSERT_TRUE(copyFrame(pixelBuffer.get(), 4, 2, rect));
    for (size_t y = 0; y < 5; ++y) {
        for (size_t x = 0; x < 9; ++x)
            EXPECT_EQ(sampleAt(PlanarImageConverter::asSource(rect.y), x, y), sampleAt(PlanarImageConverter::asSource(source.y), x + 4, y + 2));
    }
    for (size_t y = 0; y < 3; ++y) {
        for (size_t x = 0; x < 5; ++x) {
            EXPECT_EQ(sampleAt(PlanarImageConverter::asSource(rect.u), x, y), sampleAt(PlanarImageConverter::asSource(source.u), x + 2, y + 1));
            EXPECT_EQ(sampleAt(PlanarImageConverter::asSource(rect.v), x, y), sampleAt(PlanarImageConverter::asSource(source.v), x + 2, y + 1));
        }
    }

    // The origin of a 4:2:0 rect is on even coordinates, and the rect stays inside the pixel buffer.
    EXPECT_FALSE(copyFrame(pixelBuffer.get(), 3, 2, rect));
    EXPECT_FALSE(copyFrame(pixelBuffer.get(), 4, 1, rect));
    EXPECT_FALSE(copyFrame(pixelBuffer.get(), 26, 2, rect));
}

TEST(PlanarImageConverterCV, CopyFrameDownsamplesChroma)
{
    if (!isBackendAvailable(Backend::LibYUV))
        return;

    constexpr size_t width = 8;
    constexpr size_t height = 4;
    RetainPtr pixelBuffer = createPixelBuffer(kCVPixelFormatType_422YpCbCr10BiPlanarVideoRange, width, height);
    FrameStorage sourceStorage;
    auto source = allocateFrame(sourceStorage, ChromaSubsampling::Horizontal, tenBitLSB, false, { }, width, height);
    fillPlane(source.y, 0);
    for (size_t y = 0; y < source.u.height; ++y) {
        for (size_t x = 0; x < source.u.width; ++x) {
            setSampleAt(source.u, x, y, 100 + 20 * y + 4 * x);
            setSampleAt(source.v, x, y, 600 - 20 * y - 4 * x);
        }
    }
    ASSERT_TRUE(copyFrame(asSource(source), pixelBuffer.get()));

    FrameStorage resultStorage;
    auto result = allocateFrame(resultStorage, ChromaSubsampling::HorizontalAndVertical, tenBitLSB, false, { }, width, height);
    ASSERT_TRUE(copyFrame(pixelBuffer.get(), 0, 0, result));
    expectEqualPlanes(PlanarImageConverter::asSource(source.y), PlanarImageConverter::asSource(result.y), "y");
    for (size_t y = 0; y < result.u.height; ++y) {
        for (size_t x = 0; x < result.u.width; ++x) {
            EXPECT_EQ(sampleAt(PlanarImageConverter::asSource(result.u), x, y), 110 + 40 * y + 4 * x);
            EXPECT_EQ(sampleAt(PlanarImageConverter::asSource(result.v), x, y), 590 - 40 * y - 4 * x);
        }
    }

    // Chroma is never upsampled.
    RetainPtr pixelBuffer420 = createPixelBuffer(kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange, width, height);
    EXPECT_FALSE(copyFrame(pixelBuffer420.get(), 0, 0, source));
}

TEST(PlanarImageConverterCV, CopyFrameReadsAlphaOverride)
{
    RetainPtr pixelBuffer = createPixelBuffer('x0a8');
    FrameStorage withAlphaStorage;
    auto withAlpha = allocateFrame(withAlphaStorage, ChromaSubsampling::HorizontalAndVertical, tenBitLSB, false, eightBit);
    fillFrame(withAlpha);
    ASSERT_TRUE(copyFrame(asSource(withAlpha), pixelBuffer.get()));

    Vector<uint8_t> overrideStorage;
    auto alphaOverride = allocatePlane(overrideStorage, testWidth, testHeight, tenBitLSB);
    fillPlane(alphaOverride, 9);

    FrameStorage resultStorage;
    auto result = allocateFrame(resultStorage, ChromaSubsampling::HorizontalAndVertical, tenBitLSB, false, tenBitLSB);
    ASSERT_TRUE(copyFrame(pixelBuffer.get(), 0, 0, result, PlanarImageConverter::asSource(alphaOverride)));
    expectEqualPlanes(PlanarImageConverter::asSource(alphaOverride), PlanarImageConverter::asSource(*result.alpha), "alpha");

    Vector<uint8_t> smallStorage;
    auto small = allocatePlane(smallStorage, testWidth - 1, testHeight, tenBitLSB);
    EXPECT_FALSE(copyFrame(pixelBuffer.get(), 0, 0, result, PlanarImageConverter::asSource(small)));
}

TEST(PlanarImageConverterCV, CopyColorPlanes)
{
    RetainPtr fullRange = createPixelBuffer(kCVPixelFormatType_420YpCbCr8BiPlanarFullRange);
    FrameStorage sourceStorage;
    auto source = allocateFrame(sourceStorage, ChromaSubsampling::HorizontalAndVertical, eightBit, true, { });
    fillFrame(source);
    ASSERT_TRUE(copyFrame(asSource(source), fullRange.get()));

    RetainPtr withAlpha = createPixelBuffer('x0a8');
    ASSERT_TRUE(copyColorPlanes(fullRange.get(), withAlpha.get()));
    {
        ReadOnlyYUVPixelBufferPlanes lockedPlanes { withAlpha.get() };
        ASSERT_TRUE(lockedPlanes);
        EXPECT_EQ(sampleAt(borrow(lockedPlanes)->y(), 0, 0), 64 << 6);
    }

    RetainPtr colorOnly = createPixelBuffer(kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange);
    ASSERT_TRUE(copyColorPlanes(withAlpha.get(), colorOnly.get()));

    FrameStorage resultStorage;
    auto result = allocateFrame(resultStorage, ChromaSubsampling::HorizontalAndVertical, eightBit, true, { });
    ASSERT_TRUE(copyFrame(colorOnly.get(), 0, 0, result));
    expectEqualPlanes(PlanarImageConverter::asSource(source.y), PlanarImageConverter::asSource(result.y), "y");
    expectEqualPlanes(PlanarImageConverter::asSource(source.u), PlanarImageConverter::asSource(result.u), "u");
    expectEqualPlanes(PlanarImageConverter::asSource(source.v), PlanarImageConverter::asSource(result.v), "v");

    RetainPtr subsampled422 = createPixelBuffer(kCVPixelFormatType_422YpCbCr8BiPlanarVideoRange);
    EXPECT_FALSE(copyColorPlanes(fullRange.get(), subsampled422.get()));
}

TEST(PlanarImageConverterCV, CopyLumaToAlpha)
{
    RetainPtr alphaSource = createPixelBuffer(kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange);
    FrameStorage sourceStorage;
    auto source = allocateFrame(sourceStorage, ChromaSubsampling::HorizontalAndVertical, tenBitLSB, false, { });
    fillFrame(source);
    ASSERT_TRUE(copyFrame(asSource(source), alphaSource.get()));

    RetainPtr destination = createPixelBuffer(kCVPixelFormatType_420YpCbCr8VideoRange_8A_TriPlanar);
    ASSERT_TRUE(copyLumaToAlphaPlane(alphaSource.get(), destination.get()));
    {
        ReadOnlyYUVPixelBufferPlanes lockedPlanes { destination.get() };
        ASSERT_TRUE(lockedPlanes);
        Borrow planes = lockedPlanes;
        for (size_t y = 0; y < testHeight; ++y) {
            for (size_t x = 0; x < testWidth; ++x)
                EXPECT_EQ(sampleAt(*planes->alpha(), x, y), sampleAt(PlanarImageConverter::asSource(source.y), x, y) >> 2);
        }
    }

    Vector<uint8_t> alphaStorage;
    auto alpha = allocatePlane(alphaStorage, testWidth, testHeight, eightBit);
    ASSERT_TRUE(copyLumaToAlpha(alphaSource.get(), alpha));
    EXPECT_EQ(sampleAt(PlanarImageConverter::asSource(alpha), 5, 3), sampleAt(PlanarImageConverter::asSource(source.y), 5, 3) >> 2);

    RetainPtr opaque = createPixelBuffer(kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange);
    EXPECT_FALSE(copyLumaToAlphaPlane(alphaSource.get(), opaque.get()));
}

TEST(PlanarImageConverterCV, CopyAlphaPlaneToBGRA)
{
    RetainPtr source = createPixelBuffer(kCVPixelFormatType_420YpCbCr8VideoRange_8A_TriPlanar);
    FrameStorage sourceStorage;
    auto frame = allocateFrame(sourceStorage, ChromaSubsampling::HorizontalAndVertical, eightBit, false, eightBit);
    fillFrame(frame);
    ASSERT_TRUE(copyFrame(asSource(frame), source.get()));

    RetainPtr bgra = createPixelBuffer(kCVPixelFormatType_32BGRA);
    size_t bytesPerRow = CVPixelBufferGetBytesPerRow(bgra.get());
    {
        PixelBufferLock lock { bgra.get(), 0 };
        auto bytes = unsafeMakeSpan(static_cast<uint8_t*>(CVPixelBufferGetBaseAddress(bgra.get())), bytesPerRow * testHeight);
        auto pixels = *makePlane(bytes, 0, bytesPerRow, testWidth, testHeight, eightBit, 4);
        for (size_t y = 0; y < testHeight; ++y) {
            auto row = pixels.row(y);
            for (size_t x = 0; x < testWidth; ++x) {
                row[4 * x] = 1;
                row[4 * x + 1] = 2;
                row[4 * x + 2] = 3;
                row[4 * x + 3] = 4;
            }
        }
    }

    ASSERT_TRUE(copyAlphaPlaneToBGRA(source.get(), bgra.get()));

    PixelBufferLock lock { bgra.get(), kCVPixelBufferLock_ReadOnly };
    auto bytes = unsafeMakeSpan(static_cast<const uint8_t*>(CVPixelBufferGetBaseAddress(bgra.get())), bytesPerRow * testHeight);
    auto pixels = *makePlane(bytes, 0, bytesPerRow, testWidth, testHeight, eightBit, 4);
    for (size_t y = 0; y < testHeight; ++y) {
        for (size_t x = 0; x < testWidth; ++x) {
            EXPECT_EQ(sampleAt(pixels, x, y, 0), 1);
            EXPECT_EQ(sampleAt(pixels, x, y, 1), 2);
            EXPECT_EQ(sampleAt(pixels, x, y, 2), 3);
            EXPECT_EQ(sampleAt(pixels, x, y, 3), sampleAt(PlanarImageConverter::asSource(*frame.alpha), x, y));
        }
    }

    RetainPtr opaque = createPixelBuffer(kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange);
    EXPECT_FALSE(copyAlphaPlaneToBGRA(opaque.get(), bgra.get()));
}

TEST(PlanarImageConverterCV, CopyBiPlanarFrame)
{
    constexpr auto subsampling = ChromaSubsampling::HorizontalAndVertical;
    size_t uvWidth = chromaWidth(subsampling, testWidth);
    size_t uvHeight = chromaHeight(subsampling, testHeight);
    RetainPtr pixelBuffer = createPixelBuffer(kCVPixelFormatType_420YpCbCr8BiPlanarFullRange);

    Vector<uint8_t> yStorage;
    Vector<uint8_t> uvStorage;
    DestinationBiPlanarFrame source { subsampling, true, allocatePlane(yStorage, testWidth, testHeight, eightBit), allocatePlane(uvStorage, uvWidth, uvHeight, eightBit, 2) };
    fillPlane(source.y, 0);
    for (size_t y = 0; y < uvHeight; ++y) {
        auto row = source.uv.row(y);
        for (size_t index = 0; index < row.size(); ++index)
            row[index] = (index * 5 + y * 11) % 256;
    }
    ASSERT_TRUE(copyFrame(SourceBiPlanarFrame { subsampling, true, PlanarImageConverter::asSource(source.y), PlanarImageConverter::asSource(source.uv) }, pixelBuffer.get()));

    Vector<uint8_t> rectYStorage;
    Vector<uint8_t> rectUVStorage;
    DestinationBiPlanarFrame rect { subsampling, true, allocatePlane(rectYStorage, 9, 5, eightBit), allocatePlane(rectUVStorage, 5, 3, eightBit, 2) };
    ASSERT_TRUE(copyFrame(pixelBuffer.get(), 4, 2, rect));
    for (size_t y = 0; y < 5; ++y) {
        for (size_t x = 0; x < 9; ++x)
            EXPECT_EQ(sampleAt(PlanarImageConverter::asSource(rect.y), x, y), sampleAt(PlanarImageConverter::asSource(source.y), x + 4, y + 2));
    }
    for (size_t y = 0; y < 3; ++y) {
        for (size_t x = 0; x < 5; ++x) {
            for (size_t channel = 0; channel < 2; ++channel)
                EXPECT_EQ(sampleAt(PlanarImageConverter::asSource(rect.uv), x, y, channel), sampleAt(PlanarImageConverter::asSource(source.uv), x + 2, y + 1, channel));
        }
    }

    EXPECT_FALSE(copyFrame(pixelBuffer.get(), 3, 2, rect));
    RetainPtr subsampled422 = createPixelBuffer(kCVPixelFormatType_422YpCbCr8BiPlanarFullRange);
    EXPECT_FALSE(copyFrame(subsampled422.get(), 0, 0, rect));
}

static constexpr std::array<uint8_t, 4> rgbaToARGB { 3, 0, 1, 2 };
static constexpr std::array<uint8_t, 4> argbToRGBA { 1, 2, 3, 0 };

TEST(PlanarImageConverterCV, CopyPackedPixelBuffer)
{
    Vector<uint8_t> rgbaStorage;
    auto rgba = allocatePlane(rgbaStorage, testWidth, testHeight, eightBit, 4);
    for (size_t y = 0; y < testHeight; ++y) {
        auto row = rgba.row(y);
        for (size_t index = 0; index < row.size(); ++index)
            row[index] = (index * 3 + y * 7) % 256;
    }

    RetainPtr argb = createPixelBuffer(kCVPixelFormatType_32ARGB);
    ASSERT_TRUE(copyToPixelBuffer(PlanarImageConverter::asSource(rgba), argb.get(), rgbaToARGB));
    {
        PixelBufferLock lock { argb.get(), kCVPixelBufferLock_ReadOnly };
        size_t bytesPerRow = CVPixelBufferGetBytesPerRow(argb.get());
        auto bytes = unsafeMakeSpan(static_cast<const uint8_t*>(CVPixelBufferGetBaseAddress(argb.get())), bytesPerRow * testHeight);
        auto pixels = *makePlane(bytes, 0, bytesPerRow, testWidth, testHeight, eightBit, 4);
        for (size_t channel = 0; channel < 4; ++channel)
            EXPECT_EQ(sampleAt(pixels, 5, 3, channel), sampleAt(PlanarImageConverter::asSource(rgba), 5, 3, rgbaToARGB[channel]));
    }

    Vector<uint8_t> rectStorage;
    auto rect = allocatePlane(rectStorage, 9, 5, eightBit, 4);
    ASSERT_TRUE(copyFromPixelBuffer(argb.get(), 3, 2, rect, argbToRGBA));
    for (size_t y = 0; y < 5; ++y) {
        for (size_t x = 0; x < 9; ++x) {
            for (size_t channel = 0; channel < 4; ++channel)
                EXPECT_EQ(sampleAt(PlanarImageConverter::asSource(rect), x, y, channel), sampleAt(PlanarImageConverter::asSource(rgba), x + 3, y + 2, channel));
        }
    }

    constexpr uint8_t alphaChannel = 3;
    ASSERT_TRUE(copyFromPixelBuffer(argb.get(), 3, 2, rect, argbToRGBA, alphaChannel));
    EXPECT_EQ(sampleAt(PlanarImageConverter::asSource(rect), 4, 1, alphaChannel), 255);
    EXPECT_EQ(sampleAt(PlanarImageConverter::asSource(rect), 4, 1, 0), sampleAt(PlanarImageConverter::asSource(rgba), 7, 3, 0));

    EXPECT_FALSE(copyFromPixelBuffer(argb.get(), 25, 2, rect, argbToRGBA));
    RetainPtr biPlanar = createPixelBuffer(kCVPixelFormatType_420YpCbCr8BiPlanarFullRange);
    EXPECT_FALSE(copyToPixelBuffer(PlanarImageConverter::asSource(rgba), biPlanar.get(), rgbaToARGB));
}

} // namespace TestWebKitAPI

#endif // PLATFORM(COCOA)
