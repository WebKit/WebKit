/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller ( mueller@kde.org )
 * Copyright (C) 2003-2021 Apple Inc. All rights reserved.
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

#include "AnimationUtilities.h"
#include "CalcExpressionBlendLength.h"
#include "CalcExpressionLength.h"
#include "CalcExpressionOperation.h"
#include "CalculationValue.h"
#include <wtf/ASCIICType.h>
#include <wtf/HashMap.h>
#include <wtf/MallocPtr.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/text/StringView.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

static Length parseLength(const UChar* data, unsigned length)
{
    if (length == 0)
        return Length(1, LengthType::Relative);

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
            return Length(r, LengthType::Percent);
        return Length(1, LengthType::Relative);
    }
    auto r = parseInteger<int>({ data, intLength });
    if (next == '*')
        return Length(r.value_or(1), LengthType::Relative);
    if (r)
        return Length(*r, LengthType::Fixed);
    return Length(0, LengthType::Relative);
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
    LChar* spacifiedCharacters;
    auto str = StringImpl::createUninitialized(length, spacifiedCharacters);
    for (unsigned i = 0; i < length; i++) {
        UChar cc = string[i];
        if (cc > '9' || (cc < '0' && cc != '-' && cc != '*' && cc != '.'))
            spacifiedCharacters[i] = ' ';
        else
            spacifiedCharacters[i] = cc;
    }
    str = str->simplifyWhiteSpace();

    len = countCharacter(str, ' ') + 1;
    auto r = makeUniqueArray<Length>(len);

    int i = 0;
    unsigned pos = 0;
    size_t pos2;

    while ((pos2 = str->find(' ', pos)) != notFound) {
        r[i++] = parseLength(str->characters16() + pos, pos2 - pos);
        pos = pos2+1;
    }
    r[i] = parseLength(str->characters16() + pos, str->length() - pos);

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
    : m_type(LengthType::Calculated)
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

float Length::nonNanCalculatedValue(float maxValue) const
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

static Length makeCalculated(CalcOperator calcOperator, const Length& a, const Length& b)
{
    auto lengths = Vector<std::unique_ptr<CalcExpressionNode>>::from(makeUnique<CalcExpressionLength>(a), makeUnique<CalcExpressionLength>(b));
    auto op = makeUnique<CalcExpressionOperation>(WTFMove(lengths), calcOperator);
    return Length(CalculationValue::create(WTFMove(op), ValueRange::All));
}

Length convertTo100PercentMinusLength(const Length& length)
{
    if (length.isPercent())
        return Length(100 - length.value(), LengthType::Percent);
    
    // Turn this into a calc expression: calc(100% - length)
    return makeCalculated(CalcOperator::Subtract, Length(100, LengthType::Percent), length);
}

static Length blendMixedTypes(const Length& from, const Length& to, const BlendingContext& context)
{
    if (context.compositeOperation != CompositeOperation::Replace)
        return makeCalculated(CalcOperator::Add, from, to);

    if (!to.isCalculated() && !from.isPercent() && (context.progress == 1 || from.isZero()))
        return blend(Length(0, to.type()), to, context);

    if (!from.isCalculated() && !to.isPercent() && (!context.progress || to.isZero()))
        return blend(from, Length(0, from.type()), context);

    if (from.isIntrinsicOrAuto() || to.isIntrinsicOrAuto() || from.isRelative() || to.isRelative())
        return { 0, LengthType::Fixed };

    auto blend = makeUnique<CalcExpressionBlendLength>(from, to, context.progress);
    return Length(CalculationValue::create(WTFMove(blend), ValueRange::All));
}

Length blend(const Length& from, const Length& to, const BlendingContext& context)
{
    if ((from.isAuto() || to.isAuto()) || (from.isUndefined() || to.isUndefined()))
        return context.progress < 0.5 ? from : to;

    if (from.isCalculated() || to.isCalculated() || (from.type() != to.type()))
        return blendMixedTypes(from, to, context);

    if (!context.progress && context.isReplace())
        return from;

    if (context.progress == 1 && context.isReplace())
        return to;

    LengthType resultType = to.type();
    if (to.isZero())
        resultType = from.type();

    if (resultType == LengthType::Percent) {
        float fromPercent = from.isZero() ? 0 : from.percent();
        float toPercent = to.isZero() ? 0 : to.percent();
        return Length(WebCore::blend(fromPercent, toPercent, context), LengthType::Percent);
    }

    float fromValue = from.isZero() ? 0 : from.value();
    float toValue = to.isZero() ? 0 : to.value();
    return Length(WebCore::blend(fromValue, toValue, context), resultType);
}

Length blend(const Length& from, const Length& to, const BlendingContext& context, ValueRange valueRange)
{
    auto blended = blend(from, to, context);
    if (valueRange == ValueRange::NonNegative && blended.isNegative()) {
        auto type = from.isZero() ? to.type() : from.type();
        if (type != LengthType::Calculated)
            return { 0, type };
        return { 0, LengthType::Fixed };
    }
    return blended;
}

struct SameSizeAsLength {
    int32_t value;
    int32_t metaData;
};
static_assert(sizeof(Length) == sizeof(SameSizeAsLength), "length should stay small");

static TextStream& operator<<(TextStream& ts, LengthType type)
{
    switch (type) {
    case LengthType::Auto: ts << "auto"; break;
    case LengthType::Relative: ts << "relative"; break;
    case LengthType::Percent: ts << "percent"; break;
    case LengthType::Fixed: ts << "fixed"; break;
    case LengthType::Intrinsic: ts << "intrinsic"; break;
    case LengthType::MinIntrinsic: ts << "min-intrinsic"; break;
    case LengthType::MinContent: ts << "min-content"; break;
    case LengthType::MaxContent: ts << "max-content"; break;
    case LengthType::FillAvailable: ts << "fill-available"; break;
    case LengthType::FitContent: ts << "fit-content"; break;
    case LengthType::Calculated: ts << "calc"; break;
    case LengthType::Content: ts << "content"; break;
    case LengthType::Undefined: ts << "undefined"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, Length length)
{
    switch (length.type()) {
    case LengthType::Auto:
    case LengthType::Content:
    case LengthType::Undefined:
        ts << length.type();
        break;
    case LengthType::Fixed:
        ts << TextStream::FormatNumberRespectingIntegers(length.value()) << "px";
        break;
    case LengthType::Relative:
    case LengthType::Intrinsic:
    case LengthType::MinIntrinsic:
    case LengthType::MinContent:
    case LengthType::MaxContent:
    case LengthType::FillAvailable:
    case LengthType::FitContent:
        ts << length.type() << " " << TextStream::FormatNumberRespectingIntegers(length.value());
        break;
    case LengthType::Percent:
        ts << TextStream::FormatNumberRespectingIntegers(length.percent()) << "%";
        break;
    case LengthType::Calculated:
        ts << length.calculationValue();
        break;
    }
    
    if (length.hasQuirk())
        ts << " has-quirk";

    return ts;
}

} // namespace WebCore
