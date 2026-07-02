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
#include "ISOBMFFPreParser.h"

#if ENABLE(MEDIA_SOURCE)

#include "BitReader.h"
#include "SharedBuffer.h"

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ISOBMFFPreParser);

static constexpr auto ftypBox = FourCC(std::span { "ftyp" });
static constexpr auto moovBox = FourCC(std::span { "moov" });
static constexpr auto stypBox = FourCC(std::span { "styp" });
static constexpr auto moofBox = FourCC(std::span { "moof" });

ISOBMFFPreParser::ISOBMFFPreParser(ForwardDataCallback&& callback)
    : m_forwardDataCallback(WTF::move(callback))
{
}

std::optional<ISOBMFFPreParser::BoxHeader> ISOBMFFPreParser::parseBoxHeader(std::span<const uint8_t> data)
{
    if (data.size() < 8)
        return std::nullopt;

    BitReader reader(data);

    auto sizeValue = reader.read<uint32_t>();
    if (!sizeValue)
        return std::nullopt;

    auto typeValue = reader.read<uint32_t>();
    if (!typeValue)
        return std::nullopt;

    BoxHeader header;
    header.type = FourCC(*typeValue);
    header.size = *sizeValue;
    header.headerSize = 8;

    if (*sizeValue == 1) {
        auto extendedSize = reader.read<uint64_t>();
        if (!extendedSize)
            return std::nullopt;
        header.size = *extendedSize;
        header.headerSize = 16;
    } else if (!*sizeValue)
        return std::nullopt;

    if (header.size < header.headerSize)
        return std::nullopt;

    return header;
}

bool ISOBMFFPreParser::isInitSegmentStartBox(FourCC type)
{
    return type == ftypBox;
}

bool ISOBMFFPreParser::isMediaSegmentStartBox(FourCC type)
{
    return type == stypBox || type == moofBox;
}

void ISOBMFFPreParser::reset()
{
    m_state = State::WaitingForSegment;
    m_pendingHeaderBytes.clear();
    m_remainingBytesInCurrentBox = 0;
}

void ISOBMFFPreParser::setPendingInitializationSegmentForChangeType()
{
    m_pendingInitializationSegmentForChangeType = true;
    reset();
}

