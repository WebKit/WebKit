/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
 * Copyright (C) 2010 Daniel Bates (dbates@intudata.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "RenderListMarker.h"

#include "CSSCounterStyleRegistry.h"
#include "Document.h"
#include "FontCascade.h"
#include "GraphicsContext.h"
#include "LegacyInlineElementBox.h"
#include "RenderLayer.h"
#include "RenderListItem.h"
#include "RenderMultiColumnFlow.h"
#include "RenderMultiColumnSpannerPlaceholder.h"
#include "RenderView.h"
#include "StyleScope.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/StackStats.h>
#include <wtf/text/StringConcatenateNumbers.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

// As of June 2021, the Microsoft C++ compiler seems unable to include std::initializer_list in a constexpr.
#if COMPILER(MSVC)
#define CONSTEXPR_WITH_MSVC_INITIALIZER_LIST_WORKAROUND const
#else
#define CONSTEXPR_WITH_MSVC_INITIALIZER_LIST_WORKAROUND constexpr
#endif

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderListMarker);

constexpr int cMarkerPadding = 7;

enum class SequenceType : bool { Numeric, Alphabetic };
enum class Formality : bool { Informal, Formal };

template<SequenceType type, typename CharacterType> static String toAlphabeticOrNumeric(int number, const CharacterType* sequence, unsigned sequenceSize)
{
    ASSERT(sequenceSize >= 2);

    // Taking sizeof(number) in the expression below doesn't work with some compilers.
    constexpr unsigned lettersSize = sizeof(int) * 8 + 1; // Binary is the worst case; requires one character per bit plus a minus sign.

    CharacterType letters[lettersSize];

    bool isNegativeNumber = false;
    unsigned numberShadow = number;
    if constexpr (type == SequenceType::Alphabetic) {
        ASSERT(number > 0);
        --numberShadow;
    } else if (number < 0) {
        numberShadow = -number;
        isNegativeNumber = true;
    }
    letters[lettersSize - 1] = sequence[numberShadow % sequenceSize];
    unsigned length = 1;

    if constexpr (type == SequenceType::Alphabetic) {
        while ((numberShadow /= sequenceSize) > 0) {
            --numberShadow;
            letters[lettersSize - ++length] = sequence[numberShadow % sequenceSize];
        }
    } else {
        while ((numberShadow /= sequenceSize) > 0)
            letters[lettersSize - ++length] = sequence[numberShadow % sequenceSize];
    }
    if (isNegativeNumber)
        letters[lettersSize - ++length] = hyphenMinus;

    ASSERT(length <= lettersSize);
    return { &letters[lettersSize - length], length };
}

template<typename CharacterType> static String toSymbolic(int number, const CharacterType* symbols, unsigned symbolsSize)
{
    ASSERT(number > 0);
    ASSERT(symbolsSize >= 1);

    // The asterisks list-style-type is the worst case; we show |number| asterisks.
    auto symbol = symbols[(number - 1) % symbolsSize];
    unsigned count = (number - 1) / symbolsSize + 1;

    CharacterType* characters;
    String result = String::createUninitialized(count, characters);
    for (unsigned i = 0; i < count; ++i)
        characters[i] = symbol;
    return result;
}

template<typename CharacterType> static String toAlphabetic(int number, const CharacterType* alphabet, unsigned alphabetSize)
{
    return toAlphabeticOrNumeric<SequenceType::Alphabetic>(number, alphabet, alphabetSize);
}

template<typename CharacterType> static String toNumeric(int number, const CharacterType* numerals, unsigned numeralsSize)
{
    return toAlphabeticOrNumeric<SequenceType::Numeric>(number, numerals, numeralsSize);
}

template<typename CharacterType, size_t size> static String toAlphabetic(int number, const CharacterType(&alphabet)[size])
{
    return toAlphabetic(number, alphabet, size);
}

template<typename CharacterType, size_t size> static String toNumeric(int number, const CharacterType(&alphabet)[size])
{
    return toNumeric(number, alphabet, size);
}

template<typename CharacterType, size_t size> static String toSymbolic(int number, const CharacterType(&alphabet)[size])
{    
    return toSymbolic(number, alphabet, size);
}

// This table format was derived from an old draft of the CSS specification: 3 group markers, 3 digit markers, 10 digits, negative sign.
static String toCJKIdeographic(int number, const UChar table[17], Formality formality)
{
    enum AbstractCJKCharacter {
        NoChar,
        SecondGroupMarker, ThirdGroupMarker, FourthGroupMarker,
        SecondDigitMarker, ThirdDigitMarker, FourthDigitMarker,
        Digit0, Digit1, Digit2, Digit3, Digit4,
        Digit5, Digit6, Digit7, Digit8, Digit9,
        NegativeSign
    };

    if (!number)
        return { &table[Digit0 - 1] , 1 };

    ASSERT(number != std::numeric_limits<int>::min());
    bool needsNegativeSign = number < 0;
    if (needsNegativeSign)
        number = -number;

    constexpr unsigned groupLength = 8; // 4 digits, 3 digit markers, and a group marker
    constexpr unsigned bufferLength = 4 * groupLength;
    AbstractCJKCharacter buffer[bufferLength] = { NoChar };

    for (int i = 0; i < 4; ++i) {
        int groupValue = number % 10000;
        number /= 10000;

        // Process least-significant group first, but put it in the buffer last.
        auto group = &buffer[(3 - i) * groupLength];

        if (groupValue && i)
            group[7] = static_cast<AbstractCJKCharacter>(SecondGroupMarker - 1 + i);

        // Put in the four digits and digit markers for any non-zero digits.
        group[6] = static_cast<AbstractCJKCharacter>(Digit0 + (groupValue % 10));
        if (number != 0 || groupValue > 9) {
            int digitValue = ((groupValue / 10) % 10);
            group[4] = static_cast<AbstractCJKCharacter>(Digit0 + digitValue);
            if (digitValue)
                group[5] = SecondDigitMarker;
        }
        if (number != 0 || groupValue > 99) {
            int digitValue = ((groupValue / 100) % 10);
            group[2] = static_cast<AbstractCJKCharacter>(Digit0 + digitValue);
            if (digitValue)
                group[3] = ThirdDigitMarker;
        }
        if (number != 0 || groupValue > 999) {
            int digitValue = groupValue / 1000;
            group[0] = static_cast<AbstractCJKCharacter>(Digit0 + digitValue);
            if (digitValue)
                group[1] = FourthDigitMarker;
        }

        if (formality == Formality::Informal && groupValue < 20) {
            // Remove the tens digit, but leave the marker.
            ASSERT(group[4] == NoChar || group[4] == Digit0 || group[4] == Digit1);
            group[4] = NoChar;
        }

        if (!number)
            break;
    }

    // Convert into characters, omitting consecutive runs of digit0 and trailing digit0.
    unsigned length = 0;
    UChar characters[1 + bufferLength];
    auto last = NoChar;
    if (needsNegativeSign)
        characters[length++] = table[NegativeSign - 1];
    for (unsigned i = 0; i < bufferLength; ++i) {
        auto character = buffer[i];
        if (character != NoChar) {
            if (character != Digit0 || last != Digit0)
                characters[length++] = table[character - 1];
            last = character;
        }
    }
    if (last == Digit0)
        --length;

    return { characters, length };
}

struct AdditiveSymbol {
    int value;
    std::initializer_list<UChar> characters;
};
struct AdditiveSystem {
    std::initializer_list<AdditiveSymbol> symbols;
    std::initializer_list<UChar> negative;
};

static String toPredefinedAdditiveSystem(int value, const AdditiveSystem& system)
{
    if (!value) {
        ASSERT(!system.symbols.end()[-1].value);
        auto& zeroCharacters = system.symbols.end()[-1].characters;
        return { zeroCharacters.begin(), static_cast<unsigned>(zeroCharacters.size()) };
    }

    StringBuilder result;
    auto append = [&] (std::initializer_list<UChar> characters) {
        result.append(StringView { characters.begin(), static_cast<unsigned>(characters.size()) });
    };
    if (value < 0) {
        ASSERT(value != std::numeric_limits<int>::min());
        append(system.negative);
        value = -value;
    }
    for (auto& symbol : system.symbols) {
        while (value >= symbol.value) {
            append(symbol.characters);
            value -= symbol.value;
        }
        if (!value)
            break;
    }
    ASSERT(!value);
    return result.toString();
}

static String toEthiopicNumeric(int value)
{
    ASSERT(value >= 1);

    if (value == 1) {
        UChar ethiopicDigitOne = 0x1369;
        return { &ethiopicDigitOne, 1 };
    }

    // Split the number into groups of two digits, starting with the least significant decimal digit.
    uint8_t groups[5];
    for (auto& group : groups) {
        group = value % 100;
        value /= 100;
    }

    UChar buffer[std::size(groups) * 3];
    unsigned length = 0;
    bool isMostSignificantGroup = true;
    for (int i = std::size(groups) - 1; i >= 0; --i) {
        auto value = groups[i];
        bool isOddIndex = i & 1;
        // If the group has the value zero, or if the group is the most significant one and has the value 1,
        // or if the group has an odd index (as given in the previous step) and has the value 1,
        // then remove the digits (but leave the group, so it still has a separator appended below).
        if (!(value == 1 && (isMostSignificantGroup || isOddIndex))) {
            if (auto tens = value / 10)
                buffer[length++] = 0x1371 + tens;
            if (auto ones = value % 10)
                buffer[length++] = 0x1368 + ones;
        }
        if (value && isOddIndex)
            buffer[length++] = 0x137B;
        if ((value || !isMostSignificantGroup) && !isOddIndex && i)
            buffer[length++] = 0x137C;
        if (value)
            isMostSignificantGroup = false;
    }

    return { buffer, length };
}

