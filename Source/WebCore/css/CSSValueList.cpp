/**
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2023 Apple Inc. All rights reserved.
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
 */

#include "config.h"
#include "CSSValueList.h"

#include <wtf/text/StringBuilder.h>

namespace WebCore {

CSSValueContainingVector::CSSValueContainingVector(Type type, ValueSeparator separator)
    : CSSValue(type)
{
    m_valueSeparator = separator;

    static_assert(ValueIDsShift + 2 * IdentBits <= 32);
    static_assert(ValueIDsShift + 4 * IdentBits <= 64);
}

CSSValueContainingVector::CSSValueContainingVector(Type type, ValueSeparator separator, CSSValueListBuilder&& values)
    : CSSValue(type)
    , m_size(values.size())
{
    m_valueSeparator = separator;

    RELEASE_ASSERT(values.size() <= std::numeric_limits<unsigned>::max());
    unsigned maxInlineSize = m_inlineStorage.size();
    if (m_size <= maxInlineSize) {
        for (unsigned i = 0; i < m_size; ++i)
            m_inlineStorage[i] = &values[i].leakRef();
    } else {
        for (unsigned i = 0; i < maxInlineSize; ++i)
            m_inlineStorage[i] = &values[i].leakRef();
        m_additionalStorage = static_cast<const CSSValue**>(fastMalloc(sizeof(const CSSValue*) * (m_size - maxInlineSize)));
        for (unsigned i = maxInlineSize; i < m_size; ++i)
            m_additionalStorage[i - maxInlineSize] = &values[i].leakRef();
    }
}

CSSValueContainingVector::CSSValueContainingVector(Type type, ValueSeparator separator, Ref<CSSValue>&& value)
    : CSSValue(type)
    , m_size(1)
{
    m_valueSeparator = separator;
    m_inlineStorage[0] = &value.leakRef();
}

CSSValueContainingVector::CSSValueContainingVector(Type type, ValueSeparator separator, Ref<CSSValue>&& value1, Ref<CSSValue>&& value2)
    : CSSValue(type)
    , m_size(2)
{
    m_valueSeparator = separator;
    m_inlineStorage[0] = &value1.leakRef();
    m_inlineStorage[1] = &value2.leakRef();
}

CSSValueContainingVector::CSSValueContainingVector(Type type, ValueSeparator separator, Ref<CSSValue>&& value1, Ref<CSSValue>&& value2, Ref<CSSValue>&& value3)
    : CSSValue(type)
    , m_size(3)
{
    m_valueSeparator = separator;
    m_inlineStorage[0] = &value1.leakRef();
    m_inlineStorage[1] = &value2.leakRef();
    m_inlineStorage[2] = &value3.leakRef();
}

CSSValueContainingVector::CSSValueContainingVector(Type type, ValueSeparator separator, Ref<CSSValue>&& value1, Ref<CSSValue>&& value2, Ref<CSSValue>&& value3, Ref<CSSValue>&& value4)
    : CSSValue(type)
    , m_size(4)
{
    m_valueSeparator = separator;
    m_inlineStorage[0] = &value1.leakRef();
    m_inlineStorage[1] = &value2.leakRef();
    m_inlineStorage[2] = &value3.leakRef();
    m_inlineStorage[3] = &value4.leakRef();
}

CSSValueList::CSSValueList(ValueSeparator separator)
    : CSSValueContainingVector(Type::ValueList, separator)
{
}

CSSValueList::CSSValueList(ValueSeparator separator, CSSValueListBuilder&& values)
    : CSSValueContainingVector(Type::ValueList, separator, WTFMove(values))
{
}

CSSValueList::CSSValueList(ValueSeparator separator, Ref<CSSValue>&& value)
    : CSSValueContainingVector(Type::ValueList, separator, WTFMove(value))
{
}

CSSValueList::CSSValueList(ValueSeparator separator, Ref<CSSValue>&& value1, Ref<CSSValue>&& value2)
    : CSSValueContainingVector(Type::ValueList, separator, WTFMove(value1), WTFMove(value2))
{
}

CSSValueList::CSSValueList(ValueSeparator separator, Ref<CSSValue>&& value1, Ref<CSSValue>&& value2, Ref<CSSValue>&& value3)
    : CSSValueContainingVector(Type::ValueList, separator, WTFMove(value1), WTFMove(value2), WTFMove(value3))
{
}

CSSValueList::CSSValueList(ValueSeparator separator, Ref<CSSValue>&& value1, Ref<CSSValue>&& value2, Ref<CSSValue>&& value3, Ref<CSSValue>&& value4)
    : CSSValueContainingVector(Type::ValueList, separator, WTFMove(value1), WTFMove(value2), WTFMove(value3), WTFMove(value4))
{
}

Ref<CSSValueList> CSSValueList::createCommaSeparated(CSSValueListBuilder&& values)
{
    return adoptRef(*new CSSValueList(CommaSeparator, WTFMove(values)));
}

Ref<CSSValueList> CSSValueList::createCommaSeparated(Ref<CSSValue>&& value)
{
    return adoptRef(*new CSSValueList(CommaSeparator, WTFMove(value)));
}

Ref<CSSValueList> CSSValueList::createSlashSeparated(CSSValueListBuilder&& values)
{
    return adoptRef(*new CSSValueList(SlashSeparator, WTFMove(values)));
}

Ref<CSSValueList> CSSValueList::createSlashSeparated(Ref<CSSValue>&& value)
{
    return adoptRef(*new CSSValueList(SlashSeparator, WTFMove(value)));
}

Ref<CSSValueList> CSSValueList::createSlashSeparated(Ref<CSSValue>&& value1, Ref<CSSValue>&& value2)
{
    return adoptRef(*new CSSValueList(SlashSeparator, WTFMove(value1), WTFMove(value2)));
}

Ref<CSSValueList> CSSValueList::createSpaceSeparated()
{
    return adoptRef(*new CSSValueList(SpaceSeparator));
}

inline CSSValueList& CSSValueList::listScalar(unsigned size, uintptr_t shiftedValues)
{
    return *reinterpret_cast<CSSValueList*>(typeScalar(Type::ValueList) | static_cast<uintptr_t>(size - 1) << SizeMinusOneShift | shiftedValues);
}

inline uintptr_t CSSValueList::shiftedValueID(CSSValueID value, unsigned index)
{
    ASSERT(value);
    return static_cast<uintptr_t>(value) << (ValueIDsShift + index * IdentBits);
}

Ref<CSSValueList> CSSValueList::createSpaceSeparated(CSSValueID value)
{
    return listScalar(1, shiftedValueID(value, 0));
}

Ref<CSSValueList> CSSValueList::createSpaceSeparated(CSSValueID value1, CSSValueID value2)
{
    return listScalar(2, shiftedValueID(value1, 0) | shiftedValueID(value2, 1));
}

Ref<CSSValueList> CSSValueList::createSpaceSeparated(CSSValueID value1, CSSValueID value2, CSSValueID value3)
{
    if constexpr (sizeof(uintptr_t) >= 4)
        return listScalar(3, shiftedValueID(value1, 0) | shiftedValueID(value2, 1) | shiftedValueID(value3, 2));
    else
        return createSpaceSeparated(CSSIdentValue::create(value1), CSSIdentValue::create(value2), CSSIdentValue::create(value3));
}

Ref<CSSValueList> CSSValueList::createSpaceSeparated(CSSValueID value1, CSSValueID value2, CSSValueID value3, CSSValueID value4)
{
    if constexpr (sizeof(uintptr_t) >= 4)
        return listScalar(4, shiftedValueID(value1, 0) | shiftedValueID(value2, 1) | shiftedValueID(value3, 2) | shiftedValueID(value4, 3));
    else
        return createSpaceSeparated(CSSIdentValue::create(value1), CSSIdentValue::create(value2), CSSIdentValue::create(value3));
}

Ref<CSSValueList> CSSValueList::createSpaceSeparated(Ref<CSSValue>&& value)
{
    if (value->isValueID())
        return createSpaceSeparated(value->valueID());
    return adoptRef(*new CSSValueList(SpaceSeparator, WTFMove(value)));
}

Ref<CSSValueList> CSSValueList::createSpaceSeparated(Ref<CSSValue>&& value1, Ref<CSSValue>&& value2)
{
    if (value1->isValueID() && value2->isValueID())
        return createSpaceSeparated(value1->valueID(), value2->valueID());
    return adoptRef(*new CSSValueList(SpaceSeparator, WTFMove(value1), WTFMove(value2)));
}

Ref<CSSValueList> CSSValueList::createSpaceSeparated(Ref<CSSValue>&& value1, Ref<CSSValue>&& value2, Ref<CSSValue>&& value3)
{
    if constexpr (sizeof(uintptr_t) >= 4) {
        if (value1->isValueID() && value2->isValueID() && value3->isValueID())
            return createSpaceSeparated(value1->valueID(), value2->valueID(), value3->valueID());
    }
    return adoptRef(*new CSSValueList(SpaceSeparator, WTFMove(value1), WTFMove(value2), WTFMove(value3)));
}

Ref<CSSValueList> CSSValueList::createSpaceSeparated(Ref<CSSValue>&& value1, Ref<CSSValue>&& value2, Ref<CSSValue>&& value3, Ref<CSSValue>&& value4)
{
    if constexpr (sizeof(uintptr_t) >= 4) {
        if (value1->isValueID() && value2->isValueID() && value3->isValueID() && value4->isValueID())
            return createSpaceSeparated(value1->valueID(), value2->valueID(), value3->valueID(), value4->valueID());
    }
    return adoptRef(*new CSSValueList(SpaceSeparator, WTFMove(value1), WTFMove(value2), WTFMove(value3), WTFMove(value4)));
}

Ref<CSSValueList> CSSValueList::createSpaceSeparated(CSSValueListBuilder&& values)
{
    switch (values.size()) {
    case 1:
        return createSpaceSeparated(WTFMove(values[0]));
    case 2:
        return createSpaceSeparated(WTFMove(values[0]), WTFMove(values[1]));
    case 3:
        return createSpaceSeparated(WTFMove(values[0]), WTFMove(values[1]), WTFMove(values[2]));
    case 4:
        return createSpaceSeparated(WTFMove(values[0]), WTFMove(values[1]), WTFMove(values[2]), WTFMove(values[3]));
    }
    return adoptRef(*new CSSValueList(SpaceSeparator, WTFMove(values)));
}

Ref<CSSValueList> CSSValueList::createSpaceSeparated(Span<const CSSValueID> values)
{
    switch (values.size()) {
    case 1:
        return createSpaceSeparated(values[0]);
    case 2:
        return createSpaceSeparated(values[0], values[1]);
    case 3:
        return createSpaceSeparated(values[0], values[1], values[2]);
    case 4:
        return createSpaceSeparated(values[0], values[1], values[2], values[3]);
    }
    CSSValueListBuilder contents;
    contents.reserveInitialCapacity(values.size());
    for (auto value : values)
        contents.uncheckedAppend(CSSIdentValue::create(value));
    return adoptRef(*new CSSValueList(SpaceSeparator, WTFMove(contents)));
}

Ref<CSSValueList> CSSValueList::create(UChar separator, CSSValueListBuilder&& builder)
{
    switch (separator) {
    case ',':
        return createCommaSeparated(WTFMove(builder));
    case '/':
        return createSlashSeparated(WTFMove(builder));
    case ' ':
        return createSpaceSeparated(WTFMove(builder));
    default:
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

bool CSSValueContainingVector::hasValue(CSSValue& otherValue) const
{
    for (auto& value : *this) {
        if (value.equals(otherValue))
            return true;
    }
    return false;
}

bool CSSValueContainingVector::hasValue(CSSValueID otherValue) const
{
    for (auto& value : *this) {
        if (value == otherValue)
            return true;
    }
    return false;
}

CSSValueListBuilder CSSValueContainingVector::copyValues() const
{
    CSSValueListBuilder builder;
    builder.reserveInitialCapacity(size());
    for (auto& value : *this)
        builder.uncheckedAppend(const_cast<CSSValue&>(value));
    return builder;
}

void CSSValueContainingVector::serializeItems(StringBuilder& builder) const
{
    auto prefix = ""_s;
    auto separator = separatorCSSText();
    for (auto& value : *this)
        builder.append(std::exchange(prefix, separator), value.cssText());
}

String CSSValueContainingVector::serializeItems() const
{
    StringBuilder result;
    serializeItems(result);
    return result.toString();
}

String CSSValueList::customCSSText() const
{
    return serializeItems();
}

bool CSSValueContainingVector::itemsEqual(const CSSValueContainingVector& other) const
{
    unsigned size = this->size();
    if (size != other.size())
        return false;
    for (unsigned i = 0; i < size; ++i) {
        if (!(*this)[i].equals(other[i]))
            return false;
    }
    return true;
}

bool CSSValueList::equals(const CSSValueList& other) const
{
    return separator() == other.separator() && itemsEqual(other);
}

bool CSSValueContainingVector::containsSingleEqualItem(const CSSValue& other) const
{
    return size() == 1 && (*this)[0].equals(other);
}

bool CSSValueContainingVector::customTraverseSubresources(const Function<bool(const CachedResource&)>& handler) const
{
    for (auto& value : *this) {
        if (value.traverseSubresources(handler))
            return true;
    }
    return false;
}

} // namespace WebCore
