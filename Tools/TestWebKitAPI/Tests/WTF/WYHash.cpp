/*
 * Copyright (C) 2013-2023 Apple Inc. All rights reserved.
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
#include <wtf/text/WYHash.h>

#include <cstdio>

namespace TestWebKitAPI {

static const LChar nullLChars[2] = { 0, 0 };
static const UChar nullUChars[2] = { 0, 0 };

static const unsigned emptyStringHash = 14758465;
static const unsigned singleNullCharacterHash = 8544522;

static const unsigned expected[256] = {
    14758465, 8544522, 11404164, 861494, 8147482, 15180481, 5812382, 6297818, 13830821, 2051146, 13032536, 11309798, 442405,
    16661170, 7211908, 12060168, 10671874, 1787484, 3820051, 6206311, 4325801, 1076746, 10889457, 12139913, 6808129, 14949824,
    10098166, 14766123, 10802793, 11419560, 11295320, 8219223, 6128848, 16226888, 12652959, 5283609, 4375385, 16362353, 2916462,
    9821487, 1516432, 9236229, 7463487, 392793, 10302663, 14064731, 15314643, 13485968, 13524780, 2937456, 6931040, 9901604,
    8240496, 16314214, 1894656, 4005245, 2065465, 9737294, 3743978, 2990774, 15831146, 3022861, 13214557, 5884924, 2565619,
    10160654, 8323122, 11100754, 15339548, 5446771, 4203062, 2376044, 3759996, 1912617, 750804, 15697932, 14719712, 2939095,
    7172057, 10895203, 4004571, 920124, 776006, 6866412, 3797418, 15232550, 8664048, 323204, 5523437, 2798003, 4984110, 1614236,
    2795743, 10435134, 3031732, 11875758, 8615220, 2575754, 3883493, 10102211, 10297633, 13373357, 3320565, 805566, 3343227,
    4702423, 4034523, 15099913, 8064251, 1945916, 9741977, 2012559, 3181593, 3207764, 14142253, 10442223, 11411846, 16525950,
    9957903, 12301435, 15669903, 11244224, 14207494, 8993606, 9062553, 15203642, 9734872, 207721, 14683663, 3382897, 11780544,
    9246218, 14864591, 2457241, 9077952, 7605682, 3215147, 11360339, 12943578, 5376357, 9143719, 15710072, 14161298, 7558156,
    1773950, 13564671, 6365633, 2881616, 937782, 15100889, 567923, 13837074, 6187394, 5781497, 11636463, 5925627, 11867616,
    14332365, 13654107, 8828332, 13848556, 14279218, 4324605, 961564, 10678609, 4671399, 7898509, 7591381, 9207508, 12245055,
    8748853, 1042927, 10698920, 256364, 1560089, 5959228, 12314130, 9033052, 15320058, 12705489, 6160063, 1307639, 11065954,
    6943522, 3235377, 8570301, 13457310, 9839558, 5646803, 265845, 2611042, 1752059, 7513590, 8207753, 2278170, 4303580, 9566134,
    5551742, 2921143, 8374989, 11969561, 2871579, 10810435, 7120162, 12776236, 12547492, 11653742, 13222394, 5411919, 857888,
    117158, 1138701, 12458506, 8747896, 5870363, 7991304, 3114735, 15783297, 11125116, 6239249, 11656890, 11350502, 13882704,
    7507399, 11896134, 3015283, 9368263, 13017653, 7517268, 6102751, 2498681, 13035942, 1039022, 13176410, 6013379, 7289398,
    16588282, 16252852, 15998878, 7465713, 6958679, 9043848, 9190094, 4665714, 5267293, 16333588, 253491, 2294431, 10044443,
    6642359, 8110748, 14335827, 8010543, 9025350, 3645095, 15949665
};

TEST(WTF, WYHasher_computeHashAndMaskTop8Bits)
{
    auto generateLCharArray = [&](size_t size) {
        auto array = std::unique_ptr<LChar[]>(new LChar[size]);
        for (size_t i = 0; i < size; i++)
            array[i] = i;
        return array;
    };

    auto generateUCharArray = [&](size_t size) {
        auto array = std::unique_ptr<UChar[]>(new UChar[size]);
        for (size_t i = 0; i < size; i++)
            array[i] = i;
        return array;
    };

    unsigned max8Bit = std::numeric_limits<uint8_t>::max();
    for (size_t size = 0; size <= max8Bit; size++) {
        std::unique_ptr<LChar[]> arr1 = generateLCharArray(size);
        std::unique_ptr<UChar[]> arr2 = generateUCharArray(size);
        unsigned left = WYHash::computeHashAndMaskTop8Bits(arr1.get(), size);
        unsigned right = WYHash::computeHashAndMaskTop8Bits(arr2.get(), size);
        ASSERT_EQ(left, right);
        ASSERT_EQ(left, expected[size]);
    }

    ASSERT_EQ(emptyStringHash, WYHash::computeHashAndMaskTop8Bits(static_cast<LChar*>(0), 0));
    ASSERT_EQ(emptyStringHash, WYHash::computeHashAndMaskTop8Bits(static_cast<UChar*>(0), 0));

    ASSERT_EQ(emptyStringHash, WYHash::computeHashAndMaskTop8Bits(nullLChars, 0));
    ASSERT_EQ(emptyStringHash, WYHash::computeHashAndMaskTop8Bits(nullUChars, 0));

    ASSERT_EQ(singleNullCharacterHash, WYHash::computeHashAndMaskTop8Bits(nullLChars, 1));
    ASSERT_EQ(singleNullCharacterHash, WYHash::computeHashAndMaskTop8Bits(nullUChars, 1));
}

} // namespace TestWebKitAPI