static ListStyleType effectiveListMarkerType(ListStyleType type, int value)
{
    // Note, the following switch statement has been explicitly grouped by list-style-type ordinal range.
    switch (type) {
    // FIXME: handle counter-style: rdar://102988393.
    case ListStyleType::CustomCounterStyle:
    case ListStyleType::ArabicIndic:
    case ListStyleType::Bengali:
    case ListStyleType::Binary:
    case ListStyleType::Cambodian:
    case ListStyleType::Circle:
    case ListStyleType::DecimalLeadingZero:
    case ListStyleType::Decimal:
    case ListStyleType::Devanagari:
    case ListStyleType::Disc:
    case ListStyleType::DisclosureClosed:
    case ListStyleType::DisclosureOpen:
    case ListStyleType::Gujarati:
    case ListStyleType::Gurmukhi:
    case ListStyleType::Kannada:
    case ListStyleType::Khmer:
    case ListStyleType::Lao:
    case ListStyleType::LowerHexadecimal:
    case ListStyleType::Malayalam:
    case ListStyleType::Mongolian:
    case ListStyleType::Myanmar:
    case ListStyleType::None:
    case ListStyleType::Octal:
    case ListStyleType::Oriya:
    case ListStyleType::Persian:
    case ListStyleType::Square:
    case ListStyleType::Tamil:
    case ListStyleType::Telugu:
    case ListStyleType::Thai:
    case ListStyleType::Tibetan:
    case ListStyleType::UpperHexadecimal:
    case ListStyleType::Urdu:
        return type; // Can represent all ordinals.
    case ListStyleType::Afar:
    case ListStyleType::Amharic:
    case ListStyleType::AmharicAbegede:
    case ListStyleType::Asterisks:
    case ListStyleType::CJKEarthlyBranch:
    case ListStyleType::CJKHeavenlyStem:
    case ListStyleType::Ethiopic:
    case ListStyleType::EthiopicAbegede:
    case ListStyleType::EthiopicAbegedeAmEt:
    case ListStyleType::EthiopicAbegedeGez:
    case ListStyleType::EthiopicAbegedeTiEr:
    case ListStyleType::EthiopicAbegedeTiEt:
    case ListStyleType::EthiopicHalehameAaEr:
    case ListStyleType::EthiopicHalehameAaEt:
    case ListStyleType::EthiopicHalehameAmEt:
    case ListStyleType::EthiopicHalehameGez:
    case ListStyleType::EthiopicHalehameOmEt:
    case ListStyleType::EthiopicHalehameSidEt:
    case ListStyleType::EthiopicHalehameSoEt:
    case ListStyleType::EthiopicHalehameTiEr:
    case ListStyleType::EthiopicHalehameTiEt:
    case ListStyleType::EthiopicHalehameTig:
    case ListStyleType::EthiopicNumeric:
    case ListStyleType::Footnotes:
    case ListStyleType::Hangul:
    case ListStyleType::HangulConsonant:
    case ListStyleType::Hiragana:
    case ListStyleType::HiraganaIroha:
    case ListStyleType::Katakana:
    case ListStyleType::KatakanaIroha:
    case ListStyleType::LowerAlpha:
    case ListStyleType::LowerGreek:
    case ListStyleType::LowerLatin:
    case ListStyleType::LowerNorwegian:
    case ListStyleType::Oromo:
    case ListStyleType::Sidama:
    case ListStyleType::Somali:
    case ListStyleType::Tigre:
    case ListStyleType::TigrinyaEr:
    case ListStyleType::TigrinyaErAbegede:
    case ListStyleType::TigrinyaEt:
    case ListStyleType::TigrinyaEtAbegede:
    case ListStyleType::UpperAlpha:
    case ListStyleType::UpperGreek:
    case ListStyleType::UpperLatin:
    case ListStyleType::UpperNorwegian:
        return (value < 1) ? ListStyleType::Decimal : type;
    case ListStyleType::Armenian:
    case ListStyleType::LowerArmenian:
    case ListStyleType::UpperArmenian:
        return (value < 1 || value > 9999) ? ListStyleType::Decimal : type;
    case ListStyleType::CJKDecimal:
        return (value < 0) ? ListStyleType::Decimal : type;
    case ListStyleType::CJKIdeographic:
    case ListStyleType::JapaneseFormal:
    case ListStyleType::JapaneseInformal:
    case ListStyleType::SimplifiedChineseFormal:
    case ListStyleType::SimplifiedChineseInformal:
    case ListStyleType::TraditionalChineseFormal:
    case ListStyleType::TraditionalChineseInformal:
        return value < -9999 ? ListStyleType::Decimal : value > 9999 ? ListStyleType::CJKDecimal : type;
    case ListStyleType::Georgian:
        return (value < 1 || value > 19999) ? ListStyleType::Decimal : type;
    case ListStyleType::Hebrew:
        return (value < 1 || value > 10999) ? ListStyleType::Decimal : type;
    case ListStyleType::KoreanHangulFormal:
    case ListStyleType::KoreanHanjaInformal:
    case ListStyleType::KoreanHanjaFormal:
        return (value < -9999 || value > 9999) ? ListStyleType::Decimal : type;
    case ListStyleType::LowerRoman:
    case ListStyleType::UpperRoman:
        return (value < 1 || value > 3999) ? ListStyleType::Decimal : type;
    case ListStyleType::String:
        ASSERT_NOT_REACHED();
        break;
    }

    ASSERT_NOT_REACHED();
    return type;
}

static StringView listMarkerSuffix(ListStyleType type)
{
    switch (type) {
    // FIXME: handle counter-style: rdar://102988393.
    case ListStyleType::CustomCounterStyle:
        return { };
    case ListStyleType::Asterisks:
    case ListStyleType::Circle:
    case ListStyleType::Disc:
    case ListStyleType::DisclosureClosed:
    case ListStyleType::DisclosureOpen:
    case ListStyleType::Footnotes:
    case ListStyleType::None:
    case ListStyleType::Square:
        return { &space, 1 };
    case ListStyleType::Afar:
    case ListStyleType::Amharic:
    case ListStyleType::AmharicAbegede:
    case ListStyleType::Ethiopic:
    case ListStyleType::EthiopicAbegede:
    case ListStyleType::EthiopicAbegedeAmEt:
    case ListStyleType::EthiopicAbegedeGez:
    case ListStyleType::EthiopicAbegedeTiEr:
    case ListStyleType::EthiopicAbegedeTiEt:
    case ListStyleType::EthiopicHalehameAaEr:
    case ListStyleType::EthiopicHalehameAaEt:
    case ListStyleType::EthiopicHalehameAmEt:
    case ListStyleType::EthiopicHalehameGez:
    case ListStyleType::EthiopicHalehameOmEt:
    case ListStyleType::EthiopicHalehameSidEt:
    case ListStyleType::EthiopicHalehameSoEt:
    case ListStyleType::EthiopicHalehameTiEr:
    case ListStyleType::EthiopicHalehameTiEt:
    case ListStyleType::EthiopicHalehameTig:
    case ListStyleType::Oromo:
    case ListStyleType::Sidama:
    case ListStyleType::Somali:
    case ListStyleType::Tigre:
    case ListStyleType::TigrinyaEr:
    case ListStyleType::TigrinyaErAbegede:
    case ListStyleType::TigrinyaEt:
    case ListStyleType::TigrinyaEtAbegede: {
        static constexpr UChar ethiopicPrefaceColonAndSpace[2] = { ethiopicPrefaceColon, ' ' };
        return { ethiopicPrefaceColonAndSpace, 2 };
    }
    case ListStyleType::Armenian:
    case ListStyleType::ArabicIndic:
    case ListStyleType::Bengali:
    case ListStyleType::Binary:
    case ListStyleType::Cambodian:
    case ListStyleType::DecimalLeadingZero:
    case ListStyleType::Decimal:
    case ListStyleType::Devanagari:
    case ListStyleType::Georgian:
    case ListStyleType::Gujarati:
    case ListStyleType::Gurmukhi:
    case ListStyleType::Hangul:
    case ListStyleType::HangulConsonant:
    case ListStyleType::Hebrew:
    case ListStyleType::Kannada:
    case ListStyleType::Khmer:
    case ListStyleType::Lao:
    case ListStyleType::LowerAlpha:
    case ListStyleType::LowerArmenian:
    case ListStyleType::LowerGreek:
    case ListStyleType::LowerHexadecimal:
    case ListStyleType::LowerLatin:
    case ListStyleType::LowerNorwegian:
    case ListStyleType::LowerRoman:
    case ListStyleType::Malayalam:
    case ListStyleType::Mongolian:
    case ListStyleType::Myanmar:
    case ListStyleType::Octal:
    case ListStyleType::Oriya:
    case ListStyleType::Persian:
    case ListStyleType::Tamil:
    case ListStyleType::Telugu:
    case ListStyleType::Thai:
    case ListStyleType::Tibetan:
    case ListStyleType::UpperAlpha:
    case ListStyleType::UpperArmenian:
    case ListStyleType::UpperGreek:
    case ListStyleType::UpperHexadecimal:
    case ListStyleType::UpperLatin:
    case ListStyleType::UpperNorwegian:
    case ListStyleType::UpperRoman:
    case ListStyleType::Urdu:
        return ". "_s;
    case ListStyleType::CJKDecimal:
    case ListStyleType::CJKEarthlyBranch:
    case ListStyleType::CJKHeavenlyStem:
    case ListStyleType::CJKIdeographic:
    case ListStyleType::Hiragana:
    case ListStyleType::HiraganaIroha:
    case ListStyleType::JapaneseFormal:
    case ListStyleType::JapaneseInformal:
    case ListStyleType::Katakana:
    case ListStyleType::KatakanaIroha:
    case ListStyleType::SimplifiedChineseFormal:
    case ListStyleType::SimplifiedChineseInformal:
    case ListStyleType::TraditionalChineseFormal:
    case ListStyleType::TraditionalChineseInformal:
        return { &ideographicComma, 1 };
    case ListStyleType::EthiopicNumeric:
        return "/ "_s;
    case ListStyleType::KoreanHangulFormal:
    case ListStyleType::KoreanHanjaInformal:
    case ListStyleType::KoreanHanjaFormal:
        return ", "_s;
    case ListStyleType::String:
        ASSERT_NOT_REACHED();
        break;
    }

    ASSERT_NOT_REACHED();
    return ". "_s;
}

