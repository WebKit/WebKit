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

#import "config.h"
#import "GPUVideoEncoderVTBH264.h"

#if USE(AVFOUNDATION)

#import <WebCore/H264UtilitiesCocoa.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/text/StringToIntegerConversion.h>
#import <wtf/text/WTFString.h>

#import <pal/cf/VideoToolboxSoftLink.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(GPUVideoEncoderVTBH264);

// Ported from webrtc::H264Profile/H264Level (api/video_codecs/h264_profile_level_id.h).
enum class H264Profile {
    ConstrainedBaseline,
    Baseline,
    Main,
    ConstrainedHigh,
    High,
    PredictiveHigh444,
};

enum class H264Level : uint8_t {
    Level1_b = 0,
    Level1 = 10,
    Level1_1 = 11,
    Level1_2 = 12,
    Level1_3 = 13,
    Level2 = 20,
    Level2_1 = 21,
    Level2_2 = 22,
    Level3 = 30,
    Level3_1 = 31,
    Level3_2 = 32,
    Level4 = 40,
    Level4_1 = 41,
    Level4_2 = 42,
    Level5 = 50,
    Level5_1 = 51,
    Level5_2 = 52
};

struct H264ProfileLevelId {
    H264Profile profile;
    H264Level level;
};

namespace {

// Bit pattern matcher for profile_iop, e.g. "x1xx0000" where 'x' matches either 0 or 1.
// Ported from libwebrtc api/video_codecs/h264_profile_level_id.cc.
class BitPattern {
public:
    constexpr BitPattern(const char (&str)[9])
        : m_mask(~byteMaskString('x', str))
        , m_maskedValue(byteMaskString('1', str))
    {
    }

    bool isMatch(uint8_t value) const { return m_maskedValue == (value & m_mask); }

private:
    static constexpr uint8_t byteMaskString(char c, const char (&str)[9])
    {
        return (str[0] == c) << 7 | (str[1] == c) << 6 | (str[2] == c) << 5 | (str[3] == c) << 4
            | (str[4] == c) << 3 | (str[5] == c) << 2 | (str[6] == c) << 1 | (str[7] == c) << 0;
    }

    uint8_t m_mask;
    uint8_t m_maskedValue;
};

struct ProfilePattern {
    uint8_t profileIDC;
    BitPattern profileIOP;
    H264Profile profile;
};

// This is from https://tools.ietf.org/html/rfc6184#section-8.1.
constexpr ProfilePattern profilePatterns[] = {
    { 0x42, BitPattern("x1xx0000"), H264Profile::ConstrainedBaseline },
    { 0x4D, BitPattern("1xxx0000"), H264Profile::ConstrainedBaseline },
    { 0x58, BitPattern("11xx0000"), H264Profile::ConstrainedBaseline },
    { 0x42, BitPattern("x0xx0000"), H264Profile::Baseline },
    { 0x58, BitPattern("10xx0000"), H264Profile::Baseline },
    { 0x4D, BitPattern("0x0x0000"), H264Profile::Main },
    { 0x64, BitPattern("00000000"), H264Profile::High },
    { 0x64, BitPattern("00001100"), H264Profile::ConstrainedHigh },
    { 0xF4, BitPattern("00000000"), H264Profile::PredictiveHigh444 },
};

// For level_idc=11 and profile_idc=0x42, 0x4D, or 0x58, the constraint set3 flag specifies if level 1b or level 1.1 is used.
constexpr uint8_t constraintSet3Flag = 0x10;

}

// Ported from libwebrtc api/video_codecs/h264_profile_level_id.cc.
static std::optional<H264ProfileLevelId> parseH264ProfileLevelId(StringView str)
{
    auto profileLevelIdNumeric = parseInteger<uint32_t>(str, 16);
    if (!profileLevelIdNumeric || str.length() != 6 || !*profileLevelIdNumeric)
        return { };

    uint8_t levelIDC = *profileLevelIdNumeric & 0xFF;
    uint8_t profileIOP = (*profileLevelIdNumeric >> 8) & 0xFF;
    uint8_t profileIDC = (*profileLevelIdNumeric >> 16) & 0xFF;

    H264Level level;
    switch (static_cast<H264Level>(levelIDC)) {
    case H264Level::Level1_1:
        level = (profileIOP & constraintSet3Flag) ? H264Level::Level1_b : H264Level::Level1_1;
        break;
    case H264Level::Level1:
    case H264Level::Level1_2:
    case H264Level::Level1_3:
    case H264Level::Level2:
    case H264Level::Level2_1:
    case H264Level::Level2_2:
    case H264Level::Level3:
    case H264Level::Level3_1:
    case H264Level::Level3_2:
    case H264Level::Level4:
    case H264Level::Level4_1:
    case H264Level::Level4_2:
    case H264Level::Level5:
    case H264Level::Level5_1:
    case H264Level::Level5_2:
        level = static_cast<H264Level>(levelIDC);
        break;
    default:
        return { };
    }

    for (auto& pattern : profilePatterns) {
        if (profileIDC == pattern.profileIDC && pattern.profileIOP.isMatch(profileIOP))
            return H264ProfileLevelId { pattern.profile, level };
    }

    return { };
}

