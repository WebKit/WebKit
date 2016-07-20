/*
 * Copyright (C) Research In Motion Limited 2010, 2012. All rights reserved.
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

#ifndef SVGAnimatedPathSegListPropertyTearOff_h
#define SVGAnimatedPathSegListPropertyTearOff_h

#include "SVGAnimatedListPropertyTearOff.h"
#include "SVGPathByteStream.h"
#include "SVGPathElement.h"
#include "SVGPathSegList.h"
#include "SVGPathSegListPropertyTearOff.h"
#include "SVGPathUtilities.h"

namespace WebCore {

class SVGAnimatedPathSegListPropertyTearOff final : public SVGAnimatedListPropertyTearOff<SVGPathSegList> {
public:
    RefPtr<ListProperty> baseVal() final
    {
        if (m_baseVal)
            return m_baseVal;

        auto property = SVGPathSegListPropertyTearOff::create(this, BaseValRole, PathSegUnalteredRole, m_values, m_wrappers);
        m_baseVal = property.ptr();
        return WTFMove(property);
    }

    RefPtr<ListProperty> animVal() final
    {
        if (m_animVal)
            return m_animVal;

        auto property = SVGPathSegListPropertyTearOff::create(this, AnimValRole, PathSegUnalteredRole, m_values, m_wrappers);
        m_animVal = property.ptr();
        return WTFMove(property);
    }

    int findItem(const RefPtr<SVGPathSeg>& segment)
    {
        // This should ever be called for our baseVal, as animVal can't modify the list.
        return static_cast<SVGPathSegListPropertyTearOff*>(baseVal().get())->findItem(segment);
    }

    void removeItemFromList(size_t itemIndex, bool shouldSynchronizeWrappers)
    {
        // This should ever be called for our baseVal, as animVal can't modify the list.
        static_cast<SVGPathSegListPropertyTearOff*>(baseVal().get())->removeItemFromList(itemIndex, shouldSynchronizeWrappers);
    }

    static Ref<SVGAnimatedPathSegListPropertyTearOff> create(SVGElement* contextElement, const QualifiedName& attributeName, AnimatedPropertyType animatedPropertyType, SVGPathSegList& values)
    {
        ASSERT(contextElement);
        return adoptRef(*new SVGAnimatedPathSegListPropertyTearOff(contextElement, attributeName, animatedPropertyType, values));
    }

    using SVGAnimatedListPropertyTearOff<SVGPathSegList>::animationStarted;
    void animationStarted(SVGPathByteStream* byteStream, const SVGPathSegList* baseValue)
    {
        ASSERT(byteStream);
        ASSERT(baseValue);
        ASSERT(!m_animatedPathByteStream);
        m_animatedPathByteStream = byteStream;

        // Pass shouldOwnValues=true, as the SVGPathSegList lifetime is solely managed by its tear off class.
        SVGPathSegList* copy = new SVGPathSegList(*baseValue);
        SVGAnimatedListPropertyTearOff<SVGPathSegList>::animationStarted(copy, true);
    }

    void animationEnded()
    {
        ASSERT(m_animatedPathByteStream);
        m_animatedPathByteStream = nullptr;
        SVGAnimatedListPropertyTearOff<SVGPathSegList>::animationEnded();
    }

    void animValDidChange()
    {
        ASSERT(m_animatedPathByteStream);
        SVGPathElement* pathElement = downcast<SVGPathElement>(contextElement());

        // If the animVal is observed from JS, we have to update it on each animation step.
        // This is an expensive operation and only done, if someone actually observes the animatedPathSegList() while an animation is running.
        if (pathElement->isAnimValObserved()) {
            SVGPathSegList& animatedList = currentAnimatedValue();
            animatedList.clear();
            buildSVGPathSegListFromByteStream(*m_animatedPathByteStream, *pathElement, animatedList, UnalteredParsing);
        }

        SVGAnimatedListPropertyTearOff<SVGPathSegList>::animValDidChange();
    }

    SVGPathByteStream* animatedPathByteStream() const { return m_animatedPathByteStream; }

private:
    SVGAnimatedPathSegListPropertyTearOff(SVGElement* contextElement, const QualifiedName& attributeName, AnimatedPropertyType animatedPropertyType, SVGPathSegList& values)
        : SVGAnimatedListPropertyTearOff<SVGPathSegList>(contextElement, attributeName, animatedPropertyType, values)
        , m_animatedPathByteStream(nullptr)
    {
        ASSERT(contextElement);
        ASSERT(is<SVGPathElement>(contextElement));
    }

    virtual ~SVGAnimatedPathSegListPropertyTearOff()
    {
        downcast<SVGPathElement>(contextElement())->animatedPropertyWillBeDeleted();
    }

    SVGPathByteStream* m_animatedPathByteStream;
};

}

#endif // SVGAnimatedPathSegListPropertyTearOff_h