String listMarkerText(ListStyleType type, int value, CSSCounterStyle* counterStyle)
{
    switch (effectiveListMarkerType(type, value)) {
    case ListStyleType::None:
        return emptyString();

    case ListStyleType::Asterisks: {
        static constexpr LChar asterisksSymbols[1] = { 0x2A };
        return toSymbolic(value, asterisksSymbols);
    }

    // We use the same characters for text security. See RenderText::setRenderedText.
    case ListStyleType::Circle:
        return { &whiteBullet, 1 };
    case ListStyleType::Disc:
        return { &bullet, 1 };
    case ListStyleType::Footnotes: {
        static constexpr UChar footnotesSymbols[4] = { 0x002A, 0x2051, 0x2020, 0x2021 };
        return toSymbolic(value, footnotesSymbols);
    }

    // CSS specification calls for U+25FE BLACK MEDIUM SMALL SQUARE; we have been using U+25A0 BLACK SQUARE instead for a long time.
    case ListStyleType::Square:
        return { &blackSquare, 1 };

    case ListStyleType::DisclosureClosed:
        return { &blackRightPointingSmallTriangle, 1 };
    case ListStyleType::DisclosureOpen:
        return { &blackDownPointingSmallTriangle, 1 };

    // FIXME: handle counter-style: rdar://102988393.
    case ListStyleType::CustomCounterStyle:
        if (!counterStyle)
            return String::number(value);
        return counterStyle->text(value);
    case ListStyleType::Decimal:
        return String::number(value);

    case ListStyleType::DecimalLeadingZero:
        if (value >= 0 && value <= 9) {
            LChar characters[2] = { '0', static_cast<LChar>('0' + value) }; // 00 to 09
            return { characters, 2 };
        }
        if (value >= -9 && value <= -1) {
            LChar characters[3] = { '-', '0', static_cast<LChar>('0' - value) }; // -01 to -09
            return { characters, 3 };
        }
        return String::number(value);

    case ListStyleType::ArabicIndic: {
        static constexpr UChar arabicIndicNumerals[10] = {
            0x0660, 0x0661, 0x0662, 0x0663, 0x0664, 0x0665, 0x0666, 0x0667, 0x0668, 0x0669
        };
        return toNumeric(value, arabicIndicNumerals);
    }

    case ListStyleType::Binary: {
        static constexpr LChar binaryNumerals[2] = { '0', '1' };
        return toNumeric(value, binaryNumerals);
    }

    case ListStyleType::Bengali: {
        static constexpr UChar bengaliNumerals[10] = {
            0x09E6, 0x09E7, 0x09E8, 0x09E9, 0x09EA, 0x09EB, 0x09EC, 0x09ED, 0x09EE, 0x09EF
        };
        return toNumeric(value, bengaliNumerals);
    }

    case ListStyleType::Cambodian:
    case ListStyleType::Khmer: {
        static constexpr UChar khmerNumerals[10] = {
            0x17E0, 0x17E1, 0x17E2, 0x17E3, 0x17E4, 0x17E5, 0x17E6, 0x17E7, 0x17E8, 0x17E9
        };
        return toNumeric(value, khmerNumerals);
    }
    case ListStyleType::Devanagari: {
        static constexpr UChar devanagariNumerals[10] = {
            0x0966, 0x0967, 0x0968, 0x0969, 0x096A, 0x096B, 0x096C, 0x096D, 0x096E, 0x096F
        };
        return toNumeric(value, devanagariNumerals);
    }
    case ListStyleType::Gujarati: {
        static constexpr UChar gujaratiNumerals[10] = {
            0x0AE6, 0x0AE7, 0x0AE8, 0x0AE9, 0x0AEA, 0x0AEB, 0x0AEC, 0x0AED, 0x0AEE, 0x0AEF
        };
        return toNumeric(value, gujaratiNumerals);
    }
    case ListStyleType::Gurmukhi: {
        static constexpr UChar gurmukhiNumerals[10] = {
            0x0A66, 0x0A67, 0x0A68, 0x0A69, 0x0A6A, 0x0A6B, 0x0A6C, 0x0A6D, 0x0A6E, 0x0A6F
        };
        return toNumeric(value, gurmukhiNumerals);
    }
    case ListStyleType::Kannada: {
        static constexpr UChar kannadaNumerals[10] = {
            0x0CE6, 0x0CE7, 0x0CE8, 0x0CE9, 0x0CEA, 0x0CEB, 0x0CEC, 0x0CED, 0x0CEE, 0x0CEF
        };
        return toNumeric(value, kannadaNumerals);
    }
    case ListStyleType::LowerHexadecimal: {
        static constexpr LChar lowerHexadecimalNumerals[16] = {
            '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
        };
        return toNumeric(value, lowerHexadecimalNumerals);
    }
    case ListStyleType::Lao: {
        static constexpr UChar laoNumerals[10] = {
            0x0ED0, 0x0ED1, 0x0ED2, 0x0ED3, 0x0ED4, 0x0ED5, 0x0ED6, 0x0ED7, 0x0ED8, 0x0ED9
        };
        return toNumeric(value, laoNumerals);
    }
    case ListStyleType::Malayalam: {
        static constexpr UChar malayalamNumerals[10] = {
            0x0D66, 0x0D67, 0x0D68, 0x0D69, 0x0D6A, 0x0D6B, 0x0D6C, 0x0D6D, 0x0D6E, 0x0D6F
        };
        return toNumeric(value, malayalamNumerals);
    }
    case ListStyleType::Mongolian: {
        static constexpr UChar mongolianNumerals[10] = {
            0x1810, 0x1811, 0x1812, 0x1813, 0x1814, 0x1815, 0x1816, 0x1817, 0x1818, 0x1819
        };
        return toNumeric(value, mongolianNumerals);
    }
    case ListStyleType::Myanmar: {
        static constexpr UChar myanmarNumerals[10] = {
            0x1040, 0x1041, 0x1042, 0x1043, 0x1044, 0x1045, 0x1046, 0x1047, 0x1048, 0x1049
        };
        return toNumeric(value, myanmarNumerals);
    }
    case ListStyleType::Octal: {
        static constexpr LChar octalNumerals[8] = {
            '0', '1', '2', '3', '4', '5', '6', '7'
        };
        return toNumeric(value, octalNumerals);
    }
    case ListStyleType::Oriya: {
        static constexpr UChar oriyaNumerals[10] = {
            0x0B66, 0x0B67, 0x0B68, 0x0B69, 0x0B6A, 0x0B6B, 0x0B6C, 0x0B6D, 0x0B6E, 0x0B6F
        };
        return toNumeric(value, oriyaNumerals);
    }
    case ListStyleType::Persian:
    case ListStyleType::Urdu: {
        static constexpr UChar urduNumerals[10] = {
            0x06F0, 0x06F1, 0x06F2, 0x06F3, 0x06F4, 0x06F5, 0x06F6, 0x06F7, 0x06F8, 0x06F9
        };
        return toNumeric(value, urduNumerals);
    }
    case ListStyleType::Telugu: {
        static constexpr UChar teluguNumerals[10] = {
            0x0C66, 0x0C67, 0x0C68, 0x0C69, 0x0C6A, 0x0C6B, 0x0C6C, 0x0C6D, 0x0C6E, 0x0C6F
        };
        return toNumeric(value, teluguNumerals);
    }
    case ListStyleType::Tibetan: {
        static constexpr UChar tibetanNumerals[10] = {
            0x0F20, 0x0F21, 0x0F22, 0x0F23, 0x0F24, 0x0F25, 0x0F26, 0x0F27, 0x0F28, 0x0F29
        };
        return toNumeric(value, tibetanNumerals);
    }
    case ListStyleType::Thai: {
        static constexpr UChar thaiNumerals[10] = {
            0x0E50, 0x0E51, 0x0E52, 0x0E53, 0x0E54, 0x0E55, 0x0E56, 0x0E57, 0x0E58, 0x0E59
        };
        return toNumeric(value, thaiNumerals);
    }
    case ListStyleType::UpperHexadecimal: {
        static constexpr LChar upperHexadecimalNumerals[16] = {
            '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
        };
        return toNumeric(value, upperHexadecimalNumerals);
    }

    case ListStyleType::LowerAlpha:
    case ListStyleType::LowerLatin: {
        static constexpr LChar lowerLatinAlphabet[26] = {
            'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
            'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z'
        };
        return toAlphabetic(value, lowerLatinAlphabet);
    }
    case ListStyleType::UpperAlpha:
    case ListStyleType::UpperLatin: {
        static constexpr LChar upperLatinAlphabet[26] = {
            'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
            'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z'
        };
        return toAlphabetic(value, upperLatinAlphabet);
    }
    case ListStyleType::LowerGreek: {
        static constexpr UChar lowerGreekAlphabet[24] = {
            0x03B1, 0x03B2, 0x03B3, 0x03B4, 0x03B5, 0x03B6, 0x03B7, 0x03B8,
            0x03B9, 0x03BA, 0x03BB, 0x03BC, 0x03BD, 0x03BE, 0x03BF, 0x03C0,
            0x03C1, 0x03C3, 0x03C4, 0x03C5, 0x03C6, 0x03C7, 0x03C8, 0x03C9
        };
        return toAlphabetic(value, lowerGreekAlphabet);
    }

    case ListStyleType::Hiragana: {
        static constexpr UChar hiraganaAlphabet[48] = {
            0x3042, 0x3044, 0x3046, 0x3048, 0x304A, 0x304B, 0x304D, 0x304F,
            0x3051, 0x3053, 0x3055, 0x3057, 0x3059, 0x305B, 0x305D, 0x305F,
            0x3061, 0x3064, 0x3066, 0x3068, 0x306A, 0x306B, 0x306C, 0x306D,
            0x306E, 0x306F, 0x3072, 0x3075, 0x3078, 0x307B, 0x307E, 0x307F,
            0x3080, 0x3081, 0x3082, 0x3084, 0x3086, 0x3088, 0x3089, 0x308A,
            0x308B, 0x308C, 0x308D, 0x308F, 0x3090, 0x3091, 0x3092, 0x3093
        };
        return toAlphabetic(value, hiraganaAlphabet);
    }
    case ListStyleType::HiraganaIroha: {
        static constexpr UChar hiraganaIrohaAlphabet[47] = {
            0x3044, 0x308D, 0x306F, 0x306B, 0x307B, 0x3078, 0x3068, 0x3061,
            0x308A, 0x306C, 0x308B, 0x3092, 0x308F, 0x304B, 0x3088, 0x305F,
            0x308C, 0x305D, 0x3064, 0x306D, 0x306A, 0x3089, 0x3080, 0x3046,
            0x3090, 0x306E, 0x304A, 0x304F, 0x3084, 0x307E, 0x3051, 0x3075,
            0x3053, 0x3048, 0x3066, 0x3042, 0x3055, 0x304D, 0x3086, 0x3081,
            0x307F, 0x3057, 0x3091, 0x3072, 0x3082, 0x305B, 0x3059
        };
        return toAlphabetic(value, hiraganaIrohaAlphabet);
    }
    case ListStyleType::Katakana: {
        static constexpr UChar katakanaAlphabet[48] = {
            0x30A2, 0x30A4, 0x30A6, 0x30A8, 0x30AA, 0x30AB, 0x30AD, 0x30AF,
            0x30B1, 0x30B3, 0x30B5, 0x30B7, 0x30B9, 0x30BB, 0x30BD, 0x30BF,
            0x30C1, 0x30C4, 0x30C6, 0x30C8, 0x30CA, 0x30CB, 0x30CC, 0x30CD,
            0x30CE, 0x30CF, 0x30D2, 0x30D5, 0x30D8, 0x30DB, 0x30DE, 0x30DF,
            0x30E0, 0x30E1, 0x30E2, 0x30E4, 0x30E6, 0x30E8, 0x30E9, 0x30EA,
            0x30EB, 0x30EC, 0x30ED, 0x30EF, 0x30F0, 0x30F1, 0x30F2, 0x30F3
        };
        return toAlphabetic(value, katakanaAlphabet);
    }
    case ListStyleType::KatakanaIroha: {
        static constexpr UChar katakanaIrohaAlphabet[47] = {
            0x30A4, 0x30ED, 0x30CF, 0x30CB, 0x30DB, 0x30D8, 0x30C8, 0x30C1,
            0x30EA, 0x30CC, 0x30EB, 0x30F2, 0x30EF, 0x30AB, 0x30E8, 0x30BF,
            0x30EC, 0x30BD, 0x30C4, 0x30CD, 0x30CA, 0x30E9, 0x30E0, 0x30A6,
            0x30F0, 0x30CE, 0x30AA, 0x30AF, 0x30E4, 0x30DE, 0x30B1, 0x30D5,
            0x30B3, 0x30A8, 0x30C6, 0x30A2, 0x30B5, 0x30AD, 0x30E6, 0x30E1,
            0x30DF, 0x30B7, 0x30F1, 0x30D2, 0x30E2, 0x30BB, 0x30B9
        };
        return toAlphabetic(value, katakanaIrohaAlphabet);
    }

    case ListStyleType::Afar:
    case ListStyleType::EthiopicHalehameAaEt:
    case ListStyleType::EthiopicHalehameAaEr: {
        static constexpr UChar ethiopicHalehameAaErAlphabet[18] = {
            0x1200, 0x1208, 0x1210, 0x1218, 0x1228, 0x1230, 0x1260, 0x1270, 0x1290,
            0x12A0, 0x12A8, 0x12C8, 0x12D0, 0x12E8, 0x12F0, 0x1308, 0x1338, 0x1348
        };
        return toAlphabetic(value, ethiopicHalehameAaErAlphabet);
    }
    case ListStyleType::Amharic:
    case ListStyleType::EthiopicHalehameAmEt: {
        static constexpr UChar ethiopicHalehameAmEtAlphabet[33] = {
            0x1200, 0x1208, 0x1210, 0x1218, 0x1220, 0x1228, 0x1230, 0x1238, 0x1240,
            0x1260, 0x1270, 0x1278, 0x1280, 0x1290, 0x1298, 0x12A0, 0x12A8, 0x12B8,
            0x12C8, 0x12D0, 0x12D8, 0x12E0, 0x12E8, 0x12F0, 0x1300, 0x1308, 0x1320,
            0x1328, 0x1330, 0x1338, 0x1340, 0x1348, 0x1350
        };
        return toAlphabetic(value, ethiopicHalehameAmEtAlphabet);
    }
    case ListStyleType::AmharicAbegede:
    case ListStyleType::EthiopicAbegedeAmEt: {
        static constexpr UChar ethiopicAbegedeAmEtAlphabet[33] = {
            0x12A0, 0x1260, 0x1308, 0x12F0, 0x1300, 0x1200, 0x12C8, 0x12D8, 0x12E0,
            0x1210, 0x1320, 0x1328, 0x12E8, 0x12A8, 0x12B8, 0x1208, 0x1218, 0x1290,
            0x1298, 0x1220, 0x12D0, 0x1348, 0x1338, 0x1240, 0x1228, 0x1230, 0x1238,
            0x1270, 0x1278, 0x1280, 0x1340, 0x1330, 0x1350
        };
        return toAlphabetic(value, ethiopicAbegedeAmEtAlphabet);
    }
    case ListStyleType::CJKEarthlyBranch: {
        static constexpr UChar cjkEarthlyBranchAlphabet[12] = {
            0x5B50, 0x4E11, 0x5BC5, 0x536F, 0x8FB0, 0x5DF3, 0x5348, 0x672A, 0x7533,
            0x9149, 0x620C, 0x4EA5
        };
        return toAlphabetic(value, cjkEarthlyBranchAlphabet);
    }
    case ListStyleType::CJKHeavenlyStem: {
        static constexpr UChar cjkHeavenlyStemAlphabet[10] = {
            0x7532, 0x4E59, 0x4E19, 0x4E01, 0x620A, 0x5DF1, 0x5E9A, 0x8F9B, 0x58EC,
            0x7678
        };
        return toAlphabetic(value, cjkHeavenlyStemAlphabet);
    }
    case ListStyleType::Ethiopic:
    case ListStyleType::EthiopicHalehameGez: {
        static constexpr UChar ethiopicHalehameGezAlphabet[26] = {
            0x1200, 0x1208, 0x1210, 0x1218, 0x1220, 0x1228, 0x1230, 0x1240, 0x1260,
            0x1270, 0x1280, 0x1290, 0x12A0, 0x12A8, 0x12C8, 0x12D0, 0x12D8, 0x12E8,
            0x12F0, 0x1308, 0x1320, 0x1330, 0x1338, 0x1340, 0x1348, 0x1350
        };
        return toAlphabetic(value, ethiopicHalehameGezAlphabet);
    }
    case ListStyleType::EthiopicAbegede:
    case ListStyleType::EthiopicAbegedeGez: {
        static constexpr UChar ethiopicAbegedeGezAlphabet[26] = {
            0x12A0, 0x1260, 0x1308, 0x12F0, 0x1200, 0x12C8, 0x12D8, 0x1210, 0x1320,
            0x12E8, 0x12A8, 0x1208, 0x1218, 0x1290, 0x1220, 0x12D0, 0x1348, 0x1338,
            0x1240, 0x1228, 0x1230, 0x1270, 0x1280, 0x1340, 0x1330, 0x1350
        };
        return toAlphabetic(value, ethiopicAbegedeGezAlphabet);
    }
    case ListStyleType::HangulConsonant: {
        static constexpr UChar hangulConsonantAlphabet[14] = {
            0x3131, 0x3134, 0x3137, 0x3139, 0x3141, 0x3142, 0x3145, 0x3147, 0x3148,
            0x314A, 0x314B, 0x314C, 0x314D, 0x314E
        };
        return toAlphabetic(value, hangulConsonantAlphabet);
    }
    case ListStyleType::Hangul: {
        static constexpr UChar hangulAlphabet[14] = {
            0xAC00, 0xB098, 0xB2E4, 0xB77C, 0xB9C8, 0xBC14, 0xC0AC, 0xC544, 0xC790,
            0xCC28, 0xCE74, 0xD0C0, 0xD30C, 0xD558
        };
        return toAlphabetic(value, hangulAlphabet);
    }
    case ListStyleType::Oromo:
    case ListStyleType::EthiopicHalehameOmEt: {
        static constexpr UChar ethiopicHalehameOmEtAlphabet[25] = {
            0x1200, 0x1208, 0x1218, 0x1228, 0x1230, 0x1238, 0x1240, 0x1260, 0x1270,
            0x1278, 0x1290, 0x1298, 0x12A0, 0x12A8, 0x12C8, 0x12E8, 0x12F0, 0x12F8,
            0x1300, 0x1308, 0x1320, 0x1328, 0x1338, 0x1330, 0x1348
        };
        return toAlphabetic(value, ethiopicHalehameOmEtAlphabet);
    }
    case ListStyleType::Sidama:
    case ListStyleType::EthiopicHalehameSidEt: {
        static constexpr UChar ethiopicHalehameSidEtAlphabet[26] = {
            0x1200, 0x1208, 0x1210, 0x1218, 0x1228, 0x1230, 0x1238, 0x1240, 0x1260,
            0x1270, 0x1278, 0x1290, 0x1298, 0x12A0, 0x12A8, 0x12C8, 0x12E8, 0x12F0,
            0x12F8, 0x1300, 0x1308, 0x1320, 0x1328, 0x1338, 0x1330, 0x1348
        };
        return toAlphabetic(value, ethiopicHalehameSidEtAlphabet);
    }
    case ListStyleType::Somali:
    case ListStyleType::EthiopicHalehameSoEt: {
        static constexpr UChar ethiopicHalehameSoEtAlphabet[22] = {
            0x1200, 0x1208, 0x1210, 0x1218, 0x1228, 0x1230, 0x1238, 0x1240, 0x1260,
            0x1270, 0x1290, 0x12A0, 0x12A8, 0x12B8, 0x12C8, 0x12D0, 0x12E8, 0x12F0,
            0x1300, 0x1308, 0x1338, 0x1348
        };
        return toAlphabetic(value, ethiopicHalehameSoEtAlphabet);
    }
    case ListStyleType::Tigre:
    case ListStyleType::EthiopicHalehameTig: {
        static constexpr UChar ethiopicHalehameTigAlphabet[27] = {
            0x1200, 0x1208, 0x1210, 0x1218, 0x1228, 0x1230, 0x1238, 0x1240, 0x1260,
            0x1270, 0x1278, 0x1290, 0x12A0, 0x12A8, 0x12C8, 0x12D0, 0x12D8, 0x12E8,
            0x12F0, 0x1300, 0x1308, 0x1320, 0x1328, 0x1338, 0x1330, 0x1348, 0x1350
        };
        return toAlphabetic(value, ethiopicHalehameTigAlphabet);
    }
    case ListStyleType::TigrinyaEr:
    case ListStyleType::EthiopicHalehameTiEr: {
        static constexpr UChar ethiopicHalehameTiErAlphabet[31] = {
            0x1200, 0x1208, 0x1210, 0x1218, 0x1228, 0x1230, 0x1238, 0x1240, 0x1250,
            0x1260, 0x1270, 0x1278, 0x1290, 0x1298, 0x12A0, 0x12A8, 0x12B8, 0x12C8,
            0x12D0, 0x12D8, 0x12E0, 0x12E8, 0x12F0, 0x1300, 0x1308, 0x1320, 0x1328,
            0x1330, 0x1338, 0x1348, 0x1350
        };
        return toAlphabetic(value, ethiopicHalehameTiErAlphabet);
    }
    case ListStyleType::TigrinyaErAbegede:
    case ListStyleType::EthiopicAbegedeTiEr: {
        static constexpr UChar ethiopicAbegedeTiErAlphabet[31] = {
            0x12A0, 0x1260, 0x1308, 0x12F0, 0x1300, 0x1200, 0x12C8, 0x12D8, 0x12E0,
            0x1210, 0x1320, 0x1328, 0x12E8, 0x12A8, 0x12B8, 0x1208, 0x1218, 0x1290,
            0x1298, 0x12D0, 0x1348, 0x1338, 0x1240, 0x1250, 0x1228, 0x1230, 0x1238,
            0x1270, 0x1278, 0x1330, 0x1350
        };
        return toAlphabetic(value, ethiopicAbegedeTiErAlphabet);
    }
    case ListStyleType::TigrinyaEt:
    case ListStyleType::EthiopicHalehameTiEt: {
        static constexpr UChar ethiopicHalehameTiEtAlphabet[34] = {
            0x1200, 0x1208, 0x1210, 0x1218, 0x1220, 0x1228, 0x1230, 0x1238, 0x1240,
            0x1250, 0x1260, 0x1270, 0x1278, 0x1280, 0x1290, 0x1298, 0x12A0, 0x12A8,
            0x12B8, 0x12C8, 0x12D0, 0x12D8, 0x12E0, 0x12E8, 0x12F0, 0x1300, 0x1308,
            0x1320, 0x1328, 0x1330, 0x1338, 0x1340, 0x1348, 0x1350
        };
        return toAlphabetic(value, ethiopicHalehameTiEtAlphabet);
    }
    case ListStyleType::TigrinyaEtAbegede:
    case ListStyleType::EthiopicAbegedeTiEt: {
        static constexpr UChar ethiopicAbegedeTiEtAlphabet[34] = {
            0x12A0, 0x1260, 0x1308, 0x12F0, 0x1300, 0x1200, 0x12C8, 0x12D8, 0x12E0,
            0x1210, 0x1320, 0x1328, 0x12E8, 0x12A8, 0x12B8, 0x1208, 0x1218, 0x1290,
            0x1298, 0x1220, 0x12D0, 0x1348, 0x1338, 0x1240, 0x1250, 0x1228, 0x1230,
            0x1238, 0x1270, 0x1278, 0x1280, 0x1340, 0x1330, 0x1350
        };
        return toAlphabetic(value, ethiopicAbegedeTiEtAlphabet);
    }
    case ListStyleType::UpperGreek: {
        static constexpr UChar upperGreekAlphabet[24] = {
            0x0391, 0x0392, 0x0393, 0x0394, 0x0395, 0x0396, 0x0397, 0x0398, 0x0399,
            0x039A, 0x039B, 0x039C, 0x039D, 0x039E, 0x039F, 0x03A0, 0x03A1, 0x03A3,
            0x03A4, 0x03A5, 0x03A6, 0x03A7, 0x03A8, 0x03A9
        };
        return toAlphabetic(value, upperGreekAlphabet);
    }
    case ListStyleType::LowerNorwegian: {
        static constexpr LChar lowerNorwegianAlphabet[29] = {
            0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
            0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72,
            0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0xE6,
            0xF8, 0xE5
        };
        return toAlphabetic(value, lowerNorwegianAlphabet);
    }
    case ListStyleType::UpperNorwegian: {
        static constexpr LChar upperNorwegianAlphabet[29] = {
            0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
            0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52,
            0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0xC6,
            0xD8, 0xC5
        };
        return toAlphabetic(value, upperNorwegianAlphabet);
    }

    case ListStyleType::SimplifiedChineseInformal: {
        static constexpr UChar simplifiedChineseInformalTable[17] = {
            0x842C, 0x5104, 0x5146, // These three group markers are probably wrong; OK because we don't use this on big enough numbers.
            0x5341, 0x767E, 0x5343,
            0x96F6, 0x4E00, 0x4E8C, 0x4E09, 0x56DB,
            0x4E94, 0x516D, 0x4E03, 0x516B, 0x4E5D,
            0x8D1F
        };
        return toCJKIdeographic(value, simplifiedChineseInformalTable, Formality::Informal);
    }
    case ListStyleType::SimplifiedChineseFormal: {
        static constexpr UChar simplifiedChineseFormalTable[17] = {
            0x842C, 0x5104, 0x5146, // These three group markers are probably wrong; OK because we don't use this on big enough numbers.
            0x62FE, 0x4F70, 0x4EDF,
            0x96F6, 0x58F9, 0x8D30, 0x53C1, 0x8086,
            0x4F0D, 0x9646, 0x67D2, 0x634C, 0x7396,
            0x8D1F
        };
        return toCJKIdeographic(value, simplifiedChineseFormalTable, Formality::Formal);
    }
    case ListStyleType::CJKIdeographic:
    case ListStyleType::TraditionalChineseInformal: {
        static constexpr UChar traditionalChineseInformalTable[17] = {
            0x842C, 0x5104, 0x5146,
            0x5341, 0x767E, 0x5343,
            0x96F6, 0x4E00, 0x4E8C, 0x4E09, 0x56DB,
            0x4E94, 0x516D, 0x4E03, 0x516B, 0x4E5D,
            0x8CA0
        };
        return toCJKIdeographic(value, traditionalChineseInformalTable, Formality::Informal);
    }
    case ListStyleType::TraditionalChineseFormal: {
        static constexpr UChar traditionalChineseFormalTable[17] = {
            0x842C, 0x5104, 0x5146, // These three group markers are probably wrong; OK because we don't use this on big enough numbers.
            0x62FE, 0x4F70, 0x4EDF,
            0x96F6, 0x58F9, 0x8CB3, 0x53C3, 0x8086,
            0x4F0D, 0x9678, 0x67D2, 0x634C, 0x7396,
            0x8CA0
        };
        return toCJKIdeographic(value, traditionalChineseFormalTable, Formality::Formal);
    }

    case ListStyleType::LowerRoman: {
        static CONSTEXPR_WITH_MSVC_INITIALIZER_LIST_WORKAROUND AdditiveSystem lowerRomanSystem {
            {
                { 1000, { 'm' } },
                { 900, { 'c', 'm' } },
                { 500, { 'd' } },
                { 400, { 'c', 'd' } },
                { 100, { 'c' } },
                { 90, { 'x', 'c' } },
                { 50, { 'l' } },
                { 40, { 'x', 'l' } },
                { 10, { 'x' } },
                { 9, { 'i', 'x' } },
                { 5, { 'v' } },
                { 4, { 'i', 'v' } },
                { 1, { 'i' } }
            },
            { }
        };
        return toPredefinedAdditiveSystem(value, lowerRomanSystem);
    }
    case ListStyleType::UpperRoman: {
        static CONSTEXPR_WITH_MSVC_INITIALIZER_LIST_WORKAROUND AdditiveSystem upperRomanSystem {
            {
                { 1000, { 'M' } },
                { 900, { 'C', 'M' } },
                { 500, { 'D' } },
                { 400, { 'C', 'D' } },
                { 100, { 'C' } },
                { 90, { 'X', 'C' } },
                { 50, { 'L' } },
                { 40, { 'X', 'L' } },
                { 10, { 'X' } },
                { 9, { 'I', 'X' } },
                { 5, { 'V' } },
                { 4, { 'I', 'V' } },
                { 1, { 'I' } }
            },
            { }
        };
        return toPredefinedAdditiveSystem(value, upperRomanSystem);
    }

    case ListStyleType::Armenian:
    case ListStyleType::UpperArmenian: {
        static CONSTEXPR_WITH_MSVC_INITIALIZER_LIST_WORKAROUND AdditiveSystem upperArmenianSystem {
            {
                { 9000, { 0x554 } },
                { 8000, { 0x553 } },
                { 7000, { 0x552 } },
                { 6000, { 0x551 } },
                { 5000, { 0x550 } },
                { 4000, { 0x54F } },
                { 3000, { 0x54E } },
                { 2000, { 0x54D } },
                { 1000, { 0x54C } },
                { 900, { 0x54B } },
                { 800, { 0x54A } },
                { 700, { 0x549 } },
                { 600, { 0x548 } },
                { 500, { 0x547 } },
                { 400, { 0x546 } },
                { 300, { 0x545 } },
                { 200, { 0x544 } },
                { 100, { 0x543 } },
                { 90, { 0x542 } },
                { 80, { 0x541 } },
                { 70, { 0x540 } },
                { 60, { 0x53F } },
                { 50, { 0x53E } },
                { 40, { 0x53D } },
                { 30, { 0x53C } },
                { 20, { 0x53B } },
                { 10, { 0x53A } },
                { 9, { 0x539 } },
                { 8, { 0x538 } },
                { 7, { 0x537 } },
                { 6, { 0x536 } },
                { 5, { 0x535 } },
                { 4, { 0x534 } },
                { 3, { 0x533 } },
                { 2, { 0x532 } },
                { 1, { 0x531 } }
            },
            { }
        };
        return toPredefinedAdditiveSystem(value, upperArmenianSystem);
    }
    case ListStyleType::LowerArmenian: {
        static CONSTEXPR_WITH_MSVC_INITIALIZER_LIST_WORKAROUND AdditiveSystem lowerArmenianSystem {
            {
                { 9000, { 0x584 } },
                { 8000, { 0x583 } },
                { 7000, { 0x582 } },
                { 6000, { 0x581 } },
                { 5000, { 0x580 } },
                { 4000, { 0x57F } },
                { 3000, { 0x57E } },
                { 2000, { 0x57D } },
                { 1000, { 0x57C } },
                { 900, { 0x57B } },
                { 800, { 0x57A } },
                { 700, { 0x579 } },
                { 600, { 0x578 } },
                { 500, { 0x577 } },
                { 400, { 0x576 } },
                { 300, { 0x575 } },
                { 200, { 0x574 } },
                { 100, { 0x573 } },
                { 90, { 0x572 } },
                { 80, { 0x571 } },
                { 70, { 0x570 } },
                { 60, { 0x56F } },
                { 50, { 0x56E } },
                { 40, { 0x56D } },
                { 30, { 0x56C } },
                { 20, { 0x56B } },
                { 10, { 0x56A } },
                { 9, { 0x569 } },
                { 8, { 0x568 } },
                { 7, { 0x567 } },
                { 6, { 0x566 } },
                { 5, { 0x565 } },
                { 4, { 0x564 } },
                { 3, { 0x563 } },
                { 2, { 0x562 } },
                { 1, { 0x561 } }
            },
            { }
        };
        return toPredefinedAdditiveSystem(value, lowerArmenianSystem);
    }

    case ListStyleType::Georgian: {
        static CONSTEXPR_WITH_MSVC_INITIALIZER_LIST_WORKAROUND AdditiveSystem georgianSystem {
            {
                { 10000, { 0x10F5 } },
                { 9000, { 0x10F0 } },
                { 8000, { 0x10EF } },
                { 7000, { 0x10F4 } },
                { 6000, { 0x10EE } },
                { 5000, { 0x10ED } },
                { 4000, { 0x10EC } },
                { 3000, { 0x10EB } },
                { 2000, { 0x10EA } },
                { 1000, { 0x10E9 } },
                { 900, { 0x10E8 } },
                { 800, { 0x10E7 } },
                { 700, { 0x10E6 } },
                { 600, { 0x10E5 } },
                { 500, { 0x10E4 } },
                { 400, { 0x10F3 } },
                { 300, { 0x10E2 } },
                { 200, { 0x10E1 } },
                { 100, { 0x10E0 } },
                { 90, { 0x10DF } },
                { 80, { 0x10DE } },
                { 70, { 0x10DD } },
                { 60, { 0x10F2 } },
                { 50, { 0x10DC } },
                { 40, { 0x10DB } },
                { 30, { 0x10DA } },
                { 20, { 0x10D9 } },
                { 10, { 0x10D8 } },
                { 9, { 0x10D7 } },
                { 8, { 0x10F1 } },
                { 7, { 0x10D6 } },
                { 6, { 0x10D5 } },
                { 5, { 0x10D4 } },
                { 4, { 0x10D3 } },
                { 3, { 0x10D2 } },
                { 2, { 0x10D1 } },
                { 1, { 0x10D0 } }
            },
            { }
        };
        return toPredefinedAdditiveSystem(value, georgianSystem);
    }
    case ListStyleType::Hebrew: {
        static CONSTEXPR_WITH_MSVC_INITIALIZER_LIST_WORKAROUND AdditiveSystem hebrewSystem {
            {
                { 10000, { 0x5D9, 0x5F3 } },
                { 9000, { 0x5D8, 0x5F3 } },
                { 8000, { 0x5D7, 0x5F3 } },
                { 7000, { 0x5D6, 0x5F3 } },
                { 6000, { 0x5D5, 0x5F3 } },
                { 5000, { 0x5D4, 0x5F3 } },
                { 4000, { 0x5D3, 0x5F3 } },
                { 3000, { 0x5D2, 0x5F3 } },
                { 2000, { 0x5D1, 0x5F3 } },
                { 1000, { 0x5D0, 0x5F3 } },
                { 400, { 0x5EA } },
                { 300, { 0x5E9 } },
                { 200, { 0x5E8 } },
                { 100, { 0x5E7 } },
                { 90, { 0x5E6 } },
                { 80, { 0x5E4 } },
                { 70, { 0x5E2 } },
                { 60, { 0x5E1 } },
                { 50, { 0x5E0 } },
                { 40, { 0x5DE } },
                { 30, { 0x5DC } },
                { 20, { 0x5DB } },
                { 19, { 0x5D9, 0x5D8 } },
                { 18, { 0x5D9, 0x5D7 } },
                { 17, { 0x5D9, 0x5D6 } },
                { 16, { 0x5D8, 0x5D6 } },
                { 15, { 0x5D8, 0x5D5 } },
                { 10, { 0x5D9 } },
                { 9, { 0x5D8 } },
                { 8, { 0x5D7 } },
                { 7, { 0x5D6 } },
                { 6, { 0x5D5 } },
                { 5, { 0x5D4 } },
                { 4, { 0x5D3 } },
                { 3, { 0x5D2 } },
                { 2, { 0x5D1 } },
                { 1, { 0x5D0 } }
            },
            { }
        };
        return toPredefinedAdditiveSystem(value, hebrewSystem);
    }

    case ListStyleType::CJKDecimal: {
        static constexpr UChar CJKDecimalNumerals[10] = {
            0x3007, 0x4E00, 0x4E8C, 0x4E09, 0x56DB, 0x4E94, 0x516D, 0x4E03, 0x516B, 0x4E5D
        };
        return toNumeric(value, CJKDecimalNumerals);
    }
    case ListStyleType::Tamil: {
        static constexpr UChar tamilNumerals[10] = {
            0x0BE6, 0x0BE7, 0x0BE8, 0x0BE9, 0x0BEA, 0x0BEB, 0x0BEC, 0x0BED, 0x0BEE, 0x0BEF
        };
        return toNumeric(value, tamilNumerals);
    }

    case ListStyleType::JapaneseInformal: {
        static CONSTEXPR_WITH_MSVC_INITIALIZER_LIST_WORKAROUND AdditiveSystem japaneseInformalSystem {
            {
                { 9000, { 0x4E5D, 0x5343 } },
                { 8000, { 0x516B, 0x5343 } },
                { 7000, { 0x4E03, 0x5343 } },
                { 6000, { 0x516D, 0x5343 } },
                { 5000, { 0x4E94, 0x5343 } },
                { 4000, { 0x56DB, 0x5343 } },
                { 3000, { 0x4E09, 0x5343 } },
                { 2000, { 0x4E8C, 0x5343 } },
                { 1000, { 0x5343 } },
                { 900, { 0x4E5D, 0x767E } },
                { 800, { 0x516B, 0x767E } },
                { 700, { 0x4E03, 0x767E } },
                { 600, { 0x516D, 0x767E } },
                { 500, { 0x4E94, 0x767E } },
                { 400, { 0x56DB, 0x767E } },
                { 300, { 0x4E09, 0x767E } },
                { 200, { 0x4E8C, 0x767E } },
                { 100, { 0x767E } },
                { 90, { 0x4E5D, 0x5341 } },
                { 80, { 0x516B, 0x5341 } },
                { 70, { 0x4E03, 0x5341 } },
                { 60, { 0x516D, 0x5341 } },
                { 50, { 0x4E94, 0x5341 } },
                { 40, { 0x56DB, 0x5341 } },
                { 30, { 0x4E09, 0x5341 } },
                { 20, { 0x4E8C, 0x5341 } },
                { 10, { 0x5341 } },
                { 9, { 0x4E5D } },
                { 8, { 0x516B } },
                { 7, { 0x4E03 } },
                { 6, { 0x516D } },
                { 5, { 0x4E94 } },
                { 4, { 0x56DB } },
                { 3, { 0x4E09 } },
                { 2, { 0x4E8C } },
                { 1, { 0x4E00 } },
                { 0, { 0x3007 } }
            },
            { 0x30DE, 0x30A4, 0x30CA, 0x30B9 }
        };
        return toPredefinedAdditiveSystem(value, japaneseInformalSystem);
    }
    case ListStyleType::JapaneseFormal: {
        static CONSTEXPR_WITH_MSVC_INITIALIZER_LIST_WORKAROUND AdditiveSystem japaneseFormalSystem {
            {
                { 9000, { 0x4E5D, 0x9621 } },
                { 8000, { 0x516B, 0x9621 } },
                { 7000, { 0x4E03, 0x9621 } },
                { 6000, { 0x516D, 0x9621 } },
                { 5000, { 0x4F0D, 0x9621 } },
                { 4000, { 0x56DB, 0x9621 } },
                { 3000, { 0x53C2, 0x9621 } },
                { 2000, { 0x5F10, 0x9621 } },
                { 1000, { 0x58F1, 0x9621 } },
                { 900, { 0x4E5D, 0x767E } },
                { 800, { 0x516B, 0x767E } },
                { 700, { 0x4E03, 0x767E } },
                { 600, { 0x516D, 0x767E } },
                { 500, { 0x4F0D, 0x767E } },
                { 400, { 0x56DB, 0x767E } },
                { 300, { 0x53C2, 0x767E } },
                { 200, { 0x5F10, 0x767E } },
                { 100, { 0x58F1, 0x767E } },
                { 90, { 0x4E5D, 0x62FE } },
                { 80, { 0x516B, 0x62FE } },
                { 70, { 0x4E03, 0x62FE } },
                { 60, { 0x516D, 0x62FE } },
                { 50, { 0x4F0D, 0x62FE } },
                { 40, { 0x56DB, 0x62FE } },
                { 30, { 0x53C2, 0x62FE } },
                { 20, { 0x5F10, 0x62FE } },
                { 10, { 0x58F1, 0x62FE } },
                { 9, { 0x4E5D } },
                { 8, { 0x516B } },
                { 7, { 0x4E03 } },
                { 6, { 0x516D } },
                { 5, { 0x4F0D } },
                { 4, { 0x56DB } },
                { 3, { 0x53C2 } },
                { 2, { 0x5F10 } },
                { 1, { 0x58F1 } },
                { 0, { 0x96F6 } }
            },
            { 0x30DE, 0x30A4, 0x30CA, 0x30B9 }
        };
        return toPredefinedAdditiveSystem(value, japaneseFormalSystem);
    }
    case ListStyleType::KoreanHangulFormal: {
        static CONSTEXPR_WITH_MSVC_INITIALIZER_LIST_WORKAROUND AdditiveSystem koreanHangulFormalSystem {
            {
                { 9000, { 0xAD6C, 0xCC9C } },
                { 8000, { 0xD314, 0xCC9C } },
                { 7000, { 0xCE60, 0xCC9C } },
                { 6000, { 0xC721, 0xCC9C } },
                { 5000, { 0xC624, 0xCC9C } },
                { 4000, { 0xC0AC, 0xCC9C } },
                { 3000, { 0xC0BC, 0xCC9C } },
                { 2000, { 0xC774, 0xCC9C } },
                { 1000, { 0xC77C, 0xCC9C } },
                { 900, { 0xAD6C, 0xBC31 } },
                { 800, { 0xD314, 0xBC31 } },
                { 700, { 0xCE60, 0xBC31 } },
                { 600, { 0xC721, 0xBC31 } },
                { 500, { 0xC624, 0xBC31 } },
                { 400, { 0xC0AC, 0xBC31 } },
                { 300, { 0xC0BC, 0xBC31 } },
                { 200, { 0xC774, 0xBC31 } },
                { 100, { 0xC77C, 0xBC31 } },
                { 90, { 0xAD6C, 0xC2ED } },
                { 80, { 0xD314, 0xC2ED } },
                { 70, { 0xCE60, 0xC2ED } },
                { 60, { 0xC721, 0xC2ED } },
                { 50, { 0xC624, 0xC2ED } },
                { 40, { 0xC0AC, 0xC2ED } },
                { 30, { 0xC0BC, 0xC2ED } },
                { 20, { 0xC774, 0xC2ED } },
                { 10, { 0xC77C, 0xC2ED } },
                { 9, { 0xAD6C } },
                { 8, { 0xD314 } },
                { 7, { 0xCE60 } },
                { 6, { 0xC721 } },
                { 5, { 0xC624 } },
                { 4, { 0xC0AC } },
                { 3, { 0xC0BC } },
                { 2, { 0xC774 } },
                { 1, { 0xC77C } },
                { 0, { 0xC601 } }
            },
            { 0xB9C8, 0xC774, 0xB108, 0xC2A4, ' ' }
        };
        return toPredefinedAdditiveSystem(value, koreanHangulFormalSystem);
    }
    case ListStyleType::KoreanHanjaInformal: {
        static CONSTEXPR_WITH_MSVC_INITIALIZER_LIST_WORKAROUND AdditiveSystem koreanHanjaInformalSystem {
            {
                { 9000, { 0x4E5D, 0x5343 } },
                { 8000, { 0x516B, 0x5343 } },
                { 7000, { 0x4E03, 0x5343 } },
                { 6000, { 0x516D, 0x5343 } },
                { 5000, { 0x4E94, 0x5343 } },
                { 4000, { 0x56DB, 0x5343 } },
                { 3000, { 0x4E09, 0x5343 } },
                { 2000, { 0x4E8C, 0x5343 } },
                { 1000, { 0x5343 } },
                { 900, { 0x4E5D, 0x767E } },
                { 800, { 0x516B, 0x767E } },
                { 700, { 0x4E03, 0x767E } },
                { 600, { 0x516D, 0x767E } },
                { 500, { 0x4E94, 0x767E } },
                { 400, { 0x56DB, 0x767E } },
                { 300, { 0x4E09, 0x767E } },
                { 200, { 0x4E8C, 0x767E } },
                { 100, { 0x767E } },
                { 90, { 0x4E5D, 0x5341 } },
                { 80, { 0x516B, 0x5341 } },
                { 70, { 0x4E03, 0x5341 } },
                { 60, { 0x516D, 0x5341 } },
                { 50, { 0x4E94, 0x5341 } },
                { 40, { 0x56DB, 0x5341 } },
                { 30, { 0x4E09, 0x5341 } },
                { 20, { 0x4E8C, 0x5341 } },
                { 10, { 0x5341 } },
                { 9, { 0x4E5D } },
                { 8, { 0x516B } },
                { 7, { 0x4E03 } },
                { 6, { 0x516D } },
                { 5, { 0x4E94 } },
                { 4, { 0x56DB } },
                { 3, { 0x4E09 } },
                { 2, { 0x4E8C } },
                { 1, { 0x4E00 } },
                { 0, { 0x96F6 } }
            },
            { 0xB9C8, 0xC774, 0xB108, 0xC2A4, ' ' }
        };
        return toPredefinedAdditiveSystem(value, koreanHanjaInformalSystem);
    }
    case ListStyleType::KoreanHanjaFormal: {
        static CONSTEXPR_WITH_MSVC_INITIALIZER_LIST_WORKAROUND AdditiveSystem koreanHanjaFormalSystem {
            {
                { 9000, { 0x4E5D, 0x4EDF } },
                { 8000, { 0x516B, 0x4EDF } },
                { 7000, { 0x4E03, 0x4EDF } },
                { 6000, { 0x516D, 0x4EDF } },
                { 5000, { 0x4E94, 0x4EDF } },
                { 4000, { 0x56DB, 0x4EDF } },
                { 3000, { 0x53C3, 0x4EDF } },
                { 2000, { 0x8CB3, 0x4EDF } },
                { 1000, { 0x58F9, 0x4EDF } },
                { 900, { 0x4E5D, 0x767E } },
                { 800, { 0x516B, 0x767E } },
                { 700, { 0x4E03, 0x767E } },
                { 600, { 0x516D, 0x767E } },
                { 500, { 0x4E94, 0x767E } },
                { 400, { 0x56DB, 0x767E } },
                { 300, { 0x53C3, 0x767E } },
                { 200, { 0x8CB3, 0x767E } },
                { 100, { 0x58F9, 0x767E } },
                { 90, { 0x4E5D, 0x62FE } },
                { 80, { 0x516B, 0x62FE } },
                { 70, { 0x4E03, 0x62FE } },
                { 60, { 0x516D, 0x62FE } },
                { 50, { 0x4E94, 0x62FE } },
                { 40, { 0x56DB, 0x62FE } },
                { 30, { 0x53C3, 0x62FE } },
                { 20, { 0x8CB3, 0x62FE } },
                { 10, { 0x58F9, 0x62FE } },
                { 9, { 0x4E5D } },
                { 8, { 0x516B } },
                { 7, { 0x4E03 } },
                { 6, { 0x516D } },
                { 5, { 0x4E94 } },
                { 4, { 0x56DB } },
                { 3, { 0x53C3 } },
                { 2, { 0x8CB3 } },
                { 1, { 0x58F9 } },
                { 0, { 0x96F6 } }
            },
            { 0xB9C8, 0xC774, 0xB108, 0xC2A4, ' ' }
        };
        return toPredefinedAdditiveSystem(value, koreanHanjaFormalSystem);
    }

    case ListStyleType::EthiopicNumeric:
        return toEthiopicNumeric(value);

    case ListStyleType::String:
        ASSERT_NOT_REACHED();
        break;
    }

    ASSERT_NOT_REACHED();
    return emptyString();
}