static CFStringRef extractVTProfileLevel(const H264ProfileLevelId& profileLevelId)
{
    switch (profileLevelId.profile) {
    case H264Profile::ConstrainedBaseline:
    case H264Profile::Baseline:
        switch (profileLevelId.level) {
        case H264Level::Level3:
            return PAL::kVTProfileLevel_H264_Baseline_3_0;
        case H264Level::Level3_1:
            return PAL::kVTProfileLevel_H264_Baseline_3_1;
        case H264Level::Level3_2:
            return PAL::kVTProfileLevel_H264_Baseline_3_2;
        case H264Level::Level4:
            return PAL::kVTProfileLevel_H264_Baseline_4_0;
        case H264Level::Level4_1:
            return PAL::kVTProfileLevel_H264_Baseline_4_1;
        case H264Level::Level4_2:
            return PAL::kVTProfileLevel_H264_Baseline_4_2;
        case H264Level::Level5:
            return PAL::kVTProfileLevel_H264_Baseline_5_0;
        case H264Level::Level5_1:
            return PAL::kVTProfileLevel_H264_Baseline_5_1;
        case H264Level::Level5_2:
            return PAL::kVTProfileLevel_H264_Baseline_5_2;
        default:
            return PAL::kVTProfileLevel_H264_Baseline_AutoLevel;
        }
    case H264Profile::Main:
        switch (profileLevelId.level) {
        case H264Level::Level3:
            return PAL::kVTProfileLevel_H264_Main_3_0;
        case H264Level::Level3_1:
            return PAL::kVTProfileLevel_H264_Main_3_1;
        case H264Level::Level3_2:
            return PAL::kVTProfileLevel_H264_Main_3_2;
        case H264Level::Level4:
            return PAL::kVTProfileLevel_H264_Main_4_0;
        case H264Level::Level4_1:
            return PAL::kVTProfileLevel_H264_Main_4_1;
        case H264Level::Level4_2:
            return PAL::kVTProfileLevel_H264_Main_4_2;
        case H264Level::Level5:
            return PAL::kVTProfileLevel_H264_Main_5_0;
        case H264Level::Level5_1:
            return PAL::kVTProfileLevel_H264_Main_5_1;
        case H264Level::Level5_2:
            return PAL::kVTProfileLevel_H264_Main_5_2;
        default:
            return PAL::kVTProfileLevel_H264_Main_AutoLevel;
        }
    case H264Profile::ConstrainedHigh:
    case H264Profile::High:
        // FIXME: Consider setting a more precise profile, at least for webcodecs.
        return PAL::kVTProfileLevel_H264_High_AutoLevel;
    case H264Profile::PredictiveHigh444:
        return PAL::kVTProfileLevel_H264_High_AutoLevel;
    }

    return PAL::kVTProfileLevel_H264_Baseline_AutoLevel;
}

static CFStringRef computeVTProfileLevel(const Vector<std::pair<String, String>>& parameters)
{
    // https://datatracker.ietf.org/doc/html/rfc6184#section-8.1: default to Constrained Baseline Level 3.1 when profile-level-id is absent.
    H264ProfileLevelId profileLevelId { H264Profile::ConstrainedBaseline, H264Level::Level3_1 };
    for (auto& parameter : parameters) {
        if (equalLettersIgnoringASCIICase(parameter.first, "profile-level-id"_s)) {
            if (auto parsed = parseH264ProfileLevelId(parameter.second))
                profileLevelId = *parsed;
            break;
        }
    }
    return extractVTProfileLevel(profileLevelId);
}

GPUVideoEncoderVTBH264::GPUVideoEncoderVTBH264(CreationInfo&& creationInfo, const Vector<std::pair<String, String>>& parameters, GPUVideoEncoderCallback&& callback, GPUVideoEncoderDescriptionCallback&& descriptionCallback, GPUVideoEncoderErrorCallback&& errorCallback)
    : GPUVideoEncoderVTB(WTF::move(creationInfo), WTF::move(callback), WTF::move(descriptionCallback), WTF::move(errorCallback))
    , m_profileLevel(computeVTProfileLevel(parameters))
{
}

void GPUVideoEncoderVTBH264::configureAdditionalProperties()
{
    assertIsCurrent(queue());

    setProperty(PAL::kVTCompressionPropertyKey_ProfileLevel, m_profileLevel);
}

bool GPUVideoEncoderVTBH264::convertAndNotify(RetainPtr<CMSampleBufferRef>&& sampleBuffer, GPUVideoEncoderFrameInfo&& info, const PlatformVideoColorSpace& colorSpace)
{
    if (useAnnexB()) {
        auto annexBBuffer = convertAVCCMSampleBufferToAnnexB(sampleBuffer, info.isKeyFrame);
        if (annexBBuffer.isEmpty())
            return false;

        notifyDescriptionIfNeeded(sampleBuffer, CFSTR("avcC"), colorSpace);
        notifyEncodedFrame(annexBBuffer.span(), info);
        return true;
    }

    auto buffer = toVector(sampleBuffer);
    if (!buffer)
        return false;

    notifyDescriptionIfNeeded(sampleBuffer, CFSTR("avcC"), colorSpace);
    notifyEncodedFrame(buffer->span(), info);
    return true;
}

}

#endif // USE(AVFOUNDATION)
