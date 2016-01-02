/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#ifndef CSSValueList_h
#define CSSValueList_h

#include "CSSValue.h"
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

struct CSSParserValue;
class CSSParserValueList;

class CSSValueList : public CSSValue {
public:
    typedef Vector<Ref<CSSValue>, 4>::iterator iterator;
    typedef Vector<Ref<CSSValue>, 4>::const_iterator const_iterator;

    static Ref<CSSValueList> createCommaSeparated()
    {
        return adoptRef(*new CSSValueList(CommaSeparator));
    }
    static Ref<CSSValueList> createSpaceSeparated()
    {
        return adoptRef(*new CSSValueList(SpaceSeparator));
    }
    static Ref<CSSValueList> createSlashSeparated()
    {
        return adoptRef(*new CSSValueList(SlashSeparator));
    }
    static Ref<CSSValueList> createFromParserValueList(CSSParserValueList& list)
    {
        return adoptRef(*new CSSValueList(list));
    }

    size_t length() const { return m_values.size(); }
    CSSValue* item(size_t index) { return index < m_values.size() ? m_values[index].ptr() : nullptr; }
    const CSSValue* item(size_t index) const { return index < m_values.size() ? m_values[index].ptr() : nullptr; }
    CSSValue* itemWithoutBoundsCheck(size_t index) { return m_values[index].ptr(); }
    const CSSValue* itemWithoutBoundsCheck(size_t index) const { ASSERT(index < m_values.size()); return m_values[index].ptr(); }

    const_iterator begin() const { return m_values.begin(); }
    const_iterator end() const { return m_values.end(); }
    iterator begin() { return m_values.begin(); }
    iterator end() { return m_values.end(); }

    void append(Ref<CSSValue>&&);
    void prepend(Ref<CSSValue>&&);
    bool removeAll(CSSValue*);
    bool hasValue(CSSValue*) const;
    PassRefPtr<CSSValueList> copy();

    String customCSSText() const;
    bool equals(const CSSValueList&) const;
    bool equals(const CSSValue&) const;

    void addSubresourceStyleURLs(ListHashSet<URL>&, const StyleSheetContents*) const;

    bool traverseSubresources(const std::function<bool (const CachedResource&)>& handler) const;
    
    Ref<CSSValueList> cloneForCSSOM() const;

    bool containsVariables() const;
    bool checkVariablesForCycles(CustomPropertyValueMap& customProperties, HashSet<AtomicString>& seenProperties, HashSet<AtomicString>& invalidProperties) const;
    
    bool buildParserValueListSubstitutingVariables(CSSParserValueList*, const CustomPropertyValueMap& customProperties) const;
    bool buildParserValueSubstitutingVariables(CSSParserValue*, const CustomPropertyValueMap& customProperties) const;
    
protected:
    CSSValueList(ClassType, ValueListSeparator);
    CSSValueList(const CSSValueList& cloneFrom);

private:
    explicit CSSValueList(ValueListSeparator);
    explicit CSSValueList(CSSParserValueList&);

    Vector<Ref<CSSValue>, 4> m_values;
};

inline void CSSValueList::append(Ref<CSSValue>&& value)
{
    m_values.append(WTFMove(value));
}

inline void CSSValueList::prepend(Ref<CSSValue>&& value)
{
    m_values.insert(0, WTFMove(value));
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSValueList, isValueList())

#endif // CSSValueList_h
