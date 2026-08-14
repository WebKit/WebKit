/*
 * Copyright (C) 2026 Igalia S.L.
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

#include "Helpers/Test.h"
#include <WebCore/ImageDecoder.h>
#include <WebCore/IntSize.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/Vector.h>

// These drive ImageDecoder directly. Going through BitmapImageSource instead would hide most of
// this: it stops asking a decoder for frames as soon as the decoder reports an error, so the code
// paths that run after a malformed chunk is rejected are unreachable from a layout test.

namespace TestWebKitAPI {

using namespace WebCore;

static void append32(Vector<uint8_t>& data, uint32_t value)
{
    data.append(value >> 24);
    data.append((value >> 16) & 0xff);
    data.append((value >> 8) & 0xff);
    data.append(value & 0xff);
}

static uint32_t crc32For(std::span<const uint8_t> data)
{
    // PNG uses the standard CRC-32; computing it here keeps the fixtures self-contained.
    uint32_t crc = 0xffffffff;
    for (auto byte : data) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320 & (~(crc & 1) + 1));
    }
    return crc ^ 0xffffffff;
}

static void appendChunk(Vector<uint8_t>& data, const char (&tag)[5], std::span<const uint8_t> payload)
{
    append32(data, payload.size());
    Vector<uint8_t> tagged;
    for (int i = 0; i < 4; ++i)
        tagged.append(static_cast<uint8_t>(tag[i]));
    tagged.append(payload);
    data.append(tagged.span());
    append32(data, crc32For(tagged.span()));
}

// A 4x4 opaque red PNG, optionally carrying an acTL that declares |frameCount| frames.
static Ref<SharedBuffer> makeAnimatedPNG(uint32_t frameCount)
{
    Vector<uint8_t> data { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };

    Vector<uint8_t> ihdr;
    append32(ihdr, 4);
    append32(ihdr, 4);
    ihdr.appendList({ 8, 2, 0, 0, 0 });
    appendChunk(data, "IHDR", ihdr.span());

    Vector<uint8_t> actl;
    append32(actl, frameCount);
    append32(actl, 0);
    appendChunk(data, "acTL", actl.span());

    // A stored (uncompressed) zlib stream for four rows of four red pixels.
    Vector<uint8_t> raw;
    for (int y = 0; y < 4; ++y) {
        raw.append(0);
        for (int x = 0; x < 4; ++x)
            raw.appendList({ 0xff, 0x00, 0x00 });
    }
    Vector<uint8_t> idat { 0x78, 0x01, 0x01 };
    idat.append(raw.size() & 0xff);
    idat.append((raw.size() >> 8) & 0xff);
    idat.append(~raw.size() & 0xff);
    idat.append((~raw.size() >> 8) & 0xff);
    idat.append(raw.span());
    uint32_t s1 = 1, s2 = 0;
    for (auto byte : raw) {
        s1 = (s1 + byte) % 65521;
        s2 = (s2 + s1) % 65521;
    }
    append32(idat, (s2 << 16) | s1);
    appendChunk(data, "IDAT", idat.span());

    appendChunk(data, "IEND", { });
    return SharedBuffer::create(WTF::move(data));
}

static RefPtr<ImageDecoder> createDecoder(SharedBuffer& buffer, bool allDataReceived = true)
{
    RefPtr decoder = ImageDecoder::create(buffer, "image/png"_s, AlphaOption::Premultiplied, GammaAndColorProfileOption::Applied);
    if (decoder)
        decoder->setData(buffer, allDataReceived);
    return decoder;
}

// A frame count that PNGImageDecoder rejects must not reach frameCount(), because callers size
// per-frame containers from it. See PNGImageDecoder::readChunks().
TEST(ImageDecoder, APNGRejectedFrameCountIsNotReported)
{
    // Well over cMaxFrameCount, and large enough that a caller sizing an array from it would ask
    // for hundreds of gigabytes.
    auto buffer = makeAnimatedPNG(0xffffff00);
    RefPtr decoder = createDecoder(buffer.get());
    ASSERT_TRUE(decoder);

    EXPECT_TRUE(decoder->isSizeAvailable());
    EXPECT_EQ(1u, decoder->frameCount());
}

// A zero frame count is not an animation, so the default image must still decode.
TEST(ImageDecoder, APNGZeroFrameCountStillDecodesDefaultImage)
{
    auto buffer = makeAnimatedPNG(0);
    RefPtr decoder = createDecoder(buffer.get());
    ASSERT_TRUE(decoder);

    EXPECT_EQ(1u, decoder->frameCount());
    EXPECT_EQ(4, decoder->size().width());
    EXPECT_EQ(4, decoder->size().height());
    EXPECT_TRUE(!!decoder->createFrameImageAtIndex(0));
}

// A well-formed acTL is still honoured.
TEST(ImageDecoder, APNGValidFrameCountIsReported)
{
    auto buffer = makeAnimatedPNG(2);
    RefPtr decoder = createDecoder(buffer.get());
    ASSERT_TRUE(decoder);

    EXPECT_EQ(2u, decoder->frameCount());
}

// Dropping decoded frames must not leave a later frame reading a cleared frame's backing store.
// See the null checks in GIFImageDecoder/PNGImageDecoder initFrameBuffer() and frameComplete().
TEST(ImageDecoder, ClearFrameBufferCacheThenDecodeAgain)
{
    auto buffer = makeAnimatedPNG(2);
    RefPtr decoder = createDecoder(buffer.get());
    ASSERT_TRUE(decoder);

    for (size_t index = 0; index < decoder->frameCount(); ++index)
        decoder->createFrameImageAtIndex(index);

    decoder->clearFrameBufferCache(decoder->frameCount());

    for (size_t index = 0; index < decoder->frameCount(); ++index)
        decoder->createFrameImageAtIndex(index);
}

// Asking for a frame past the end must be answered, not indexed blindly.
TEST(ImageDecoder, FrameIndexPastEndIsHandled)
{
    auto buffer = makeAnimatedPNG(2);
    RefPtr decoder = createDecoder(buffer.get());
    ASSERT_TRUE(decoder);

    decoder->createFrameImageAtIndex(decoder->frameCount() + 1000);
}

// Data arriving in pieces must not let a frame be requested against a cache that was never sized.
TEST(ImageDecoder, PartialDataThenFullData)
{
    auto full = makeAnimatedPNG(2);
    auto partial = SharedBuffer::create(full->span().first(full->size() / 2));

    RefPtr decoder = ImageDecoder::create(partial.get(), "image/png"_s, AlphaOption::Premultiplied, GammaAndColorProfileOption::Applied);
    ASSERT_TRUE(decoder);

    decoder->setData(partial.get(), false);
    decoder->createFrameImageAtIndex(0);

    decoder->setData(full.get(), true);
    decoder->createFrameImageAtIndex(0);
}

// An ICO whose sub-image is a PNG, used to exercise the point where only part of the embedded PNG
// has arrived.
static Ref<SharedBuffer> makeICOWithPNGSubImage()
{
    auto png = makeAnimatedPNG(1);
    auto pngSpan = png->span();

    Vector<uint8_t> data;
    auto append16 = [&](uint16_t value) {
        data.append(value & 0xff);
        data.append(value >> 8);
    };
    auto append32LE = [&](uint32_t value) {
        data.append(value & 0xff);
        data.append((value >> 8) & 0xff);
        data.append((value >> 16) & 0xff);
        data.append(value >> 24);
    };

    append16(0);            // reserved
    append16(1);            // type: icon
    append16(1);            // one directory entry
    data.append(4);         // width
    data.append(4);         // height
    data.append(0);         // colour count
    data.append(0);         // reserved
    append16(1);            // colour planes
    append16(32);           // bits per pixel
    append32LE(pngSpan.size());
    append32LE(22);         // image offset, just past the directory
    data.append(pngSpan);

    return SharedBuffer::create(WTF::move(data));
}

// The sub-decoder has no frame buffer until it has parsed its own header, which has not happened
// while only the PNG signature has arrived. See ICOImageDecoder::decodeAtIndex().
TEST(ImageDecoder, ICOWithPartiallyReceivedPNGSubImage)
{
    auto full = makeICOWithPNGSubImage();

    // Past the icon directory and the PNG signature, but short of the sub-image's IHDR.
    auto partial = SharedBuffer::create(full->span().first(30));

    RefPtr decoder = ImageDecoder::create(partial.get(), "image/vnd.microsoft.icon"_s, AlphaOption::Premultiplied, GammaAndColorProfileOption::Applied);
    ASSERT_TRUE(decoder);

    decoder->setData(partial.get(), false);
    decoder->createFrameImageAtIndex(0);

    decoder->setData(full.get(), true);
    decoder->createFrameImageAtIndex(0);
}

} // namespace TestWebKitAPI
