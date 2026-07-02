/*
 * Copyright (C) 2022-2026 Apple Inc. All rights reserved.
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

#if USE(AVFOUNDATION)

#import <CoreMedia/CMFormatDescription.h>
#import <JavaScriptCore/JavaScriptCore.h>
#import <WebCore/CMUtilities.h>
#import <WebCore/FormatDescriptionUtilities.h>
#import <WebCore/PlatformVideoColorPrimaries.h>
#import <WebCore/PlatformVideoColorSpace.h>
#import <WebCore/PlatformVideoMatrixCoefficients.h>
#import <WebCore/PlatformVideoTransferCharacteristics.h>
#import <WebCore/TrackInfo.h>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <wtf/RetainPtr.h>

#import <pal/cf/CoreMediaSoftLink.h>

namespace TestWebKitAPI {

TEST(PAL, MediaTime)
{
    EXPECT_EQ(PAL::toMediaTime(PAL::toCMTime(MediaTime::invalidTime())), MediaTime::invalidTime());
}

#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)

static RetainPtr<CMFormatDescriptionRef> makeFormatDescriptionWithTencData(std::span<const uint8_t> tencData)
{
    RetainPtr data = adoptCF(CFDataCreate(kCFAllocatorDefault, tencData.data(), tencData.size()));
    RetainPtr extensions = adoptNS([[NSDictionary alloc] initWithObjectsAndKeys:(__bridge NSData *)data.get(), @"CommonEncryptionTrackEncryptionBox", nil]);
    CMFormatDescriptionRef desc = nullptr;
    PAL::CMVideoFormatDescriptionCreate(kCFAllocatorDefault, kCMVideoCodecType_H264, 1920, 1080, (__bridge CFDictionaryRef)extensions.get(), &desc);
    return adoptCF(desc);
}

TEST(CMUtilities, GetKeyIDsEmptyTencData)
{
    auto desc = makeFormatDescriptionWithTencData({ });
    ASSERT_TRUE(desc);
    EXPECT_TRUE(WebCore::getKeyIDs(desc.get()).isEmpty());
}

TEST(CMUtilities, GetKeyIDsTruncatedTencData)
{
    // 3 bytes — not enough to read the 4-byte version+flags field.
    constexpr uint8_t truncated[] = { 0x00, 0x00, 0x00 };
    auto desc = makeFormatDescriptionWithTencData(truncated);
    ASSERT_TRUE(desc);
    EXPECT_TRUE(WebCore::getKeyIDs(desc.get()).isEmpty());
}

TEST(CMUtilities, GetKeyIDsValidTencData)
{
    // Minimal v0 tenc box payload (no size/type prefix):
    // version(1) + flags(3) + reserved(1) + reserved(1) + defaultIsProtected(1)
    // + defaultPerSampleIVSize(1) + KID(16).
    constexpr uint8_t validTenc[] = {
        0x00, // version = 0
        0x00, 0x00, 0x00, // flags
        0x00, // reserved
        0x00, // reserved (v0)
        0x01, // defaultIsProtected = 1
        0x08, // defaultPerSampleIVSize = 8
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, // defaultKID (16 bytes)
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
    };
    auto desc = makeFormatDescriptionWithTencData(validTenc);
    ASSERT_TRUE(desc);
    auto keyIDs = WebCore::getKeyIDs(desc.get());
    ASSERT_EQ(keyIDs.size(), 1u);
    ASSERT_EQ(keyIDs[0]->size(), 16u);
    auto span = keyIDs[0]->span();
    for (size_t i = 0; i < 16; ++i)
        EXPECT_EQ(span[i], uint8_t(i));
}

TEST(CMUtilities, GetKeyIDsWithActiveGigacage)
{
    // Creating a JSContext initialises a JSC VM, which calls Gigacage::ensureGigacage()
    // and sets g_gigacageConfig.isEnabled = true — the same state as WebContent.
    @autoreleasepool {
        JSContext * jsContext = [[JSContext alloc] init];
        (void)jsContext;

        constexpr uint8_t validTenc[] = {
            0x00, // version = 0
            0x00, 0x00, 0x00, // flags
            0x00, // reserved
            0x00, // reserved (v0)
            0x01, // defaultIsProtected = 1
            0x08, // defaultPerSampleIVSize = 8
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, // defaultKID (16 bytes)
            0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
        };
        auto desc = makeFormatDescriptionWithTencData(validTenc);
        ASSERT_TRUE(desc);
        auto keyIDs = WebCore::getKeyIDs(desc.get());
        ASSERT_EQ(keyIDs.size(), 1u);
        ASSERT_EQ(keyIDs[0]->size(), 16u);
        auto span = keyIDs[0]->span();
        for (size_t i = 0; i < 16; ++i)
            EXPECT_EQ(span[i], uint8_t(i));
    }
}

#endif // ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)

// Creates a CMFormatDescriptionRef embedding the given color space via createFormatDescriptionFromTrackInfo.
// Uses H.264 as a nominal codec; no decoder configuration atoms are provided since the test only
// exercises the color space extensions, not decoding.
static RetainPtr<CMFormatDescriptionRef> makeColorSpaceFormatDescription(const WebCore::PlatformVideoColorSpace& colorSpace)
{
    auto videoInfo = WebCore::VideoInfo::create({
        { .codecName = WebCore::FourCC('avc1') }, {
            .size = { 640, 480 },
            .displaySize = { 640, 480 },
            .colorSpace = colorSpace,
        }
    });
    return WebCore::createFormatDescriptionFromTrackInfo(videoInfo);
}

// Returns a baseline color space whose three components all have known CM mappings,
// used to anchor the two dimensions not under test so VTGetDefaultColorAttributesWithHints
// is not invoked during colorSpaceFromFormatDescription.
static WebCore::PlatformVideoColorSpace baselineColorSpace()
{
    return {
        .primaries = WebCore::PlatformVideoColorPrimaries::Bt709,
        .transfer  = WebCore::PlatformVideoTransferCharacteristics::Bt709,
        .matrix    = WebCore::PlatformVideoMatrixCoefficients::Bt709,
    };
}

// ---- Color Primaries ----

TEST(CMUtilities, ColorPrimariesRoundTrip)
{
    using P = WebCore::PlatformVideoColorPrimaries;

    // The exhaustive switch (no default) ensures the compiler warns when new
    // PlatformVideoColorPrimaries values are added without updating this test.
    // std::nullopt means the value has no CoreMedia mapping and is not tested.
    auto testPrimary = [&](P input) {
        std::optional<P> expected;
        switch (input) {
        case P::Bt709:             expected = P::Bt709;             break;
        case P::Bt470bg:           /* no CM mapping */              break;
        case P::Bt470m:            /* no CM mapping */              break;
        case P::Smpte170m:         expected = P::Smpte170m;         break;
        case P::Smpte240m:         expected = P::Smpte170m;         break; // shares kCVImageBufferColorPrimaries_SMPTE_C with Smpte170m
        case P::Film:              /* no CM mapping */              break;
        case P::Bt2020:            expected = P::Bt2020;            break;
        case P::SmpteSt4281:       /* no CM mapping */              break;
        case P::SmpteRp431:        expected = P::SmpteRp431;        break;
        case P::SmpteEg432:        expected = P::SmpteEg432;        break;
        case P::JedecP22Phosphors: expected = P::JedecP22Phosphors; break;
        case P::Unspecified:       /* no CM mapping */              break;
        }
        if (!expected)
            return;
        auto cs = baselineColorSpace();
        cs.primaries = input;
        auto desc = makeColorSpaceFormatDescription(cs);
        ASSERT_TRUE(desc) << "createFormatDescriptionFromTrackInfo returned null for primaries " << (int)input;
        auto result = WebCore::colorSpaceFromFormatDescription(desc.get());
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->primaries, expected);
    };

    // Aliases (e.g. Smpte432 = SmpteEg432) share the same underlying value and are omitted.
    testPrimary(P::Bt709);
    testPrimary(P::Bt470bg);
    testPrimary(P::Bt470m);
    testPrimary(P::Smpte170m);
    testPrimary(P::Smpte240m);
    testPrimary(P::Film);
    testPrimary(P::Bt2020);
    testPrimary(P::SmpteSt4281);
    testPrimary(P::SmpteRp431);
    testPrimary(P::SmpteEg432);
    testPrimary(P::JedecP22Phosphors);
    testPrimary(P::Unspecified);
}

