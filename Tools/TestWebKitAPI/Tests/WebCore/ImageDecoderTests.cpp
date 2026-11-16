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

// Filtered scanlines for a 4x4 opaque red image: a leading filter byte per row, then RGB triples.
static Vector<uint8_t> redScanlines()
{
    Vector<uint8_t> raw;
    for (int y = 0; y < 4; ++y) {
        raw.append(0);
        for (int x = 0; x < 4; ++x)
            raw.appendList({ 0xff, 0x00, 0x00 });
    }
    return raw;
}

// A stored (uncompressed) zlib stream wrapping |raw|.
static Vector<uint8_t> storedZlibStream(std::span<const uint8_t> raw)
{
    Vector<uint8_t> stream { 0x78, 0x01, 0x01 };
    stream.append(raw.size() & 0xff);
    stream.append((raw.size() >> 8) & 0xff);
    stream.append(~raw.size() & 0xff);
    stream.append((~raw.size() >> 8) & 0xff);
    stream.append(raw);
    uint32_t s1 = 1, s2 = 0;
    for (auto byte : raw) {
        s1 = (s1 + byte) % 65521;
        s2 = (s2 + s1) % 65521;
    }
    append32(stream, (s2 << 16) | s1);
    return stream;
}

// A 4x4 opaque red PNG, optionally carrying an acTL that declares |frameCount| frames but no
// per-frame (fcTL/fdAT) data. Used to exercise header handling, not real animation decoding.
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

    appendChunk(data, "IDAT", storedZlibStream(redScanlines().span()).span());

    appendChunk(data, "IEND", { });
    return SharedBuffer::create(WTF::move(data));
}

