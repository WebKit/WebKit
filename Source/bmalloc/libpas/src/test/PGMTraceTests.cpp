/*
 * Copyright (c) 2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "TestHarness.h"

#include "pas_pgm_trace.h"
#include <random>
#include <vector>

#if PAS_OS(DARWIN)
#include <execinfo.h>
#endif

namespace {

using Frames = std::vector<uintptr_t>;
using Bytes = std::vector<uint8_t>;

constexpr uint32_t maxFrames = PAS_PGM_BACKTRACE_MAX_FRAMES;

void checkFramesEqual(const Frames& actual, const Frames& expected)
{
    CHECK_EQUAL(actual.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i)
        CHECK_EQUAL(actual[i], expected[i]);
}

void checkBytesEqual(const Bytes& actual, const Bytes& expected)
{
    CHECK_EQUAL(actual.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i)
        CHECK_EQUAL(static_cast<unsigned>(actual[i]), static_cast<unsigned>(expected[i]));
}

Bytes compress(const Frames& frames, size_t bufferSize)
{
    Bytes buffer(bufferSize);
    size_t size = pas_pgm_trace_compress(buffer.data(), buffer.size(), frames.data(), static_cast<uint32_t>(frames.size()));
    CHECK_LESS_EQUAL(size, buffer.size());
    buffer.resize(size);
    return buffer;
}

Bytes compress(const Frames& frames)
{
    Bytes compressed = compress(frames, PAS_PGM_TRACE_MAX_COMPRESSED_SIZE(frames.size()));
    CHECK_GREATER(compressed.size(), 1u);
    return compressed;
}

Frames decompress(const Bytes& compressed, uint32_t capacity)
{
    Frames frames(capacity);
    uint32_t count = pas_pgm_trace_decompress(compressed.data(), compressed.size(), frames.data(), capacity);
    CHECK_LESS_EQUAL(count, capacity);
    frames.resize(count);
    return frames;
}

Bytes checkRoundTrip(const Frames& frames)
{
    Bytes compressed = compress(frames);
    checkFramesEqual(decompress(compressed, static_cast<uint32_t>(frames.size())), frames);
    return compressed;
}

Frames prefix(const Frames& frames, size_t count)
{
    return Frames(frames.begin(), frames.begin() + static_cast<ptrdiff_t>(count));
}

Frames armLikeTrace(std::mt19937_64& random, uint32_t count)
{
    static constexpr uintptr_t imageBases[] = { 0x100000000, 0x180000000, 0x1a0000000, 0x1f0000000 };
    Frames frames(count);
    for (auto& frame : frames)
        frame = imageBases[random() % 4] + (random() % 0x4000000) * 4;
    return frames;
}

Frames x86LikeTrace(std::mt19937_64& random, uint32_t count)
{
    static constexpr uintptr_t imageBases[] = { 0x100000000, 0x7ff800000000, 0x7ff810000000, 0x7ff820000000 };
    Frames frames(count);
    for (auto& frame : frames)
        frame = imageBases[random() % 4] + random() % 0x10000000;
    return frames;
}

Frames fullWidthTrace(std::mt19937_64& random, uint32_t count)
{
    Frames frames(count);
    for (auto& frame : frames)
        frame = static_cast<uintptr_t>(random());
    return frames;
}

void testPGMTraceGoldenEncodings()
{
    checkBytesEqual(checkRoundTrip({ 0x1000, 0x1004, 0x1008, 0x1000 }), { 0x02, 0x80, 0x10, 0x02, 0x02, 0x03 });
    checkBytesEqual(checkRoundTrip({ 0x1001, 0x1002, 0x1000 }), { 0x00, 0x82, 0x40, 0x02, 0x03 });
    checkBytesEqual(checkRoundTrip({ 0x2002, 0x2000 }), { 0x01, 0x82, 0x40, 0x01 });
    checkBytesEqual(checkRoundTrip({ 0x2000, 0x2008, 0x2000 }), { 0x03, 0x80, 0x10, 0x02, 0x01 });
    checkBytesEqual(checkRoundTrip({ 0x0123456789abcdef, 0xfedcba9876543210 }), {
        PAS_PGM_TRACE_RAW_HEADER,
        0xef, 0xcd, 0xab, 0x89, 0x67, 0x45, 0x23, 0x01,
        0x10, 0x32, 0x54, 0x76, 0x98, 0xba, 0xdc, 0xfe });
}

void testPGMTraceDecoderReadsShiftFromHeader()
{
    for (uint8_t shift = 0; shift <= PAS_PGM_TRACE_MAX_SHIFT; ++shift)
        checkFramesEqual(decompress({ shift, 0x02, 0x02, 0x03 }, 3), { uintptr_t(1) << shift, uintptr_t(2) << shift, uintptr_t(0) });

    for (unsigned header = PAS_PGM_TRACE_MAX_SHIFT + 1; header <= 0xff; ++header) {
        if (header != PAS_PGM_TRACE_RAW_HEADER)
            CHECK_EQUAL(decompress({ static_cast<uint8_t>(header), 0x02, 0x02 }, 2).size(), 0u);
    }
}

void testPGMTraceUnalignedGaps()
{
    Frames aligned = { 0x100004000, 0x100004a38, 0x18a2c1b40, 0x18a2c1000, 0x1000080f0 };
    CHECK_EQUAL(static_cast<unsigned>(checkRoundTrip(aligned)[0]), PAS_PGM_TRACE_MAX_SHIFT);

    for (uintptr_t misalignment : { uintptr_t(1), uintptr_t(2), uintptr_t(4) }) {
        for (size_t start = 0; start < aligned.size(); ++start) {
            for (bool subtract : { false, true }) {
                Frames frames = aligned;
                for (size_t i = start; i < frames.size(); ++i)
                    frames[i] = subtract ? frames[i] - misalignment : frames[i] + misalignment;
                unsigned expectedShift = misalignment == 1 ? 0 : misalignment == 2 ? 1 : 2;
                CHECK_EQUAL(static_cast<unsigned>(checkRoundTrip(frames)[0]), expectedShift);
            }
        }
    }
}

void testPGMTraceNegativeDeltas()
{
    checkRoundTrip({ 0x7fff00001000, 0x7fff00000ff3, 0x100000003, 0x10, 0 });
    checkRoundTrip({ 0x100000040, 0x100000030, 0x100000020, 0x100000010 });
    checkRoundTrip({ UINTPTR_MAX, 0, UINTPTR_MAX, static_cast<uintptr_t>(INTPTR_MIN), static_cast<uintptr_t>(INTPTR_MAX), 0 });
}

void testPGMTraceMixedGaps()
{
    Frames frames = { 0x100003f2c, 0x100003e90, 0x18a2c1b3d, 0x18a2c1b40, 0x1000041a8, 0x7ff8123456f1, 0x7ff812345700, 0x100003f2c };
    CHECK_EQUAL(static_cast<unsigned>(checkRoundTrip(frames)[0]), 0u);
}

void testPGMTraceZeroLength()
{
    uintptr_t frames[1] = { 0x1000 };
    uint8_t buffer[16] = { };
    uintptr_t output[1] = { };

    CHECK_EQUAL(pas_pgm_trace_compress(buffer, sizeof(buffer), frames, 0), 0u);
    CHECK_EQUAL(pas_pgm_trace_compress(buffer, 0, frames, 1), 0u);
    CHECK_EQUAL(pas_pgm_trace_compress(buffer, 1, frames, 1), 0u);
    CHECK_EQUAL(pas_pgm_trace_compress(nullptr, sizeof(buffer), frames, 1), 0u);
    CHECK_EQUAL(pas_pgm_trace_compress(buffer, sizeof(buffer), nullptr, 1), 0u);

    buffer[0] = 0;
    CHECK_EQUAL(pas_pgm_trace_decompress(buffer, 1, output, 1), 0u);
    CHECK_EQUAL(pas_pgm_trace_decompress(buffer, 0, output, 1), 0u);
    CHECK_EQUAL(pas_pgm_trace_decompress(buffer, sizeof(buffer), output, 0), 0u);
    CHECK_EQUAL(pas_pgm_trace_decompress(nullptr, sizeof(buffer), output, 1), 0u);
    CHECK_EQUAL(pas_pgm_trace_decompress(buffer, sizeof(buffer), nullptr, 1), 0u);
    CHECK_EQUAL(output[0], 0u);
}

void testPGMTraceSingleFrame()
{
    checkBytesEqual(checkRoundTrip({ 0 }), { PAS_PGM_TRACE_MAX_SHIFT, 0x00 });
    checkRoundTrip({ 0x123456789abcdef0 });
    checkRoundTrip({ UINTPTR_MAX });
}

void testPGMTraceMaxFrames()
{
    std::mt19937_64 random(1);
    Frames frames = armLikeTrace(random, maxFrames);
    Bytes compressed = checkRoundTrip(frames);
    CHECK_LESS_EQUAL(compressed.size(), PAS_PGM_TRACE_MAX_COMPRESSED_SIZE(maxFrames));

    for (uint32_t capacity : { 1u, 5u, maxFrames - 1 }) {
        Frames output(maxFrames, 0xdead);
        CHECK_EQUAL(pas_pgm_trace_decompress(compressed.data(), compressed.size(), output.data(), capacity), capacity);
        for (uint32_t i = 0; i < maxFrames; ++i)
            CHECK_EQUAL(output[i], i < capacity ? frames[i] : 0xdead);
    }
}

void testPGMTraceTruncatedOutput()
{
    std::mt19937_64 random(2);
    for (const Frames& frames : { armLikeTrace(random, maxFrames), x86LikeTrace(random, maxFrames), fullWidthTrace(random, maxFrames) }) {
        Bytes full = compress(frames);
        size_t previousCount = 0;
        for (size_t bufferSize = 0; bufferSize <= full.size(); ++bufferSize) {
            Bytes truncated = compress(frames, bufferSize);
            CHECK(truncated.empty() || truncated[0] == full[0]);
            Frames decoded = decompress(truncated, maxFrames);
            CHECK_GREATER_EQUAL(decoded.size(), previousCount);
            checkFramesEqual(decoded, prefix(frames, decoded.size()));
            checkBytesEqual(truncated, Bytes(full.begin(), full.begin() + static_cast<ptrdiff_t>(truncated.size())));
            previousCount = decoded.size();
        }
        CHECK_EQUAL(previousCount, frames.size());
    }
}

void testPGMTraceTruncatedInput()
{
    std::mt19937_64 random(3);
    for (const Frames& frames : { armLikeTrace(random, maxFrames), x86LikeTrace(random, maxFrames), fullWidthTrace(random, maxFrames) }) {
        Bytes full = compress(frames);
        size_t previousCount = 0;
        for (size_t length = 0; length <= full.size(); ++length) {
            Frames decoded = decompress(Bytes(full.begin(), full.begin() + static_cast<ptrdiff_t>(length)), maxFrames);
            CHECK_GREATER_EQUAL(decoded.size(), previousCount);
            checkFramesEqual(decoded, prefix(frames, decoded.size()));
            previousCount = decoded.size();
        }
        CHECK_EQUAL(previousCount, frames.size());
    }
}

void testPGMTraceMalformedInput()
{
    CHECK_EQUAL(decompress({ 0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80 }, 1).size(), 0u);
    CHECK_EQUAL(decompress({ 0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00 }, 1).size(), 0u);
    CHECK_EQUAL(decompress({ 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x02 }, 1).size(), 0u);
    checkFramesEqual(decompress({ 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01 }, 1), { static_cast<uintptr_t>(INTPTR_MIN) });
    checkFramesEqual(decompress({ 0x03, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01 }, 1), { 0 });
    checkFramesEqual(decompress({ 0x00, 0x02, 0x80 }, 2), { 1 });
    checkFramesEqual(decompress({ PAS_PGM_TRACE_RAW_HEADER, 1, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0 }, 2), { 1 });

    std::mt19937_64 random(4);
    for (unsigned iteration = 0; iteration < 10000; ++iteration) {
        Bytes garbage(random() % 64);
        for (auto& byte : garbage)
            byte = static_cast<uint8_t>(random());
        if (!garbage.empty() && (iteration & 1))
            garbage[0] = static_cast<uint8_t>(random() % (PAS_PGM_TRACE_MAX_SHIFT + 1));
        decompress(garbage, maxFrames);
    }
}

void testPGMTraceRawFallback()
{
    std::mt19937_64 random(5);
    Frames frames = fullWidthTrace(random, maxFrames);
    Bytes compressed = checkRoundTrip(frames);
    CHECK_EQUAL(static_cast<unsigned>(compressed[0]), PAS_PGM_TRACE_RAW_HEADER);
    CHECK_EQUAL(compressed.size(), PAS_PGM_TRACE_MAX_COMPRESSED_SIZE(maxFrames));
}

void testPGMTraceCompressionRatio()
{
    Frames arm = { 0x1000041a8, 0x100003f2c, 0x100003e90, 0x18a2c1b40, 0x18a2c0f24, 0x18a2be6d0, 0x1000052e8, 0x18a28b154 };
    Bytes compressed = checkRoundTrip(arm);
    CHECK_EQUAL(static_cast<unsigned>(compressed[0]), 2u);
    CHECK_EQUAL(compressed.size(), 28u);

    Frames x86 = { 0x1000041a3, 0x100003f2e, 0x100003e91, 0x7ff8123456f1, 0x7ff812344f25, 0x7ff8123406d6, 0x1000052ea, 0x7ff81230b157 };
    compressed = checkRoundTrip(x86);
    CHECK_EQUAL(static_cast<unsigned>(compressed[0]), 0u);
    CHECK_EQUAL(compressed.size(), 36u);
}

void testPGMTraceRandomRoundTrips()
{
    std::mt19937_64 random(6);
    for (unsigned iteration = 0; iteration < 3000; ++iteration) {
        uint32_t count = 1 + static_cast<uint32_t>(random() % maxFrames);
        Frames frames;
        switch (iteration % 3) {
        case 0:
            frames = armLikeTrace(random, count);
            break;
        case 1:
            frames = x86LikeTrace(random, count);
            break;
        default:
            frames = fullWidthTrace(random, count);
            break;
        }
        Bytes compressed = checkRoundTrip(frames);
        CHECK_LESS_EQUAL(compressed.size(), PAS_PGM_TRACE_MAX_COMPRESSED_SIZE(count));
        if (!(iteration % 3))
            CHECK_GREATER_EQUAL(static_cast<unsigned>(compressed[0]), 2u);
    }
}

void testPGMTraceCollect()
{
    uint8_t buffer[PAS_PGM_TRACE_MAX_COMPRESSED_SIZE(PAS_PGM_BACKTRACE_MAX_FRAMES)];
    CHECK_EQUAL(pas_pgm_trace_collect(nullptr, sizeof(buffer)), 0u);
    CHECK_EQUAL(pas_pgm_trace_collect(buffer, 0), 0u);

#if PAS_OS(DARWIN)
    void* expected[PAS_PGM_BACKTRACE_MAX_FRAMES];
    int expectedCount = backtrace(expected, PAS_PGM_BACKTRACE_MAX_FRAMES);
    size_t size = pas_pgm_trace_collect(buffer, sizeof(buffer));
    CHECK_GREATER(size, 1u);

    uintptr_t frames[PAS_PGM_BACKTRACE_MAX_FRAMES];
    uint32_t count = pas_pgm_trace_decompress(buffer, size, frames, PAS_PGM_BACKTRACE_MAX_FRAMES);
    CHECK_EQUAL(count, static_cast<uint32_t>(expectedCount));
    CHECK_NOT_EQUAL(frames[0], reinterpret_cast<uintptr_t>(expected[0]));
    for (uint32_t i = 1; i < count; ++i)
        CHECK_EQUAL(frames[i], reinterpret_cast<uintptr_t>(expected[i]));
#endif
}

} // anonymous namespace

void addPGMTraceTests()
{
    ADD_TEST(testPGMTraceGoldenEncodings());
    ADD_TEST(testPGMTraceDecoderReadsShiftFromHeader());
    ADD_TEST(testPGMTraceUnalignedGaps());
    ADD_TEST(testPGMTraceNegativeDeltas());
    ADD_TEST(testPGMTraceMixedGaps());
    ADD_TEST(testPGMTraceZeroLength());
    ADD_TEST(testPGMTraceSingleFrame());
    ADD_TEST(testPGMTraceMaxFrames());
    ADD_TEST(testPGMTraceTruncatedOutput());
    ADD_TEST(testPGMTraceTruncatedInput());
    ADD_TEST(testPGMTraceMalformedInput());
    ADD_TEST(testPGMTraceRawFallback());
    ADD_TEST(testPGMTraceCompressionRatio());
    ADD_TEST(testPGMTraceRandomRoundTrips());
    ADD_TEST(testPGMTraceCollect());
}
