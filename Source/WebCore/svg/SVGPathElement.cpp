/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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

#include "config.h"
#include "SVGPathElement.h"

#include "RenderSVGPath.h"
#include "RenderSVGResource.h"
#include "SVGDocumentExtensions.h"
#include "SVGMPathElement.h"
#include "SVGNames.h"
#include "SVGPathSegArcAbs.h"
#include "SVGPathSegArcRel.h"
#include "SVGPathSegClosePath.h"
#include "SVGPathSegCurvetoCubicAbs.h"
#include "SVGPathSegCurvetoCubicRel.h"
#include "SVGPathSegCurvetoCubicSmoothAbs.h"
#include "SVGPathSegCurvetoCubicSmoothRel.h"
#include "SVGPathSegCurvetoQuadraticAbs.h"
#include "SVGPathSegCurvetoQuadraticRel.h"
#include "SVGPathSegCurvetoQuadraticSmoothAbs.h"
#include "SVGPathSegCurvetoQuadraticSmoothRel.h"
#include "SVGPathSegLinetoAbs.h"
#include "SVGPathSegLinetoHorizontalAbs.h"
#include "SVGPathSegLinetoHorizontalRel.h"
#include "SVGPathSegLinetoRel.h"
#include "SVGPathSegLinetoVerticalAbs.h"
#include "SVGPathSegLinetoVerticalRel.h"
#include "SVGPathSegList.h"
#include "SVGPathSegMovetoAbs.h"
#include "SVGPathSegMovetoRel.h"
#include "SVGPathUtilities.h"
#include "SVGPoint.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGPathElement);

inline SVGPathElement::SVGPathElement(const QualifiedName& tagName, Document& document)
    : SVGGeometryElement(tagName, document)
    , SVGExternalResourcesRequired(this)
{
    ASSERT(hasTagName(SVGNames::pathTag));
    registerAttributes();
}

Ref<SVGPathElement> SVGPathElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGPathElement(tagName, document));
}

float SVGPathElement::getTotalLength() const
{
    float totalLength = 0;
    getTotalLengthOfSVGPathByteStream(pathByteStream(), totalLength);
    return totalLength;
}

Ref<SVGPoint> SVGPathElement::getPointAtLength(float length) const
{
    FloatPoint point;
    getPointAtLengthOfSVGPathByteStream(pathByteStream(), length, point);
    return SVGPoint::create(point);
}

unsigned SVGPathElement::getPathSegAtLength(float length) const
{
    unsigned pathSeg = 0;
    getSVGPathSegAtLengthFromSVGPathByteStream(pathByteStream(), length, pathSeg);
    return pathSeg;
}

Ref<SVGPathSegClosePath> SVGPathElement::createSVGPathSegClosePath(SVGPathSegRole role)
{
    return SVGPathSegClosePath::create(*this, role);
}

Ref<SVGPathSegMovetoAbs> SVGPathElement::createSVGPathSegMovetoAbs(float x, float y, SVGPathSegRole role)
{
    return SVGPathSegMovetoAbs::create(*this, role, x, y);
}

Ref<SVGPathSegMovetoRel> SVGPathElement::createSVGPathSegMovetoRel(float x, float y, SVGPathSegRole role)
{
    return SVGPathSegMovetoRel::create(*this, role, x, y);
}

Ref<SVGPathSegLinetoAbs> SVGPathElement::createSVGPathSegLinetoAbs(float x, float y, SVGPathSegRole role)
{
    return SVGPathSegLinetoAbs::create(*this, role, x, y);
}

Ref<SVGPathSegLinetoRel> SVGPathElement::createSVGPathSegLinetoRel(float x, float y, SVGPathSegRole role)
{
    return SVGPathSegLinetoRel::create(*this, role, x, y);
}

Ref<SVGPathSegCurvetoCubicAbs> SVGPathElement::createSVGPathSegCurvetoCubicAbs(float x, float y, float x1, float y1, float x2, float y2, SVGPathSegRole role)
{
    return SVGPathSegCurvetoCubicAbs::create(*this, role, x, y, x1, y1, x2, y2);
}

Ref<SVGPathSegCurvetoCubicRel> SVGPathElement::createSVGPathSegCurvetoCubicRel(float x, float y, float x1, float y1, float x2, float y2, SVGPathSegRole role)
{
    return SVGPathSegCurvetoCubicRel::create(*this, role, x, y, x1, y1, x2, y2);
}

Ref<SVGPathSegCurvetoQuadraticAbs> SVGPathElement::createSVGPathSegCurvetoQuadraticAbs(float x, float y, float x1, float y1, SVGPathSegRole role)
{
    return SVGPathSegCurvetoQuadraticAbs::create(*this, role, x, y, x1, y1);
}

Ref<SVGPathSegCurvetoQuadraticRel> SVGPathElement::createSVGPathSegCurvetoQuadraticRel(float x, float y, float x1, float y1, SVGPathSegRole role)
{
    return SVGPathSegCurvetoQuadraticRel::create(*this, role, x, y, x1, y1);
}