RenderListMarker::RenderListMarker(RenderListItem& listItem, RenderStyle&& style)
    : RenderBox(listItem.document(), WTFMove(style), 0)
    , m_listItem(listItem)
{
    setInline(true);
    setReplacedOrInlineBlock(true); // pretend to be replaced
}

RenderListMarker::~RenderListMarker()
{
    // Do not add any code here. Add it to willBeDestroyed() instead.
}

void RenderListMarker::willBeDestroyed()
{
    if (m_image)
        m_image->removeClient(*this);
    RenderBox::willBeDestroyed();
}

void RenderListMarker::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBox::styleDidChange(diff, oldStyle);
    if (oldStyle) {
        if (style().listStylePosition() != oldStyle->listStylePosition() || style().listStyleType() != oldStyle->listStyleType() || (style().listStyleType() == ListStyleType::String && style().listStyleStringValue() != oldStyle->listStyleStringValue()))
            setNeedsLayoutAndPrefWidthsRecalc();
        if (oldStyle->isDisplayInlineType() && !style().isDisplayInlineType()) {
            setNeedsLayoutAndPrefWidthsRecalc();
            if (m_inlineBoxWrapper)
                m_inlineBoxWrapper->dirtyLineBoxes();
            m_inlineBoxWrapper = nullptr;
        }
    }

    if (m_image != style().listStyleImage()) {
        if (m_image)
            m_image->removeClient(*this);
        m_image = style().listStyleImage();
        if (m_image)
            m_image->addClient(*this);
    }
}