// A genuine two-frame 4x4 APNG. The second frame does not dispose the first, so decoding it reads
// the first frame's backing store; if the first frame has been evicted from the cache, that read
// is against a null backing store. See PNGImageDecoder::initFrameBuffer() and frameComplete().
static Ref<SharedBuffer> makeTwoFrameAPNG()
{
    Vector<uint8_t> data { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };

    Vector<uint8_t> ihdr;
    append32(ihdr, 4);
    append32(ihdr, 4);
    ihdr.appendList({ 8, 2, 0, 0, 0 });
    appendChunk(data, "IHDR", ihdr.span());

    Vector<uint8_t> actl;
    append32(actl, 2); // num_frames
    append32(actl, 0); // num_plays (infinite)
    appendChunk(data, "acTL", actl.span());

    auto appendFrameControl = [&](uint32_t sequenceNumber) {
        Vector<uint8_t> fctl;
        append32(fctl, sequenceNumber);
        append32(fctl, 4); // width
        append32(fctl, 4); // height
        append32(fctl, 0); // x_offset
        append32(fctl, 0); // y_offset
        fctl.appendList({ 0, 1 }); // delay_num = 1
        fctl.appendList({ 0, 10 }); // delay_den = 10
        fctl.append(0); // dispose_op = APNG_DISPOSE_OP_NONE
        fctl.append(0); // blend_op = APNG_BLEND_OP_SOURCE
        appendChunk(data, "fcTL", fctl.span());
    };

    auto zlib = storedZlibStream(redScanlines().span());

    // Frame 0: fcTL then IDAT (the default image is also the first animation frame). fcTL and fdAT
    // share one sequence counter that must have no gaps; IDAT is not numbered.
    appendFrameControl(0);
    appendChunk(data, "IDAT", zlib.span());

    // Frame 1: fcTL then fdAT (sequence number, then the same zlib stream as an IDAT payload).
    appendFrameControl(1);
    Vector<uint8_t> fdat;
    append32(fdat, 2); // sequence number
    fdat.append(zlib.span());
    appendChunk(data, "fdAT", fdat.span());

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

static Ref<SharedBuffer> makeSubRectAPNG(size_t frameCount)
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

    // Filtered scanlines for an opaque red block of |width| x |height|.
    auto scanlines = [](uint32_t width, uint32_t height) {
        Vector<uint8_t> raw;
        for (uint32_t y = 0; y < height; ++y) {
            raw.append(0);
            for (uint32_t x = 0; x < width; ++x)
                raw.appendList({ 0xff, 0x00, 0x00 });
        }
        return raw;
    };

    uint32_t sequence = 0;
    auto appendFrameControl = [&](uint32_t width, uint32_t height, uint32_t x, uint32_t y, uint8_t disposeOp) {
        Vector<uint8_t> fctl;
        append32(fctl, sequence++);
        append32(fctl, width);
        append32(fctl, height);
        append32(fctl, x);
        append32(fctl, y);
        fctl.appendList({ 0, 1 });
        fctl.appendList({ 0, 10 });
        fctl.append(disposeOp);
        fctl.append(0); // blend_op = APNG_BLEND_OP_SOURCE
        appendChunk(data, "fcTL", fctl.span());
    };

    // Frame 0 covers the canvas, so its own opacity is decided without consulting any earlier frame.
    appendFrameControl(4, 4, 0, 0, 0);
    appendChunk(data, "IDAT", storedZlibStream(scanlines(4, 4).span()).span());

    // A sub-rectangle makes frameComplete() consult the previous frame rather than settle its
    // opacity from the frame rect, and APNG_DISPOSE_OP_PREVIOUS makes the frame evictable.
    auto zlib = storedZlibStream(scanlines(2, 2).span());
    for (size_t frame = 1; frame < frameCount; ++frame) {
        appendFrameControl(2, 2, 1, 1, 2);
        Vector<uint8_t> fdat;
        append32(fdat, sequence++);
        fdat.append(zlib.span());
        appendChunk(data, "fdAT", fdat.span());
    }

    appendChunk(data, "IEND", { });
    return SharedBuffer::create(WTF::move(data));
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

// A well-formed acTL is still honoured, and both frames of a real APNG decode.
TEST(ImageDecoder, APNGValidFrameCountIsReported)
{
    auto buffer = makeTwoFrameAPNG();
    RefPtr decoder = createDecoder(buffer.get());
    ASSERT_TRUE(decoder);

    EXPECT_EQ(2u, decoder->frameCount());
    EXPECT_TRUE(!!decoder->createFrameImageAtIndex(0));
    EXPECT_TRUE(!!decoder->createFrameImageAtIndex(1));
}

// Dropping decoded frames must not leave a later frame reading a cleared frame's backing store.
// See the null checks in GIFImageDecoder/PNGImageDecoder initFrameBuffer() and frameComplete().
TEST(ImageDecoder, ClearFrameBufferCacheThenDecodeAgain)
{
    // Four frames, because clearDecodedPixelDataIfNeeded() clears neither the first frame nor the
    // last, so two frames leaves it nothing to evict. RestoreToPrevious is what it evicts.
    auto buffer = makeSubRectAPNG(4);
    RefPtr decoder = createDecoder(buffer.get());
    ASSERT_TRUE(decoder);
    ASSERT_EQ(4u, decoder->frameCount());

    for (size_t index = 0; index < decoder->frameCount(); ++index)
        decoder->createFrameImageAtIndex(index);

    decoder->clearFrameBufferCache(decoder->frameCount());
    ASSERT_FALSE(decoder->frameIsCompleteAtIndex(1));
    ASSERT_FALSE(decoder->frameIsCompleteAtIndex(2));

    for (size_t index = 0; index < decoder->frameCount(); ++index)
        decoder->createFrameImageAtIndex(index);

    for (size_t index = 0; index < decoder->frameCount(); ++index)
        ASSERT_TRUE(decoder->frameIsCompleteAtIndex(index));
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

    append16(0); // reserved
    append16(1); // type: icon
    append16(1); // one directory entry
    data.append(4); // width
    data.append(4); // height
    data.append(0); // colour count
    data.append(0); // reserved
    append16(1); // colour planes
    append16(32); // bits per pixel
    append32LE(pngSpan.size());
    append32LE(22); // image offset, just past the directory
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

// Evict a frame from the middle of the cache, then decode a later one -- the sequence
// PNGImageDecoder::frameComplete()'s null check exists for. It does not reach that check: the
// evicted frame is re-decoded first, because clear() resets the disposal method that
// findFirstRequiredFrameToDecode() uses to decide a frame can be skipped. So this passes with the
// check removed, and guards the sequence rather than the branch.
TEST(ImageDecoder, APNGSubRectFrameWithEvictedPreviousFrame)
{
    auto buffer = makeSubRectAPNG(4);
    RefPtr decoder = createDecoder(buffer.get());
    ASSERT_TRUE(decoder);
    ASSERT_EQ(4u, decoder->frameCount());

    for (size_t index = 0; index < decoder->frameCount(); ++index)
        decoder->createFrameImageAtIndex(index);

    // Evicts frames 1 and 2; the first and last are never cleared.
    decoder->clearFrameBufferCache(decoder->frameCount());
    ASSERT_TRUE(decoder->frameIsCompleteAtIndex(0));
    ASSERT_FALSE(decoder->frameIsCompleteAtIndex(1));
    ASSERT_FALSE(decoder->frameIsCompleteAtIndex(2));

    // Frame 2 only: asking for frame 1 first would restore the backing store under test.
    decoder->createFrameImageAtIndex(2);
}

// --- GIF ----------------------------------------------------------------------------------------

// Single-pixel frames on a 2x2 canvas, so every frame after the first is a sub-rectangle, with
// disposal method 3 (restore to previous) so the middle ones are evictable.
static Ref<SharedBuffer> makeSubRectGIF(size_t frameCount)
{
    Vector<uint8_t> data;
    for (auto c : { 'G', 'I', 'F', '8', '9', 'a' })
        data.append(static_cast<uint8_t>(c));

    auto append16LE = [&](uint16_t value) {
        data.append(value & 0xff);
        data.append(value >> 8);
    };

    // Logical screen descriptor: 2x2, global colour table of two entries.
    append16LE(2);
    append16LE(2);
    data.append(0x80);
    data.append(0);
    data.append(0);
    data.appendList({ 0xff, 0x00, 0x00 }); // index 0: red
    data.appendList({ 0x00, 0x00, 0x00 }); // index 1: black

    // One pixel of colour index 0: CLEAR(4), 0, END(5) as 3-bit LZW codes packed low bit first.
    // Nothing is ever reused, so the code width never grows.
    auto appendImage = [&](uint16_t left, uint16_t top, uint8_t disposalMethod) {
        data.appendList({ 0x21, 0xf9, 0x04 });
        data.append(static_cast<uint8_t>(disposalMethod << 2));
        append16LE(10); // delay, in hundredths
        data.append(0); // transparent colour index
        data.append(0); // block terminator

        data.append(0x2c); // image separator
        append16LE(left);
        append16LE(top);
        append16LE(1); // width
        append16LE(1); // height
        data.append(0); // no local colour table, not interlaced

        data.append(2); // LZW minimum code size
        data.append(2); // sub-block length
        data.appendList({ 0x44, 0x01 });
        data.append(0); // block terminator
    };

    appendImage(0, 0, 1); // frame 0: do not dispose
    for (size_t frame = 1; frame < frameCount; ++frame)
        appendImage(1, 1, 3); // restore to previous

    data.append(0x3b); // trailer
    return SharedBuffer::create(WTF::move(data));
}

// The GIF twin of the case above, unreachable for the same reason.
// See GIFImageDecoder::frameComplete().
TEST(ImageDecoder, GIFSubRectFrameWithEvictedPreviousFrame)
{
    auto buffer = makeSubRectGIF(4);
    RefPtr decoder = ImageDecoder::create(buffer.get(), "image/gif"_s, AlphaOption::Premultiplied, GammaAndColorProfileOption::Applied);
    ASSERT_TRUE(decoder);
    decoder->setData(buffer.get(), true);
    ASSERT_EQ(4u, decoder->frameCount());

    for (size_t index = 0; index < decoder->frameCount(); ++index)
        decoder->createFrameImageAtIndex(index);

    decoder->clearFrameBufferCache(decoder->frameCount());
    ASSERT_TRUE(decoder->frameIsCompleteAtIndex(0));
    ASSERT_FALSE(decoder->frameIsCompleteAtIndex(1));
    ASSERT_FALSE(decoder->frameIsCompleteAtIndex(2));

    decoder->createFrameImageAtIndex(2);
}

// --- WebP ---------------------------------------------------------------------------------------

// LayoutTests/fast/images/resources/awebp00.webp, byte for byte. The demuxer has to accept the
// file well enough to report a frame count, which it will not do for hand-built bytes.
static Ref<SharedBuffer> makeAnimatedWebP()
{
    static constexpr std::array<uint8_t, 948> bytes { {
        0x52, 0x49, 0x46, 0x46, 0xac, 0x03, 0x00, 0x00, 0x57, 0x45, 0x42, 0x50,
        0x56, 0x50, 0x38, 0x58, 0x0a, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
        0x3e, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x41, 0x4e, 0x49, 0x4d, 0x06, 0x00,
        0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x01, 0x00, 0x41, 0x4e, 0x4d, 0x46,
        0x2c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x00,
        0x00, 0x3e, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x56, 0x50, 0x38, 0x4c,
        0x14, 0x00, 0x00, 0x00, 0x2f, 0x3e, 0x80, 0x0f, 0x00, 0x07, 0x10, 0x11,
        0xfd, 0x0f, 0x00, 0x09, 0xe1, 0xff, 0x7b, 0x2d, 0xa2, 0xff, 0xa9, 0x1d,
        0x41, 0x4e, 0x4d, 0x46, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x3e, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00,
        0x56, 0x50, 0x38, 0x4c, 0x2b, 0x00, 0x00, 0x00, 0x2f, 0x3e, 0x80, 0x0f,
        0x00, 0x0f, 0x30, 0xff, 0xf3, 0x3f, 0xff, 0xf3, 0x1f, 0x78, 0x50, 0xd3,
        0x48, 0x0a, 0x74, 0x4e, 0x4e, 0x0a, 0xd2, 0x40, 0x1a, 0xde, 0xe8, 0x7e,
        0xca, 0x88, 0xfe, 0x4f, 0x80, 0x3a, 0x11, 0xf3, 0x99, 0xa0, 0x88, 0x3a,
        0x8b, 0xfc, 0x33, 0x00, 0x41, 0x4e, 0x4d, 0x46, 0x4e, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x3e, 0x00, 0x00,
        0x0b, 0x00, 0x00, 0x00, 0x56, 0x50, 0x38, 0x4c, 0x36, 0x00, 0x00, 0x00,
        0x2f, 0x3e, 0x80, 0x0f, 0x00, 0x0f, 0x30, 0xff, 0xf3, 0x3f, 0xff, 0xf3,
        0x1f, 0x78, 0x50, 0x1b, 0xdb, 0x36, 0x74, 0xff, 0xb7, 0x85, 0x4a, 0x52,
        0x1a, 0xa5, 0x29, 0x49, 0x28, 0x93, 0xef, 0xc2, 0x88, 0xfe, 0x0b, 0x08,
        0x8a, 0x2c, 0x70, 0x23, 0xd7, 0x11, 0xbb, 0x81, 0xf9, 0x4a, 0xa2, 0x05,
        0x56, 0x58, 0xf6, 0xf9, 0x9f, 0x01, 0x41, 0x4e, 0x4d, 0x46, 0x4c, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x3e,
        0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x56, 0x50, 0x38, 0x4c, 0x33, 0x00,
        0x00, 0x00, 0x2f, 0x3e, 0x80, 0x0f, 0x00, 0x0f, 0x30, 0xff, 0xf3, 0x3f,
        0xff, 0xf3, 0x1f, 0x78, 0x50, 0x15, 0xdb, 0x36, 0xb5, 0x24, 0x11, 0x45,
        0xb4, 0x2b, 0x9a, 0x60, 0x46, 0x80, 0xb7, 0xcf, 0x88, 0xfe, 0x0b, 0x08,
        0x8a, 0x2c, 0x70, 0x23, 0x5a, 0xa6, 0xa7, 0x98, 0xaf, 0x16, 0x18, 0xe6,
        0x5b, 0xf7, 0xe9, 0x37, 0x03, 0x00, 0x41, 0x4e, 0x4d, 0x46, 0x50, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x3e,
        0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x56, 0x50, 0x38, 0x4c, 0x38, 0x00,
        0x00, 0x00, 0x2f, 0x3e, 0x80, 0x0f, 0x00, 0x0f, 0x30, 0xff, 0xf3, 0x3f,
        0xff, 0xf3, 0x1f, 0x78, 0x50, 0xd4, 0x48, 0x8a, 0xd4, 0x28, 0x19, 0x29,
        0x48, 0x1b, 0xa4, 0x21, 0x8c, 0xac, 0x80, 0xf3, 0x1d, 0xd1, 0xff, 0x09,
        0x70, 0x0b, 0x26, 0xbc, 0x5f, 0x25, 0xc0, 0x62, 0x3f, 0xb7, 0x60, 0xc2,
        0x87, 0x00, 0xfb, 0x55, 0x72, 0x88, 0x5b, 0x87, 0xfc, 0x33, 0x41, 0x4e,
        0x4d, 0x46, 0x5e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x3e, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x56, 0x50,
        0x38, 0x4c, 0x45, 0x00, 0x00, 0x00, 0x2f, 0x3e, 0x80, 0x0f, 0x00, 0x0f,
        0x30, 0xff, 0xf3, 0x3f, 0xff, 0xf3, 0x1f, 0x78, 0x60, 0x14, 0xdb, 0x56,
        0x9b, 0x4b, 0x5f, 0x22, 0x01, 0x29, 0x48, 0xfb, 0x48, 0x43, 0x0a, 0x12,
        0x58, 0xb6, 0x24, 0x02, 0xea, 0x3a, 0xa2, 0xff, 0x13, 0x20, 0x35, 0xc1,
        0xa1, 0xe4, 0x7e, 0x25, 0x03, 0x5c, 0xdc, 0x4f, 0x6a, 0x82, 0x43, 0xc9,
        0xfd, 0xa0, 0xc5, 0x61, 0xe0, 0x09, 0xf4, 0xc0, 0x33, 0x5a, 0x1c, 0xe6,
        0x19, 0x7f, 0x19, 0x00, 0x41, 0x4e, 0x4d, 0x46, 0x5a, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x3e, 0x00, 0x00,
        0x0b, 0x00, 0x00, 0x00, 0x56, 0x50, 0x38, 0x4c, 0x42, 0x00, 0x00, 0x00,
        0x2f, 0x3e, 0x80, 0x0f, 0x00, 0x0f, 0x30, 0xff, 0xf3, 0x3f, 0xff, 0xf3,
        0x1f, 0x78, 0x50, 0x12, 0xdb, 0x56, 0x74, 0x51, 0x48, 0x22, 0x10, 0x85,
        0x68, 0x9f, 0x68, 0x44, 0x21, 0x02, 0x72, 0x07, 0xbf, 0xea, 0x88, 0xfe,
        0x0b, 0x08, 0x8a, 0x2c, 0x70, 0x43, 0x82, 0x85, 0xae, 0x64, 0x3f, 0xef,
        0x00, 0x26, 0xfb, 0x9d, 0xfb, 0x84, 0x68, 0xf2, 0x3a, 0x83, 0xa4, 0x99,
        0x1c, 0x70, 0xee, 0xf3, 0x97, 0x01, 0x41, 0x4e, 0x4d, 0x46, 0x64, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x3e,
        0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x56, 0x50, 0x38, 0x4c, 0x4c, 0x00,
        0x00, 0x00, 0x2f, 0x3e, 0x80, 0x0f, 0x00, 0x0f, 0x30, 0xff, 0xf3, 0x3f,
        0xff, 0xf3, 0x1f, 0x78, 0x60, 0x13, 0xd9, 0x6e, 0xa3, 0x77, 0xb9, 0x34,
        0x04, 0x41, 0x31, 0x34, 0x19, 0x9a, 0xa0, 0x18, 0xc2, 0x95, 0x39, 0x77,
        0x12, 0x81, 0x88, 0xfe, 0x4f, 0x80, 0x9a, 0xc0, 0xd1, 0xa8, 0xbf, 0x92,
        0x01, 0x2e, 0xfc, 0xa9, 0x09, 0x1c, 0x8d, 0xfa, 0x83, 0x9a, 0x96, 0x0e,
        0x22, 0x30, 0x4f, 0xc4, 0xa8, 0x69, 0xe9, 0x22, 0xa8, 0x09, 0x7f, 0x7f,
        0x25, 0x13, 0x44, 0x4d, 0x82, 0x00, 0x41, 0x4e, 0x4d, 0x46, 0x64, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x3e,
        0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x56, 0x50, 0x38, 0x4c, 0x4c, 0x00,
        0x00, 0x00, 0x2f, 0x3e, 0x80, 0x0f, 0x00, 0x0f, 0x30, 0xff, 0xf3, 0x3f,
        0xff, 0xf3, 0x1f, 0x78, 0x60, 0x13, 0xdb, 0x56, 0x9c, 0x4b, 0xd6, 0xf2,
        0x4b, 0x40, 0x0a, 0xd2, 0x1e, 0xd2, 0xbe, 0x14, 0x24, 0x50, 0x66, 0x4f,
        0xcf, 0x37, 0x10, 0xd1, 0xff, 0x90, 0x67, 0x78, 0x92, 0xc6, 0xd7, 0x02,
        0xf0, 0x31, 0x5e, 0x9e, 0xe1, 0x49, 0x1a, 0x0f, 0xcd, 0x8e, 0x19, 0x22,
        0xe8, 0x3b, 0x31, 0xcd, 0x8e, 0x39, 0x42, 0xfe, 0x07, 0xd5, 0x42, 0xdf,
        0x89, 0x90, 0xff, 0x76, 0xcc, 0x11, 0x41, 0x4e, 0x4d, 0x46, 0x5e, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x3e,
        0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x56, 0x50, 0x38, 0x4c, 0x46, 0x00,
        0x00, 0x00, 0x2f, 0x3e, 0x80, 0x0f, 0x00, 0x0f, 0x30, 0xff, 0xf3, 0x3f,
        0xff, 0xf3, 0x1f, 0x78, 0x60, 0x14, 0xdb, 0x56, 0x9b, 0x4b, 0x5f, 0x22,
        0x01, 0x29, 0x48, 0xfb, 0x48, 0x43, 0x0a, 0x12, 0x58, 0xb6, 0x24, 0x02,
        0xea, 0x3a, 0xa2, 0xff, 0x02, 0x92, 0xc4, 0x07, 0xd1, 0x0b, 0xa9, 0x09,
        0x0e, 0x25, 0xf7, 0x2b, 0x19, 0xe0, 0xe2, 0x7e, 0xcf, 0x75, 0x42, 0x8b,
        0xc3, 0xc0, 0x13, 0xe8, 0x81, 0x47, 0x3c, 0xd6, 0xf9, 0x9b, 0x1f, 0x05
    } };
    return SharedBuffer::create(std::span<const uint8_t> { bytes });
}

// frameCount() comes from the demuxer, and decode() returns before sizing the cache when the
// demuxer rejects the data, so the two can disagree. The frame count is established from a good
// prefix and survives a buffer too short for parseHeader() to re-read it from -- which is also
// what makes WebPDemuxPartial() fail. See WEBPImageDecoder::frameBufferAtIndex().
TEST(ImageDecoder, WEBPFrameIndexBeyondCacheAfterFailedDecode)
{
    auto full = makeAnimatedWebP();

    // Enough to count frames, and nothing decoded, so the cache is still empty.
    auto prefix = SharedBuffer::create(full->span().first(full->size() / 2));
    RefPtr decoder = ImageDecoder::create(prefix.get(), "image/webp"_s, AlphaOption::Premultiplied, GammaAndColorProfileOption::Applied);
    ASSERT_TRUE(decoder);
    decoder->setData(prefix.get(), false);

    auto frameCount = decoder->frameCount();
    ASSERT_GT(frameCount, 1u);

    // Under the 30 bytes parseHeader() needs, so the count above stands, and far too little for
    // WebPDemuxPartial().
    Vector<uint8_t> stub;
    for (size_t i = 0; i < 16; ++i)
        stub.append(0xff);
    auto truncated = SharedBuffer::create(WTF::move(stub));
    decoder->setData(truncated.get(), false);
    ASSERT_EQ(frameCount, decoder->frameCount());

    // Highest index first: it is the furthest outside the empty cache.
    for (size_t index = frameCount; index-- > 0;)
        decoder->createFrameImageAtIndex(index);
}

// --- JPEG XL ------------------------------------------------------------------------------------

#if USE(JPEGXL)

// A three-frame animated JXL, byte for byte
// LayoutTests/fast/images/resources/animated-red-green-blue-repeat-2.jxl.
static Ref<SharedBuffer> makeAnimatedJPEGXL()
{
    static constexpr std::array<uint8_t, 99> bytes { {
        0xff, 0x0a, 0x18, 0x13, 0x41, 0x48, 0x10, 0x13, 0x08, 0x04, 0x0a, 0x08,
        0x00, 0x00, 0x64, 0x00, 0x4b, 0x18, 0x8b, 0x15, 0x00, 0xd4, 0x1b, 0x63,
        0x30, 0x76, 0x2f, 0x10, 0x01, 0x00, 0x20, 0x01, 0x52, 0xa7, 0x3f, 0x00,
        0x41, 0x7f, 0x5f, 0x3c, 0x1a, 0x08, 0x04, 0x0a, 0x08, 0x00, 0x00, 0x60,
        0x00, 0x4b, 0x18, 0x8b, 0x15, 0x00, 0xd4, 0x1b, 0x63, 0x30, 0x76, 0x2f,
        0x10, 0x01, 0x00, 0x6f, 0x75, 0x02, 0x00, 0x00, 0x01, 0x40, 0x5f, 0xfc,
        0x1f, 0x08, 0x04, 0x0a, 0x04, 0x00, 0x4c, 0x00, 0x4b, 0x18, 0x8b, 0x15,
        0x80, 0x5d, 0xfe, 0xff, 0x02, 0x11, 0x00, 0xd8, 0x98, 0x36, 0x00, 0x28,
        0x02, 0xfa, 0x03
    } };
    return SharedBuffer::create(std::span<const uint8_t> { bytes });
}

// clearFrameBufferCache() takes whatever index its caller has, which can be past the end of the
// cache, so the end iterator has to be clamped to the cache rather than to that index.
// See JPEGXLImageDecoder::clearDecodedPixelDataIfNeeded().
TEST(ImageDecoder, JPEGXLClearFrameBufferCachePastEnd)
{
    auto buffer = makeAnimatedJPEGXL();
    RefPtr decoder = ImageDecoder::create(buffer.get(), "image/jxl"_s, AlphaOption::Premultiplied, GammaAndColorProfileOption::Applied);
    ASSERT_TRUE(decoder);
    decoder->setData(buffer.get(), true);

    decoder->createFrameImageAtIndex(0);

    // Far enough past the end to leave mapped memory. A few entries past would be a silent heap
    // overwrite that only a sanitizer would catch.
    decoder->clearFrameBufferCache(1 << 28);
}

#endif // USE(JPEGXL)

static Vector<uint8_t> makeThreeFrameAPNGWithDisposePrevious(size_t& cutAfterSecondFrame)
{
    Vector<uint8_t> data { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };

    Vector<uint8_t> ihdr;
    append32(ihdr, 4);
    append32(ihdr, 4);
    ihdr.appendList({ 8, 2, 0, 0, 0 });
    appendChunk(data, "IHDR", ihdr.span());

    Vector<uint8_t> actl;
    append32(actl, 3); // num_frames
    append32(actl, 0); // num_plays (infinite)
    appendChunk(data, "acTL", actl.span());

    auto appendFrameControl = [&](uint32_t sequenceNumber, uint8_t disposeOp) {
        Vector<uint8_t> fctl;
        append32(fctl, sequenceNumber);
        append32(fctl, 4); // width
        append32(fctl, 4); // height
        append32(fctl, 0); // x_offset
        append32(fctl, 0); // y_offset
        fctl.appendList({ 0, 1 }); // delay_num = 1
        fctl.appendList({ 0, 10 }); // delay_den = 10
        fctl.append(disposeOp);
        fctl.append(0); // blend_op = APNG_BLEND_OP_SOURCE
        appendChunk(data, "fcTL", fctl.span());
    };

    auto zlib = storedZlibStream(redScanlines().span());
    auto appendFrameData = [&](uint32_t sequenceNumber) {
        Vector<uint8_t> fdat;
        append32(fdat, sequenceNumber);
        fdat.append(zlib.span());
        appendChunk(data, "fdAT", fdat.span());
    };

    appendFrameControl(0, 0); // APNG_DISPOSE_OP_NONE
    appendChunk(data, "IDAT", zlib.span());

    appendFrameControl(1, 2); // APNG_DISPOSE_OP_PREVIOUS
    appendFrameData(2);

    // Frame 1 is only marked complete once the next fcTL is parsed, so the cut point must include
    // this chunk for the eviction below to find a complete frame.
    appendFrameControl(3, 0);
    cutAfterSecondFrame = data.size();

    appendFrameData(4);

    appendChunk(data, "IEND", { });
    return data;
}

// Decode two frames from partial data, evict the complete RestoreToPrevious frame while the
// reader is still alive, then let the decode continue: initFrameBuffer() for frame 2 walks back
// to the evicted frame and reads its backing store.
TEST(ImageDecoder, EvictRestoreToPreviousFrameMidStream)
{
    size_t cut = 0;
    auto bytes = makeThreeFrameAPNGWithDisposePrevious(cut);
    auto full = SharedBuffer::create(Vector<uint8_t>(bytes));
    auto partial = SharedBuffer::create(bytes.span().first(cut));

    RefPtr decoder = ImageDecoder::create(partial.get(), "image/png"_s, AlphaOption::Premultiplied, GammaAndColorProfileOption::Applied);
    ASSERT_TRUE(decoder);

    decoder->setData(partial.get(), false);
    decoder->createFrameImageAtIndex(1);

    decoder->clearFrameBufferCache(3);

    decoder->setData(full.get(), true);
    decoder->createFrameImageAtIndex(2);
}

} // namespace TestWebKitAPI