Ref<SVGPathSegArcAbs> SVGPathElement::createSVGPathSegArcAbs(float x, float y, float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag, SVGPathSegRole role)
{
    return SVGPathSegArcAbs::create(*this, role, x, y, r1, r2, angle, largeArcFlag, sweepFlag);
}

Ref<SVGPathSegArcRel> SVGPathElement::createSVGPathSegArcRel(float x, float y, float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag, SVGPathSegRole role)
{
    return SVGPathSegArcRel::create(*this, role, x, y, r1, r2, angle, largeArcFlag, sweepFlag);
}

Ref<SVGPathSegLinetoHorizontalAbs> SVGPathElement::createSVGPathSegLinetoHorizontalAbs(float x, SVGPathSegRole role)
{
    return SVGPathSegLinetoHorizontalAbs::create(*this, role, x);
}

Ref<SVGPathSegLinetoHorizontalRel> SVGPathElement::createSVGPathSegLinetoHorizontalRel(float x, SVGPathSegRole role)
{
    return SVGPathSegLinetoHorizontalRel::create(*this, role, x);
}

Ref<SVGPathSegLinetoVerticalAbs> SVGPathElement::createSVGPathSegLinetoVerticalAbs(float y, SVGPathSegRole role)
{
    return SVGPathSegLinetoVerticalAbs::create(*this, role, y);
}

Ref<SVGPathSegLinetoVerticalRel> SVGPathElement::createSVGPathSegLinetoVerticalRel(float y, SVGPathSegRole role)
{
    return SVGPathSegLinetoVerticalRel::create(*this, role, y);
}

Ref<SVGPathSegCurvetoCubicSmoothAbs> SVGPathElement::createSVGPathSegCurvetoCubicSmoothAbs(float x, float y, float x2, float y2, SVGPathSegRole role)
{
    return SVGPathSegCurvetoCubicSmoothAbs::create(*this, role, x, y, x2, y2);
}

Ref<SVGPathSegCurvetoCubicSmoothRel> SVGPathElement::createSVGPathSegCurvetoCubicSmoothRel(float x, float y, float x2, float y2, SVGPathSegRole role)
{
    return SVGPathSegCurvetoCubicSmoothRel::create(*this, role, x, y, x2, y2);
}

Ref<SVGPathSegCurvetoQuadraticSmoothAbs> SVGPathElement::createSVGPathSegCurvetoQuadraticSmoothAbs(float x, float y, SVGPathSegRole role)
{
    return SVGPathSegCurvetoQuadraticSmoothAbs::create(*this, role, x, y);
}

Ref<SVGPathSegCurvetoQuadraticSmoothRel> SVGPathElement::createSVGPathSegCurvetoQuadraticSmoothRel(float x, float y, SVGPathSegRole role)
{
    return SVGPathSegCurvetoQuadraticSmoothRel::create(*this, role, x, y);
}

void SVGPathElement::registerAttributes()
{
    auto& registry = attributeRegistry();
    if (!registry.isEmpty())
        return;
    registry.registerAttribute(SVGAnimatedCustomPathSegListAttributeAccessor::singleton<SVGNames::dAttr, &SVGPathElement::m_pathSegList>());
}

void SVGPathElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == SVGNames::dAttr) {
        if (!buildSVGPathByteStreamFromString(value, m_pathByteStream, UnalteredParsing))
            document().accessSVGExtensions().reportError("Problem parsing d=\"" + value + "\"");
        m_cachedPath = WTF::nullopt;
        return;
    }

    SVGGeometryElement::parseAttribute(name, value);
    SVGExternalResourcesRequired::parseAttribute(name, value);
}

void SVGPathElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (isKnownAttribute(attrName)) {
        InstanceInvalidationGuard guard(*this);

        RenderSVGPath* renderer = downcast<RenderSVGPath>(this->renderer());
        if (attrName == SVGNames::dAttr) {
            if (m_pathSegList.shouldSynchronize() && !lookupAnimatedProperty(m_pathSegList)->isAnimating()) {
                SVGPathSegListValues newList(PathSegUnalteredRole);
                buildSVGPathSegListValuesFromByteStream(m_pathByteStream, *this, newList, UnalteredParsing);
                m_pathSegList.setValue(WTFMove(newList));
            }

            if (renderer)
                renderer->setNeedsShapeUpdate();
            invalidateMPathDependencies();
        }

        if (renderer)
            RenderSVGResource::markForLayoutAndParentResourceInvalidation(*renderer);
        return;
    }

    SVGGeometryElement::svgAttributeChanged(attrName);
    SVGExternalResourcesRequired::svgAttributeChanged(attrName);
}

void SVGPathElement::invalidateMPathDependencies()
{
    // <mpath> can only reference <path> but this dependency is not handled in
    // markForLayoutAndParentResourceInvalidation so we update any mpath dependencies manually.
    if (HashSet<SVGElement*>* dependencies = document().accessSVGExtensions().setOfElementsReferencingTarget(this)) {
        for (auto* element : *dependencies) {
            if (is<SVGMPathElement>(*element))
                downcast<SVGMPathElement>(*element).targetPathChanged();
        }
    }
}