std::unique_ptr<LegacyInlineElementBox> RenderListMarker::createInlineBox()
{
    auto box = RenderBox::createInlineBox();
    box->setBehavesLikeText(!isImage());
    return box;
}

bool RenderListMarker::isImage() const
{
    return m_image && !m_image->errorOccurred();
}

LayoutRect RenderListMarker::localSelectionRect()
{
    LegacyInlineBox* box = inlineBoxWrapper();
    if (!box)
        return LayoutRect(LayoutPoint(), size());
    const LegacyRootInlineBox& rootBox = m_inlineBoxWrapper->root();
    LayoutUnit newLogicalTop { rootBox.blockFlow().style().isFlippedBlocksWritingMode() ? m_inlineBoxWrapper->logicalBottom() - rootBox.selectionBottom() : rootBox.selectionTop() - m_inlineBoxWrapper->logicalTop() };
    if (rootBox.blockFlow().style().isHorizontalWritingMode())
        return LayoutRect(0_lu, newLogicalTop, width(), rootBox.selectionHeight());
    return LayoutRect(newLogicalTop, 0_lu, rootBox.selectionHeight(), height());
}

static String reversed(StringView string)
{
    auto length = string.length();
    if (length <= 1)
        return string.toString();
    UChar* characters;
    auto result = String::createUninitialized(length, characters);
    for (unsigned i = 0; i < length; ++i)
        *characters++ = string[length - i - 1];
    return result;
}