// ---- Transfer Characteristics ----

TEST(CMUtilities, TransferCharacteristicsRoundTrip)
{
    using T = WebCore::PlatformVideoTransferCharacteristics;

    auto testTransfer = [&](T input) {
        std::optional<T> expected;
        switch (input) {
        case T::Bt709:         expected = T::Bt709;         break;
        case T::Smpte170m:     expected = T::Bt709;         break; // shares kCVImageBufferTransferFunction_ITU_R_709_2 with Bt709
        case T::Iec6196621:    expected = T::Iec6196621;    break;
        case T::Gamma22curve:  expected = T::Gamma22curve;  break;
        case T::Gamma28curve:  expected = T::Gamma28curve;  break;
        case T::Smpte240m:     expected = T::Smpte240m;     break;
        case T::Linear:        expected = T::Linear;        break;
        case T::Log:           /* no CM mapping */          break;
        case T::LogSqrt:       /* no CM mapping */          break;
        case T::Iec6196624:    /* no CM mapping */          break;
        case T::Bt1361ExtendedColourGamut: /* no CM mapping */ break;
        case T::Bt2020_10bit:  expected = T::Bt2020_10bit;  break;
        case T::Bt2020_12bit:  expected = T::Bt2020_10bit;  break; // shares kCMFormatDescriptionTransferFunction_ITU_R_2020 with Bt2020_10bit
        case T::SmpteSt2084:   expected = T::SmpteSt2084;   break;
        case T::SmpteSt4281:   expected = T::SmpteSt4281;   break;
        case T::AribStdB67Hlg: expected = T::AribStdB67Hlg; break;
        case T::Unspecified:   /* no CM mapping */          break;
        }
        if (!expected)
            return;
        auto cs = baselineColorSpace();
        cs.transfer = input;
        auto desc = makeColorSpaceFormatDescription(cs);
        ASSERT_TRUE(desc) << "createFormatDescriptionFromTrackInfo returned null for transfer " << (int)input;
        auto result = WebCore::colorSpaceFromFormatDescription(desc.get());
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->transfer, expected);
    };

    // Aliases (e.g. PQ = SmpteSt2084, HLG = AribStdB67Hlg) share the same underlying value and are omitted.
    testTransfer(T::Bt709);
    testTransfer(T::Smpte170m);
    testTransfer(T::Iec6196621);
    testTransfer(T::Gamma22curve);
    testTransfer(T::Gamma28curve);
    testTransfer(T::Smpte240m);
    testTransfer(T::Linear);
    testTransfer(T::Log);
    testTransfer(T::LogSqrt);
    testTransfer(T::Iec6196624);
    testTransfer(T::Bt1361ExtendedColourGamut);
    testTransfer(T::Bt2020_10bit);
    testTransfer(T::Bt2020_12bit);
    testTransfer(T::SmpteSt2084);
    testTransfer(T::SmpteSt4281);
    testTransfer(T::AribStdB67Hlg);
    testTransfer(T::Unspecified);
}

