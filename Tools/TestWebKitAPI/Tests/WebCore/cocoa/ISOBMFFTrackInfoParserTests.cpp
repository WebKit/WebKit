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

#if PLATFORM(COCOA) && ENABLE(MEDIA_SOURCE)

#include "Test.h"
#include <WebCore/ISOBMFFTrackInfoParser.h>
#include <wtf/Vector.h>

namespace TestWebKitAPI {

using WebCore::ISOBMFFTrackInfoParser;

// Minimal big-endian ISO-BMFF box builders, just enough to exercise the moov
// walker. Every box is: u32 size (header + body) followed by 4 type bytes.

static void appendBE32(Vector<uint8_t>& out, uint32_t value)
{
    out.append(static_cast<uint8_t>(value >> 24));
    out.append(static_cast<uint8_t>(value >> 16));
    out.append(static_cast<uint8_t>(value >> 8));
    out.append(static_cast<uint8_t>(value));
}

static void appendBE64(Vector<uint8_t>& out, uint64_t value)
{
    appendBE32(out, static_cast<uint32_t>(value >> 32));
    appendBE32(out, static_cast<uint32_t>(value));
}

static Vector<uint8_t> makeBox(const char (&type)[5], const Vector<uint8_t>& body)
{
    Vector<uint8_t> out;
    appendBE32(out, static_cast<uint32_t>(8 + body.size()));
    out.append(static_cast<uint8_t>(type[0]));
    out.append(static_cast<uint8_t>(type[1]));
    out.append(static_cast<uint8_t>(type[2]));
    out.append(static_cast<uint8_t>(type[3]));
    out.appendVector(body);
    return out;
}

// mvhd (v0): FullBox(4) + creation(4) + modification(4) + timescale(4).
static Vector<uint8_t> makeMvhd(uint32_t timescale)
{
    Vector<uint8_t> body;
    appendBE32(body, 0); // version 0, flags 0
    appendBE32(body, 0); // creation_time
    appendBE32(body, 0); // modification_time
    appendBE32(body, timescale);
    return makeBox("mvhd", body);
}

// tkhd (v0): FullBox(4) + creation(4) + modification(4) + track_ID(4).
static Vector<uint8_t> makeTkhd(uint32_t trackID)
{
    Vector<uint8_t> body;
    appendBE32(body, 0); // version 0, flags 0
    appendBE32(body, 0); // creation_time
    appendBE32(body, 0); // modification_time
    appendBE32(body, trackID);
    return makeBox("tkhd", body);
}

// elst (v0): FullBox(4) + entry_count(4) + segment_duration(4) + media_time(4) + media_rate(4).
static Vector<uint8_t> makeElstV0(uint32_t segmentDuration, int32_t mediaTime)
{
    Vector<uint8_t> body;
    appendBE32(body, 0); // version 0, flags 0
    appendBE32(body, 1); // entry_count
    appendBE32(body, segmentDuration);
    appendBE32(body, static_cast<uint32_t>(mediaTime));
    appendBE32(body, 0x00010000); // media_rate (unread, present for realism)
    return makeBox("elst", body);
}

// elst (v1): FullBox(4) + entry_count(4) + segment_duration(8) + media_time(8) + media_rate(4).
static Vector<uint8_t> makeElstV1(uint64_t segmentDuration, int64_t mediaTime)
{
    Vector<uint8_t> body;
    appendBE32(body, 0x01000000); // version 1, flags 0
    appendBE32(body, 1); // entry_count
    appendBE64(body, segmentDuration);
    appendBE64(body, static_cast<uint64_t>(mediaTime));
    appendBE32(body, 0x00010000); // media_rate
    return makeBox("elst", body);
}

static Vector<uint8_t> makeEdts(const Vector<uint8_t>& elst)
{
    return makeBox("edts", elst);
}

// trak with a tkhd and an optional edts.
static Vector<uint8_t> makeTrak(uint32_t trackID, const Vector<uint8_t>* edts)
{
    Vector<uint8_t> body;
    body.appendVector(makeTkhd(trackID));
    if (edts)
        body.appendVector(*edts);
    return makeBox("trak", body);
}

TEST(ISOBMFFTrackInfoParser, EmptyInputReturnsNothing)
{
    auto result = ISOBMFFTrackInfoParser::parseMoovBody({ });
    EXPECT_TRUE(result.isEmpty());
}

TEST(ISOBMFFTrackInfoParser, SingleTrackEmptyEditVersion0)
{
    Vector<uint8_t> moov;
    moov.appendVector(makeMvhd(600));
    auto edts = makeEdts(makeElstV0(300, -1));
    moov.appendVector(makeTrak(1, &edts));

    auto result = ISOBMFFTrackInfoParser::parseMoovBody(moov.span());
    ASSERT_EQ(1U, result.size());
    EXPECT_EQ(1U, result[0].trackID);
    EXPECT_TRUE(result[0].emptyEditOffset.isValid());
    EXPECT_EQ(MediaTime(300, 600), result[0].emptyEditOffset);
}

TEST(ISOBMFFTrackInfoParser, SingleTrackEmptyEditVersion1)
{
    Vector<uint8_t> moov;
    moov.appendVector(makeMvhd(48000));
    auto edts = makeEdts(makeElstV1(24000, -1));
    moov.appendVector(makeTrak(7, &edts));

    auto result = ISOBMFFTrackInfoParser::parseMoovBody(moov.span());
    ASSERT_EQ(1U, result.size());
    EXPECT_EQ(7U, result[0].trackID);
    EXPECT_EQ(MediaTime(24000, 48000), result[0].emptyEditOffset);
}

TEST(ISOBMFFTrackInfoParser, NonEmptyEditIsIgnored)
{
    // media_time != -1 is a real (non-empty) edit; the track is omitted entirely.
    Vector<uint8_t> moov;
    moov.appendVector(makeMvhd(600));
    auto edts = makeEdts(makeElstV0(300, 0));
    moov.appendVector(makeTrak(1, &edts));

    auto result = ISOBMFFTrackInfoParser::parseMoovBody(moov.span());
    EXPECT_TRUE(result.isEmpty());
}

TEST(ISOBMFFTrackInfoParser, TrackWithoutEditListIsOmitted)
{
    // Only the track carrying an empty edit is reported; a track with no edts
    // never appears in the results.
    Vector<uint8_t> moov;
    moov.appendVector(makeMvhd(600));
    auto edts = makeEdts(makeElstV0(150, -1));
    moov.appendVector(makeTrak(1, &edts));
    moov.appendVector(makeTrak(2, nullptr));

    auto result = ISOBMFFTrackInfoParser::parseMoovBody(moov.span());
    ASSERT_EQ(1U, result.size());
    EXPECT_EQ(1U, result[0].trackID);
    EXPECT_EQ(MediaTime(150, 600), result[0].emptyEditOffset);
}

TEST(ISOBMFFTrackInfoParser, TrackIDZeroIsRejected)
{
    Vector<uint8_t> moov;
    moov.appendVector(makeMvhd(600));
    auto edts = makeEdts(makeElstV0(300, -1));
    moov.appendVector(makeTrak(0, &edts));

    auto result = ISOBMFFTrackInfoParser::parseMoovBody(moov.span());
    EXPECT_TRUE(result.isEmpty());
}

TEST(ISOBMFFTrackInfoParser, MissingMvhdReturnsNothing)
{
    // Without a movie timescale there is nothing to express the offset in.
    Vector<uint8_t> moov;
    auto edts = makeEdts(makeElstV0(300, -1));
    moov.appendVector(makeTrak(1, &edts));

    auto result = ISOBMFFTrackInfoParser::parseMoovBody(moov.span());
    EXPECT_TRUE(result.isEmpty());
}

TEST(ISOBMFFTrackInfoParser, OversizedEmptyEditDurationIsRejected)
{
    // segment_duration greater than INT64_MAX cannot be represented as a
    // signed MediaTime time value and must be dropped rather than wrapped.
    Vector<uint8_t> moov;
    moov.appendVector(makeMvhd(600));
    auto edts = makeEdts(makeElstV1(0x8000000000000000ULL, -1));
    moov.appendVector(makeTrak(1, &edts));

    auto result = ISOBMFFTrackInfoParser::parseMoovBody(moov.span());
    EXPECT_TRUE(result.isEmpty());
}

TEST(ISOBMFFTrackInfoParser, TruncatedChildBoxDoesNotCrash)
{
    // A well-formed mvhd followed by a box whose declared size overruns the
    // buffer: the walker must stop cleanly instead of reading out of bounds.
    Vector<uint8_t> moov;
    moov.appendVector(makeMvhd(600));
    appendBE32(moov, 0xFFFFFFFF); // size claims far more than is present
    moov.append('t');
    moov.append('r');
    moov.append('a');
    moov.append('k');

    auto result = ISOBMFFTrackInfoParser::parseMoovBody(moov.span());
    EXPECT_TRUE(result.isEmpty());
}

TEST(ISOBMFFTrackInfoParser, MultipleEmptyEditTracks)
{
    Vector<uint8_t> moov;
    moov.appendVector(makeMvhd(1000));
    auto edtsA = makeEdts(makeElstV0(100, -1));
    auto edtsB = makeEdts(makeElstV0(250, -1));
    moov.appendVector(makeTrak(1, &edtsA));
    moov.appendVector(makeTrak(2, &edtsB));

    auto result = ISOBMFFTrackInfoParser::parseMoovBody(moov.span());
    ASSERT_EQ(2U, result.size());
    EXPECT_EQ(1U, result[0].trackID);
    EXPECT_EQ(MediaTime(100, 1000), result[0].emptyEditOffset);
    EXPECT_EQ(2U, result[1].trackID);
    EXPECT_EQ(MediaTime(250, 1000), result[1].emptyEditOffset);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(COCOA) && ENABLE(MEDIA_SOURCE)
