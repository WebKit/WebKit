/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "CodecUtilities.h"

#include "AV1Utilities.h"
#include "HEVCUtilities.h"
#include "LocalizedStrings.h"
#include "VP9Utilities.h"
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

String humanReadableStringFromCodecString(const String& codecString)
{
    if (auto parameters = parseVPCodecParameters(codecString)) {
        StringBuilder builder;
        if (parameters->codecName == "vp08"_s)
            builder.append(WEB_UI_STRING_KEY("VP8", "VP8 (Codec String)", "Codec Strings"));
        else if (parameters->codecName == "vp09"_s)
            builder.append(WEB_UI_STRING_KEY("VP9", "VP9 (Codec String)", "Codec Strings"));
        builder.append(" (");

        uint8_t levelMajor = parameters->level / 10;
        uint8_t levelMinor = parameters->level % 10;
        auto levelString = levelMinor ? WEB_UI_FORMAT_STRING("%d.%d", "Codec Level (Codec Strings)", levelMajor, levelMinor) : String::number(levelMajor);
        builder.append(WEB_UI_FORMAT_STRING("Profile %d, Level %s", "VP8/9 Codec Level & Profile (Codec Strings)", parameters->profile, levelString.utf8().data()));
        builder.append(")");

        return builder.toString();
    }

    if (auto parameters = parseAVCCodecParameters(codecString)) {
        StringBuilder builder;
        builder.append(WEB_UI_STRING_KEY("AVC", "AVC (Codec String)", "Codec Strings"));
        builder.append(" (");
        switch (parameters->profileIDC) {
        case 66:
            builder.append(WEB_UI_STRING_KEY("Baseline Profile", "Baseline Profile (AVC Codec Profile)", "Codec Strings"));
            builder.append(", ");
            break;
        case 77:
            builder.append(WEB_UI_STRING_KEY("Main Profile", "Main Profile (AVC Codec Profile)", "Codec Strings"));
            builder.append(", ");
            break;
        case 88:
            builder.append(WEB_UI_STRING_KEY("Extended Profile", "Extended Profile (AVC Codec Profile)", "Codec Strings"));
            builder.append(", ");
            break;
        case 100:
            builder.append(WEB_UI_STRING_KEY("High Profile", "High Profile (AVC Codec Profile)", "Codec Strings"));
            builder.append(", ");
            break;
        case 110:
            builder.append(WEB_UI_STRING_KEY("High 10 Profile", "High 10 Profile (AVC Codec Profile)", "Codec Strings"));
            builder.append(", ");
            break;
        case 122:
            builder.append(WEB_UI_STRING_KEY("High 422 Profile", "High 422 Profile (AVC Codec Profile)", "Codec Strings"));
            builder.append(", ");
            break;
        case 244:
            builder.append(WEB_UI_STRING_KEY("High 444 Predictive Profile", "High 444 Predictive Profile (AVC Codec Profile)", "Codec Strings"));
            builder.append(", ");
            break;
        }
        if (parameters->levelIDC == 11)
            builder.append(WEB_UI_STRING_KEY("Level 1b", "Level 1b (AVC Codec Level)", "Codec Strings"));
        else {
            uint8_t levelMajor = parameters->levelIDC / 10;
            uint8_t levelMinor = parameters->levelIDC % 10;
            auto levelString = levelMinor ? WEB_UI_FORMAT_STRING("Level %d.%d", "Level %d.%d (AVC Codec Level)", levelMajor, levelMinor) : WEB_UI_FORMAT_STRING("Level %d", "Level %d (AVC Codec Level)", levelMajor);
            builder.append(levelString);
        }
        builder.append(")");
        return builder.toString();
    }

    if (auto parameters = parseHEVCCodecParameters(codecString)) {
        StringBuilder builder;
        builder.append(WEB_UI_STRING_KEY("HEVC", "HEVC (Codec String)", "Codec Strings"));
        builder.append(" (");
        switch (parameters->generalProfileIDC) {
        case 1:
            builder.append(WEB_UI_STRING_KEY("Main Profile", "Main Profile (HEVC Codec Profile)", "Codec Strings"));
            builder.append(", ");
            break;
        case 2:
            builder.append(WEB_UI_STRING_KEY("Main 10 Profile", "Main 10 Profile (HEVC Codec Profile)", "Codec Strings"));
            builder.append(", ");
            break;
        }
        if (parameters->generalTierFlag)
            builder.append(WEB_UI_STRING_KEY("High Tier", "High Tier (HEVC Codec Tier", "Codec Strings"));
        else
            builder.append(WEB_UI_STRING_KEY("Main Tier", "Main Tier (HEVC Codec Tier", "Codec Strings"));
        builder.append(")");
        return builder.toString();
    }

    if (auto parameters = parseAV1CodecParameters(codecString)) {
        StringBuilder builder;
        builder.append(WEB_UI_STRING_KEY("AV1", "AV1 (Codec String)", "Codec Strings"));
        builder.append(" (");
        switch (parameters->profile) {
        case AV1ConfigurationProfile::Main:
            builder.append(WEB_UI_STRING_KEY("Main Profile, ", "Main Profile (AV1 Codec Profile)", "Codec Strings"));
            builder.append(", ");
            break;
        case AV1ConfigurationProfile::High:
            builder.append(WEB_UI_STRING_KEY("High Profile, ", "High Profile (AV1 Codec Profile)", "Codec Strings"));
            builder.append(", ");
            break;
        case AV1ConfigurationProfile::Professional:
            builder.append(WEB_UI_STRING_KEY("Professional Profile, ", "Professional Profile (AV1 Codec Profile)", "Codec Strings"));
            builder.append(", ");
            break;
        };

        switch (parameters->level) {
        case AV1ConfigurationLevel::Level_2_0:
            builder.append(WEB_UI_STRING_KEY("Level 2.0", "Level 2.0 (AV1 Codec Level)", "Codec Strings"));
            break;
        case AV1ConfigurationLevel::Level_2_1:
            builder.append(WEB_UI_STRING_KEY("Level 2.1", "Level 2.1 (AV1 Codec Level)", "Codec Strings"));
            break;
        case AV1ConfigurationLevel::Level_2_2:
            builder.append(WEB_UI_STRING_KEY("Level 2.2", "Level 2.2 (AV1 Codec Level)", "Codec Strings"));
            break;
        case AV1ConfigurationLevel::Level_2_3:
            builder.append(WEB_UI_STRING_KEY("Level 2.3", "Level 2.3 (AV1 Codec Level)", "Codec Strings"));
            break;
        case AV1ConfigurationLevel::Level_3_0:
            builder.append(WEB_UI_STRING_KEY("Level 3.0", "Level 3.0 (AV1 Codec Level)", "Codec Strings"));
            break;
        case AV1ConfigurationLevel::Level_3_1:
            builder.append(WEB_UI_STRING_KEY("Level 3.1", "Level 3.1 (AV1 Codec Level)", "Codec Strings"));
            break;
        case AV1ConfigurationLevel::Level_3_2:
            builder.append(WEB_UI_STRING_KEY("Level 3.2", "Level 3.2 (AV1 Codec Level)", "Codec Strings"));
            break;
        case AV1ConfigurationLevel::Level_3_3:
            builder.append(WEB_UI_STRING_KEY("Level 3.3", "Level 3.3 (AV1 Codec Level)", "Codec Strings"));
            break;
        case AV1ConfigurationLevel::Level_4_0:
            builder.append(WEB_UI_STRING_KEY("Level 4.0", "Level 4.0 (AV1 Codec Level)", "Codec Strings"));
            break;
        case AV1ConfigurationLevel::Level_4_1:
            builder.append(WEB_UI_STRING_KEY("Level 4.1", "Level 4.1 (AV1 Codec Level)", "Codec Strings"));
            break;
        case AV1ConfigurationLevel::Level_4_2:
            builder.append(WEB_UI_STRING_KEY("Level 4.2", "Level 4.2 (AV1 Codec Level)", "Codec Strings"));
            break;
        case AV1ConfigurationLevel::Level_4_3:
            builder.append(WEB_UI_STRING_KEY("Level 4.3", "Level 4.3 (AV1 Codec Level)", "Codec Strings"));
            break;
        case AV1ConfigurationLevel::Level_5_0:
            builder.append(WEB_UI_STRING_KEY("Level 5.0", "Level 5.0 (AV1 Codec Level)", "Codec Strings"));
            break;
        case AV1ConfigurationLevel::Level_5_1:
            builder.append(WEB_UI_STRING_KEY("Level 5.1", "Level 5.1 (AV1 Codec Level)", "Codec Strings"));
            break;
        case AV1ConfigurationLevel::Level_5_2:
            builder.append(WEB_UI_STRING_KEY("Level 5.2", "Level 5.2 (AV1 Codec Level)", "Codec Strings"));
            break;
        case AV1ConfigurationLevel::Level_5_3:
            builder.append(WEB_UI_STRING_KEY("Level 5.3", "Level 5.3 (AV1 Codec Level)", "Codec Strings"));
            break;
        case AV1ConfigurationLevel::Level_6_0:
            builder.append(WEB_UI_STRING_KEY("Level 6.0", "Level 6.0 (AV1 Codec Level)", "Codec Strings"));
            break;
        case AV1ConfigurationLevel::Level_6_1:
            builder.append(WEB_UI_STRING_KEY("Level 6.1", "Level 6.1 (AV1 Codec Level)", "Codec Strings"));
            break;
        case AV1ConfigurationLevel::Level_6_2:
            builder.append(WEB_UI_STRING_KEY("Level 6.2", "Level 6.2 (AV1 Codec Level)", "Codec Strings"));
            break;
        case AV1ConfigurationLevel::Level_6_3:
            builder.append(WEB_UI_STRING_KEY("Level 6.3", "Level 6.3 (AV1 Codec Level)", "Codec Strings"));
            break;
        case AV1ConfigurationLevel::Level_7_0:
            builder.append(WEB_UI_STRING_KEY("Level 7.0", "Level 7.0 (AV1 Codec Level)", "Codec Strings"));
            break;
        case AV1ConfigurationLevel::Level_7_1:
            builder.append(WEB_UI_STRING_KEY("Level 7.1", "Level 7.1 (AV1 Codec Level)", "Codec Strings"));
            break;
        case AV1ConfigurationLevel::Level_7_2:
            builder.append(WEB_UI_STRING_KEY("Level 7.2", "Level 7.2 (AV1 Codec Level)", "Codec Strings"));
            break;
        case AV1ConfigurationLevel::Level_7_3:
            builder.append(WEB_UI_STRING_KEY("Level 7.3", "Level 7.3 (AV1 Codec Level)", "Codec Strings"));
            break;
        }
        builder.append(")");
        return builder.toString();
    }

    if (auto parameters = parseDoViCodecParameters(codecString))
        return WEB_UI_STRING_KEY("Dolby Vision", "Dolby Vision Codec String", "Codec Strings");

    if (codecString.startsWith("opus"_s))
        return WEB_UI_STRING_KEY("Opus", "Opus Codec String", "Codec Strings");

    if (codecString.startsWith("vorbis"_s))
        return WEB_UI_STRING_KEY("Vorbis", "Vorbis Codec String", "Codec Strings");

    if (codecString.startsWith("mp4a.40."_s)) {
        auto objectType = codecString.substring(8);
        auto objectValue = parseInteger<uint8_t>(objectType, 10);

        switch (objectValue.value_or(0)) {
        case 1:
        case 3:
        case 4:
        case 5:
        case 6:
        case 29:
            return WEB_UI_STRING_KEY("MPEG-4 AAC", "MPEG-4 AAC Codec String", "Codec Strings");
        case 2:
            return WEB_UI_STRING_KEY("AAC-LC", "AAC-LC Codec String", "Codec Strings");
        case 34:
            return WEB_UI_STRING_KEY("MP3", "MP3 Codec String", "Codec Strings");
        }

        return WEB_UI_STRING_KEY("MPEG-4 AAC", "MPEG-4 AAC Codec String", "Codec Strings");
    }

    auto dotIndex = codecString.find('.');
    return codecString.substring(0, dotIndex == notFound ? String::MaxLength : dotIndex);
}

}
