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
#include "CSSCounterStyle.h"

#include "CSSCounterStyleDescriptors.h"
#include "CSSCounterStyleRegistry.h"
#include <cmath>
#include <wtf/Assertions.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/TextBreakIterator.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

// https://www.w3.org/TR/css-counter-styles-3/#cyclic-system
String CSSCounterStyle::counterForSystemCyclic(int value) const
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
String CSSCounterStyle::counterForSystemFixed(int value) const
{
    if (value < firstSymbolValueForFixedSystem())
        return { };
    unsigned valueOffset = value - firstSymbolValueForFixedSystem();
    if (valueOffset >= symbols().size())
        return { };
    return symbols().at(valueOffset).text;
}

// https://www.w3.org/TR/css-counter-styles-3/#symbolic-system
String CSSCounterStyle::counterForSystemSymbolic(unsigned value) const
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
String CSSCounterStyle::counterForSystemAlphabetic(unsigned value) const
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
String CSSCounterStyle::counterForSystemNumeric(unsigned value) const
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
String CSSCounterStyle::counterForSystemAdditive(unsigned value) const
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
enum class CJK : int { Chinese, Japanese, Korean };

// This table format was derived from an old draft of the CSS specification: 3 group markers, 3 digit markers, 10 digits, negative sign.
static String counterForSystemCJK(int64_t number, const std::array<UChar, 18>& table, Formality formality, CJK cjk)
{
    enum AbstractCJKCharacter {
        NoChar,
        SecondGroupMarker, ThirdGroupMarker, FourthGroupMarker,
        SecondDigitMarker, ThirdDigitMarker, FourthDigitMarker,
        Digit0, Digit1, Digit2, Digit3, Digit4,
        Digit5, Digit6, Digit7, Digit8, Digit9,
        NegativeSign, Space
    };

    if (!number)
        return span(table[Digit0 - 1]);

    ASSERT(number != std::numeric_limits<int64_t>::min());
    bool needsNegativeSign = number < 0;
    if (needsNegativeSign)
        number = -number;

    constexpr unsigned groupLength =  9; // 4 digits, 3 digit markers, a group marker, and a space for korean styles.

    constexpr unsigned bufferLength = 4 * groupLength;
    std::array<AbstractCJKCharacter, bufferLength> buffer;
    buffer.fill(NoChar);

    bool addSpace = false; // Tracking if digits were added in previous group. Korean styles have spacing between groups.

    for (int i = 0; i < 4; ++i) {
        int64_t groupValue = number % 10000;
        number /= 10000;

        // Process least-significant group first, but put it in the buffer last.
        auto group = std::span { buffer }.subspan((3 - i) * groupLength);

        if (groupValue) {
            if (i) {
                group[7] = static_cast<AbstractCJKCharacter>(SecondGroupMarker - 1 + i);
                if (cjk == CJK::Korean && addSpace)
                    group[8] = Space;
            }
            addSpace = true;
        }
        if (!groupValue && cjk == CJK::Chinese)
            group[6] = Digit0;

        bool trailingZero = true; // Chinese styles drop trailing zeros for non-zero groups.

        // Put in the four digits and digit markers for any non-zero digits.
        if (groupValue % 10) {
            group[6] = static_cast<AbstractCJKCharacter>(Digit0 + (groupValue % 10));
            trailingZero = false;
        }

        if (number || groupValue > 9) {
            int64_t digitValue = ((groupValue / 10) % 10);
            if (digitValue || !trailingZero)
                group[4] = static_cast<AbstractCJKCharacter>(Digit0 + digitValue);

            if (digitValue) {
                group[5] = SecondDigitMarker;
                trailingZero = false;
            }
        }
        if (number || groupValue > 99) {
            int64_t digitValue = ((groupValue / 100) % 10);
            if (digitValue || !trailingZero)
                group[2] = static_cast<AbstractCJKCharacter>(Digit0 + digitValue);

            if (digitValue) {
                group[3] = ThirdDigitMarker;
                trailingZero = false;
            }
        }
        if (number || groupValue > 999) {
            int64_t digitValue = groupValue / 1000;
            if (digitValue || !trailingZero)
                group[0] = static_cast<AbstractCJKCharacter>(Digit0 + digitValue);

            if (digitValue)
                group[1] = FourthDigitMarker;

        }

        if (formality == Formality::Informal) {
            if (cjk == CJK::Chinese && groupValue < 20) {
                // Remove the tens digit, but leave the marker.
                ASSERT(group[4] == NoChar || group[4] == Digit0 || group[4] == Digit1); // Fixed syntax errors
                group[4] = NoChar;
            }
            if (cjk == CJK::Japanese || cjk == CJK::Korean) {
                // Check if digits preceding digit markers are preceded by 1.
                for (int j : { 4, 2, 0 }) {
                    if (group[j] == Digit1)
                        group[j] = NoChar;
                }
            }
            if (cjk == CJK::Korean) {
                if (i == 1 && groupValue == 1) {
                    // Drop the digit,but leave the marker.
                    group[6] = NoChar;
                }
            }
        }

        if (!number)
            break;
    }

    // Convert into characters, omitting consecutive runs of digit0 and trailing digit0.
    unsigned length = 0;
    std::array<char16_t, bufferLength + 1> characters;
    auto last = NoChar;
    if (needsNegativeSign) {
        if (cjk == CJK::Japanese) {
            // Japanese negative sign is a string of 4 characters.
            characters[length++] = 0x30DE;
            characters[length++] = 0x30A4;
            characters[length++] = 0x30CA;
            characters[length++] = 0x30B9;
        } else if (cjk == CJK::Korean) {
            // Korean negative sign is a string of 4 characters (with a space between groups).
            characters[length++] = 0xB9C8;
            characters[length++] = 0xC774;
            characters[length++] = 0xB108;
            characters[length++] = 0xC2A4;
            characters[length++] = 0x0020;
        } else
            characters[length++] = table[NegativeSign - 1];
    }
    for (unsigned i = 0; i < bufferLength; ++i) {
        auto character = buffer[i];
        if (character != NoChar) {
            if (character != Digit0 || (cjk == CJK::Chinese && last != Digit0))
                characters[length++] = table[character - 1];
            last = character;
            if (cjk == CJK::Chinese && last == FourthGroupMarker && table[character - 1] == 0x4E07) {
                // Simple Chinese fourth group marker is 2 characters.
                characters[length++] = 0x4EBF;
            }
        }
    }
    if (cjk == CJK::Chinese && last == Digit0)
        --length;

    return std::span<const char16_t> { characters }.first(length);
}


