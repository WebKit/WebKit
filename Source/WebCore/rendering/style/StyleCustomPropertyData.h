/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef StyleCustomPropertyData_h
#define StyleCustomPropertyData_h

#include "CSSValue.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/text/AtomicStringHash.h>

namespace WebCore {

class StyleCustomPropertyData : public RefCounted<StyleCustomPropertyData> {
public:
    static Ref<StyleCustomPropertyData> create() { return adoptRef(*new StyleCustomPropertyData); }
    Ref<StyleCustomPropertyData> copy() const { return adoptRef(*new StyleCustomPropertyData(*this)); }
    
    bool operator==(const StyleCustomPropertyData& o) const
    {
        if (m_containsVariables != o.m_containsVariables)
            return false;
        
        if (m_values.size() != o.m_values.size())
            return false;
        
        for (WTF::KeyValuePair<AtomicString, RefPtr<CSSValue>> entry : m_values) {
            RefPtr<CSSValue> other = o.m_values.get(entry.key);
            if (!other || !entry.value->equals(*other))
                return false;
        }
        return true;
    }

    bool operator!=(const StyleCustomPropertyData &o) const { return !(*this == o); }
    
    void setCustomPropertyValue(const AtomicString& name, const RefPtr<CSSValue>& value)
    {
        m_values.set(name, value);
        if (value->isVariableDependentValue())
            m_containsVariables = true;
    }

    RefPtr<CSSValue> getCustomPropertyValue(const AtomicString& name) const { return m_values.get(name); }
    CustomPropertyValueMap& values() { return m_values; }
    
    bool hasCustomProperty(const AtomicString& name) const { return m_values.contains(name); }
    
    bool containsVariables() const { return m_containsVariables; }
    void setContainsVariables(bool containsVariables) { m_containsVariables = containsVariables; }

    CustomPropertyValueMap m_values;
    bool m_containsVariables { false };

private:
    explicit StyleCustomPropertyData()
        : RefCounted<StyleCustomPropertyData>()
    { }
    StyleCustomPropertyData(const StyleCustomPropertyData& other)
        : RefCounted<StyleCustomPropertyData>()
        , m_values(CustomPropertyValueMap(other.m_values))
        , m_containsVariables(other.m_containsVariables)
    { }
};

} // namespace WebCore

#endif // StyleCustomPropertyData_h