Node::InsertedIntoAncestorResult SVGPathElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    SVGGeometryElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    invalidateMPathDependencies();
    return InsertedIntoAncestorResult::Done;
}

void SVGPathElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    SVGGeometryElement::removedFromAncestor(removalType, oldParentOfRemovedTree);
    invalidateMPathDependencies();
}

const SVGPathByteStream& SVGPathElement::pathByteStream() const
{
    auto property = lookupAnimatedProperty(m_pathSegList);
    if (!property || !property->isAnimating())
        return m_pathByteStream;
    
    if (auto* animatedPathByteStream = static_cast<SVGAnimatedPathSegListPropertyTearOff*>(property.get())->animatedPathByteStream())
        return *animatedPathByteStream;

    return m_pathByteStream;
}
    
Path SVGPathElement::pathForByteStream() const
{
    const auto& pathByteStreamToUse = pathByteStream();

    if (&pathByteStreamToUse == &m_pathByteStream) {
        if (!m_cachedPath)
            m_cachedPath = buildPathFromByteStream(m_pathByteStream);
        return *m_cachedPath;
    }
    
    return buildPathFromByteStream(pathByteStreamToUse);
}

RefPtr<SVGAnimatedProperty> SVGPathElement::lookupOrCreateDWrapper()
{
    return m_pathSegList.animatedProperty(attributeOwnerProxy());
}

void SVGPathElement::animatedPropertyWillBeDeleted()
{
    // m_pathSegList.shouldSynchronize is set to true when the 'd' wrapper for m_pathSegList
    // is created and cached. We need to reset it back to false when this wrapper is deleted
    // so we can be sure if shouldSynchronize is true, SVGAttributeAccessor::lookupAnimatedProperty()
    // will return a valid cached 'd' wrapper for the m_pathSegList.
    m_pathSegList.setShouldSynchronize(false);
}

Ref<SVGPathSegList> SVGPathElement::pathSegList()
{
    return static_pointer_cast<SVGAnimatedPathSegListPropertyTearOff>(lookupOrCreateDWrapper())->baseVal();
}

RefPtr<SVGPathSegList> SVGPathElement::normalizedPathSegList()
{
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=15412 - Implement normalized path segment lists!
    return nullptr;
}

Ref<SVGPathSegList> SVGPathElement::animatedPathSegList()
{
    m_isAnimValObserved = true;
    return static_pointer_cast<SVGAnimatedPathSegListPropertyTearOff>(lookupOrCreateDWrapper())->animVal();
}

RefPtr<SVGPathSegList> SVGPathElement::animatedNormalizedPathSegList()
{
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=15412 - Implement normalized path segment lists!
    return nullptr;
}

size_t SVGPathElement::approximateMemoryCost() const
{
    // This is an approximation for path memory cost since the path is parsed on demand.
    size_t pathMemoryCost = (m_pathByteStream.size() / 10) * sizeof(FloatPoint);
    // We need to account for the memory which is allocated by the RenderSVGPath::m_path.
    return sizeof(*this) + (renderer() ? pathMemoryCost * 2 + sizeof(RenderSVGPath) : pathMemoryCost);
}

void SVGPathElement::pathSegListChanged(SVGPathSegRole role, ListModification listModification)
{
    switch (role) {
    case PathSegNormalizedRole:
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=15412 - Implement normalized path segment lists!
        break;
    case PathSegUnalteredRole: {
        auto& pathSegList = m_pathSegList.value(false);
        if (listModification == ListModificationAppend) {
            ASSERT(!pathSegList.isEmpty());
            appendSVGPathByteStreamFromSVGPathSeg(pathSegList.last().copyRef(), m_pathByteStream, UnalteredParsing);
        } else
            buildSVGPathByteStreamFromSVGPathSegListValues(pathSegList, m_pathByteStream, UnalteredParsing);
        m_cachedPath = WTF::nullopt;
        break;
    }
    case PathSegUndefinedRole:
        return;
    }

    invalidateSVGAttributes();
    
    RenderSVGPath* renderer = downcast<RenderSVGPath>(this->renderer());
    if (!renderer)
        return;

    renderer->setNeedsShapeUpdate();
    RenderSVGResource::markForLayoutAndParentResourceInvalidation(*renderer);
}

FloatRect SVGPathElement::getBBox(StyleUpdateStrategy styleUpdateStrategy)
{
    if (styleUpdateStrategy == AllowStyleUpdate)
        document().updateLayoutIgnorePendingStylesheets();

    RenderSVGPath* renderer = downcast<RenderSVGPath>(this->renderer());

    // FIXME: Eventually we should support getBBox for detached elements.
    // FIXME: If the path is null it means we're calling getBBox() before laying out this element,
    // which is an error.
    if (!renderer || !renderer->hasPath())
        return { };

    return renderer->path().boundingRect();
}

RenderPtr<RenderElement> SVGPathElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderSVGPath>(*this, WTFMove(style));
}

}
