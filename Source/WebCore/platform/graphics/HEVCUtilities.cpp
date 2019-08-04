/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "HEVCUtilities.h"

#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

Optional<HEVCParameterSet> parseHEVCCodecParameters(const String& codecString)
{
    // The format of the 'hevc' codec string is specified in ISO/IEC 14496-15:2014, Annex E.3.
    StringView codecView(codecString);
    auto codecSplit = codecView.split('.');
    auto nextElement = codecSplit.begin();
    if (nextElement == codecSplit.end())
        return WTF::nullopt;

    HEVCParameterSet parameters;

    // Codec identifier: legal values are specified in ISO/IEC 14496-15:2014, section 8:
    parameters.codecName = (*nextElement).toString();
    if (!equal(parameters.codecName, "hvc1") && !equal(parameters.codecName, "hev1"))
        return WTF::nullopt;

    if (++nextElement == codecSplit.end())
        return WTF::nullopt;

    // First element: Optional General Profile Space parameter ['A', 'B', 'C'], mapping to [1, 2, 3]
    // and [0] for absent, then General Profile IDC as a 5-bit decimal number.
    auto profileSpace = *nextElement;
    if (!profileSpace.length())
        return WTF::nullopt;

    auto firstCharacter = profileSpace[0];
    bool hasProfileSpace = firstCharacter >= 'A' && firstCharacter <= 'C';
    if (hasProfileSpace) {
        parameters.generalProfileSpace = 1 + (firstCharacter - 'A');
        profileSpace = profileSpace.substring(1);
    }

    bool isValidProfileIDC = false;
    parameters.generalProfileIDC = toIntegralType<uint8_t>(profileSpace, &isValidProfileIDC);
    if (!isValidProfileIDC)
        return WTF::nullopt;

    if (++nextElement == codecSplit.end())
        return WTF::nullopt;

    // Second element: 32 bit of General Profile Compatibility Flags, in reverse bit order,
    // in hex with leading zeros omitted.
    auto compatibilityFlags = *nextElement;
    bool isValidCompatibilityFlags = false;
    parameters.generalProfileCompatibilityFlags = toIntegralType<uint32_t>(compatibilityFlags, &isValidCompatibilityFlags, 16);
    if (!isValidCompatibilityFlags)
        return WTF::nullopt;

    if (++nextElement == codecSplit.end())
        return WTF::nullopt;

    // Third element: General Tier Flag ['L', 'H'], mapping to [false, true], followed by
    // General Level IDC as a 8-bit decimal number.
    auto generalTier = *nextElement;
    firstCharacter = generalTier[0];
    if (firstCharacter != 'L' && firstCharacter != 'H')
        return WTF::nullopt;

    parameters.generalTierFlag = firstCharacter == 'H';
    bool isValidGeneralLevelIDC = false;
    parameters.generalLevelIDC = toIntegralType<uint8_t>(generalTier.substring(1), &isValidGeneralLevelIDC);
    if (!isValidGeneralLevelIDC)
        return WTF::nullopt;

    // Optional fourth and remaning elements: a sequence of 6 1-byte constraint flags, each byte encoded
    // in hex, and separated by a period, with trailing zero bytes omitted.
    parameters.constraintFlags.fill(0, 6);
    for (auto& flag : parameters.constraintFlags) {
        if (++nextElement == codecSplit.end())
            break;

        bool isValidFlag = false;
        flag = toIntegralType<uint8_t>(*nextElement, &isValidFlag, 16);
        if (!isValidFlag)
            return WTF::nullopt;
    }

    return parameters;
}

}