String CSSCounterStyle::counterForSystemDisclosureClosed(WritingMode writingMode)
{
    if (writingMode.isVerticalTypographic())
        return span(writingMode.isInlineTopToBottom() ? blackDownPointingSmallTriangle : blackUpPointingSmallTriangle);
    return span(writingMode.isBidiLTR() ? blackRightPointingSmallTriangle : blackLeftPointingSmallTriangle);
}

String CSSCounterStyle::counterForSystemDisclosureOpen(WritingMode writingMode)
{
    switch (writingMode.blockDirection()) {
    case FlowDirection::TopToBottom:
        return span(blackDownPointingSmallTriangle);
    case FlowDirection::BottomToTop:
        return span(blackUpPointingSmallTriangle);
    case FlowDirection::LeftToRight:
        return span(blackRightPointingSmallTriangle);
    case FlowDirection::RightToLeft:
        return span(blackLeftPointingSmallTriangle);
    }
    ASSERT_NOT_REACHED();
    return { };
}

String CSSCounterStyle::counterForSystemSimplifiedChineseInformal(int64_t value)
{
    static constexpr std::array<UChar, 18> simplifiedChineseInformalTable {
        0x4E07, 0x4EBF, 0x4E07, // 0x4EBF is added to the fourth group marker in counterForSystemCJK().
        0x5341, 0x767E, 0x5343,
        0x96F6, 0x4E00, 0x4E8C, 0x4E09, 0x56DB,
        0x4E94, 0x516D, 0x4E03, 0x516B, 0x4E5D,
        0x8D1F, 0xFFFD
    };
    return counterForSystemCJK(value, simplifiedChineseInformalTable, Formality::Informal, CJK::Chinese);
}

String CSSCounterStyle::counterForSystemSimplifiedChineseFormal(int64_t value)
{
    static constexpr std::array<UChar, 18> simplifiedChineseFormalTable {
        0x4E07, 0x4EBF, 0x4E07, // 0x4EBF is added to the fourth group marker in counterForSystemCJK().
        0x62FE, 0x4F70, 0x4EDF,
        0x96F6, 0x58F9, 0x8D30, 0x53C1, 0x8086,
        0x4F0D, 0x9646, 0x67D2, 0x634C, 0x7396,
        0x8D1F, 0xFFFD
    };
    return counterForSystemCJK(value, simplifiedChineseFormalTable, Formality::Formal, CJK::Chinese);
}

String CSSCounterStyle::counterForSystemTraditionalChineseInformal(int64_t value)
{
    static constexpr std::array<UChar, 18> traditionalChineseInformalTable {
        0x842C, 0x5104, 0x5146,
        0x5341, 0x767E, 0x5343,
        0x96F6, 0x4E00, 0x4E8C, 0x4E09, 0x56DB,
        0x4E94, 0x516D, 0x4E03, 0x516B, 0x4E5D,
        0x8CA0, 0xFFFD
    };
    return counterForSystemCJK(value, traditionalChineseInformalTable, Formality::Informal, CJK::Chinese);
}

