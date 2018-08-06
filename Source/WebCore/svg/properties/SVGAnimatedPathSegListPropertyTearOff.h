/*
 * Copyright (C) Research In Motion Limited 2010, 2012. All rights reserved.
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "SVGPathByteStream.h"
#include "SVGPathSegList.h"
#include "SVGPathUtilities.h"

namespace WebCore {

class SVGAnimatedPathSegListPropertyTearOff final : public SVGAnimatedListPropertyTearOff<SVGPathSegListValues> {
public:
    using Base = SVGAnimatedListPropertyTearOff<SVGPathSegListValues>;

    static Ref<SVGAnimatedPathSegListPropertyTearOff> create(SVGElement* contextElement, const QualifiedName& attributeName, AnimatedPropertyType animatedPropertyType, SVGPathSegListValues& values)
    {
        ASSERT(contextElement);
        return adoptRef(*new SVGAnimatedPathSegListPropertyTearOff(contextElement, attributeName, animatedPropertyType, values));
    }

    Ref<ListPropertyTearOff> baseVal() final
    {
        if (m_baseVal)
            return *static_cast<ListPropertyTearOff*>(m_baseVal.get());

        auto property = SVGPathSegList::create(*this, BaseValRole, PathSegUnalteredRole, m_values, m_wrappers);
        m_baseVal = makeWeakPtr(property.get());
        return property;
    }

    Ref<ListPropertyTearOff> animVal() final
    {
        if (m_animVal)
            return *static_cast<ListPropertyTearOff*>(m_animVal.get());

        auto property = SVGPathSegList::create(*this, AnimValRole, PathSegUnalteredRole, m_values, m_wrappers);
        m_animVal = makeWeakPtr(property.get());
        return property;
    }

    int findItem(const RefPtr<SVGPathSeg>& segment)
    {
        return baseVal()->findItem(segment);
    }

    void removeItemFromList(size_t itemIndex, bool shouldSynchronizeWrappers)
    {
        baseVal()->removeItemFromList(itemIndex, shouldSynchronizeWrappers);
    }

    using Base::animationStarted;
    void animationStarted(SVGPathByteStream* byteStream, const SVGPathSegListValues* baseValue)
    {
        ASSERT(byteStream);
        ASSERT(baseValue);
        ASSERT(!m_animatedPathByteStream);
        m_animatedPathByteStream = byteStream;

        // Pass shouldOwnValues=true, as the SVGPathSegListValues lifetime is solely managed by its tear off class.
        auto* copy = new SVGPathSegListValues(*baseValue);
        Base::animationStarted(copy, true);
    }

    void animationEnded()
    {
        ASSERT(m_animatedPathByteStream);
        m_animatedPathByteStream = nullptr;
        Base::animationEnded();
    }

    void animValDidChange();

    SVGPathByteStream* animatedPathByteStream() const { return m_animatedPathByteStream; }

private:
    SVGAnimatedPathSegListPropertyTearOff(SVGElement*, const QualifiedName&, AnimatedPropertyType, SVGPathSegListValues&);
    virtual ~SVGAnimatedPathSegListPropertyTearOff();

    SVGPathByteStream* m_animatedPathByteStream { nullptr };
};

} // namespace WebCore
