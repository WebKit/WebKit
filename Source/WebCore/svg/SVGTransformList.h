/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
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
#include "SVGListPropertyTearOff.h"
#include "SVGTransformListValues.h"

namespace WebCore {

class SVGTransformList final : public SVGListPropertyTearOff<SVGTransformListValues> {
public:
    using AnimatedListPropertyTearOff = SVGAnimatedListPropertyTearOff<SVGTransformListValues>;
    using ListWrapperCache = AnimatedListPropertyTearOff::ListWrapperCache;

    static Ref<SVGTransformList> create(AnimatedListPropertyTearOff& animatedProperty, SVGPropertyRole role, SVGTransformListValues& values, ListWrapperCache& wrappers)
    {
        return adoptRef(*new SVGTransformList(animatedProperty, role, values, wrappers));
    }

    ExceptionOr<Ref<SVGTransform>> createSVGTransformFromMatrix(SVGMatrix& matrix)
    {
        ASSERT(m_values);
        return m_values->createSVGTransformFromMatrix(matrix);
    }

    ExceptionOr<RefPtr<SVGTransform>> consolidate()
    {
        ASSERT(m_values);
        ASSERT(m_wrappers);

        auto result = canAlterList();
        if (result.hasException())
            return result.releaseException();
        ASSERT(result.releaseReturnValue());

        ASSERT(m_values->size() == m_wrappers->size());

        // Spec: If the list was empty, then a value of null is returned.
        if (m_values->isEmpty())
            return nullptr;

        detachListWrappers(0);
        
        RefPtr<SVGTransform> wrapper = m_values->consolidate();
        m_wrappers->append(makeWeakPtr(*wrapper));

        ASSERT(m_values->size() == m_wrappers->size());
        return wrapper;
    }

private:
    SVGTransformList(AnimatedListPropertyTearOff& animatedProperty, SVGPropertyRole role, SVGTransformListValues& values, ListWrapperCache& wrappers)
        : SVGListPropertyTearOff<SVGTransformListValues>(animatedProperty, role, values, wrappers)
    {
    }
};

} // namespace WebCore