Expected<void, PlatformMediaError> ISOBMFFPreParser::appendData(
    Ref<const SharedBuffer>&& segment, AppendFlags callerFlags)
{
    if (segment->isEmpty())
        return { };

    // Take ownership of any leftover pending header bytes from the previous
    // call. They form a (≤15 byte) prefix of the conceptual data we walk; we
    // splice across them and the new segment without copying segment's bytes.
    Vector<uint8_t, 16> pendingBytes = WTF::move(m_pendingHeaderBytes);

    auto inputSpan = segment->span();
    auto pendingSize = pendingBytes.size();
    auto totalSize = pendingSize + inputSpan.size();

    auto shouldForward = [&]() {
        return m_state != State::WaitingForSegment || m_firstInitializationSegmentReceived;
    };

    // Read up to 16 contiguous bytes from conceptual offset `offset`. When the
    // request straddles the pending/segment boundary, the bytes are spliced
    // into the caller-supplied staging buffer.
    auto readHeaderBytes = [&](size_t offset, size_t len, std::span<uint8_t> staging) -> std::span<const uint8_t> {
        ASSERT(len <= staging.size());
        if (offset >= pendingSize)
            return inputSpan.subspan(offset - pendingSize, len);
        if (offset + len <= pendingSize)
            return pendingBytes.subspan(offset, len);
        auto pendingPart = pendingSize - offset;
        memcpySpan(staging.first(pendingPart), pendingBytes.subspan(offset, pendingPart));
        memcpySpan(staging.subspan(pendingPart, len - pendingPart), inputSpan.first(len - pendingPart));
        return staging.first(len);
    };

    // Forward the conceptual range [start, end). When it straddles the
    // pending/segment boundary, send two SharedBuffers — the pending side as
    // a fresh small buffer (≤15 bytes), the segment side as a zero-copy view.
    auto forward = [&](size_t start, size_t end, AppendFlags flags) {
        ASSERT(start < end);
        if (start >= pendingSize) {
            m_forwardDataCallback(segment->getContiguousData(start - pendingSize, end - start), flags);
            return;
        }
        if (end <= pendingSize) {
            m_forwardDataCallback(SharedBuffer::create(pendingBytes.subspan(start, end - start)), flags);
            return;
        }
        auto pendingPart = pendingSize - start;
        m_forwardDataCallback(SharedBuffer::create(pendingBytes.subspan(start, pendingPart)), flags);
        m_forwardDataCallback(segment->getContiguousData(0, end - pendingSize), flags);
    };

    // Stash conceptual bytes [offset, totalSize) as the new pending header bytes.
    auto stashRemaining = [&](size_t offset) {
        ASSERT(totalSize - offset < 16);
        if (offset < pendingSize) {
            m_pendingHeaderBytes.append(pendingBytes.subspan(offset));
            m_pendingHeaderBytes.append(inputSpan);
        } else
            m_pendingHeaderBytes.append(inputSpan.subspan(offset - pendingSize));
    };

    AppendFlags nextChunkFlags = callerFlags;
    size_t offset = 0;
    size_t forwardStart = 0;

    while (offset < totalSize) {
        if (m_remainingBytesInCurrentBox > 0) {
            auto toConsume = std::min<uint64_t>(totalSize - offset, m_remainingBytesInCurrentBox);
            m_remainingBytesInCurrentBox -= toConsume;
            offset += static_cast<size_t>(toConsume);
            continue;
        }

        auto available = totalSize - offset;

        if (available < 8) {
            if (shouldForward() && offset > forwardStart)
                forward(forwardStart, offset, nextChunkFlags);
            stashRemaining(offset);
            return { };
        }

        Vector<uint8_t, 16> staging(16);
        auto headerBytes = readHeaderBytes(offset, 8, staging.mutableSpan());
        uint32_t sizeField = (static_cast<uint32_t>(headerBytes[0]) << 24)
            | (static_cast<uint32_t>(headerBytes[1]) << 16)
            | (static_cast<uint32_t>(headerBytes[2]) << 8)
            | static_cast<uint32_t>(headerBytes[3]);
        if (sizeField == 1 && available < 16) {
            if (shouldForward() && offset > forwardStart)
                forward(forwardStart, offset, nextChunkFlags);
            stashRemaining(offset);
            return { };
        }
        if (sizeField == 1)
            headerBytes = readHeaderBytes(offset, 16, staging.mutableSpan());

        auto header = parseBoxHeader(headerBytes);
        if (!header)
            return makeUnexpected(PlatformMediaError::ParsingError);

        auto boxType = header->type;
        auto boxBodySize = header->size - header->headerSize;

        switch (m_state) {
        case State::WaitingForSegment:
            if (isInitSegmentStartBox(boxType)) {
                if (m_firstInitializationSegmentReceived) {
                    if (offset > forwardStart)
                        forward(forwardStart, offset, nextChunkFlags);
                    nextChunkFlags = AppendFlags::Discontinuity;
                }
                forwardStart = offset;
                m_state = State::ParsingInitSegment;
                break;
            }
            if (isMediaSegmentStartBox(boxType)) {
                if (!m_firstInitializationSegmentReceived || m_pendingInitializationSegmentForChangeType)
                    return makeUnexpected(PlatformMediaError::ParsingError);
                m_state = State::ParsingMediaSegment;
            }
            break;

        case State::ParsingInitSegment:
            if (boxType == moovBox) {
                m_firstInitializationSegmentReceived = true;
                m_pendingInitializationSegmentForChangeType = false;
                m_state = State::WaitingForSegment;
                break;
            }
            if (isMediaSegmentStartBox(boxType))
                return makeUnexpected(PlatformMediaError::ParsingError);
            break;

        case State::ParsingMediaSegment:
            if (isInitSegmentStartBox(boxType)) {
                m_state = State::WaitingForSegment;
                continue;
            }
            break;
        }

        m_remainingBytesInCurrentBox = boxBodySize;
        offset += header->headerSize;
    }

    if (shouldForward() && forwardStart < totalSize)
        forward(forwardStart, totalSize, nextChunkFlags);

    return { };
}

} // namespace WebCore

#endif // ENABLE(MEDIA_SOURCE)
