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
#include <wtf/NeverDestroyed.h>

namespace WebCore {

// Define custom animated property 'd'.
const SVGPropertyInfo* SVGPathElement::dPropertyInfo()
{
    static const SVGPropertyInfo* s_propertyInfo = nullptr;
    if (!s_propertyInfo) {
        s_propertyInfo = new SVGPropertyInfo(AnimatedPath,
            PropertyIsReadWrite,
            SVGNames::dAttr,
            SVGNames::dAttr->localName(),
            &SVGPathElement::synchronizeD,
            &SVGPathElement::lookupOrCreateDWrapper);
    }
    return s_propertyInfo;
}

// Animated property definitions
DEFINE_ANIMATED_NUMBER(SVGPathElement, SVGNames::pathLengthAttr, PathLength, pathLength)
DEFINE_ANIMATED_BOOLEAN(SVGPathElement, SVGNames::externalResourcesRequiredAttr, ExternalResourcesRequired, externalResourcesRequired)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGPathElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(d)
    REGISTER_LOCAL_ANIMATED_PROPERTY(pathLength)
    REGISTER_LOCAL_ANIMATED_PROPERTY(externalResourcesRequired)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGGraphicsElement)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGPathElement::SVGPathElement(const QualifiedName& tagName, Document& document)
    : SVGGraphicsElement(tagName, document)
    , m_pathSegList(PathSegUnalteredRole)
    , m_isAnimValObserved(false)
{
    ASSERT(hasTagName(SVGNames::pathTag));
    registerAnimatedPropertiesForSVGPathElement();
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

bool SVGPathElement::isSupportedAttribute(const QualifiedName& attrName)
{
    static const auto supportedAttributes = makeNeverDestroyed([] {
        HashSet<QualifiedName> set;
        SVGLangSpace::addSupportedAttributes(set);
        SVGExternalResourcesRequired::addSupportedAttributes(set);
        set.add({ SVGNames::dAttr.get(), SVGNames::pathLengthAttr.get() });
        return set;
    }());
    return supportedAttributes.get().contains<SVGAttributeHashTranslator>(attrName);
}

void SVGPathElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == SVGNames::dAttr) {
        if (!buildSVGPathByteStreamFromString(value, m_pathByteStream, UnalteredParsing))
            document().accessSVGExtensions().reportError("Problem parsing d=\"" + value + "\"");
        m_cachedPath = std::nullopt;
        return;
    }

    if (name == SVGNames::pathLengthAttr) {
        setPathLengthBaseValue(value.toFloat());
        if (pathLengthBaseValue() < 0)
            document().accessSVGExtensions().reportError("A negative value for path attribute <pathLength> is not allowed");
        return;
    }

    SVGGraphicsElement::parseAttribute(name, value);
    SVGExternalResourcesRequired::parseAttribute(name, value);
}

void SVGPathElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGGraphicsElement::svgAttributeChanged(attrName);
        return;
    }

    InstanceInvalidationGuard guard(*this);

    RenderSVGPath* renderer = downcast<RenderSVGPath>(this->renderer());

    if (attrName == SVGNames::dAttr) {
        if (m_pathSegList.shouldSynchronize && !SVGAnimatedProperty::lookupWrapper<SVGPathElement, SVGAnimatedPathSegListPropertyTearOff>(this, dPropertyInfo())->isAnimating()) {
            SVGPathSegListValues newList(PathSegUnalteredRole);
            buildSVGPathSegListValuesFromByteStream(m_pathByteStream, *this, newList, UnalteredParsing);
            m_pathSegList.value = WTFMove(newList);
        }

        if (renderer)
            renderer->setNeedsShapeUpdate();

        invalidateMPathDependencies();
    }

    if (renderer)
        RenderSVGResource::markForLayoutAndParentResourceInvalidation(*renderer);
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
    SVGGraphicsElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    invalidateMPathDependencies();
    return InsertedIntoAncestorResult::Done;
}

void SVGPathElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    SVGGraphicsElement::removedFromAncestor(removalType, oldParentOfRemovedTree);
    invalidateMPathDependencies();
}

const SVGPathByteStream& SVGPathElement::pathByteStream() const
{
    auto property = SVGAnimatedProperty::lookupWrapper<SVGPathElement, SVGAnimatedPathSegListPropertyTearOff>(this, dPropertyInfo());
    if (!property || !property->isAnimating())
        return m_pathByteStream;
    
    SVGPathByteStream* animatedPathByteStream = static_cast<SVGAnimatedPathSegListPropertyTearOff*>(property.get())->animatedPathByteStream();
    if (!animatedPathByteStream)
        return m_pathByteStream;

    return *animatedPathByteStream;
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

Ref<SVGAnimatedProperty> SVGPathElement::lookupOrCreateDWrapper(SVGElement* contextElement)
{
    ASSERT(contextElement);
    SVGPathElement& ownerType = downcast<SVGPathElement>(*contextElement);

    if (auto property = SVGAnimatedProperty::lookupWrapper<SVGPathElement, SVGAnimatedPathSegListPropertyTearOff>(&ownerType, dPropertyInfo()))
        return *property;

    if (ownerType.m_pathSegList.value.isEmpty())
        buildSVGPathSegListValuesFromByteStream(ownerType.m_pathByteStream, ownerType, ownerType.m_pathSegList.value, UnalteredParsing);

    return SVGAnimatedProperty::lookupOrCreateWrapper<SVGPathElement, SVGAnimatedPathSegListPropertyTearOff, SVGPathSegListValues>(&ownerType, dPropertyInfo(), ownerType.m_pathSegList.value);
}

void SVGPathElement::synchronizeD(SVGElement* contextElement)
{
    ASSERT(contextElement);
    SVGPathElement& ownerType = downcast<SVGPathElement>(*contextElement);
    if (!ownerType.m_pathSegList.shouldSynchronize)
        return;
    ownerType.m_pathSegList.synchronize(&ownerType, dPropertyInfo()->attributeName, ownerType.m_pathSegList.value.valueAsString());
}

void SVGPathElement::animatedPropertyWillBeDeleted()
{
    // m_pathSegList.shouldSynchronize is set to true when the 'd' wrapper for m_pathSegList
    // is created and cached. We need to reset it back to false when this wrapper is deleted
    // so we can be sure if shouldSynchronize is true, SVGAnimatedProperty::lookupWrapper()
    // will return a valid cached 'd' wrapper for the m_pathSegList.
    m_pathSegList.shouldSynchronize = false;
}

Ref<SVGPathSegList> SVGPathElement::pathSegList()
{
    m_pathSegList.shouldSynchronize = true;
    return static_reference_cast<SVGAnimatedPathSegListPropertyTearOff>(lookupOrCreateDWrapper(this))->baseVal();
}

RefPtr<SVGPathSegList> SVGPathElement::normalizedPathSegList()
{
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=15412 - Implement normalized path segment lists!
    return nullptr;
}

Ref<SVGPathSegList> SVGPathElement::animatedPathSegList()
{
    m_pathSegList.shouldSynchronize = true;
    m_isAnimValObserved = true;
    return static_reference_cast<SVGAnimatedPathSegListPropertyTearOff>(lookupOrCreateDWrapper(this))->animVal();
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
    case PathSegUnalteredRole:
        if (listModification == ListModificationAppend) {
            ASSERT(!m_pathSegList.value.isEmpty());
            appendSVGPathByteStreamFromSVGPathSeg(m_pathSegList.value.last().copyRef(), m_pathByteStream, UnalteredParsing);
        } else
            buildSVGPathByteStreamFromSVGPathSegListValues(m_pathSegList.value, m_pathByteStream, UnalteredParsing);
        m_cachedPath = std::nullopt;
        break;
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
