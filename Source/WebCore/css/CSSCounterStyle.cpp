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
#include "CSSCounterStyleRule.h"
#include "CSSPrimitiveValue.h"
#include "CSSValuePair.h"

namespace WebCore {

String CSSCounterStyle::counterForSystemCyclic(int)
{
    // FIXME: implement counter for system rdar://103648354
    return ""_s;
}
String CSSCounterStyle::counterForSystemFixed(int)
{
    // FIXME: implement counter for system rdar://103648354
    return ""_s;
}
String CSSCounterStyle::counterForSystemSymbolic(int)
{
    // FIXME: implement counter for system rdar://103648354
    return ""_s;
}
String CSSCounterStyle::counterForSystemAlphabetic(int)
{
    // FIXME: implement counter for system rdar://103648354
    return ""_s;
}
String CSSCounterStyle::counterForSystemNumeric(int)
{
    // FIXME: implement counter for system rdar://103648354
    return ""_s;
}
String CSSCounterStyle::counterForSystemAdditive(int)
{
    // FIXME: implement counter for system rdar://103648354
    return ""_s;
}
String CSSCounterStyle::initialRepresentation(int)
{
    // FIXME: implement counter initial representation rdar://103648354
    return ""_s;
}
String CSSCounterStyle::text(int value)
{
// FIXME: implement text representation for CSSCounterStyle rdar://103648354.
    return String::number(value);
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

Ref<CSSCounterStyle> CSSCounterStyle::createCounterStyleDecimal()
{
    Vector<CSSCounterStyleDescriptors::Symbol> symbols { "0"_s, "1"_s, "2"_s, "3"_s, "4"_s, "5"_s, "6"_s, "7"_s, "8"_s, "9"_s };
    CSSCounterStyleDescriptors descriptors {
        .m_name = "decimal"_s,
        .m_system = CSSCounterStyleDescriptors::System::Numeric,
        .m_negativeSymbols = { },
        .m_prefix = { },
        .m_suffix = { },
        .m_ranges = { },
        .m_pad = { },
        .m_fallbackName = { },
        .m_symbols = WTFMove(symbols),
        .m_additiveSymbols = { },
        .m_speakAs = CSSCounterStyleDescriptors::SpeakAs::Auto,
        .m_extendsName = { },
        .m_fixedSystemFirstSymbolValue = 0,
        .m_explicitlySetDescriptors = { }
    };
    return adoptRef(*new CSSCounterStyle(descriptors, true));
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
