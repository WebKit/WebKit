/*
 * Copyright (C) 2022-2024 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSRegisteredCounterStyle.h"

#include "CSSCounterStyleDescriptors.h"
#include "CSSCounterStyleRegistry.h"
#include <cmath>
#include <wtf/Assertions.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/TextBreakIterator.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

// https://www.w3.org/TR/css-counter-styles-3/#cyclic-system
String CSSRegisteredCounterStyle::counterForSystemCyclic(int value) const
{
    auto amountOfSymbols = symbols().size();
    ASSERT(amountOfSymbols > 0);

    // For avoiding subtracting -1 from INT_MAX we will sum-up amountOfSymbols in case the value is not positive.
    // This works because x % y = (x + y) % y
    unsigned symbolIndex = static_cast<unsigned>(value > 0 ? value : value + amountOfSymbols);
    symbolIndex = (symbolIndex - 1) % amountOfSymbols;
    ASSERT(static_cast<unsigned>(symbolIndex) < amountOfSymbols);

    return symbols().at(static_cast<unsigned>(symbolIndex)).text;
}

// https://www.w3.org/TR/css-counter-styles-3/#fixed-system
String CSSRegisteredCounterStyle::counterForSystemFixed(int value) const
{
    if (value < firstSymbolValueForFixedSystem())
        return { };
    unsigned valueOffset = value - firstSymbolValueForFixedSystem();
    if (valueOffset >= symbols().size())
        return { };
    return symbols().at(valueOffset).text;
}

// https://www.w3.org/TR/css-counter-styles-3/#symbolic-system
String CSSRegisteredCounterStyle::counterForSystemSymbolic(unsigned value) const
{
    auto amountOfSymbols = symbols().size();
    ASSERT(amountOfSymbols > 0);

    if (value < 1)
        return { };

    unsigned symbolIndex = ((value - 1) % amountOfSymbols);
    unsigned frequency = static_cast<unsigned>(std::ceil(static_cast<float>(value) / amountOfSymbols));

    StringBuilder result;
    for (unsigned i = 0; i < frequency; ++i)
        result.append(symbols().at(symbolIndex).text);
    return result.toString();
}

// https://www.w3.org/TR/css-counter-styles-3/#alphabetic-system
String CSSRegisteredCounterStyle::counterForSystemAlphabetic(unsigned value) const
{
    auto amountOfSymbols = symbols().size();
    ASSERT(amountOfSymbols >= 2);

    if (value < 1)
        return { };

    Vector<String> reversed;
    while (value) {
        value -= 1;
        reversed.append(symbols().at(value % amountOfSymbols).text);
        value = std::floor(value / amountOfSymbols);
    }
    StringBuilder result;
    for (auto iter = reversed.rbegin(); iter != reversed.rend(); ++iter)
        result.append(*iter);
    return result.toString();
}

// https://www.w3.org/TR/css-counter-styles-3/#numeric-system
String CSSRegisteredCounterStyle::counterForSystemNumeric(unsigned value) const
{
    auto amountOfSymbols = symbols().size();
    ASSERT(amountOfSymbols >= 2);

    if (!value)
        return symbols().at(0).text;

    Vector<String> reversed;
    while (value) {
        reversed.append(symbols().at(value % amountOfSymbols).text);
        value = static_cast<unsigned>(std::floor(value / amountOfSymbols));
    }
    StringBuilder result;
    for (auto iter = reversed.rbegin(); iter != reversed.rend(); ++iter)
        result.append(*iter);
    return result.toString();
}

// https://www.w3.org/TR/css-counter-styles-3/#additive-system
String CSSRegisteredCounterStyle::counterForSystemAdditive(unsigned value) const
{
    auto& additiveSymbols = this->additiveSymbols();
    if (!value) {
        for (auto& [symbol, weight] : additiveSymbols) {
            if (!weight)
                return symbol.text;
        }
        return { };
    }

    StringBuilder result;
    auto appendToResult = [&](const String& symbol, unsigned frequency) {
        for (unsigned i = 0; i < frequency; ++i)
            result.append(symbol);
    };

    for (auto& [symbol, weight] : additiveSymbols) {
        if (!weight || weight > value)
            continue;
        auto repetitions = static_cast<unsigned>(std::floor(value / weight));
        appendToResult(symbol.text, repetitions);
        value -= weight * repetitions;
        if (!value)
            return result.toString();
    }
    return { };
}

enum class Formality : bool { Informal, Formal };
enum class CJKLanguage : uint8_t { Chinese, Japanese, Korean };

struct CJKCharacters {
    std::array<char16_t, 10> digits;
    std::array<char16_t, 3> digitMarkers;
    // The simplified Chinese styles spell the fourth group marker with two characters, so a group
    // marker is a one or two character sequence padded with a null character.
    std::array<std::array<char16_t, 2>, 3> groupMarkers;
};

// https://drafts.csswg.org/css-counter-styles/#extended-range-optional
static String counterForSystemCJK(unsigned number, const CJKCharacters& characters, CJKLanguage language, Formality formality)
{
    enum AbstractCJKCharacter : uint8_t {
        NoCharacter,
        Digit0, Digit1, Digit2, Digit3, Digit4, Digit5, Digit6, Digit7, Digit8, Digit9,
        SecondDigitMarker, ThirdDigitMarker, FourthDigitMarker,
        SecondGroupMarker, ThirdGroupMarker, FourthGroupMarker,
        GroupSeparator
    };

    if (!number)
        return span(characters.digits[0]);

    // Slots within a group, in output order: thousands digit and its marker, hundreds digit and
    // its marker, tens digit and its marker, ones digit, the group marker, and the separator that
    // the Korean styles put between groups.
    constexpr unsigned groupLength = 9;
    constexpr unsigned groupCount = 4;
    constexpr unsigned bufferLength = groupCount * groupLength;
    std::array<AbstractCJKCharacter, bufferLength> buffer;
    buffer.fill(NoCharacter);

    bool hasLessSignificantGroup = false;
    for (unsigned groupIndex = 0; groupIndex < groupCount; ++groupIndex) {
        unsigned groupValue = number % 10000;
        number /= 10000;

        // Process the least-significant group first, but put it in the buffer last.
        auto group = std::span { buffer }.subspan((groupCount - 1 - groupIndex) * groupLength, groupLength);

        if (groupValue) {
            if (groupIndex)
                group[7] = static_cast<AbstractCJKCharacter>(SecondGroupMarker - 1 + groupIndex);
            if (language == CJKLanguage::Korean && hasLessSignificantGroup)
                group[8] = GroupSeparator;
            hasLessSignificantGroup = true;
        }

        group[6] = static_cast<AbstractCJKCharacter>(Digit0 + groupValue % 10);
        if (number || groupValue > 9) {
            unsigned digitValue = (groupValue / 10) % 10;
            group[4] = static_cast<AbstractCJKCharacter>(Digit0 + digitValue);
            if (digitValue)
                group[5] = SecondDigitMarker;
        }
        if (number || groupValue > 99) {
            unsigned digitValue = (groupValue / 100) % 10;
            group[2] = static_cast<AbstractCJKCharacter>(Digit0 + digitValue);
            if (digitValue)
                group[3] = ThirdDigitMarker;
        }
        if (number || groupValue > 999) {
            unsigned digitValue = groupValue / 1000;
            group[0] = static_cast<AbstractCJKCharacter>(Digit0 + digitValue);
            if (digitValue)
                group[1] = FourthDigitMarker;
        }

        if (formality == Formality::Informal) {
            if (language == CJKLanguage::Chinese) {
                if (groupValue >= 10 && groupValue <= 19)
                    group[4] = NoCharacter;
            } else {
                for (unsigned digitSlot : { 0u, 2u, 4u }) {
                    if (group[digitSlot] == Digit1 && group[digitSlot + 1] != NoCharacter)
                        group[digitSlot] = NoCharacter;
                }
                if (language == CJKLanguage::Korean && groupIndex == 1 && groupValue == 1)
                    group[6] = NoCharacter;
            }
        }

        if (language == CJKLanguage::Chinese) {
            if (groupValue) {
                for (unsigned slot = 7; slot-- > 0;) {
                    if (group[slot] == NoCharacter)
                        continue;
                    if (group[slot] != Digit0)
                        break;
                    group[slot] = NoCharacter;
                }
            }
        } else {
            for (unsigned digitSlot : { 0u, 2u, 4u, 6u }) {
                if (group[digitSlot] == Digit0)
                    group[digitSlot] = NoCharacter;
            }
        }

        if (!number)
            break;
    }

    std::array<AbstractCJKCharacter, bufferLength> abstractCharacters;
    unsigned abstractCharacterCount = 0;
    auto lastKept = NoCharacter;
    for (auto abstractCharacter : buffer) {
        if (abstractCharacter == NoCharacter)
            continue;
        // The Chinese styles collapse each run of zeros, across groups, into a single zero.
        if (language == CJKLanguage::Chinese && abstractCharacter == Digit0 && lastKept == Digit0)
            continue;
        abstractCharacters[abstractCharacterCount++] = abstractCharacter;
        lastKept = abstractCharacter;
    }
    if (language == CJKLanguage::Chinese && abstractCharacterCount && abstractCharacters[abstractCharacterCount - 1] == Digit0)
        --abstractCharacterCount;

    StringBuilder result;
    result.reserveCapacity(abstractCharacterCount + 1);
    for (auto abstractCharacter : std::span { abstractCharacters }.first(abstractCharacterCount)) {
        if (abstractCharacter == GroupSeparator)
            result.append(space);
        else if (abstractCharacter >= SecondGroupMarker) {
            for (auto character : characters.groupMarkers[abstractCharacter - SecondGroupMarker]) {
                if (character)
                    result.append(character);
            }
        } else if (abstractCharacter >= SecondDigitMarker)
            result.append(characters.digitMarkers[abstractCharacter - SecondDigitMarker]);
        else
            result.append(characters.digits[abstractCharacter - Digit0]);
    }
    return result.toString();
}

String CSSRegisteredCounterStyle::counterForSystemDisclosureClosed(WritingMode writingMode)
{
    if (writingMode.isVerticalTypographic())
        return span(writingMode.isInlineTopToBottom() ? blackDownPointingTriangle : blackUpPointingTriangle);
    return span(writingMode.isBidiLTR() ? blackRightPointingTriangle : blackLeftPointingTriangle);
}

String CSSRegisteredCounterStyle::counterForSystemDisclosureOpen(WritingMode writingMode)
{
    switch (writingMode.blockDirection()) {
    case FlowDirection::TopToBottom:
        return span(blackDownPointingTriangle);
    case FlowDirection::BottomToTop:
        return span(blackUpPointingTriangle);
    case FlowDirection::LeftToRight:
        return span(blackRightPointingTriangle);
    case FlowDirection::RightToLeft:
        return span(blackLeftPointingTriangle);
    }
    ASSERT_NOT_REACHED();
    return { };
}

String CSSRegisteredCounterStyle::counterForSystemSimplifiedChineseInformal(unsigned value)
{
    static constexpr CJKCharacters simplifiedChineseInformal {
        { 0x96F6, 0x4E00, 0x4E8C, 0x4E09, 0x56DB, 0x4E94, 0x516D, 0x4E03, 0x516B, 0x4E5D },
        { 0x5341, 0x767E, 0x5343 },
        { { { 0x4E07 }, { 0x4EBF }, { 0x4E07, 0x4EBF } } }
    };
    return counterForSystemCJK(value, simplifiedChineseInformal, CJKLanguage::Chinese, Formality::Informal);
}

String CSSRegisteredCounterStyle::counterForSystemSimplifiedChineseFormal(unsigned value)
{
    static constexpr CJKCharacters simplifiedChineseFormal {
        { 0x96F6, 0x58F9, 0x8D30, 0x53C1, 0x8086, 0x4F0D, 0x9646, 0x67D2, 0x634C, 0x7396 },
        { 0x62FE, 0x4F70, 0x4EDF },
        { { { 0x4E07 }, { 0x4EBF }, { 0x4E07, 0x4EBF } } }
    };
    return counterForSystemCJK(value, simplifiedChineseFormal, CJKLanguage::Chinese, Formality::Formal);
}

String CSSRegisteredCounterStyle::counterForSystemTraditionalChineseInformal(unsigned value)
{
    static constexpr CJKCharacters traditionalChineseInformal {
        { 0x96F6, 0x4E00, 0x4E8C, 0x4E09, 0x56DB, 0x4E94, 0x516D, 0x4E03, 0x516B, 0x4E5D },
        { 0x5341, 0x767E, 0x5343 },
        { { { 0x842C }, { 0x5104 }, { 0x5146 } } }
    };
    return counterForSystemCJK(value, traditionalChineseInformal, CJKLanguage::Chinese, Formality::Informal);
}

String CSSRegisteredCounterStyle::counterForSystemTraditionalChineseFormal(unsigned value)
{
    static constexpr CJKCharacters traditionalChineseFormal {
        { 0x96F6, 0x58F9, 0x8CB3, 0x53C3, 0x8086, 0x4F0D, 0x9678, 0x67D2, 0x634C, 0x7396 },
        { 0x62FE, 0x4F70, 0x4EDF },
        { { { 0x842C }, { 0x5104 }, { 0x5146 } } }
    };
    return counterForSystemCJK(value, traditionalChineseFormal, CJKLanguage::Chinese, Formality::Formal);
}

String CSSRegisteredCounterStyle::counterForSystemJapaneseInformal(unsigned value)
{
    static constexpr CJKCharacters japaneseInformal {
        { 0x3007, 0x4E00, 0x4E8C, 0x4E09, 0x56DB, 0x4E94, 0x516D, 0x4E03, 0x516B, 0x4E5D },
        { 0x5341, 0x767E, 0x5343 },
        { { { 0x4E07 }, { 0x5104 }, { 0x5146 } } }
    };
    return counterForSystemCJK(value, japaneseInformal, CJKLanguage::Japanese, Formality::Informal);
}

String CSSRegisteredCounterStyle::counterForSystemJapaneseFormal(unsigned value)
{
    static constexpr CJKCharacters japaneseFormal {
        { 0x96F6, 0x58F1, 0x5F10, 0x53C2, 0x56DB, 0x4F0D, 0x516D, 0x4E03, 0x516B, 0x4E5D },
        { 0x62FE, 0x767E, 0x9621 },
        { { { 0x842C }, { 0x5104 }, { 0x5146 } } }
    };
    return counterForSystemCJK(value, japaneseFormal, CJKLanguage::Japanese, Formality::Formal);
}

String CSSRegisteredCounterStyle::counterForSystemKoreanHangulFormal(unsigned value)
{
    static constexpr CJKCharacters koreanHangulFormal {
        { 0xC601, 0xC77C, 0xC774, 0xC0BC, 0xC0AC, 0xC624, 0xC721, 0xCE60, 0xD314, 0xAD6C },
        { 0xC2ED, 0xBC31, 0xCC9C },
        { { { 0xB9CC }, { 0xC5B5 }, { 0xC870 } } }
    };
    return counterForSystemCJK(value, koreanHangulFormal, CJKLanguage::Korean, Formality::Formal);
}

String CSSRegisteredCounterStyle::counterForSystemKoreanHanjaInformal(unsigned value)
{
    static constexpr CJKCharacters koreanHanjaInformal {
        { 0x96F6, 0x4E00, 0x4E8C, 0x4E09, 0x56DB, 0x4E94, 0x516D, 0x4E03, 0x516B, 0x4E5D },
        { 0x5341, 0x767E, 0x5343 },
        { { { 0x842C }, { 0x5104 }, { 0x5146 } } }
    };
    return counterForSystemCJK(value, koreanHanjaInformal, CJKLanguage::Korean, Formality::Informal);
}

String CSSRegisteredCounterStyle::counterForSystemKoreanHanjaFormal(unsigned value)
{
    static constexpr CJKCharacters koreanHanjaFormal {
        { 0x96F6, 0x58F9, 0x8CB3, 0x53C3, 0x56DB, 0x4E94, 0x516D, 0x4E03, 0x516B, 0x4E5D },
        { 0x62FE, 0x767E, 0x4EDF },
        { { { 0x842C }, { 0x5104 }, { 0x5146 } } }
    };
    return counterForSystemCJK(value, koreanHanjaFormal, CJKLanguage::Korean, Formality::Formal);
}

String CSSRegisteredCounterStyle::counterForSystemEthiopicNumeric(unsigned value)
{
    ASSERT(value >= 1);

    if (value == 1) {
        char16_t ethiopicDigitOne = 0x1369;
        return span(ethiopicDigitOne);
    }

    // Split the number into groups of two digits, starting with the least significant decimal digit.
    std::array<uint8_t, 5> groups;
    for (auto& group : groups) {
        group = value % 100;
        value /= 100;
    }

    std::array<char16_t, groups.size() * 3> buffer;
    unsigned length = 0;
    bool isMostSignificantGroup = true;
    for (int i = groups.size() - 1; i >= 0; --i) {
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

    return std::span<const char16_t> { buffer }.first(length);
}

String CSSRegisteredCounterStyle::initialRepresentation(int value, WritingMode writingMode) const
{
    unsigned absoluteValue = value < 0 ? -static_cast<unsigned>(value) : static_cast<unsigned>(value);
    switch (system()) {
    case CSSCounterStyleDescriptors::System::Cyclic:
        return counterForSystemCyclic(value);
    case CSSCounterStyleDescriptors::System::Numeric:
        return counterForSystemNumeric(absoluteValue);
    case CSSCounterStyleDescriptors::System::Alphabetic:
        return counterForSystemAlphabetic(absoluteValue);
    case CSSCounterStyleDescriptors::System::Symbolic:
        return counterForSystemSymbolic(absoluteValue);
    case CSSCounterStyleDescriptors::System::Additive:
        return counterForSystemAdditive(absoluteValue);
    case CSSCounterStyleDescriptors::System::Fixed:
        return counterForSystemFixed(value);
    case CSSCounterStyleDescriptors::System::DisclosureClosed:
        return counterForSystemDisclosureClosed(writingMode);
    case CSSCounterStyleDescriptors::System::DisclosureOpen:
        return counterForSystemDisclosureOpen(writingMode);
    case CSSCounterStyleDescriptors::System::SimplifiedChineseInformal:
        return counterForSystemSimplifiedChineseInformal(absoluteValue);
    case CSSCounterStyleDescriptors::System::SimplifiedChineseFormal:
        return counterForSystemSimplifiedChineseFormal(absoluteValue);
    case CSSCounterStyleDescriptors::System::TraditionalChineseInformal:
        return counterForSystemTraditionalChineseInformal(absoluteValue);
    case CSSCounterStyleDescriptors::System::TraditionalChineseFormal:
        return counterForSystemTraditionalChineseFormal(absoluteValue);
    case CSSCounterStyleDescriptors::System::JapaneseInformal:
        return counterForSystemJapaneseInformal(absoluteValue);
    case CSSCounterStyleDescriptors::System::JapaneseFormal:
        return counterForSystemJapaneseFormal(absoluteValue);
    case CSSCounterStyleDescriptors::System::KoreanHangulFormal:
        return counterForSystemKoreanHangulFormal(absoluteValue);
    case CSSCounterStyleDescriptors::System::KoreanHanjaInformal:
        return counterForSystemKoreanHanjaInformal(absoluteValue);
    case CSSCounterStyleDescriptors::System::KoreanHanjaFormal:
        return counterForSystemKoreanHanjaFormal(absoluteValue);
    case CSSCounterStyleDescriptors::System::EthiopicNumeric:
        return counterForSystemEthiopicNumeric(value);
    case CSSCounterStyleDescriptors::System::Extends:
        // CounterStyle with extends system should have been promoted to another system at this point
        ASSERT_NOT_REACHED();
        break;
    }
    return { };
}

String CSSRegisteredCounterStyle::fallbackText(int value, WritingMode writingMode)
{
    if (m_isFallingBack || !fallback().get()) {
        m_isFallingBack = false;
        return CSSCounterStyleRegistry::decimalCounter()->text(value, writingMode);
    }
    m_isFallingBack = true;
    auto fallbackText = protect(fallback())->text(value, writingMode);
    m_isFallingBack = false;
    return fallbackText;
}

String CSSRegisteredCounterStyle::text(int value, WritingMode writingMode)
{
    if (!isInRange(value))
        return fallbackText(value, writingMode);

    auto result = initialRepresentation(value, writingMode);
    if (result.isNull())
        return fallbackText(value, writingMode);
    if (!applyPadSymbols(result, value))
        return fallbackText(value, writingMode);
    if (shouldApplyNegativeSymbols(value))
        applyNegativeSymbols(result);

    return result;
}

bool CSSRegisteredCounterStyle::shouldApplyNegativeSymbols(int value) const
{
    if (value >= 0)
        return false;
    switch (system()) {
    case CSSCounterStyleDescriptors::System::Symbolic:
    case CSSCounterStyleDescriptors::System::Numeric:
    case CSSCounterStyleDescriptors::System::Alphabetic:
    case CSSCounterStyleDescriptors::System::Additive:
    case CSSCounterStyleDescriptors::System::SimplifiedChineseInformal:
    case CSSCounterStyleDescriptors::System::SimplifiedChineseFormal:
    case CSSCounterStyleDescriptors::System::TraditionalChineseInformal:
    case CSSCounterStyleDescriptors::System::TraditionalChineseFormal:
    case CSSCounterStyleDescriptors::System::JapaneseInformal:
    case CSSCounterStyleDescriptors::System::JapaneseFormal:
    case CSSCounterStyleDescriptors::System::KoreanHangulFormal:
    case CSSCounterStyleDescriptors::System::KoreanHanjaInformal:
    case CSSCounterStyleDescriptors::System::KoreanHanjaFormal:
        return true;
    case CSSCounterStyleDescriptors::System::Cyclic:
    case CSSCounterStyleDescriptors::System::Fixed:
    case CSSCounterStyleDescriptors::System::DisclosureClosed:
    case CSSCounterStyleDescriptors::System::DisclosureOpen:
    case CSSCounterStyleDescriptors::System::EthiopicNumeric:
    case CSSCounterStyleDescriptors::System::Extends:
        return false;
    }
    return false;
}

void CSSRegisteredCounterStyle::applyNegativeSymbols(String& text) const
{
    text = negative().m_suffix.text.isEmpty() ? makeString(negative().m_prefix.text, text) : makeString(negative().m_prefix.text, text, negative().m_suffix.text);
}

bool CSSRegisteredCounterStyle::applyPadSymbols(String& text, int value) const
{
    // We limit the max UTF-16 padding length to 150 to match Firefox. This complies with CSS
    // Counter Styles Level 3, which requires us to support counter representations of at least 60
    // code points before using the fallback representation.
    static constexpr unsigned maxPadLength = 150;

    if (pad().m_padMinimumLength <= 0)
        return true;

    int numberOfSymbolsToAdd = static_cast<int>(pad().m_padMinimumLength - WTF::numGraphemeClusters(text));
    if (shouldApplyNegativeSymbols(value))
        numberOfSymbolsToAdd -= static_cast<int>(WTF::numGraphemeClusters(negative().m_prefix.text) + WTF::numGraphemeClusters(negative().m_suffix.text));

    if (numberOfSymbolsToAdd <= 0)
        return true;

    auto totalPadLength = checkedProduct<unsigned>(numberOfSymbolsToAdd, pad().m_padSymbol.text.length());
    if (totalPadLength.hasOverflowed() || totalPadLength.value() > maxPadLength)
        return false;

    StringBuilder result;
    result.reserveCapacity(totalPadLength + text.length());
    for (int i = 0; i < numberOfSymbolsToAdd; ++i)
        result.append(pad().m_padSymbol.text);
    result.append(text);
    text = result.toString();
    return true;
}

bool CSSRegisteredCounterStyle::isInRange(int value) const
{
    if (isAutoRange()) {
        switch (system()) {
        case CSSCounterStyleDescriptors::System::Cyclic:
        case CSSCounterStyleDescriptors::System::Numeric:
        case CSSCounterStyleDescriptors::System::Fixed:
        case CSSCounterStyleDescriptors::System::DisclosureClosed:
        case CSSCounterStyleDescriptors::System::DisclosureOpen:
        case CSSCounterStyleDescriptors::System::SimplifiedChineseInformal:
        case CSSCounterStyleDescriptors::System::SimplifiedChineseFormal:
        case CSSCounterStyleDescriptors::System::TraditionalChineseInformal:
        case CSSCounterStyleDescriptors::System::TraditionalChineseFormal:
        case CSSCounterStyleDescriptors::System::JapaneseInformal:
        case CSSCounterStyleDescriptors::System::JapaneseFormal:
        case CSSCounterStyleDescriptors::System::KoreanHangulFormal:
        case CSSCounterStyleDescriptors::System::KoreanHanjaInformal:
        case CSSCounterStyleDescriptors::System::KoreanHanjaFormal:
            return true;
        case CSSCounterStyleDescriptors::System::Alphabetic:
        case CSSCounterStyleDescriptors::System::Symbolic:
        case CSSCounterStyleDescriptors::System::EthiopicNumeric:
            return value >= 1;
        case CSSCounterStyleDescriptors::System::Additive:
            return value >= 0;
        case CSSCounterStyleDescriptors::System::Extends:
            ASSERT_NOT_REACHED();
            return true;
        }
    }

    for (const auto& [lowerBound, higherBound] : ranges()) {
        if (value >= lowerBound && value <= higherBound)
            return true;
    }
    return false;
}

CSSRegisteredCounterStyle::CSSRegisteredCounterStyle(const CSSCounterStyleDescriptors& descriptors, bool isPredefinedCounterStyle)
    : m_descriptors { descriptors }
    , m_predefinedCounterStyle { isPredefinedCounterStyle }
{
}

Ref<CSSRegisteredCounterStyle> CSSRegisteredCounterStyle::create(const CSSCounterStyleDescriptors& descriptors, bool isPredefinedCounterStyle)
{
    return adoptRef(*new CSSRegisteredCounterStyle(descriptors, isPredefinedCounterStyle));
}

void CSSRegisteredCounterStyle::setFallbackReference(Ref<CSSRegisteredCounterStyle>&& fallback)
{
    m_fallbackReference = WeakPtr { fallback };
}

// The counter's system value is promoted to the value of the counter we are extending.
void CSSRegisteredCounterStyle::extendAndResolve(const CSSRegisteredCounterStyle& extendedCounterStyle)
{
    m_descriptors.m_isExtendedResolved = true;

    setSystem(extendedCounterStyle.system());
    setFirstSymbolValueForFixedSystem(extendedCounterStyle.firstSymbolValueForFixedSystem());

    if (!explicitlySetDescriptors().contains(CSSCounterStyleDescriptors::ExplicitlySetDescriptors::Negative))
        setNegative(extendedCounterStyle.negative());
    if (!explicitlySetDescriptors().contains(CSSCounterStyleDescriptors::ExplicitlySetDescriptors::Prefix))
        setPrefix(extendedCounterStyle.prefix());
    if (!explicitlySetDescriptors().contains(CSSCounterStyleDescriptors::ExplicitlySetDescriptors::Suffix))
        setSuffix(extendedCounterStyle.suffix());
    if (!explicitlySetDescriptors().contains(CSSCounterStyleDescriptors::ExplicitlySetDescriptors::Range))
        setRanges(extendedCounterStyle.ranges());
    if (!explicitlySetDescriptors().contains(CSSCounterStyleDescriptors::ExplicitlySetDescriptors::Pad))
        setPad(extendedCounterStyle.pad());
    if (!explicitlySetDescriptors().contains(CSSCounterStyleDescriptors::ExplicitlySetDescriptors::Fallback)) {
        setFallbackName(extendedCounterStyle.fallbackName());
        m_fallbackReference = extendedCounterStyle.m_fallbackReference;
    }
    if (!explicitlySetDescriptors().contains(CSSCounterStyleDescriptors::ExplicitlySetDescriptors::Symbols))
        setSymbols(extendedCounterStyle.symbols());
    if (!explicitlySetDescriptors().contains(CSSCounterStyleDescriptors::ExplicitlySetDescriptors::AdditiveSymbols))
        setAdditiveSymbols(extendedCounterStyle.additiveSymbols());
    if (!explicitlySetDescriptors().contains(CSSCounterStyleDescriptors::ExplicitlySetDescriptors::SpeakAs))
        setSpeakAs(extendedCounterStyle.speakAs());
}
} // namespace WebCore