// ---- Matrix Coefficients ----

TEST(CMUtilities, MatrixCoefficientsRoundTrip)
{
    using M = WebCore::PlatformVideoMatrixCoefficients;

    auto testMatrix = [&](M input) {
        std::optional<M> expected;
        switch (input) {
        case M::Rgb:                         /* no CM mapping */             break;
        case M::Bt709:                       expected = M::Bt709;            break;
        case M::Bt470bg:                     expected = M::Bt470bg;          break;
        case M::Smpte170m:                   expected = M::Bt470bg;          break; // shares kCVImageBufferYCbCrMatrix_ITU_R_601_4 with Bt470bg
        case M::Smpte240m:                   expected = M::Smpte240m;        break;
        case M::Fcc:                         /* no CM mapping */             break;
        case M::YCgCo:                       /* no CM mapping */             break;
        case M::Bt2020NonconstantLuminance:  expected = M::Bt2020NonconstantLuminance; break;
        case M::Bt2020ConstantLuminance:     /* no CM mapping */             break;
        case M::Unspecified:                 /* no CM mapping */             break;
        }
        if (!expected)
            return;
        auto cs = baselineColorSpace();
        cs.matrix = input;
        auto desc = makeColorSpaceFormatDescription(cs);
        ASSERT_TRUE(desc) << "createFormatDescriptionFromTrackInfo returned null for matrix " << (int)input;
        auto result = WebCore::colorSpaceFromFormatDescription(desc.get());
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->matrix, expected);
    };

    // Aliases (e.g. Bt2020Ncl = Bt2020NonconstantLuminance) share the same underlying value and are omitted.
    testMatrix(M::Rgb);
    testMatrix(M::Bt709);
    testMatrix(M::Bt470bg);
    testMatrix(M::Smpte170m);
    testMatrix(M::Smpte240m);
    testMatrix(M::Fcc);
    testMatrix(M::YCgCo);
    testMatrix(M::Bt2020NonconstantLuminance);
    testMatrix(M::Bt2020ConstantLuminance);
    testMatrix(M::Unspecified);
}

} // namespace TestWebKitAPI

#endif // USE(AVFOUNDATION)
