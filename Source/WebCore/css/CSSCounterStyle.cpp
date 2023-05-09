/*
 * Copyright (C) 2022 Apple Inc.  All rights reserved.
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
#include <wtf/text/StringBuilder.h>
#include <wtf/text/TextBreakIterator.h>

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

// This table format was derived from an old draft of the CSS specification: 3 group markers, 3 digit markers, 10 digits, negative sign.
static String counterForSystemCJK(int number, const std::array<UChar, 17>& table, Formality formality)
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
        if (number || groupValue > 9) {
            int digitValue = ((groupValue / 10) % 10);
            group[4] = static_cast<AbstractCJKCharacter>(Digit0 + digitValue);
            if (digitValue)
                group[5] = SecondDigitMarker;
        }
        if (number || groupValue > 99) {
            int digitValue = ((groupValue / 100) % 10);
            group[2] = static_cast<AbstractCJKCharacter>(Digit0 + digitValue);
            if (digitValue)
                group[3] = ThirdDigitMarker;
        }
        if (number || groupValue > 999) {
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

String CSSCounterStyle::counterForSystemSimplifiedChineseInformal(int value)
{
    static constexpr std::array<UChar, 17> simplifiedChineseInformalTable {
        0x842C, 0x5104, 0x5146, // These three group markers are probably wrong; OK because we don't use this on big enough numbers.
        0x5341, 0x767E, 0x5343,
        0x96F6, 0x4E00, 0x4E8C, 0x4E09, 0x56DB,
        0x4E94, 0x516D, 0x4E03, 0x516B, 0x4E5D,
        0x8D1F
    };
    return counterForSystemCJK(value, simplifiedChineseInformalTable, Formality::Informal);
}

String CSSCounterStyle::counterForSystemSimplifiedChineseFormal(int value)
{
    static constexpr std::array<UChar, 17> simplifiedChineseFormalTable {
        0x842C, 0x5104, 0x5146, // These three group markers are probably wrong; OK because we don't use this on big enough numbers.
        0x62FE, 0x4F70, 0x4EDF,
        0x96F6, 0x58F9, 0x8D30, 0x53C1, 0x8086,
        0x4F0D, 0x9646, 0x67D2, 0x634C, 0x7396,
        0x8D1F
    };
    return counterForSystemCJK(value, simplifiedChineseFormalTable, Formality::Formal);
}

String CSSCounterStyle::counterForSystemTraditionalChineseInformal(int value)
{
    static constexpr std::array<UChar, 17> traditionalChineseInformalTable {
        0x842C, 0x5104, 0x5146,
        0x5341, 0x767E, 0x5343,
        0x96F6, 0x4E00, 0x4E8C, 0x4E09, 0x56DB,
        0x4E94, 0x516D, 0x4E03, 0x516B, 0x4E5D,
        0x8CA0
    };
    return counterForSystemCJK(value, traditionalChineseInformalTable, Formality::Informal);
}

String CSSCounterStyle::counterForSystemTraditionalChineseFormal(int value)
{
    static constexpr std::array<UChar, 17> traditionalChineseFormalTable {
        0x842C, 0x5104, 0x5146, // These three group markers are probably wrong; OK because we don't use this on big enough numbers.
        0x62FE, 0x4F70, 0x4EDF,
        0x96F6, 0x58F9, 0x8CB3, 0x53C3, 0x8086,
        0x4F0D, 0x9678, 0x67D2, 0x634C, 0x7396,
        0x8CA0
    };
    return counterForSystemCJK(value, traditionalChineseFormalTable, Formality::Formal);
}

String CSSCounterStyle::counterForSystemEthiopicNumeric(unsigned value)
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

String CSSCounterStyle::initialRepresentation(int value) const
{
    unsigned absoluteValue = std::abs(value);
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
    case CSSCounterStyleDescriptors::System::SimplifiedChineseInformal:
        return CSSCounterStyle::counterForSystemSimplifiedChineseInformal(value);
    case CSSCounterStyleDescriptors::System::SimplifiedChineseFormal:
        return CSSCounterStyle::counterForSystemSimplifiedChineseFormal(value);
    case CSSCounterStyleDescriptors::System::TraditionalChineseInformal:
        return CSSCounterStyle::counterForSystemTraditionalChineseInformal(value);
    case CSSCounterStyleDescriptors::System::TraditionalChineseFormal:
        return CSSCounterStyle::counterForSystemTraditionalChineseFormal(value);
    case CSSCounterStyleDescriptors::System::EthiopicNumeric:
        return CSSCounterStyle::counterForSystemEthiopicNumeric(value);
    case CSSCounterStyleDescriptors::System::Extends:
        // CounterStyle with extends system should have been promoted to another system at this point
        ASSERT_NOT_REACHED();
        break;
    }
    return { };
}

String CSSCounterStyle::fallbackText(int value)
{
    if (m_isFallingBack || !fallback().get()) {
        m_isFallingBack = false;
        return CSSCounterStyleRegistry::decimalCounter()->text(value);
    }
    m_isFallingBack = true;
    auto fallbackText = fallback()->text(value);
    m_isFallingBack = false;
    return fallbackText;
}

String CSSCounterStyle::text(int value)
{
    if (!isInRange(value))
        return fallbackText(value);

    auto result = initialRepresentation(value);
    if (result.isNull())
        return fallbackText(value);
    applyPadSymbols(result, value);
    if (shouldApplyNegativeSymbols(value))
        applyNegativeSymbols(result);

    return result;
}

bool CSSCounterStyle::shouldApplyNegativeSymbols(int value) const
{
    auto system = this->system();
    return value < 0 && (system == CSSCounterStyleDescriptors::System::Symbolic || system == CSSCounterStyleDescriptors::System::Numeric || system == CSSCounterStyleDescriptors::System::Alphabetic || system == CSSCounterStyleDescriptors::System::Additive);
}

void CSSCounterStyle::applyNegativeSymbols(String& text) const
{
    text = negative().m_suffix.text.isEmpty() ? makeString(negative().m_prefix.text, text) : makeString(negative().m_prefix.text, text, negative().m_suffix.text);
}

void CSSCounterStyle::applyPadSymbols(String& text, int value) const
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

bool CSSCounterStyle::isInRange(int value) const
{
    if (isAutoRange()) {
        switch (system()) {
        case CSSCounterStyleDescriptors::System::Cyclic:
        case CSSCounterStyleDescriptors::System::Numeric:
        case CSSCounterStyleDescriptors::System::Fixed:
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
            return value >= -9999 && value <= 9999;
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
    : m_descriptors { descriptors },
    m_predefinedCounterStyle { isPredefinedCounterStyle }
{
}

Ref<CSSCounterStyle> CSSCounterStyle::create(const CSSCounterStyleDescriptors& descriptors, bool isPredefinedCounterStyle)
{
    return adoptRef(*new CSSCounterStyle(descriptors, isPredefinedCounterStyle));
}

void CSSCounterStyle::setFallbackReference(RefPtr<CSSCounterStyle>&& fallback)
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
