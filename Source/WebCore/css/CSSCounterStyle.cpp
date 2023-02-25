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
#include "CSSCounterStyleRule.h"
#include "CSSPrimitiveValue.h"
#include "CSSValuePair.h"
#include <cmath>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/TextBreakIterator.h>

namespace WebCore {

// https://www.w3.org/TR/css-counter-styles-3/#cyclic-system
String CSSCounterStyle::counterForSystemCyclic(int value) const
{
    auto amountOfSymbols = symbols().size();
    int symbolIndex = (value - 1) % amountOfSymbols;
    if (symbolIndex < 0)
        symbolIndex += amountOfSymbols;
    if (symbolIndex < 0 || static_cast<unsigned>(symbolIndex) >= amountOfSymbols) {
        ASSERT_NOT_REACHED();
        return  { };
    }
    return symbols().at(static_cast<unsigned>(symbolIndex));
}

// https://www.w3.org/TR/css-counter-styles-3/#fixed-system
String CSSCounterStyle::counterForSystemFixed(int value) const
{
    // Empty string will force value to be handled by fallback.
    if (value < firstSymbolValueForFixedSystem())
        return { };
    unsigned valueOffset = value - firstSymbolValueForFixedSystem();
    if (valueOffset >= symbols().size())
        return { };
    return symbols().at(valueOffset);
}

// https://www.w3.org/TR/css-counter-styles-3/#symbolic-system
String CSSCounterStyle::counterForSystemSymbolic(int value) const
{
    auto amountOfSymbols = symbols().size();
    if (!amountOfSymbols || value <= 0) {
        ASSERT_NOT_REACHED();
        return { };
    }
    unsigned symbolIndex = ((value - 1) % amountOfSymbols);
    int frequency = static_cast<int>(std::ceil(static_cast<float>(value) / amountOfSymbols));

    StringBuilder result;
    for (int i = 0; i < frequency; ++i)
        result.append(symbols().at(symbolIndex));
    return result.toString();
}

// https://www.w3.org/TR/css-counter-styles-3/#alphabetic-system
String CSSCounterStyle::counterForSystemAlphabetic(int value) const
{
    auto amountOfSymbols = symbols().size();
    if (value < 1 || !amountOfSymbols) {
        ASSERT_NOT_REACHED();
        return { };
    }

    Vector<String> reversed;
    while (value) {
        value -= 1;
        reversed.append(symbols().at(value % amountOfSymbols));
        value = static_cast<int>(std::floor(value / amountOfSymbols));
    }
    StringBuilder result;
    for (auto iter = reversed.rbegin(); iter != reversed.rend(); ++iter)
        result.append(*iter);
    return result.toString();
}

// https://www.w3.org/TR/css-counter-styles-3/#numeric-system
String CSSCounterStyle::counterForSystemNumeric(int value) const
{
    auto amountOfSymbols = symbols().size();
    if (!amountOfSymbols) {
        ASSERT_NOT_REACHED();
        return { };
    }
    if (!value)
        return symbols().at(0);

    Vector<String> reversed;
    while (value) {
        reversed.append(symbols().at(value % amountOfSymbols));
        value = static_cast<int>(std::floor(value / amountOfSymbols));
    }
    StringBuilder result;
    for (auto iter = reversed.rbegin(); iter != reversed.rend(); ++iter)
        result.append(*iter);
    return result.toString();
}

// https://www.w3.org/TR/css-counter-styles-3/#additive-system
String CSSCounterStyle::counterForSystemAdditive(int value) const
{
    auto& additiveSymbols = this->additiveSymbols();
    if (!value) {
        for (auto& [symbol, weight] : additiveSymbols) {
            if (!weight)
                return symbol;
        }
        return { };
    }

    StringBuilder result;
    auto appendToResult = [&](String symb, unsigned frequency) {
        for (unsigned i = 0; i < frequency; ++i)
            result.append(symb);
    };

    for (auto& [symbol, weight] : additiveSymbols) {
        if (!weight || static_cast<int>(weight) > value)
            continue;
        auto repetitions = static_cast<unsigned>(std::floor(value / weight));
        appendToResult(symbol, repetitions);
        value -= weight * repetitions;
        if (!value)
            return result.toString();
    }
    return { };
}

String CSSCounterStyle::initialRepresentation(int value) const
{
    switch (system()) {
    case CSSCounterStyleDescriptors::System::Cyclic:
        return counterForSystemCyclic(value);
    case CSSCounterStyleDescriptors::System::Numeric:
        return counterForSystemNumeric(value);
    case CSSCounterStyleDescriptors::System::Alphabetic:
        return counterForSystemAlphabetic(value);
    case CSSCounterStyleDescriptors::System::Symbolic:
        return counterForSystemSymbolic(value);
    case CSSCounterStyleDescriptors::System::Additive:
        return counterForSystemAdditive(value);
    case CSSCounterStyleDescriptors::System::Fixed:
        return counterForSystemFixed(value);
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

    auto shouldApplyNegative = shouldApplyNegativeSymbols(value);
    auto result = initialRepresentation(shouldApplyNegative ? std::abs(value) : value);
    if (result.isEmpty())
        return fallbackText(value);
    applyPadSymbols(result, value);
    if (shouldApplyNegative)
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
    text = negative().m_suffix.isEmpty() ? makeString(negative().m_prefix, text) : makeString(negative().m_prefix, text, negative().m_suffix);
}

void CSSCounterStyle::applyPadSymbols(String& text, int value) const
{
    // FIXME: should we cap pad minimum length?
    if (pad().m_padMinimumLength <= 0)
        return;

    unsigned numberOfSymbolsToAdd { pad().m_padMinimumLength - WTF::numGraphemeClusters(text) };
    if (shouldApplyNegativeSymbols(value))
        numberOfSymbolsToAdd -= WTF::numGraphemeClusters(prefix()) + WTF::numGraphemeClusters(negative().m_suffix);

    String padText;
    for (unsigned i = 0; i < numberOfSymbolsToAdd; ++i)
        padText = makeString(padText, pad().m_padSymbol);
    text = makeString(text, padText);
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
    m_isExtendedUnresolved = false;

    setSystem(extendedCounterStyle.system());

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
}
