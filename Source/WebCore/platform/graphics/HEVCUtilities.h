/*
 * Copyright (C) 2018-2021 Apple Inc. All rights reserved.
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

#include <array>
#include <optional>
#include <span>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/HashTraits.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

namespace WebCore {

class BitReader;
class SharedBuffer;
struct FourCC;

struct AVCParameters {
    uint8_t profileIDC { 0 };
    uint8_t constraintsFlags { 0 };
    uint8_t levelIDC { 0 };
};

WEBCORE_EXPORT std::optional<AVCParameters> parseAVCCodecParameters(StringView);
WEBCORE_EXPORT std::optional<AVCParameters> parseAVCDecoderConfigurationRecord(const SharedBuffer&);
WEBCORE_EXPORT String createAVCCodecParametersString(const AVCParameters&);

struct HEVCParameters {
    enum class Codec { Hev1, Hvc1 } codec { Codec::Hvc1 };
    uint16_t generalProfileSpace { 0 };
    uint16_t generalProfileIDC { 0 };
    uint32_t generalProfileCompatibilityFlags { 0 };
    uint8_t generalTierFlag { 0 };
    Vector<unsigned char, 6> generalConstraintIndicatorFlags { 0, 0, 0, 0, 0, 0 };
    uint16_t generalLevelIDC { 0 };
};

WEBCORE_EXPORT std::optional<HEVCParameters> parseHEVCCodecParameters(StringView);
WEBCORE_EXPORT std::optional<HEVCParameters> parseHEVCDecoderConfigurationRecord(FourCC, const SharedBuffer&);
WEBCORE_EXPORT String createHEVCCodecParametersString(const HEVCParameters&);

struct DoViParameters {
    enum class Codec { AVC1, AVC3, HEV1, HVC1 } codec { Codec::HVC1 };
    uint16_t bitstreamProfileID { 0 };
    uint16_t bitstreamLevelID { 0 };
};

WEBCORE_EXPORT std::optional<DoViParameters> parseDoViCodecParameters(StringView);
WEBCORE_EXPORT std::optional<DoViParameters> parseDoViDecoderConfigurationRecord(const SharedBuffer&);
WEBCORE_EXPORT String createDoViCodecParametersString(const DoViParameters&);

// Type values 0-40 are defined in Table 7-1, 48-49 in RFC 7798 sections 4.4.2/4.4.3.
enum class HEVCNaluType : uint8_t {
    TrailN = 0,
    TrailR = 1,
    TsaN = 2,
    TsaR = 3,
    StsaN = 4,
    StsaR = 5,
    RadlN = 6,
    RadlR = 7,
    BlaWLp = 16,
    BlaWRadl = 17,
    BlaNLp = 18,
    IdrWRadl = 19,
    IdrNLp = 20,
    Cra = 21,
    RsvIrapVcl23 = 23,
    RsvVcl31 = 31,
    Vps = 32,
    Sps = 33,
    Pps = 34,
    Aud = 35,
    PrefixSei = 39,
    SuffixSei = 40,
    Ap = 48,
    Fu = 49,
    Paci = 50
};

// See table 7-7 of the H.265 spec.
enum class HEVCSliceType : uint8_t { B = 0, P = 1, I = 2 };

constexpr size_t hevcNaluHeaderSize = 2;

struct HEVCNaluIndex {
    size_t startOffset { 0 };
    size_t payloadStartOffset { 0 };
    size_t payloadSize { 0 };
};

struct HEVCAnnexBNaluIndices {
    Vector<HEVCNaluIndex> indices;
    std::optional<size_t> vpsIndex;
};

WEBCORE_EXPORT HEVCAnnexBNaluIndices findHEVCNaluIndices(std::span<const uint8_t>);
WEBCORE_EXPORT HEVCNaluType hevcNaluType(uint8_t);

// Removes emulation prevention bytes (the trailing byte of any 0x00 0x00 0x03 sequence).
WEBCORE_EXPORT Vector<uint8_t> parseRbsp(std::span<const uint8_t>);

// Scans an Annex B chunk for a VPS NAL unit to return its vps_max_num_reorder_pics or std::nullopt if no VPS or error.
WEBCORE_EXPORT std::optional<uint8_t> findHEVCAnnexBMaxNumReorderPics(std::span<const uint8_t>, const HEVCAnnexBNaluIndices&);

struct HVCCParameterSet {
    HEVCNaluType type;
    std::span<const uint8_t> data;
};

struct HVCCParameterSets {
    Vector<HVCCParameterSet> paramSets;
    size_t lengthFieldSize { 0 };
};

// Parses an "hvcC" box (ISO/IEC 14496-15) into its length field size and flattened list of
// parameter set NAL units, in the order they appear in the box.
WEBCORE_EXPORT std::optional<HVCCParameterSets> parseHVCCParameterSets(std::span<const uint8_t>);

// Scans a parsed "hvcC" box for a VPS NAL unit to return its vps_max_num_reorder_pics or std::nullopt if no VPS or error.
WEBCORE_EXPORT std::optional<uint8_t> findHVCCMaxNumReorderPics(const HVCCParameterSets&);

// Stateful H.265 Annex B bitstream parser used to recover the QP of the most recently parsed slice.
class WEBCORE_EXPORT HEVCBitstreamParser {
    WTF_MAKE_TZONE_ALLOCATED(HEVCBitstreamParser);
public:
    HEVCBitstreamParser() = default;

    void parseBitstream(std::span<const uint8_t>);
    std::optional<int> lastSliceQP() const;

    static std::optional<uint8_t> parseVpsMaxNumReorderPics(std::span<const uint8_t>);

private:
    struct ProfileTierLevel {
        int generalProfileIDC { 0 };
        int generalLevelIDC { 0 };
    };

    struct ShortTermRefPicSet {
        uint32_t numNegativePics { 0 };
        uint32_t numPositivePics { 0 };
        std::array<int32_t, 64> deltaPocS0 { };
        std::array<bool, 64> usedByCurrPicS0 { };
        std::array<int32_t, 64> deltaPocS1 { };
        std::array<bool, 64> usedByCurrPicS1 { };
        uint32_t numDeltaPocs { 0 };
    };

    struct SpsState {
        uint32_t spsMaxSubLayersMinus1 { 0 };
        uint32_t chromaFormatIDC { 0 };
        bool separateColourPlaneFlag { false };
        uint32_t picWidthInLumaSamples { 0 };
        uint32_t picHeightInLumaSamples { 0 };
        uint32_t log2MaxPicOrderCntLsbMinus4 { 0 };
        std::array<uint32_t, 7> spsMaxDecPicBufferingMinus1 { };
        uint32_t log2MinLumaCodingBlockSizeMinus3 { 0 };
        uint32_t log2DiffMaxMinLumaCodingBlockSize { 0 };
        bool sampleAdaptiveOffsetEnabledFlag { false };
        uint32_t numShortTermRefPicSets { 0 };
        Vector<ShortTermRefPicSet> shortTermRefPicSet;
        bool longTermRefPicsPresentFlag { false };
        uint32_t numLongTermRefPicsSps { 0 };
        Vector<bool> usedByCurrPicLtSpsFlag;
        bool spsTemporalMvpEnabledFlag { false };
        uint16_t spsId { 0 };
        uint32_t picWidthInCtbsY { 0 };
        uint32_t picHeightInCtbsY { 0 };
        uint32_t bitDepthLumaMinus8 { 0 };
    };

    struct PpsState {
        bool dependentSliceSegmentsEnabledFlag { false };
        bool cabacInitPresentFlag { false };
        bool outputFlagPresentFlag { false };
        uint32_t numExtraSliceHeaderBits { 0 };
        uint32_t numRefIdxL0DefaultActiveMinus1 { 0 };
        uint32_t numRefIdxL1DefaultActiveMinus1 { 0 };
        int initQPMinus26 { 0 };
        bool weightedPredFlag { false };
        bool weightedBipredFlag { false };
        bool listsModificationPresentFlag { false };
        uint16_t ppsId { 0 };
        uint16_t spsId { 0 };
        int qpBdOffsetY { 0 };
    };

    enum class ParseResult { Ok, InvalidStream, UnsupportedStream };

    static std::optional<ProfileTierLevel> parseProfileTierLevel(bool profilePresent, uint32_t maxNumSubLayersMinus1, BitReader&);
    static bool parseScalingListData(BitReader&);
    static std::optional<ShortTermRefPicSet> parseShortTermRefPicSet(uint32_t stRpsIndex, uint32_t numShortTermRefPicSets, const Vector<ShortTermRefPicSet>&, uint32_t spsMaxDecPicBufferingMinus1, BitReader&);
    static std::optional<SpsState> parseSps(std::span<const uint8_t>);
    static std::optional<PpsState> parsePps(std::span<const uint8_t>, const SpsState*);

    void parseSlice(std::span<const uint8_t>);
    ParseResult parseNonParameterSetNalu(std::span<const uint8_t>, uint8_t naluType);
    const PpsState* pps(uint16_t ppsId) const;
    const SpsState* sps(uint16_t spsId) const;

    HashMap<uint32_t, SpsState, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>> m_sps;
    HashMap<uint32_t, PpsState, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>> m_pps;
    std::optional<int32_t> m_lastSliceQPDelta;
    std::optional<uint16_t> m_lastSlicePpsId;
};

}
