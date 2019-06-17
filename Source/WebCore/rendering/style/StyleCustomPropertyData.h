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

#pragma once

#include "CSSCustomPropertyValue.h"
#include "CSSVariableReferenceValue.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/text/AtomStringHash.h>

namespace WebCore {

class StyleCustomPropertyData : public RefCounted<StyleCustomPropertyData> {
public:
    static Ref<StyleCustomPropertyData> create() { return adoptRef(*new StyleCustomPropertyData); }
    Ref<StyleCustomPropertyData> copy() const { return adoptRef(*new StyleCustomPropertyData(*this)); }
    
    bool operator==(const StyleCustomPropertyData& other) const
    {
        if (values.size() != other.values.size())
            return false;

        for (auto& entry : values) {
            auto otherEntry = other.values.find(entry.key);
            if (otherEntry == other.values.end() || !entry.value->equals(*otherEntry->value))
                return false;
        }

        return true;
    }

    bool operator!=(const StyleCustomPropertyData& other) const { return !(*this == other); }
    
    void setCustomPropertyValue(const AtomString& name, Ref<CSSCustomPropertyValue>&& value)
    {
        values.set(name, WTFMove(value));
    }

    CustomPropertyValueMap values;

private:
    StyleCustomPropertyData() = default;
    StyleCustomPropertyData(const StyleCustomPropertyData& other)
        : RefCounted<StyleCustomPropertyData>()
        , values(other.values)
    { }
};

} // namespace WebCore
