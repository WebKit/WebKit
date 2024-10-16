/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2022 Apple Inc. All rights reserved.
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

#include "CSSPrimitiveValue.h"
#include <wtf/Hasher.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

CSSValueContainingVector::CSSValueContainingVector(ClassType type, ValueSeparator separator)
    : CSSValue(type)
{
    m_valueSeparator = separator;
}

CSSValueContainingVector::CSSValueContainingVector(ClassType type, ValueSeparator separator, CSSValueListBuilder values)
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

CSSValueContainingVector::CSSValueContainingVector(ClassType type, ValueSeparator separator, Ref<CSSValue> value)
    : CSSValue(type)
    , m_size(1)
{
    m_valueSeparator = separator;
    m_inlineStorage[0] = &value.leakRef();
}

CSSValueContainingVector::CSSValueContainingVector(ClassType type, ValueSeparator separator, Ref<CSSValue> value1, Ref<CSSValue> value2)
    : CSSValue(type)
    , m_size(2)
{
    m_valueSeparator = separator;
    m_inlineStorage[0] = &value1.leakRef();
    m_inlineStorage[1] = &value2.leakRef();
}

CSSValueContainingVector::CSSValueContainingVector(ClassType type, ValueSeparator separator, Ref<CSSValue> value1, Ref<CSSValue> value2, Ref<CSSValue> value3)
    : CSSValue(type)
    , m_size(3)
{
    m_valueSeparator = separator;
    m_inlineStorage[0] = &value1.leakRef();
    m_inlineStorage[1] = &value2.leakRef();
    m_inlineStorage[2] = &value3.leakRef();
}

CSSValueContainingVector::CSSValueContainingVector(ClassType type, ValueSeparator separator, Ref<CSSValue> value1, Ref<CSSValue> value2, Ref<CSSValue> value3, Ref<CSSValue> value4)
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
    : CSSValueContainingVector(ClassType::ValueList, separator)
{
}

CSSValueList::CSSValueList(ValueSeparator separator, CSSValueListBuilder values)
    : CSSValueContainingVector(ClassType::ValueList, separator, WTFMove(values))
{
}

CSSValueList::CSSValueList(ValueSeparator separator, Ref<CSSValue> value)
    : CSSValueContainingVector(ClassType::ValueList, separator, WTFMove(value))
{
}

CSSValueList::CSSValueList(ValueSeparator separator, Ref<CSSValue> value1, Ref<CSSValue> value2)
    : CSSValueContainingVector(ClassType::ValueList, separator, WTFMove(value1), WTFMove(value2))
{
}

CSSValueList::CSSValueList(ValueSeparator separator, Ref<CSSValue> value1, Ref<CSSValue> value2, Ref<CSSValue> value3)
    : CSSValueContainingVector(ClassType::ValueList, separator, WTFMove(value1), WTFMove(value2), WTFMove(value3))
{
}

CSSValueList::CSSValueList(ValueSeparator separator, Ref<CSSValue> value1, Ref<CSSValue> value2, Ref<CSSValue> value3, Ref<CSSValue> value4)
    : CSSValueContainingVector(ClassType::ValueList, separator, WTFMove(value1), WTFMove(value2), WTFMove(value3), WTFMove(value4))
{
}

Ref<CSSValueList> CSSValueList::createCommaSeparated(CSSValueListBuilder values)
{
    return adoptRef(*new CSSValueList(CommaSeparator, WTFMove(values)));
}

Ref<CSSValueList> CSSValueList::createCommaSeparated(Ref<CSSValue> value)
{
    return adoptRef(*new CSSValueList(CommaSeparator, WTFMove(value)));
}

Ref<CSSValueList> CSSValueList::createSlashSeparated(CSSValueListBuilder values)
{
    return adoptRef(*new CSSValueList(SlashSeparator, WTFMove(values)));
}

Ref<CSSValueList> CSSValueList::createSlashSeparated(Ref<CSSValue> value)
{
    return adoptRef(*new CSSValueList(SlashSeparator, WTFMove(value)));
}

Ref<CSSValueList> CSSValueList::createSlashSeparated(Ref<CSSValue> value1, Ref<CSSValue> value2)
{
    return adoptRef(*new CSSValueList(SlashSeparator, WTFMove(value1), WTFMove(value2)));
}

Ref<CSSValueList> CSSValueList::createSpaceSeparated()
{
    return adoptRef(*new CSSValueList(SpaceSeparator));
}

Ref<CSSValueList> CSSValueList::createSpaceSeparated(CSSValueListBuilder values)
{
    return adoptRef(*new CSSValueList(SpaceSeparator, WTFMove(values)));
}

Ref<CSSValueList> CSSValueList::createSpaceSeparated(Ref<CSSValue> value)
{
    return adoptRef(*new CSSValueList(SpaceSeparator, WTFMove(value)));
}

Ref<CSSValueList> CSSValueList::createSpaceSeparated(Ref<CSSValue> value1, Ref<CSSValue> value2)
{
    return adoptRef(*new CSSValueList(SpaceSeparator, WTFMove(value1), WTFMove(value2)));
}

Ref<CSSValueList> CSSValueList::createSpaceSeparated(Ref<CSSValue> value1, Ref<CSSValue> value2, Ref<CSSValue> value3)
{
    return adoptRef(*new CSSValueList(SpaceSeparator, WTFMove(value1), WTFMove(value2), WTFMove(value3)));
}

Ref<CSSValueList> CSSValueList::createSpaceSeparated(Ref<CSSValue> value1, Ref<CSSValue> value2, Ref<CSSValue> value3, Ref<CSSValue> value4)
{
    return adoptRef(*new CSSValueList(SpaceSeparator, WTFMove(value1), WTFMove(value2), WTFMove(value3), WTFMove(value4)));
}

Ref<CSSValueList> CSSValueList::create(UChar separator, CSSValueListBuilder builder)
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
        if (WebCore::isValueID(value, otherValue))
            return true;
    }
    return false;
}

CSSValueListBuilder CSSValueContainingVector::copyValues() const
{
    return WTF::map<CSSValueListBuilderInlineCapacity>(*this, [](auto& value) -> Ref<CSSValue> {
        return const_cast<CSSValue&>(value);
    });
}

void CSSValueContainingVector::serializeItems(StringBuilder& builder) const
{
    builder.append(interleave(*this, [](auto& value) { return value.cssText(); }, separatorCSSText()));
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

bool CSSValueContainingVector::addDerivedHash(Hasher& hasher) const
{
    add(hasher, separator());

    for (auto& item : *this) {
        if (!item.addHash(hasher))
            return false;
    }
    return true;
}

bool CSSValueContainingVector::customTraverseSubresources(const Function<bool(const CachedResource&)>& handler) const
{
    for (auto& value : *this) {
        if (value.traverseSubresources(handler))
            return true;
    }
    return false;
}

void CSSValueContainingVector::customSetReplacementURLForSubresources(const UncheckedKeyHashMap<String, String>& replacementURLStrings)
{
    for (auto& value : *this)
        const_cast<CSSValue&>(value).setReplacementURLForSubresources(replacementURLStrings);
}

void CSSValueContainingVector::customClearReplacementURLForSubresources()
{
    for (auto& value : *this)
        const_cast<CSSValue&>(value).clearReplacementURLForSubresources();
}

IterationStatus CSSValueContainingVector::customVisitChildren(const Function<IterationStatus(CSSValue&)>& func) const
{
    for (auto& value : *this) {
        if (func(const_cast<CSSValue&>(value)) == IterationStatus::Done)
            return IterationStatus::Done;
    }
    return IterationStatus::Continue;
}

} // namespace WebCore
