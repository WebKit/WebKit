/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#pragma once

#include "SVGAnimatedListPropertyTearOff.h"
#include "SVGTransformList.h"

namespace WebCore {

class SVGAnimatedTransformListPropertyTearOff final : public SVGAnimatedListPropertyTearOff<SVGTransformListValues> {
public:
    static Ref<SVGAnimatedTransformListPropertyTearOff> create(SVGElement* contextElement, const QualifiedName& attributeName, AnimatedPropertyType animatedPropertyType, SVGTransformListValues& values)
    {
        ASSERT(contextElement);
        return adoptRef(*new SVGAnimatedTransformListPropertyTearOff(contextElement, attributeName, animatedPropertyType, values));
    }

    Ref<ListPropertyTearOff> baseVal() final
    {
        if (m_baseVal)
            return *m_baseVal;

        auto property = SVGTransformList::create(*this, BaseValRole, m_values, m_wrappers);
        m_baseVal = property.ptr();
        return property;
    }

    Ref<ListPropertyTearOff> animVal() final
    {
        if (m_animVal)
            return *m_animVal;

        auto property = SVGTransformList::create(*this, AnimValRole, m_values, m_wrappers);
        m_animVal = property.ptr();
        return property;
    }

private:
    SVGAnimatedTransformListPropertyTearOff(SVGElement* contextElement, const QualifiedName& attributeName, AnimatedPropertyType animatedPropertyType, SVGTransformListValues& values)
        : SVGAnimatedListPropertyTearOff<SVGTransformListValues>(contextElement, attributeName, animatedPropertyType, values)
    {
    }
};

} // namespace WebCore
