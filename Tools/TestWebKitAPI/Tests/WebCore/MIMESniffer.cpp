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

#import "Helpers/PlatformUtilities.h"
#include <WebCore/MIMESniffer.h>

namespace TestWebKitAPI {

using namespace WebCore;

TEST(MIMESniffer, MIMETypes)
{
    constexpr std::array<const uint8_t, 11> oggData = { 'O', 'g', 'g', 'S', 0, 'm', 'e', 'e', 'e', 'h', '.' };
    ASSERT_EQ(MIMESniffer::getMIMETypeFromContent({ oggData.data(), oggData.size()}), String("application/ogg"_s));

    constexpr std::array<const uint8_t, 41> webmData = { 0x1A, 0x45, 0xDF, 0xA3, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x42, 0x86, 0x81, 0x01, 0x42, 0xF7, 0x81, 0x01, 0x42, 0xF2, 0x81, 0x04, 0x42, 0xF3, 0x81, 0x08, 0x42, 0x82, 0x84, 0x77, 0x65, 0x62, 0x6D, 0x42, 0x87, 0x81, 0x04, 0x42, 0x85 };
    ASSERT_EQ(MIMESniffer::getMIMETypeFromContent({ webmData.data(), webmData.size() }), String("video/webm"_s));

    constexpr std::array<const uint8_t, 29> mp4data = { 0x00, 0x00, 0x00, 0x1C, 0x66, 0x74, 0x79, 0x70, 0x6D, 0x70, 0x34, 0x32, 0x00, 0x00, 0x00, 0x01, 0x69, 0x73, 0x6F, 0x6D, 0x6D, 0x70, 0x34, 0x31, 0x6D, 0x70, 0x34, 0x32, 0x00 };
    ASSERT_EQ(MIMESniffer::getMIMETypeFromContent({ mp4data.data(), mp4data.size() }), String("video/mp4"_s));

    constexpr std::array<const uint8_t, 32> mp3id3data = { 0x49, 0x44, 0x33, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x54, 0x53, 0x53, 0x45, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x03, 0x4C, 0x61, 0x76, 0x66, 0x36, 0x30, 0x2E, 0x35, 0x2E, 0x31, 0x30 };
    ASSERT_EQ(MIMESniffer::getMIMETypeFromContent({ mp3id3data.data(), mp3id3data.size() }), String("audio/mpeg"_s));

    static std::array<const uint8_t, 32> wavedata = { 0x52, 0x49, 0x46, 0x46, 0x30, 0x13, 0x00, 0x00, 0x57, 0x41, 0x56, 0x45, 0x66, 0x6D, 0x74, 0x20, 0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x44, 0xAC, 0x00, 0x00, 0x10, 0xB1, 0x02, 0x00 };
    ASSERT_EQ(MIMESniffer::getMIMETypeFromContent({ wavedata.data(), wavedata.size() }), String("audio/wave"_s));

    constexpr std::array<const uint8_t, 14> midiData = { 0x4D, 0x54, 0x68, 0x64, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x01, 0x00, 0x60 };
    ASSERT_EQ(MIMESniffer::getMIMETypeFromContent({ midiData.data(), midiData.size() }), String("audio/midi"_s));

    constexpr std::array<const uint8_t, 12> aviData = { 0x52, 0x49, 0x46, 0x46, 0x00, 0x00, 0x00, 0x00, 0x41, 0x56, 0x49, 0x20 };
    ASSERT_EQ(MIMESniffer::getMIMETypeFromContent({ aviData.data(), aviData.size() }), String("video/avi"_s));

    // Legacy Audible (.aa) - Magic Cookie: 0x57 0x90 0x75 0x36 0x35 0x34 0x31 0x32
    constexpr std::array<const uint8_t, 12> aaData = { 0x00, 0x00, 0x00, 0x20, 0x57, 0x90, 0x75, 0x36, 0x35, 0x34, 0x31, 0x32 };
    EXPECT_WK_STREQ(MIMESniffer::getMIMETypeFromContent({ aaData.data(), aaData.size() }), "audio/audible");

    // Modern Audible (.aax) - ISO-BMFF with 'aax ' major brand
    constexpr std::array<const uint8_t, 12> aaxData = { 0x00, 0x00, 0x00, 0x20, 0x66, 0x74, 0x79, 0x70, 0x61, 0x61, 0x78, 0x20 };
    EXPECT_WK_STREQ(MIMESniffer::getMIMETypeFromContent({ aaxData.data(), aaxData.size() }), "audio/vnd.audible.aax");
}

// Builds a buffer holding the first frame header of an MP3 file without ID3 tag, followed by the header of the second frame located at frameSize.
static Vector<uint8_t> mp3TwoFrameHeaders(std::array<uint8_t, 4> firstHeader, std::array<uint8_t, 4> secondHeader, size_t frameSize)
{
    Vector<uint8_t> data(FillWith { }, frameSize + secondHeader.size(), 0);
    memcpySpan(data.mutableSpan(), std::span { firstHeader });
    memcpySpan(data.mutableSpan().subspan(frameSize), std::span { secondHeader });
    return data;
}

TEST(MIMESniffer, MP3WithoutID3)
{
    // MPEG-1 Layer III, 128kbps, 48kHz, no padding: frame size is 144 * 128000 / 48000 = 384 (half-a-second-48000.mp3).
    auto mpeg1 = mp3TwoFrameHeaders({ 0xFF, 0xFB, 0x94, 0x64 }, { 0xFF, 0xFB, 0xC4, 0x64 }, 384);
    EXPECT_EQ(MIMESniffer::getMIMETypeFromContent(mpeg1.span()), String("audio/mpeg"_s));

    // MPEG-1 Layer III, 128kbps, 44.1kHz, padded: frame size is 144 * 128000 / 44100 + 1 = 418 (sine440.mp3).
    auto mpeg1Padded = mp3TwoFrameHeaders({ 0xFF, 0xFA, 0x93, 0xCC }, { 0xFF, 0xFA, 0x93, 0xCC }, 418);
    EXPECT_EQ(MIMESniffer::getMIMETypeFromContent(mpeg1Padded.span()), String("audio/mpeg"_s));

    // MPEG-2 Layer III, 64kbps, 22.05kHz, no padding: frame size is 72 * 64000 / 22050 = 208 (sound_5.mp3).
    auto mpeg2 = mp3TwoFrameHeaders({ 0xFF, 0xF3, 0x80, 0xC4 }, { 0xFF, 0xF3, 0x20, 0xC4 }, 208);
    EXPECT_EQ(MIMESniffer::getMIMETypeFromContent(mpeg2.span()), String("audio/mpeg"_s));

    // MPEG-2.5 Layer III, 32kbps, 8kHz, no padding: frame size is 72 * 32000 / 8000 = 288 (half-a-second-8000.mp3).
    auto mpeg25 = mp3TwoFrameHeaders({ 0xFF, 0xE3, 0x48, 0x64 }, { 0xFF, 0xE3, 0x58, 0x64 }, 288);
    EXPECT_EQ(MIMESniffer::getMIMETypeFromContent(mpeg25.span()), String("audio/mpeg"_s));

    // The second frame header must lie entirely within the sequence.
    mpeg1.shrink(mpeg1.size() - 1);
    EXPECT_NE(MIMESniffer::getMIMETypeFromContent(mpeg1.span()), String("audio/mpeg"_s));
}

TEST(MIMESniffer, WebMSnifferDoesNotReadPastEnd)
{
    // Truncated WebM-style header crafted so hasSignatureForWebM() advances
    // iter exactly to length(): EBML magic (4 bytes) + 0x42 0x82 (2 bytes) +
    // a 1-byte vint with the high bit set. Catches a 1-byte out-of-bounds
    // read in MIMESniffer.cpp under ASan / libc++ hardened mode.
    constexpr std::array<const uint8_t, 7> truncatedWebM = { 0x1A, 0x45, 0xDF, 0xA3, 0x42, 0x82, 0x80 };
    MIMESniffer::getMIMETypeFromContent(std::span { truncatedWebM });

    // EBML magic (4 bytes) followed by a lone 0x42 as the last byte. The loop
    // guard only ensures iter < length, so the 0x42 0x82 two-byte compare reads
    // sequence[iter + 1] one byte past the end when iter == length - 1.
    constexpr std::array<const uint8_t, 5> truncatedWebMAtMagicByte = { 0x1A, 0x45, 0xDF, 0xA3, 0x42 };
    MIMESniffer::getMIMETypeFromContent(std::span { truncatedWebMAtMagicByte });
}

TEST(MIMESniffer, ModelMIMETypes)
{
    auto modelMIMEType = [](ASCIILiteral content) {
        return MIMESniffer::getModelMIMETypeFromContent(byteCast<uint8_t>(content.span()));
    };

    ASSERT_EQ(modelMIMEType("PXR-USDC"_s), String("model/vnd.pixar.usd"_s));
    ASSERT_EQ(modelMIMEType("#usda 1.0"_s), String("model/vnd.pixar.usd"_s));
    ASSERT_EQ(modelMIMEType("PK\x03\x04"_s), String("model/vnd.usdz+zip"_s));
    ASSERT_EQ(modelMIMEType("glTF"_s), String("model/gltf-binary"_s));
    ASSERT_EQ(modelMIMEType("{\"asset\":{}}"_s), String("model/gltf+json"_s));
    ASSERT_EQ(modelMIMEType("solid cube"_s), String("model/stl"_s));

    // Binary STL: an 80-byte header, a little-endian triangle count, then 50 bytes per triangle.
    auto binarySTL = [](uint8_t triangleCount, size_t extraBytes, ASCIILiteral header = ""_s) {
        Vector<uint8_t> data(FillWith { }, 84 + triangleCount * 50 + extraBytes, 0);
        memcpySpan(data.mutableSpan(), byteCast<uint8_t>(header.span()));
        data[80] = triangleCount;
        return MIMESniffer::getModelMIMETypeFromContent(data.span());
    };
    ASSERT_EQ(binarySTL(0, 0), String("model/stl"_s));
    ASSERT_EQ(binarySTL(2, 0), String("model/stl"_s));
    // A signature wins over a size that matches binary STL.
    ASSERT_EQ(binarySTL(2, 0, "glTF"_s), String("model/gltf-binary"_s));
    // The size must match the triangle count exactly.
    ASSERT_TRUE(binarySTL(2, 1).isNull());

    // The USDA signature includes the space after "#usda".
    ASSERT_TRUE(modelMIMEType("#usda1.0"_s).isNull());
    // Only the first half of the ZIP signature.
    ASSERT_TRUE(modelMIMEType("PK"_s).isNull());
    ASSERT_TRUE(modelMIMEType(""_s).isNull());
}

} // namespace TestWebKitAPI
