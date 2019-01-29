/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller ( mueller@kde.org )
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
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
#include "Length.h"

#include "CalculationValue.h"
#include <wtf/ASCIICType.h>
#include <wtf/HashMap.h>
#include <wtf/MallocPtr.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringBuffer.h>
#include <wtf/text/StringView.h>
#include <wtf/text/TextStream.h>


namespace WebCore {

static Length parseLength(const UChar* data, unsigned length)
{
    if (length == 0)
        return Length(1, Relative);

    unsigned i = 0;
    while (i < length && isSpaceOrNewline(data[i]))
        ++i;
    if (i < length && (data[i] == '+' || data[i] == '-'))
        ++i;
    while (i < length && isASCIIDigit(data[i]))
        ++i;
    unsigned intLength = i;
    while (i < length && (isASCIIDigit(data[i]) || data[i] == '.'))
        ++i;
    unsigned doubleLength = i;

    // IE quirk: Skip whitespace between the number and the % character (20 % => 20%).
    while (i < length && isSpaceOrNewline(data[i]))
        ++i;

    bool ok;
    UChar next = (i < length) ? data[i] : ' ';
    if (next == '%') {
        // IE quirk: accept decimal fractions for percentages.
        double r = charactersToDouble(data, doubleLength, &ok);
        if (ok)
            return Length(r, Percent);
        return Length(1, Relative);
    }
    int r = charactersToIntStrict(data, intLength, &ok);
    if (next == '*') {
        if (ok)
            return Length(r, Relative);
        return Length(1, Relative);
    }
    if (ok)
        return Length(r, Fixed);
    return Length(0, Relative);
}

static unsigned countCharacter(StringImpl& string, UChar character)
{
    unsigned count = 0;
    unsigned length = string.length();
    for (unsigned i = 0; i < length; ++i)
        count += string[i] == character;
    return count;
}

UniqueArray<Length> newCoordsArray(const String& string, int& len)
{
    unsigned length = string.length();
    StringBuffer<UChar> spacified(length);
    for (unsigned i = 0; i < length; i++) {
        UChar cc = string[i];
        if (cc > '9' || (cc < '0' && cc != '-' && cc != '*' && cc != '.'))
            spacified[i] = ' ';
        else
            spacified[i] = cc;
    }
    RefPtr<StringImpl> str = StringImpl::adopt(WTFMove(spacified));

    str = str->simplifyWhiteSpace();

    len = countCharacter(*str, ' ') + 1;
    auto r = makeUniqueArray<Length>(len);

    int i = 0;
    unsigned pos = 0;
    size_t pos2;

    auto upconvertedCharacters = StringView(str.get()).upconvertedCharacters();
    while ((pos2 = str->find(' ', pos)) != notFound) {
        r[i++] = parseLength(upconvertedCharacters + pos, pos2 - pos);
        pos = pos2+1;
    }
    r[i] = parseLength(upconvertedCharacters + pos, str->length() - pos);

    ASSERT(i == len - 1);

    return r;
}

UniqueArray<Length> newLengthArray(const String& string, int& len)
{
    RefPtr<StringImpl> str = string.impl()->simplifyWhiteSpace();
    if (!str->length()) {
        len = 1;
        return nullptr;
    }

    len = countCharacter(*str, ',') + 1;
    auto r = makeUniqueArray<Length>(len);

    int i = 0;
    unsigned pos = 0;
    size_t pos2;

    auto upconvertedCharacters = StringView(str.get()).upconvertedCharacters();
    while ((pos2 = str->find(',', pos)) != notFound) {
        r[i++] = parseLength(upconvertedCharacters + pos, pos2 - pos);
        pos = pos2+1;
    }

    ASSERT(i == len - 1);

    // IE Quirk: If the last comma is the last char skip it and reduce len by one.
    if (str->length()-pos > 0)
        r[i] = parseLength(upconvertedCharacters + pos, str->length() - pos);
    else
        len--;

    return r;
}

class CalculationValueMap {
public:
    CalculationValueMap();

    unsigned insert(Ref<CalculationValue>&&);
    void ref(unsigned handle);
    void deref(unsigned handle);

    CalculationValue& get(unsigned handle) const;

private:
    struct Entry {
        uint64_t referenceCountMinusOne;
        CalculationValue* value;
        Entry();
        Entry(CalculationValue&);
    };

