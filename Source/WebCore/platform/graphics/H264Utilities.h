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

#pragma once

#include <optional>
#include <span>
#include <wtf/Forward.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

namespace WebCore {

class BitReader;

// See section 7.4.1 of the H.264 spec.
enum class H264NaluType : uint8_t {
    Slice = 1,
    Idr = 5,
    Sei = 6,
    Sps = 7,
    Pps = 8,
    Aud = 9,
    EndOfSequence = 10,
    EndOfStream = 11,
    Filler = 12,
    Prefix = 14,
    StapA = 24,
    FuA = 28
};

// See table 7-6 of the H.264 spec.
enum class H264SliceType : uint8_t { P = 0, B = 1, I = 2, Sp = 3, Si = 4 };

constexpr size_t h264NaluHeaderSize = 1;

WEBCORE_EXPORT H264NaluType h264NaluType(uint8_t);

// Stateful H.264 Annex B bitstream parser used to recover the QP of the most recently parsed slice.
class WEBCORE_EXPORT H264BitstreamParser {
    WTF_MAKE_TZONE_ALLOCATED(H264BitstreamParser);
public:
    H264BitstreamParser() = default;

    void parseBitstream(std::span<const uint8_t>);
    std::optional<int> lastSliceQP() const;

private:
    struct SpsState {
        bool deltaPicOrderAlwaysZeroFlag { false };
        uint32_t chromaFormatIDC { 1 };
        bool separateColourPlaneFlag { false };
        bool frameMbsOnlyFlag { false };
        uint32_t log2MaxFrameNum { 4 };
        uint32_t log2MaxPicOrderCntLsb { 4 };
        uint32_t picOrderCntType { 0 };
        uint32_t maxNumRefFrames { 0 };
        uint32_t id { 0 };
    };

    struct PpsState {
        bool bottomFieldPicOrderInFramePresentFlag { false };
        bool weightedPredFlag { false };
        bool entropyCodingModeFlag { false };
        uint32_t numRefIdxL0DefaultActiveMinus1 { 0 };
        uint32_t numRefIdxL1DefaultActiveMinus1 { 0 };
        uint32_t weightedBipredIDC { 0 };
        bool redundantPicCntPresentFlag { false };
        int picInitQPMinus26 { 0 };
        uint32_t id { 0 };
        uint32_t spsId { 0 };
    };

    enum class ParseResult { Ok, InvalidStream, UnsupportedStream };

    static std::optional<SpsState> parseSps(std::span<const uint8_t>);
    static std::optional<PpsState> parsePps(std::span<const uint8_t>);

    void parseSlice(std::span<const uint8_t>);
    ParseResult parseNonParameterSetNalu(std::span<const uint8_t>, uint8_t naluType);

    std::optional<SpsState> m_sps;
    std::optional<PpsState> m_pps;
    std::optional<int32_t> m_lastSliceQPDelta;
};

}