struct RenderListMarker::TextRunWithUnderlyingString {
    TextRun textRun;
    String underlyingString;
    operator const TextRun&() const { return textRun; }
};

auto RenderListMarker::textRun() const -> TextRunWithUnderlyingString
{
    ASSERT(!m_textWithSuffix.isEmpty());

    // Since the bidi algorithm doesn't run on this text, we instead reorder the characters here.
    // We use u_charDirection to figure out if the marker text is RTL and assume the suffix matches the surrounding direction.
    String textForRun;
    if (m_textIsLeftToRightDirection) {
        if (style().isLeftToRightDirection())
            textForRun = m_textWithSuffix;
        else {
            if (style().listStyleType() == ListStyleType::DisclosureClosed)
                textForRun = { &blackLeftPointingSmallTriangle, 1 };
            else
                textForRun = makeString(reversed(StringView(m_textWithSuffix).substring(m_textWithoutSuffixLength)), m_textWithSuffix.left(m_textWithoutSuffixLength));
        }
    } else {
        if (!style().isLeftToRightDirection())
            textForRun = reversed(m_textWithSuffix);
        else
            textForRun = makeString(reversed(StringView(m_textWithSuffix).left(m_textWithoutSuffixLength)), m_textWithSuffix.substring(m_textWithoutSuffixLength));
    }
    auto textRun = RenderBlock::constructTextRun(textForRun, style());
    return { WTFMove(textRun), WTFMove(textForRun) };
}

