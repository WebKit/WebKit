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
#include "ISOBMFFTrackInfoParser.h"

#if ENABLE(MEDIA_SOURCE)

#include "BitReader.h"
#include "FourCC.h"

namespace WebCore {

static constexpr auto mvhdBox = FourCC(std::span { "mvhd" });
static constexpr auto trakBox = FourCC(std::span { "trak" });
static constexpr auto tkhdBox = FourCC(std::span { "tkhd" });
static constexpr auto edtsBox = FourCC(std::span { "edts" });
static constexpr auto elstBox = FourCC(std::span { "elst" });

struct ChildBox {
    FourCC type;
    std::span<const uint8_t> body;
};

// Reads the next box header from `data` starting at offset 0 and returns the
// child's type and body span (following the 8 or 16 byte header). On success,
// `consumed` is set to the total box size (header + body). Bound-checked.
static std::optional<ChildBox> readNextChildBox(std::span<const uint8_t> data, size_t& consumed)
{
    if (data.size() < 8)
        return std::nullopt;

    BitReader reader(data);
    auto sizeValue = reader.read<uint32_t>();
    auto typeValue = reader.read<uint32_t>();
    if (!sizeValue || !typeValue)
        return std::nullopt;

    uint64_t totalSize = *sizeValue;
    size_t headerSize = 8;
    if (*sizeValue == 1) {
        if (data.size() < 16)
            return std::nullopt;
        auto extendedSize = reader.read<uint64_t>();
        if (!extendedSize)
            return std::nullopt;
        totalSize = *extendedSize;
        headerSize = 16;
    } else if (!*sizeValue) {
        // size == 0 means "extends to end of file"; in a buffered moov body
        // we treat this as "rest of container".
        totalSize = data.size();
    }

    if (totalSize < headerSize || totalSize > data.size())
        return std::nullopt;

    consumed = static_cast<size_t>(totalSize);
    return ChildBox { FourCC(*typeValue), data.subspan(headerSize, totalSize - headerSize) };
}

// Iterate every direct child box of `containerBody`, calling `fn(child)` for each.
// Stops on malformation.
template <typename F>
static void forEachChildBox(std::span<const uint8_t> containerBody, F&& fn)
{
    size_t offset = 0;
    while (offset < containerBody.size()) {
        size_t consumed = 0;
        auto child = readNextChildBox(containerBody.subspan(offset), consumed);
        if (!child || !consumed)
            return;
        fn(*child);
        offset += consumed;
    }
}

// FullBox: u8 version + u24 flags. Returns version on success, empty on truncation.
static std::optional<uint8_t> readFullBoxVersion(std::span<const uint8_t> body)
{
    if (body.size() < 4)
        return std::nullopt;
    return body[0];
}

// mvhd (FullBox): timescale at body offset 12 (v0) or 20 (v1), uint32 big-endian.
static std::optional<uint32_t> readMvhdTimescale(std::span<const uint8_t> body)
{
    auto version = readFullBoxVersion(body);
    if (!version)
        return std::nullopt;
    size_t timescaleOffset = (*version == 1) ? 20 : 12;
    if (body.size() < timescaleOffset + 4)
        return std::nullopt;
    BitReader reader(body.subspan(timescaleOffset));
    return reader.read<uint32_t>();
}

// tkhd (FullBox): track_ID at body offset 12 (v0) or 20 (v1), uint32 big-endian.
static std::optional<uint32_t> readTkhdTrackID(std::span<const uint8_t> body)
{
    auto version = readFullBoxVersion(body);
    if (!version)
        return std::nullopt;
    size_t trackIDOffset = (*version == 1) ? 20 : 12;
    if (body.size() < trackIDOffset + 4)
        return std::nullopt;
    BitReader reader(body.subspan(trackIDOffset));
    return reader.read<uint32_t>();
}

// elst (FullBox): looks at entry 0 only. If it's an empty edit
// (media_time == -1), returns segment_duration; otherwise empty.
static std::optional<uint64_t> readElstEntry0EmptyEditDuration(std::span<const uint8_t> body)
{
    auto version = readFullBoxVersion(body);
    if (!version)
        return std::nullopt;

    if (body.size() < 8)
        return std::nullopt;

    BitReader reader(body.subspan(4));
    auto entryCount = reader.read<uint32_t>();
    if (!entryCount || !*entryCount)
        return std::nullopt;

    if (*version == 1) {
        if (body.size() < 8 + 16)
            return std::nullopt;
        auto segmentDuration = reader.read<uint64_t>();
        auto mediaTime = reader.read<uint64_t>();
        if (!segmentDuration || !mediaTime)
            return std::nullopt;
        if (static_cast<int64_t>(*mediaTime) != -1)
            return std::nullopt;
        return *segmentDuration;
    }

    if (body.size() < 8 + 8)
        return std::nullopt;
    auto segmentDuration = reader.read<uint32_t>();
    auto mediaTime = reader.read<uint32_t>();
    if (!segmentDuration || !mediaTime)
        return std::nullopt;
    if (static_cast<int32_t>(*mediaTime) != -1)
        return std::nullopt;
    return static_cast<uint64_t>(*segmentDuration);
}

static std::optional<uint64_t> findEmptyEditDurationInEdts(std::span<const uint8_t> edtsBody)
{
    std::optional<uint64_t> result;
    forEachChildBox(edtsBody, [&](const ChildBox& child) {
        if (child.type == elstBox && !result)
            result = readElstEntry0EmptyEditDuration(child.body);
    });
    return result;
}

static std::optional<ISOBMFFTrackInfoParser::TrackEditInfo> parseTrak(std::span<const uint8_t> trakBody, uint32_t movieTimescale)
{
    std::optional<uint32_t> trackID;
    std::optional<uint64_t> emptyEditDuration;

    forEachChildBox(trakBody, [&](const ChildBox& child) {
        if (child.type == tkhdBox)
            trackID = readTkhdTrackID(child.body);
        else if (child.type == edtsBox)
            emptyEditDuration = findEmptyEditDurationInEdts(child.body);
    });

    if (!trackID || !*trackID || !emptyEditDuration)
        return std::nullopt;

    if (*emptyEditDuration > INT64_MAX)
        return std::nullopt;

    return ISOBMFFTrackInfoParser::TrackEditInfo {
        *trackID,
        MediaTime(static_cast<int64_t>(*emptyEditDuration), movieTimescale),
    };
}

Vector<ISOBMFFTrackInfoParser::TrackEditInfo> ISOBMFFTrackInfoParser::parseMoovBody(std::span<const uint8_t> moovBody)
{
    uint32_t movieTimescale = 0;
    Vector<TrackEditInfo> result;

    // First pass: locate mvhd to learn the movie timescale.
    forEachChildBox(moovBody, [&](const ChildBox& child) {
        if (child.type == mvhdBox && !movieTimescale) {
            if (auto timescale = readMvhdTimescale(child.body); timescale && *timescale)
                movieTimescale = *timescale;
        }
    });

    if (!movieTimescale)
        return result;

    // Second pass: collect per-trak empty edits in movie timescale.
    forEachChildBox(moovBody, [&](const ChildBox& child) {
        if (child.type != trakBox)
            return;
        if (auto info = parseTrak(child.body, movieTimescale))
            result.append(*info);
    });

    return result;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_SOURCE)
