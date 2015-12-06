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

#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/text/AtomicStringHash.h>

namespace WebCore {

class CSSValue;

class StyleCustomPropertyData : public RefCounted<StyleCustomPropertyData> {
public:
    static Ref<StyleCustomPropertyData> create() { return adoptRef(*new StyleCustomPropertyData); }
    Ref<StyleCustomPropertyData> copy() const { return adoptRef(*new StyleCustomPropertyData(*this)); }
    
    bool operator==(const StyleCustomPropertyData& o) const { return m_values == o.m_values; }
    bool operator!=(const StyleCustomPropertyData &o) const { return !(*this == o); }
    
    void setCustomPropertyValue(const AtomicString& name, const RefPtr<CSSValue>& value) { m_values.set(name, value); }
    RefPtr<CSSValue> getCustomPropertyValue(const AtomicString& name) const { return m_values.get(name); }
    bool hasCustomProperty(const AtomicString& name) const { return m_values.contains(name); }

    HashMap<AtomicString, RefPtr<CSSValue>> m_values;
    
private:
    explicit StyleCustomPropertyData()
        : RefCounted<StyleCustomPropertyData>()
    { }
    StyleCustomPropertyData(const StyleCustomPropertyData& other)
        : RefCounted<StyleCustomPropertyData>()
        , m_values(HashMap<AtomicString, RefPtr<CSSValue>>(other.m_values))
    { }
};

} // namespace WebCore

#endif // StyleCustomPropertyData_h