void RenderListMarker::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (paintInfo.phase != PaintPhase::Foreground)
        return;
    
    if (style().visibility() != Visibility::Visible)
        return;

    LayoutPoint boxOrigin(paintOffset + location());
    LayoutRect overflowRect(visualOverflowRect());
    overflowRect.moveBy(boxOrigin);
    if (!paintInfo.rect.intersects(overflowRect))
        return;

    LayoutRect box(boxOrigin, size());
    
    auto markerRect = relativeMarkerRect();
    markerRect.moveBy(boxOrigin);
    if (markerRect.isEmpty())
        return;

    GraphicsContext& context = paintInfo.context();

    if (isImage()) {
        if (RefPtr<Image> markerImage = m_image->image(this, markerRect.size()))
            context.drawImage(*markerImage, markerRect);
        if (selectionState() != HighlightState::None) {
            LayoutRect selRect = localSelectionRect();
            selRect.moveBy(boxOrigin);
            context.fillRect(snappedIntRect(selRect), m_listItem->selectionBackgroundColor());
        }
        return;
    }

    if (selectionState() != HighlightState::None) {
        LayoutRect selRect = localSelectionRect();
        selRect.moveBy(boxOrigin);
        context.fillRect(snappedIntRect(selRect), m_listItem->selectionBackgroundColor());
    }

    auto color = style().visitedDependentColorWithColorFilter(CSSPropertyColor);
    context.setStrokeColor(color);
    context.setStrokeStyle(SolidStroke);
    context.setStrokeThickness(1.0f);
    context.setFillColor(color);

    switch (style().listStyleType()) {
    case ListStyleType::Disc:
        context.fillEllipse(markerRect);
        return;
    case ListStyleType::Circle:
        context.strokeEllipse(markerRect);
        return;
    case ListStyleType::Square:
        context.fillRect(markerRect);
        return;
    default:
        break;
    }
    if (m_textWithSuffix.isEmpty())
        return;

    GraphicsContextStateSaver stateSaver(context, false);
    if (!style().isHorizontalWritingMode()) {
        markerRect.moveBy(-boxOrigin);
        markerRect = markerRect.transposedRect();
        markerRect.moveBy(FloatPoint(box.x(), box.y() - logicalHeight()));
        stateSaver.save();
        context.translate(markerRect.x(), markerRect.maxY());
        context.rotate(static_cast<float>(deg2rad(90.)));
        context.translate(-markerRect.x(), -markerRect.maxY());
    }

    FloatPoint textOrigin = FloatPoint(markerRect.x(), markerRect.y() + style().metricsOfPrimaryFont().ascent());
    textOrigin = roundPointToDevicePixels(LayoutPoint(textOrigin), document().deviceScaleFactor(), style().isLeftToRightDirection());
    context.drawText(style().fontCascade(), textRun(), textOrigin);
}

RenderBox* RenderListMarker::parentBox(RenderBox& box)
{
    ASSERT(m_listItem);
    auto* fragmentedFlow = m_listItem->enclosingFragmentedFlow();
    if (!is<RenderMultiColumnFlow>(fragmentedFlow))
        return box.parentBox();
    auto* placeholder = downcast<RenderMultiColumnFlow>(*fragmentedFlow).findColumnSpannerPlaceholder(&box);
    if (!placeholder)
        return box.parentBox();
    return placeholder->parentBox();
};

void RenderListMarker::addOverflowFromListMarker()
{
    ASSERT(m_listItem);
    if (!parent() || !parent()->isBox())
        return;

    if (isInside() || !inlineBoxWrapper())
        return;

    LayoutUnit markerOldLogicalLeft = logicalLeft();
    LayoutUnit blockOffset;
    LayoutUnit lineOffset;
    for (auto* ancestor = parentBox(*this); ancestor && ancestor != m_listItem.get(); ancestor = parentBox(*ancestor)) {
        blockOffset += ancestor->logicalTop();
        lineOffset += ancestor->logicalLeft();
    }

    bool adjustOverflow = false;
    LayoutUnit markerLogicalLeft;
    bool hitSelfPaintingLayer = false;

    const LegacyRootInlineBox& rootBox = inlineBoxWrapper()->root();
    LayoutUnit lineTop = rootBox.lineTop();
    LayoutUnit lineBottom = rootBox.lineBottom();

    // FIXME: Need to account for relative positioning in the layout overflow.
    if (m_listItem->style().isLeftToRightDirection()) {
        markerLogicalLeft = m_lineOffsetForListItem - lineOffset - m_listItem->paddingStart() - m_listItem->borderStart() + marginStart();
        inlineBoxWrapper()->adjustLineDirectionPosition(markerLogicalLeft - markerOldLogicalLeft);
        for (auto* box = inlineBoxWrapper()->parent(); box; box = box->parent()) {
            auto newLogicalVisualOverflowRect = box->logicalVisualOverflowRect(lineTop, lineBottom);
            auto newLogicalLayoutOverflowRect = box->logicalLayoutOverflowRect(lineTop, lineBottom);
            if (markerLogicalLeft < newLogicalVisualOverflowRect.x() && !hitSelfPaintingLayer) {
                newLogicalVisualOverflowRect.setWidth(newLogicalVisualOverflowRect.maxX() - markerLogicalLeft);
                newLogicalVisualOverflowRect.setX(markerLogicalLeft);
                if (box == &rootBox)
                    adjustOverflow = true;
            }
            if (markerLogicalLeft < newLogicalLayoutOverflowRect.x()) {
                newLogicalLayoutOverflowRect.setWidth(newLogicalLayoutOverflowRect.maxX() - markerLogicalLeft);
                newLogicalLayoutOverflowRect.setX(markerLogicalLeft);
                if (box == &rootBox)
                    adjustOverflow = true;
            }
            box->setOverflowFromLogicalRects(newLogicalLayoutOverflowRect, newLogicalVisualOverflowRect, lineTop, lineBottom);
            if (box->renderer().hasSelfPaintingLayer())
                hitSelfPaintingLayer = true;
        }
    } else {
        markerLogicalLeft = m_lineOffsetForListItem - lineOffset + m_listItem->paddingStart() + m_listItem->borderStart() + marginEnd();
        inlineBoxWrapper()->adjustLineDirectionPosition(markerLogicalLeft - markerOldLogicalLeft);
        for (auto* box = inlineBoxWrapper()->parent(); box; box = box->parent()) {
            auto newLogicalVisualOverflowRect = box->logicalVisualOverflowRect(lineTop, lineBottom);
            auto newLogicalLayoutOverflowRect = box->logicalLayoutOverflowRect(lineTop, lineBottom);
            if (markerLogicalLeft + logicalWidth() > newLogicalVisualOverflowRect.maxX() && !hitSelfPaintingLayer) {
                newLogicalVisualOverflowRect.setWidth(markerLogicalLeft + logicalWidth() - newLogicalVisualOverflowRect.x());
                if (box == &rootBox)
                    adjustOverflow = true;
            }
            if (markerLogicalLeft + logicalWidth() > newLogicalLayoutOverflowRect.maxX()) {
                newLogicalLayoutOverflowRect.setWidth(markerLogicalLeft + logicalWidth() - newLogicalLayoutOverflowRect.x());
                if (box == &rootBox)
                    adjustOverflow = true;
            }
            box->setOverflowFromLogicalRects(newLogicalLayoutOverflowRect, newLogicalVisualOverflowRect, lineTop, lineBottom);
            if (box->renderer().hasSelfPaintingLayer())
                hitSelfPaintingLayer = true;
        }
    }

    if (adjustOverflow) {
        LayoutRect markerRect(markerLogicalLeft + lineOffset, blockOffset, width(), height());
        if (!m_listItem->style().isHorizontalWritingMode())
            markerRect = markerRect.transposedRect();
        RenderBox* markerAncestor = this;
        bool propagateVisualOverflow = true;
        bool propagateLayoutOverflow = true;
        do {
            markerAncestor = parentBox(*markerAncestor);
            if (markerAncestor->hasNonVisibleOverflow())
                propagateVisualOverflow = false;
            if (is<RenderBlock>(*markerAncestor)) {
                if (propagateVisualOverflow)
                    downcast<RenderBlock>(*markerAncestor).addVisualOverflow(markerRect);
                if (propagateLayoutOverflow)
                    downcast<RenderBlock>(*markerAncestor).addLayoutOverflow(markerRect);
            }
            if (markerAncestor->hasNonVisibleOverflow())
                propagateLayoutOverflow = false;
            if (markerAncestor->hasSelfPaintingLayer())
                propagateVisualOverflow = false;
            markerRect.moveBy(-markerAncestor->location());
        } while (markerAncestor != m_listItem.get() && propagateVisualOverflow && propagateLayoutOverflow);
    }
}

