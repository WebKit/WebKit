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

#ifndef SVGPathElement_h
#define SVGPathElement_h

#include "SVGAnimatedBoolean.h"
#include "SVGAnimatedNumber.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGGraphicsElement.h"
#include "SVGNames.h"
#include "SVGPathByteStream.h"
#include "SVGPathSegList.h"

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
class SVGPathSegListPropertyTearOff;

class SVGPathElement final : public SVGGraphicsElement,
                             public SVGExternalResourcesRequired {
public:
    static Ref<SVGPathElement> create(const QualifiedName&, Document&);
    
    float getTotalLength() const;
    SVGPoint getPointAtLength(float distance) const;
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
    RefPtr<SVGPathSegListPropertyTearOff> pathSegList();
    RefPtr<SVGPathSegListPropertyTearOff> animatedPathSegList();
    RefPtr<SVGPathSegListPropertyTearOff> normalizedPathSegList();
    RefPtr<SVGPathSegListPropertyTearOff> animatedNormalizedPathSegList();

    const SVGPathByteStream& pathByteStream() const;

    void pathSegListChanged(SVGPathSegRole, ListModification = ListModificationUnknown);

    virtual FloatRect getBBox(StyleUpdateStrategy = AllowStyleUpdate) override;

    static const SVGPropertyInfo* dPropertyInfo();

    bool isAnimValObserved() const { return m_isAnimValObserved; }

    WeakPtr<SVGPathElement> createWeakPtr() const { return m_weakPtrFactory.createWeakPtr(); }

    void animatedPropertyWillBeDeleted();

private:
    SVGPathElement(const QualifiedName&, Document&);

    virtual bool isValid() const override { return SVGTests::isValid(); }

    static bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) override;
    virtual void svgAttributeChanged(const QualifiedName&) override;
    virtual bool supportsMarkers() const override { return true; }

    // Custom 'd' property
    static void synchronizeD(SVGElement* contextElement);
    static Ref<SVGAnimatedProperty> lookupOrCreateDWrapper(SVGElement* contextElement);

    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGPathElement)
        DECLARE_ANIMATED_NUMBER(PathLength, pathLength)
        DECLARE_ANIMATED_BOOLEAN_OVERRIDE(ExternalResourcesRequired, externalResourcesRequired)
    END_DECLARE_ANIMATED_PROPERTIES

    virtual RenderPtr<RenderElement> createElementRenderer(Ref<RenderStyle>&&, const RenderTreePosition&) override;

    virtual Node::InsertionNotificationRequest insertedInto(ContainerNode&) override;
    virtual void removedFrom(ContainerNode&) override;

    void invalidateMPathDependencies();

private:
    SVGPathByteStream m_pathByteStream;
    mutable SVGSynchronizableAnimatedProperty<SVGPathSegList> m_pathSegList;
    WeakPtrFactory<SVGPathElement> m_weakPtrFactory;
    bool m_isAnimValObserved;
};

} // namespace WebCore

#endif
