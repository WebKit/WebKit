/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

#include "Path.h"
#include "SVGGeometryElement.h"
#include "SVGNames.h"
#include "SVGPathByteStream.h"
#include "SVGPathSegImpl.h"

namespace WebCore {

class SVGPathSegList;
class SVGPoint;

class SVGPathElement final : public SVGGeometryElement {
    WTF_MAKE_ISO_ALLOCATED(SVGPathElement);
public:
    static Ref<SVGPathElement> create(const QualifiedName&, Document&);
    
    static Ref<SVGPathSegClosePath> createSVGPathSegClosePath() { return SVGPathSegClosePath::create(); }
    static Ref<SVGPathSegMovetoAbs> createSVGPathSegMovetoAbs(float x, float y) { return SVGPathSegMovetoAbs::create(x, y); }
    static Ref<SVGPathSegMovetoRel> createSVGPathSegMovetoRel(float x, float y) { return SVGPathSegMovetoRel::create(x, y); }
    static Ref<SVGPathSegLinetoAbs> createSVGPathSegLinetoAbs(float x, float y) { return SVGPathSegLinetoAbs::create(x, y); }
    static Ref<SVGPathSegLinetoRel> createSVGPathSegLinetoRel(float x, float y) { return SVGPathSegLinetoRel::create(x, y); }
    static Ref<SVGPathSegCurvetoCubicAbs> createSVGPathSegCurvetoCubicAbs(float x, float y, float x1, float y1, float x2, float y2)
    {
        return SVGPathSegCurvetoCubicAbs::create(x, y, x1, y1, x2, y2);
    }
    static Ref<SVGPathSegCurvetoCubicRel> createSVGPathSegCurvetoCubicRel(float x, float y, float x1, float y1, float x2, float y2)
    {
        return SVGPathSegCurvetoCubicRel::create(x, y, x1, y1, x2, y2);
    }
    static Ref<SVGPathSegCurvetoQuadraticAbs> createSVGPathSegCurvetoQuadraticAbs(float x, float y, float x1, float y1)
    {
        return SVGPathSegCurvetoQuadraticAbs::create(x, y, x1, y1);
    }
    static Ref<SVGPathSegCurvetoQuadraticRel> createSVGPathSegCurvetoQuadraticRel(float x, float y, float x1, float y1)
    {
        return SVGPathSegCurvetoQuadraticRel::create(x, y, x1, y1);
    }
    static Ref<SVGPathSegArcAbs> createSVGPathSegArcAbs(float x, float y, float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag)
    {
        return SVGPathSegArcAbs::create(x, y, r1, r2, angle, largeArcFlag, sweepFlag);
    }
    static Ref<SVGPathSegArcRel> createSVGPathSegArcRel(float x, float y, float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag)
    {
        return SVGPathSegArcRel::create(x, y, r1, r2, angle, largeArcFlag, sweepFlag);
    }
    static Ref<SVGPathSegLinetoHorizontalAbs> createSVGPathSegLinetoHorizontalAbs(float x) { return SVGPathSegLinetoHorizontalAbs::create(x); }
    static Ref<SVGPathSegLinetoHorizontalRel> createSVGPathSegLinetoHorizontalRel(float x) { return SVGPathSegLinetoHorizontalRel::create(x); }
    static Ref<SVGPathSegLinetoVerticalAbs> createSVGPathSegLinetoVerticalAbs(float y) { return SVGPathSegLinetoVerticalAbs::create(y); }
    static Ref<SVGPathSegLinetoVerticalRel> createSVGPathSegLinetoVerticalRel(float y) { return SVGPathSegLinetoVerticalRel::create(y); }
    static Ref<SVGPathSegCurvetoCubicSmoothAbs> createSVGPathSegCurvetoCubicSmoothAbs(float x, float y, float x2, float y2)
    {
        return SVGPathSegCurvetoCubicSmoothAbs::create(x, y, x2, y2);
    }
    static Ref<SVGPathSegCurvetoCubicSmoothRel> createSVGPathSegCurvetoCubicSmoothRel(float x, float y, float x2, float y2)
    {
        return SVGPathSegCurvetoCubicSmoothRel::create(x, y, x2, y2);
    }
    static Ref<SVGPathSegCurvetoQuadraticSmoothAbs> createSVGPathSegCurvetoQuadraticSmoothAbs(float x, float y)
    {
        return SVGPathSegCurvetoQuadraticSmoothAbs::create(x, y);
    }
    static Ref<SVGPathSegCurvetoQuadraticSmoothRel> createSVGPathSegCurvetoQuadraticSmoothRel(float x, float y)
    {
        return SVGPathSegCurvetoQuadraticSmoothRel::create(x, y);
    }

    float getTotalLength() const final;
    ExceptionOr<Ref<SVGPoint>> getPointAtLength(float distance) const final;
    unsigned getPathSegAtLength(float distance) const;

    FloatRect getBBox(StyleUpdateStrategy = AllowStyleUpdate) final;

    Ref<SVGPathSegList>& pathSegList() { return m_pathSegList->baseVal(); }
    RefPtr<SVGPathSegList>& animatedPathSegList() { return m_pathSegList->animVal(); }

    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=15412 - Implement normalized path segment lists!
    RefPtr<SVGPathSegList> normalizedPathSegList() { return nullptr; }
    RefPtr<SVGPathSegList> animatedNormalizedPathSegList() { return nullptr; }

    const SVGPathByteStream& pathByteStream() const { return m_pathSegList->currentPathByteStream(); }
    Path path() const { return m_pathSegList->currentPath(); }
    size_t approximateMemoryCost() const final { return m_pathSegList->approximateMemoryCost(); }

private:
    SVGPathElement(const QualifiedName&, Document&);

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGPathElement, SVGGeometryElement>;
    const SVGPropertyRegistry& propertyRegistry() const final { return m_propertyRegistry; }
    
    void parseAttribute(const QualifiedName&, const AtomString&) final;
    void svgAttributeChanged(const QualifiedName&) final;

    bool isValid() const final { return SVGTests::isValid(); }
    bool supportsMarkers() const final { return true; }

    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) final;

    Node::InsertedIntoAncestorResult insertedIntoAncestor(InsertionType, ContainerNode&) final;
    void removedFromAncestor(RemovalType, ContainerNode&) final;

    void invalidateMPathDependencies();

private:
    PropertyRegistry m_propertyRegistry { *this };
    Ref<SVGAnimatedPathSegList> m_pathSegList { SVGAnimatedPathSegList::create(this) };
};

} // namespace WebCore