String CSSCounterStyle::counterForSystemTraditionalChineseFormal(int64_t value)
{
    static constexpr std::array<UChar, 18> traditionalChineseFormalTable {
        0x842C, 0x5104, 0x5146,
        0x62FE, 0x4F70, 0x4EDF,
        0x96F6, 0x58F9, 0x8CB3, 0x53C3, 0x8086,
        0x4F0D, 0x9678, 0x67D2, 0x634C, 0x7396,
        0x8CA0, 0xFFFD
    };
    return counterForSystemCJK(value, traditionalChineseFormalTable, Formality::Formal, CJK::Chinese);
}

String CSSCounterStyle::counterForSystemJapaneseInformal(int64_t value)
{
    static constexpr std::array<UChar, 18> japaneseInformalTable {
        0x4E07, 0x5104, 0x5146,
        0x5341, 0x767E, 0x5343,
        0x3007, 0x4E00, 0x4E8C, 0x4E09, 0x56DB,
        0x4E94, 0x516D, 0x4E03, 0x516B, 0x4E5D,
        0xFFFD, 0xFFFD // Negative is U+30DE U+30A4 U+30CA U+30B9, which is accounted for in counterForSystemCJK().
    };
    return counterForSystemCJK(value, japaneseInformalTable, Formality::Informal, CJK::Japanese);
}

String CSSCounterStyle::counterForSystemJapaneseFormal(int64_t value)
{
    static constexpr std::array<UChar, 18> japaneseFormalTable {
        0x842C, 0x5104, 0x5146,
        0x62FE, 0x767E, 0x9621,
        0x96F6, 0x58F1, 0x5F10, 0x53C2, 0x56DB,
        0x4f0D, 0x516D, 0x4E03, 0x516B, 0x4E5D,
        0xFFFD, 0xFFFD // Negative is U+30DE U+30A4 U+30CA U+30B9, which is accounted for in counterForSystemCJK().
    };
    return counterForSystemCJK(value, japaneseFormalTable, Formality::Formal, CJK::Japanese);
}

String CSSCounterStyle::counterForSystemKoreanHangulFormal(int64_t value)
{
    static constexpr std::array<UChar, 18> koreanHangulFormalTable {
        0xB9CC, 0xC5B5, 0xC870,
        0xC2ED, 0xBC31, 0xCC9C,
        0xC601, 0xC77C, 0xC774, 0xC0BC, 0xC0AC,
        0xC624, 0xC721, 0xCE60, 0xD314, 0xAD6C,
        0xFFFD, 0x0020
    };
    return counterForSystemCJK(value, koreanHangulFormalTable, Formality::Formal, CJK::Korean);
}

String CSSCounterStyle::counterForSystemKoreanHanjaInformal(int64_t value)
{
    static constexpr std::array<UChar, 18> koreanHanjaInformalTable {
        0x842C, 0x5104, 0x5146,
        0x5341, 0x767E, 0x5343,
        0x96F6, 0x4E00, 0x4E8C, 0x4E09, 0x56DB,
        0x4E94, 0x516D, 0x4E03, 0x516B, 0x4E5D,
        0xFFFD, 0x0020
    };
    return counterForSystemCJK(value, koreanHanjaInformalTable, Formality::Informal, CJK::Korean);
}

String CSSCounterStyle::counterForSystemKoreanHanjaFormal(int64_t value)
{
    static constexpr std::array<UChar, 18> koreanHanjaFormalTable {
        0x842C, 0x5104, 0x5146,
        0x62FE, 0x767E, 0x4EDF,
        0x96F6, 0x58F9, 0x8CB3, 0x53C3, 0x56DB,
        0x4E94, 0x516D, 0x4E03, 0x516B, 0x4E5D,
        0xFFFD, 0x0020
    };
    return counterForSystemCJK(value, koreanHanjaFormalTable, Formality::Formal, CJK::Korean);
}

String CSSCounterStyle::counterForSystemEthiopicNumeric(unsigned value)
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