void RenderListMarker::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    ASSERT(needsLayout());

    LayoutUnit blockOffset;
    for (auto* ancestor = parentBox(*this); ancestor && ancestor != m_listItem.get(); ancestor = parentBox(*ancestor))
        blockOffset += ancestor->logicalTop();

    m_lineLogicalOffsetForListItem = m_listItem->logicalLeftOffsetForLine(blockOffset, DoNotIndentText, 0_lu);
    m_lineOffsetForListItem = style().isLeftToRightDirection() ? m_lineLogicalOffsetForListItem : m_listItem->logicalRightOffsetForLine(blockOffset, DoNotIndentText, 0_lu);

    if (isImage()) {
        updateMarginsAndContent();
        setWidth(m_image->imageSize(this, style().effectiveZoom()).width());
        setHeight(m_image->imageSize(this, style().effectiveZoom()).height());
    } else {
        setLogicalWidth(minPreferredLogicalWidth());
        setLogicalHeight(style().metricsOfPrimaryFont().height());
    }

    setMarginStart(0);
    setMarginEnd(0);

    Length startMargin = style().marginStart();
    Length endMargin = style().marginEnd();
    if (startMargin.isFixed())
        setMarginStart(LayoutUnit(startMargin.value()));
    if (endMargin.isFixed())
        setMarginEnd(LayoutUnit(endMargin.value()));

    clearNeedsLayout();
}

void RenderListMarker::imageChanged(WrappedImagePtr o, const IntRect* rect)
{
    if (m_image && o == m_image->data()) {
        if (width() != m_image->imageSize(this, style().effectiveZoom()).width() || height() != m_image->imageSize(this, style().effectiveZoom()).height() || m_image->errorOccurred())
            setNeedsLayoutAndPrefWidthsRecalc();
        else
            repaint();
    }
    RenderBox::imageChanged(o, rect);
}

void RenderListMarker::updateMarginsAndContent()
{
    // FIXME: It's messy to use the preferredLogicalWidths dirty bit for this optimization, also unclear if this is premature optimization.
    if (preferredLogicalWidthsDirty())
        updateContent();
    updateMargins();
}

void RenderListMarker::updateContent()
{
    if (isImage()) {
        // FIXME: This is a somewhat arbitrary width.  Generated images for markers really won't become particularly useful
        // until we support the CSS3 marker pseudoclass to allow control over the width and height of the marker box.
        LayoutUnit bulletWidth = style().metricsOfPrimaryFont().ascent() / 2_lu;
        LayoutSize defaultBulletSize(bulletWidth, bulletWidth);
        LayoutSize imageSize = calculateImageIntrinsicDimensions(m_image.get(), defaultBulletSize, DoNotScaleByEffectiveZoom);
        m_image->setContainerContextForRenderer(*this, imageSize, style().effectiveZoom());
        m_textWithSuffix = emptyString();
        m_textWithoutSuffixLength = 0;
        m_textIsLeftToRightDirection = true;
        return;
    }

    auto type = style().listStyleType();
    switch (type) {
    // FIXME: handle CSSCounterStyle case rdar://102988393.
    case ListStyleType::CustomCounterStyle:
    case ListStyleType::String:
        m_textWithSuffix = style().listStyleStringValue();
        m_textWithoutSuffixLength = m_textWithSuffix.length();
        // FIXME: Depending on the string value, we may need the real bidi algorithm.
        m_textIsLeftToRightDirection = u_charDirection(m_textWithSuffix[0]) != U_RIGHT_TO_LEFT;
        break;
    default:
        auto text = listMarkerText(type, m_listItem->value());
        m_textWithSuffix = makeString(text, listMarkerSuffix(type));
        m_textWithoutSuffixLength = text.length();
        m_textIsLeftToRightDirection = u_charDirection(text[0]) != U_RIGHT_TO_LEFT;
        break;
    }
}

void RenderListMarker::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());
    updateContent();

    if (isImage()) {
        LayoutSize imageSize = LayoutSize(m_image->imageSize(this, style().effectiveZoom()));
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = style().isHorizontalWritingMode() ? imageSize.width() : imageSize.height();
        setPreferredLogicalWidthsDirty(false);
        updateMargins();
        return;
    }

    const FontCascade& font = style().fontCascade();

    LayoutUnit logicalWidth;
    switch (style().listStyleType()) {
    case ListStyleType::Circle:
    case ListStyleType::Disc:
    case ListStyleType::Square:
        logicalWidth = (font.metricsOfPrimaryFont().ascent() * 2 / 3 + 1) / 2 + 2;
        break;
    default:
        if (!m_textWithSuffix.isEmpty())
            logicalWidth = font.width(textRun());
        break;
    }

    m_minPreferredLogicalWidth = logicalWidth;
    m_maxPreferredLogicalWidth = logicalWidth;

    setPreferredLogicalWidthsDirty(false);

    updateMargins();
}

void RenderListMarker::updateMargins()
{
    const FontMetrics& fontMetrics = style().metricsOfPrimaryFont();

    LayoutUnit marginStart;
    LayoutUnit marginEnd;

    if (isInside()) {
        if (isImage())
            marginEnd = cMarkerPadding;
        else switch (style().listStyleType()) {
            case ListStyleType::Disc:
            case ListStyleType::Circle:
            case ListStyleType::Square:
                marginStart = -1;
                marginEnd = fontMetrics.ascent() - minPreferredLogicalWidth() + 1;
                break;
            default:
                break;
        }
    } else if (isImage()) {
        marginStart = -minPreferredLogicalWidth() - cMarkerPadding;
        marginEnd = cMarkerPadding;
    } else {
        int offset = fontMetrics.ascent() * 2 / 3;
        switch (style().listStyleType()) {
        case ListStyleType::Disc:
        case ListStyleType::Circle:
        case ListStyleType::Square:
            marginStart = -offset - cMarkerPadding - 1;
            marginEnd = offset + cMarkerPadding + 1 - minPreferredLogicalWidth();
            break;
        case ListStyleType::String:
            if (!m_textWithSuffix.isEmpty())
                marginStart = -minPreferredLogicalWidth();
            break;
        default:
            if (!m_textWithSuffix.isEmpty()) {
                marginStart = -minPreferredLogicalWidth() - offset / 2;
                marginEnd = offset / 2;
            }
        }
    }

    mutableStyle().setMarginStart(Length(marginStart, LengthType::Fixed));
    mutableStyle().setMarginEnd(Length(marginEnd, LengthType::Fixed));
}

LayoutUnit RenderListMarker::lineHeight(bool firstLine, LineDirectionMode direction, LinePositionMode linePositionMode) const
{
    if (!isImage())
        return m_listItem->lineHeight(firstLine, direction, PositionOfInteriorLineBoxes);
    return RenderBox::lineHeight(firstLine, direction, linePositionMode);
}

LayoutUnit RenderListMarker::baselinePosition(FontBaseline baselineType, bool firstLine, LineDirectionMode direction, LinePositionMode linePositionMode) const
{
    if (!isImage())
        return m_listItem->baselinePosition(baselineType, firstLine, direction, PositionOfInteriorLineBoxes);
    return RenderBox::baselinePosition(baselineType, firstLine, direction, linePositionMode);
}

bool RenderListMarker::isInside() const
{
    return m_listItem->notInList() || style().listStylePosition() == ListStylePosition::Inside;
}

const RenderListItem* RenderListMarker::listItem() const
{
    return m_listItem.get();
}

FloatRect RenderListMarker::relativeMarkerRect()
{
    if (isImage())
        return FloatRect(0, 0, m_image->imageSize(this, style().effectiveZoom()).width(), m_image->imageSize(this, style().effectiveZoom()).height());

    FloatRect relativeRect;
    switch (style().listStyleType()) {
    case ListStyleType::Disc:
    case ListStyleType::Circle:
    case ListStyleType::Square: {
        // FIXME: Are these particular rounding rules necessary?
        const FontMetrics& fontMetrics = style().metricsOfPrimaryFont();
        int ascent = fontMetrics.ascent();
        int bulletWidth = (ascent * 2 / 3 + 1) / 2;
        relativeRect = FloatRect(1, 3 * (ascent - ascent * 2 / 3) / 2, bulletWidth, bulletWidth);
        break;
    }
    default:
        if (m_textWithSuffix.isEmpty())
            return FloatRect();
        auto& font = style().fontCascade();
        relativeRect = FloatRect(0, 0, font.width(textRun()), font.metricsOfPrimaryFont().height());
        break;
    }

    if (!style().isHorizontalWritingMode()) {
        relativeRect = relativeRect.transposedRect();
        relativeRect.setX(width() - relativeRect.x() - relativeRect.width());
    }

    return relativeRect;
}

LayoutRect RenderListMarker::selectionRectForRepaint(const RenderLayerModelObject* repaintContainer, bool clipToVisibleContent)
{
    ASSERT(!needsLayout());

    if (selectionState() == HighlightState::None || !inlineBoxWrapper())
        return LayoutRect();

    LegacyRootInlineBox& rootBox = inlineBoxWrapper()->root();
    LayoutRect rect(0_lu, rootBox.selectionTop() - y(), width(), rootBox.selectionHeight());
            
    if (clipToVisibleContent)
        return computeRectForRepaint(rect, repaintContainer);
    return localToContainerQuad(FloatRect(rect), repaintContainer).enclosingBoundingBox();
}

StringView RenderListMarker::textWithoutSuffix() const
{
    return StringView { m_textWithSuffix }.left(m_textWithoutSuffixLength);
}

CSSCounterStyle* RenderListMarker::counterStyle() const
{
    return document().counterStyleRegistry().resolvedCounterStyle(style().listStyleStringValue()).get();
}

} // namespace WebCore