    unsigned m_nextAvailableHandle;
    HashMap<unsigned, Entry> m_map;
};

inline CalculationValueMap::Entry::Entry()
    : referenceCountMinusOne(0)
    , value(nullptr)
{
}

inline CalculationValueMap::Entry::Entry(CalculationValue& value)
    : referenceCountMinusOne(0)
    , value(&value)
{
}

inline CalculationValueMap::CalculationValueMap()
    : m_nextAvailableHandle(1)
{
}
    
inline unsigned CalculationValueMap::insert(Ref<CalculationValue>&& value)
{
    ASSERT(m_nextAvailableHandle);

    // The leakRef below is balanced by the adoptRef in the deref member function.
    Entry leakedValue = value.leakRef();

    // FIXME: This monotonically increasing handle generation scheme is potentially wasteful
    // of the handle space. Consider reusing empty handles. https://bugs.webkit.org/show_bug.cgi?id=80489
    while (!m_map.isValidKey(m_nextAvailableHandle) || !m_map.add(m_nextAvailableHandle, leakedValue).isNewEntry)
        ++m_nextAvailableHandle;

    return m_nextAvailableHandle++;
}

inline CalculationValue& CalculationValueMap::get(unsigned handle) const
{
    ASSERT(m_map.contains(handle));

    return *m_map.find(handle)->value.value;
}

inline void CalculationValueMap::ref(unsigned handle)
{
    ASSERT(m_map.contains(handle));

    ++m_map.find(handle)->value.referenceCountMinusOne;
}

inline void CalculationValueMap::deref(unsigned handle)
{
    ASSERT(m_map.contains(handle));

    auto it = m_map.find(handle);
    if (it->value.referenceCountMinusOne) {
        --it->value.referenceCountMinusOne;
        return;
    }

    // The adoptRef here is balanced by the leakRef in the insert member function.
    Ref<CalculationValue> value { adoptRef(*it->value.value) };

    m_map.remove(it);
}

static CalculationValueMap& calculationValues()
{
    static NeverDestroyed<CalculationValueMap> map;
    return map;
}

Length::Length(Ref<CalculationValue>&& value)
    : m_hasQuirk(false)
    , m_type(Calculated)
    , m_isFloat(false)
{
    m_calculationValueHandle = calculationValues().insert(WTFMove(value));
}

CalculationValue& Length::calculationValue() const
{
    ASSERT(isCalculated());
    return calculationValues().get(m_calculationValueHandle);
}
    
void Length::ref() const
{
    ASSERT(isCalculated());
    calculationValues().ref(m_calculationValueHandle);
}

void Length::deref() const
{
    ASSERT(isCalculated());
    calculationValues().deref(m_calculationValueHandle);
}

float Length::nonNanCalculatedValue(int maxValue) const
{
    ASSERT(isCalculated());
    float result = calculationValue().evaluate(maxValue);
    if (std::isnan(result))
        return 0;
    return result;
}

bool Length::isCalculatedEqual(const Length& other) const
{
    return calculationValue() == other.calculationValue();
}

Length convertTo100PercentMinusLength(const Length& length)
{
    if (length.isPercent())
        return Length(100 - length.value(), Percent);
    
    // Turn this into a calc expression: calc(100% - length)
    Vector<std::unique_ptr<CalcExpressionNode>> lengths;
    lengths.reserveInitialCapacity(2);
    lengths.uncheckedAppend(std::make_unique<CalcExpressionLength>(Length(100, Percent)));
    lengths.uncheckedAppend(std::make_unique<CalcExpressionLength>(length));
    auto op = std::make_unique<CalcExpressionOperation>(WTFMove(lengths), CalcOperator::Subtract);
    return Length(CalculationValue::create(WTFMove(op), ValueRangeAll));
}

static Length blendMixedTypes(const Length& from, const Length& to, double progress)
{
    if (progress <= 0.0)
        return from;
        
    if (progress >= 1.0)
        return to;
        
    auto blend = std::make_unique<CalcExpressionBlendLength>(from, to, progress);
    return Length(CalculationValue::create(WTFMove(blend), ValueRangeAll));
}

Length blend(const Length& from, const Length& to, double progress)
{
    if (from.isAuto() || to.isAuto())
        return progress < 0.5 ? from : to;

    if (from.isUndefined() || to.isUndefined())
        return to;

    if (from.type() == Calculated || to.type() == Calculated)
        return blendMixedTypes(from, to, progress);

    if (!from.isZero() && !to.isZero() && from.type() != to.type())
        return blendMixedTypes(from, to, progress);

    LengthType resultType = to.type();
    if (to.isZero())
        resultType = from.type();

    if (resultType == Percent) {
        float fromPercent = from.isZero() ? 0 : from.percent();
        float toPercent = to.isZero() ? 0 : to.percent();
        return Length(WebCore::blend(fromPercent, toPercent, progress), Percent);
    }

    float fromValue = from.isZero() ? 0 : from.value();
    float toValue = to.isZero() ? 0 : to.value();
    return Length(WebCore::blend(fromValue, toValue, progress), resultType);
}

struct SameSizeAsLength {
    int32_t value;
    int32_t metaData;
};
COMPILE_ASSERT(sizeof(Length) == sizeof(SameSizeAsLength), length_should_stay_small);

static TextStream& operator<<(TextStream& ts, LengthType type)
{
    switch (type) {
    case Auto: ts << "auto"; break;
    case Relative: ts << "relative"; break;
    case Percent: ts << "percent"; break;
    case Fixed: ts << "fixed"; break;
    case Intrinsic: ts << "intrinsic"; break;
    case MinIntrinsic: ts << "min-intrinsic"; break;
    case MinContent: ts << "min-content"; break;
    case MaxContent: ts << "max-content"; break;
    case FillAvailable: ts << "fill-available"; break;
    case FitContent: ts << "fit-content"; break;
    case Calculated: ts << "calc"; break;
    case Undefined: ts << "undefined"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, Length length)
{
    switch (length.type()) {
    case Auto:
    case Undefined:
        ts << length.type();
        break;
    case Fixed:
        ts << TextStream::FormatNumberRespectingIntegers(length.value()) << "px";
        break;
    case Relative:
    case Intrinsic:
    case MinIntrinsic:
    case MinContent:
    case MaxContent:
    case FillAvailable:
    case FitContent:
        ts << length.type() << " " << TextStream::FormatNumberRespectingIntegers(length.value());
        break;
    case Percent:
        ts << TextStream::FormatNumberRespectingIntegers(length.percent()) << "%";
        break;
    case Calculated:
        ts << length.calculationValue();
        break;
    }
    
    if (length.hasQuirk())
        ts << " has-quirk";

    return ts;
}

} // namespace WebCore