String CSSCounterStyle::initialRepresentation(int64_t value, WritingMode writingMode) const
{
    unsigned absoluteValue = std::abs(value);
    if (abs(value) > INT_MAX)
        absoluteValue = value < 0 ? INT_MIN: INT_MAX;
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
        return counterForSystemSimplifiedChineseInformal(value);
    case CSSCounterStyleDescriptors::System::SimplifiedChineseFormal:
        return counterForSystemSimplifiedChineseFormal(value);
    case CSSCounterStyleDescriptors::System::TraditionalChineseInformal:
        return counterForSystemTraditionalChineseInformal(value);
    case CSSCounterStyleDescriptors::System::TraditionalChineseFormal:
        return counterForSystemTraditionalChineseFormal(value);
    case CSSCounterStyleDescriptors::System::JapaneseInformal:
        return counterForSystemJapaneseInformal(value);
    case CSSCounterStyleDescriptors::System::JapaneseFormal:
        return counterForSystemJapaneseFormal(value);
    case CSSCounterStyleDescriptors::System::KoreanHangulFormal:
        return counterForSystemKoreanHangulFormal(value);
    case CSSCounterStyleDescriptors::System::KoreanHanjaInformal:
        return counterForSystemKoreanHanjaInformal(value);
    case CSSCounterStyleDescriptors::System::KoreanHanjaFormal:
        return counterForSystemKoreanHanjaFormal(value);
    case CSSCounterStyleDescriptors::System::EthiopicNumeric:
        return counterForSystemEthiopicNumeric(value);
    case CSSCounterStyleDescriptors::System::Extends:
        // CounterStyle with extends system should have been promoted to another system at this point
        ASSERT_NOT_REACHED();
        break;
    }
    return { };
}

String CSSCounterStyle::fallbackText(int64_t value, WritingMode writingMode)
{
    if (m_isFallingBack || !fallback().get()) {
        m_isFallingBack = false;
        return CSSCounterStyleRegistry::decimalCounter()->text(value, writingMode);
    }
    m_isFallingBack = true;
    auto fallbackText = fallback()->text(value, writingMode);
    m_isFallingBack = false;
    return fallbackText;
}

String CSSCounterStyle::text(int64_t value, WritingMode writingMode)
{
    if (!isInRange(value))
        return fallbackText(value, writingMode);

    auto result = initialRepresentation(value, writingMode);
    if (result.isNull())
        return fallbackText(value, writingMode);
    applyPadSymbols(result, value);
    if (shouldApplyNegativeSymbols(value))
        applyNegativeSymbols(result);

    return result;
}

bool CSSCounterStyle::shouldApplyNegativeSymbols(int64_t value) const
{
    auto system = this->system();
    return value < 0 && (system == CSSCounterStyleDescriptors::System::Symbolic || system == CSSCounterStyleDescriptors::System::Numeric || system == CSSCounterStyleDescriptors::System::Alphabetic || system == CSSCounterStyleDescriptors::System::Additive);
}

void CSSCounterStyle::applyNegativeSymbols(String& text) const
{
    text = negative().m_suffix.text.isEmpty() ? makeString(negative().m_prefix.text, text) : makeString(negative().m_prefix.text, text, negative().m_suffix.text);
}

void CSSCounterStyle::applyPadSymbols(String& text, int64_t value) const
{
    // FIXME: should we cap pad minimum length?
    if (pad().m_padMinimumLength <= 0)
        return;

    int numberOfSymbolsToAdd = static_cast<int>(pad().m_padMinimumLength - WTF::numGraphemeClusters(text));
    if (shouldApplyNegativeSymbols(value))
        numberOfSymbolsToAdd -= static_cast<int>(WTF::numGraphemeClusters(negative().m_prefix.text) + WTF::numGraphemeClusters(negative().m_suffix.text));

    String padText;
    for (int i = 0; i < numberOfSymbolsToAdd; ++i)
        padText = makeString(padText, pad().m_padSymbol.text);
    text = makeString(padText, text);
}

bool CSSCounterStyle::isInRange(int64_t value) const
{
    if (isAutoRange()) {
        switch (system()) {
        case CSSCounterStyleDescriptors::System::Cyclic:
        case CSSCounterStyleDescriptors::System::Numeric:
        case CSSCounterStyleDescriptors::System::Fixed:
        case CSSCounterStyleDescriptors::System::DisclosureClosed:
        case CSSCounterStyleDescriptors::System::DisclosureOpen:
            return true;
        case CSSCounterStyleDescriptors::System::Alphabetic:
        case CSSCounterStyleDescriptors::System::Symbolic:
        case CSSCounterStyleDescriptors::System::EthiopicNumeric:
            return value >= 1;
        case CSSCounterStyleDescriptors::System::Additive:
            return value >= 0;
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

CSSCounterStyle::CSSCounterStyle(const CSSCounterStyleDescriptors& descriptors, bool isPredefinedCounterStyle)
    : m_descriptors { descriptors }
    , m_predefinedCounterStyle { isPredefinedCounterStyle }
{
}

Ref<CSSCounterStyle> CSSCounterStyle::create(const CSSCounterStyleDescriptors& descriptors, bool isPredefinedCounterStyle)
{
    return adoptRef(*new CSSCounterStyle(descriptors, isPredefinedCounterStyle));
}

void CSSCounterStyle::setFallbackReference(Ref<CSSCounterStyle>&& fallback)
{
    m_fallbackReference = WeakPtr { fallback };
}

// The counter's system value is promoted to the value of the counter we are extending.
void CSSCounterStyle::extendAndResolve(const CSSCounterStyle& extendedCounterStyle)
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
