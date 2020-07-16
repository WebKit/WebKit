/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include <wtf/text/StringView.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

namespace VPConfigurationLevel {
constexpr uint8_t Level_1   = 10;
constexpr uint8_t Level_1_1 = 11;
constexpr uint8_t Level_2   = 20;
constexpr uint8_t Level_2_1 = 21;
constexpr uint8_t Level_3   = 30;
constexpr uint8_t Level_3_1 = 31;
constexpr uint8_t Level_4   = 40;
constexpr uint8_t Level_4_1 = 41;
constexpr uint8_t Level_5   = 50;
constexpr uint8_t Level_5_1 = 51;
constexpr uint8_t Level_5_2 = 52;
constexpr uint8_t Level_6   = 60;
constexpr uint8_t Level_6_1 = 61;
constexpr uint8_t Level_6_2 = 62;
}

namespace VPConfigurationChromaSubsampling {
constexpr uint8_t Subsampling_420_Vertical = 0;
constexpr uint8_t Subsampling_420_Colocated = 1;
constexpr uint8_t Subsampling_422 = 2;
constexpr uint8_t Subsampling_444 = 3;
}

namespace VPConfigurationRange {
constexpr uint8_t VideoRange = 0;
constexpr uint8_t FullRange = 1;
}

struct VPCodecConfigurationRecord {
    String codecName;
    uint8_t profile { 0 };
    uint8_t level { VPConfigurationLevel::Level_1 };
    uint8_t bitDepth { 8 };
    uint8_t chromaSubsampling { VPConfigurationChromaSubsampling::Subsampling_420_Colocated };
    uint8_t videoFullRangeFlag { VPConfigurationRange::VideoRange };
    uint8_t colorPrimaries { 1 };
    uint8_t transferCharacteristics { 1 };
    uint8_t matrixCoefficients { 1 };
};

WEBCORE_EXPORT Optional<VPCodecConfigurationRecord> parseVPCodecParameters(StringView codecString);

}
