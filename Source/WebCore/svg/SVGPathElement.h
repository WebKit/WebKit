/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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
#include "SVGAnimatedBoolean.h"
#include "SVGAnimatedNumber.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGGeometryElement.h"
#include "SVGNames.h"
#include "SVGPathByteStream.h"
#include "SVGPathSegListValues.h"

namespace WebCore {

class SVGPathSegArcAbs;
class SVGPathSegArcRel;
class SVGPathSegClosePath;
class SVGPathSegLinetoAbs;
class SVGPathSegLinetoRel;
class SVGPathSegMovetoAbs;
class SVGPathSegMovetoRel;
class SVGPathSegCurvetoCubicAbs;
class SVGPathSegCurvetoCubicRel;
class SVGPathSegLinetoVerticalAbs;
class SVGPathSegLinetoVerticalRel;
class SVGPathSegLinetoHorizontalAbs;
class SVGPathSegLinetoHorizontalRel;
class SVGPathSegCurvetoQuadraticAbs;
class SVGPathSegCurvetoQuadraticRel;
class SVGPathSegCurvetoCubicSmoothAbs;
class SVGPathSegCurvetoCubicSmoothRel;
class SVGPathSegCurvetoQuadraticSmoothAbs;
class SVGPathSegCurvetoQuadraticSmoothRel;
class SVGPathSegList;
class SVGPoint;

class SVGPathElement final : public SVGGeometryElement, public SVGExternalResourcesRequired {
    WTF_MAKE_ISO_ALLOCATED(SVGPathElement);
public:
    static Ref<SVGPathElement> create(const QualifiedName&, Document&);
    
    float getTotalLength() const final;
    Ref<SVGPoint> getPointAtLength(float distance) const final;
    unsigned getPathSegAtLength(float distance) const;

    Ref<SVGPathSegClosePath> createSVGPathSegClosePath(SVGPathSegRole = PathSegUndefinedRole);
    Ref<SVGPathSegMovetoAbs> createSVGPathSegMovetoAbs(float x, float y, SVGPathSegRole = PathSegUndefinedRole);
    Ref<SVGPathSegMovetoRel> createSVGPathSegMovetoRel(float x, float y, SVGPathSegRole = PathSegUndefinedRole);
    Ref<SVGPathSegLinetoAbs> createSVGPathSegLinetoAbs(float x, float y, SVGPathSegRole = PathSegUndefinedRole);
    Ref<SVGPathSegLinetoRel> createSVGPathSegLinetoRel(float x, float y, SVGPathSegRole = PathSegUndefinedRole);
    Ref<SVGPathSegCurvetoCubicAbs> createSVGPathSegCurvetoCubicAbs(float x, float y, float x1, float y1, float x2, float y2, SVGPathSegRole = PathSegUndefinedRole);
    Ref<SVGPathSegCurvetoCubicRel> createSVGPathSegCurvetoCubicRel(float x, float y, float x1, float y1, float x2, float y2, SVGPathSegRole = PathSegUndefinedRole);
    Ref<SVGPathSegCurvetoQuadraticAbs> createSVGPathSegCurvetoQuadraticAbs(float x, float y, float x1, float y1, SVGPathSegRole = PathSegUndefinedRole);
    Ref<SVGPathSegCurvetoQuadraticRel> createSVGPathSegCurvetoQuadraticRel(float x, float y, float x1, float y1, SVGPathSegRole = PathSegUndefinedRole);
    Ref<SVGPathSegArcAbs> createSVGPathSegArcAbs(float x, float y, float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag, SVGPathSegRole = PathSegUndefinedRole);
    Ref<SVGPathSegArcRel> createSVGPathSegArcRel(float x, float y, float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag, SVGPathSegRole = PathSegUndefinedRole);
    Ref<SVGPathSegLinetoHorizontalAbs> createSVGPathSegLinetoHorizontalAbs(float x, SVGPathSegRole = PathSegUndefinedRole);
    Ref<SVGPathSegLinetoHorizontalRel> createSVGPathSegLinetoHorizontalRel(float x, SVGPathSegRole = PathSegUndefinedRole);
    Ref<SVGPathSegLinetoVerticalAbs> createSVGPathSegLinetoVerticalAbs(float y, SVGPathSegRole = PathSegUndefinedRole);
    Ref<SVGPathSegLinetoVerticalRel> createSVGPathSegLinetoVerticalRel(float y, SVGPathSegRole = PathSegUndefinedRole);
    Ref<SVGPathSegCurvetoCubicSmoothAbs> createSVGPathSegCurvetoCubicSmoothAbs(float x, float y, float x2, float y2, SVGPathSegRole = PathSegUndefinedRole);
    Ref<SVGPathSegCurvetoCubicSmoothRel> createSVGPathSegCurvetoCubicSmoothRel(float x, float y, float x2, float y2, SVGPathSegRole = PathSegUndefinedRole);
    Ref<SVGPathSegCurvetoQuadraticSmoothAbs> createSVGPathSegCurvetoQuadraticSmoothAbs(float x, float y, SVGPathSegRole = PathSegUndefinedRole);
    Ref<SVGPathSegCurvetoQuadraticSmoothRel> createSVGPathSegCurvetoQuadraticSmoothRel(float x, float y, SVGPathSegRole = PathSegUndefinedRole);

    // Used in the bindings only.
    Ref<SVGPathSegList> pathSegList();
    Ref<SVGPathSegList> animatedPathSegList();
    RefPtr<SVGPathSegList> normalizedPathSegList();
    RefPtr<SVGPathSegList> animatedNormalizedPathSegList();

    const SVGPathByteStream& pathByteStream() const;
    Path pathForByteStream() const;

    void pathSegListChanged(SVGPathSegRole, ListModification = ListModificationUnknown);

    FloatRect getBBox(StyleUpdateStrategy = AllowStyleUpdate) final;

    static const SVGPropertyInfo* dPropertyInfo();

    bool isAnimValObserved() const { return m_isAnimValObserved; }

    void animatedPropertyWillBeDeleted();

    size_t approximateMemoryCost() const final;

private:
    SVGPathElement(const QualifiedName&, Document&);

    bool isValid() const final { return SVGTests::isValid(); }

    static bool isSupportedAttribute(const QualifiedName&);
    void parseAttribute(const QualifiedName&, const AtomicString&) final;
    void svgAttributeChanged(const QualifiedName&) final;
    bool supportsMarkers() const final { return true; }

    // Custom 'd' property
    static void synchronizeD(SVGElement* contextElement);
    static Ref<SVGAnimatedProperty> lookupOrCreateDWrapper(SVGElement* contextElement);

    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGPathElement)
        DECLARE_ANIMATED_NUMBER(PathLength, pathLength)
        DECLARE_ANIMATED_BOOLEAN_OVERRIDE(ExternalResourcesRequired, externalResourcesRequired)
    END_DECLARE_ANIMATED_PROPERTIES

    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) final;

    Node::InsertedIntoAncestorResult insertedIntoAncestor(InsertionType, ContainerNode&) final;
    void removedFromAncestor(RemovalType, ContainerNode&) final;

    void invalidateMPathDependencies();

private:
    SVGPathByteStream m_pathByteStream;
    mutable std::optional<Path> m_cachedPath;
    mutable SVGSynchronizableAnimatedProperty<SVGPathSegListValues> m_pathSegList;
    bool m_isAnimValObserved;
};

} // namespace WebCore
