/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2009 Google, Inc. All rights reserved.
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "config.h"
#include "SVGGraphicsElement.h"

#include "ContainerNodeInlines.h"
#include "LegacyRenderSVGPath.h"
#include "LegacyRenderSVGResource.h"
#include "RenderAncestorIterator.h"
#include "RenderElement.h"
#include "RenderElementStyleInlines.h"
#include "RenderLayer.h"
#include "RenderLayerInlines.h"
#include "RenderLayerSVGAdditionsInlines.h"
#include "RenderObjectDocument.h"
#include "RenderSVGHiddenContainer.h"
#include "RenderSVGPath.h"
#include "RenderSVGResourceMasker.h"
#include "RenderSVGResourcePattern.h"
#include "SVGElementTypeHelpers.h"
#include "SVGImageElement.h"
#include "SVGLayerTransformComputation.h"
#include "SVGMatrix.h"
#include "SVGNames.h"
#include "SVGPathData.h"
#include "SVGRect.h"
#include "SVGRenderSupport.h"
#include "SVGSVGElement.h"
#include "SVGStringList.h"
#include "Settings.h"
#include "StyleTransformResolver.h"
#include "TransformOperationData.h"
#include "TransformState.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SVGGraphicsElement);

// FIXME: This doesn't match SVGElement::viewportElement() as it has an extra check for
// foreign object.
static bool NODELETE isViewportElement(const SVGElement* element)
{
    if (!element)
        return false;

    return element->hasTagName(SVGNames::svgTag)
        || element->hasTagName(SVGNames::symbolTag)
        || element->hasTagName(SVGNames::foreignObjectTag)
        || is<SVGImageElement>(*element);
}

SVGElement* SVGGraphicsElement::nearestViewportElement(const SVGElement* element)
{
    ASSERT(element);
    for (auto* current = element->parentOrShadowHostElement(); current; current = current->parentOrShadowHostElement()) {
        auto* svgElement = dynamicDowncast<SVGElement>(*current);
        if (isViewportElement(svgElement))
            return svgElement;
    }

    return nullptr;
}

FloatRect SVGGraphicsElement::computeBBox(SVGElement* element, StyleUpdateStrategy styleUpdateStrategy)
{
    ASSERT(element);
    if (styleUpdateStrategy == StyleUpdateStrategy::Allow)
        protect(element->document())->updateLayoutIgnorePendingStylesheets({ LayoutOptions::TreatContentVisibilityHiddenAsVisible, LayoutOptions::TreatContentVisibilityAutoAsVisible }, element);

    // FIXME: Eventually we should support getBBox for detached elements.
    CheckedPtr renderer = element->renderer();
    if (!renderer || renderer->isRenderSVGHiddenContainer() || renderer->objectBoundingBoxIsEmpty())
        return FloatRect();

    return renderer->objectBoundingBox();
}

AffineTransform SVGGraphicsElement::computeCTM(SVGElement* element, CTMScope mode, StyleUpdateStrategy styleUpdateStrategy)
{
    ASSERT(element);
    if (styleUpdateStrategy == StyleUpdateStrategy::Allow)
        protect(element->document())->updateLayoutIgnorePendingStylesheets({ LayoutOptions::TreatContentVisibilityHiddenAsVisible, LayoutOptions::TreatContentVisibilityAutoAsVisible }, element);

    RefPtr stopAtElement = mode == CTMScope::NearestViewportScope ? nearestViewportElement(element) : nullptr;

    if (element->document().settings().layerBasedSVGEngineEnabled()) {
        // Rudimentary support for operations on "detached" elements.
        CheckedPtr renderer = dynamicDowncast<RenderLayerModelObject>(element->renderer());
        if (!renderer)
            return element->localCoordinateSpaceTransform(mode);

        auto trackingMode { mode == CTMScope::ScreenScope ? TransformState::TrackSVGScreenCTMMatrix : TransformState::TrackSVGCTMMatrix };
        CheckedPtr stopAtRenderer = dynamicDowncast<RenderLayerModelObject>(stopAtElement ? stopAtElement->renderer() : nullptr);
        return SVGLayerTransformComputation(*renderer).computeAccumulatedTransform(stopAtRenderer.get(), trackingMode);
    }

    AffineTransform ctm;

    for (RefPtr<Element> currentElement = element; currentElement; currentElement = currentElement->parentOrShadowHostElement()) {
        RefPtr svgElement = dynamicDowncast<SVGElement>(*currentElement);
        if (!svgElement)
            break;

        ctm = svgElement->localCoordinateSpaceTransform(mode).multiply(ctm);

        // For getCTM() computation, stop at the nearest viewport element
        if (currentElement == stopAtElement)
            break;
    }

    return ctm;
}

SVGGraphicsElement::SVGGraphicsElement(const QualifiedName& tagName, Document& document, UniqueRef<SVGPropertyRegistry>&& propertyRegistry, OptionSet<TypeFlag> typeFlags)
    : SVGElement(tagName, document, WTF::move(propertyRegistry), typeFlags)
    , SVGTests(this)
    , m_shouldIsolateBlending(false)
    , m_transform(SVGAnimatedTransformList::create(this))
{
    static bool didRegistration = false;
    if (!didRegistration) [[unlikely]] {
        didRegistration = true;
        PropertyRegistry::registerProperty<SVGNames::transformAttr, &SVGGraphicsElement::m_transform>();
    }
}

SVGGraphicsElement::~SVGGraphicsElement() = default;

Ref<SVGMatrix> SVGGraphicsElement::getCTMForBindings()
{
    return SVGMatrix::create(getCTM());
}

AffineTransform SVGGraphicsElement::getCTM(StyleUpdateStrategy styleUpdateStrategy)
{
    return computeCTM(this, CTMScope::NearestViewportScope, styleUpdateStrategy);
}

Ref<SVGMatrix> SVGGraphicsElement::getScreenCTMForBindings()
{
    return SVGMatrix::create(getScreenCTM());
}

AffineTransform SVGGraphicsElement::getScreenCTM(StyleUpdateStrategy styleUpdateStrategy)
{
    return computeCTM(this, CTMScope::ScreenScope, styleUpdateStrategy);
}

AffineTransform SVGGraphicsElement::animatedLocalTransform() const
{
    // LBSE handles transforms via RenderLayer, no need to handle CSS transforms here.
    if (document().settings().layerBasedSVGEngineEnabled()) {
        auto concatenatedTransform = protect(transform())->concatenate();
        if (concatenatedTransform && m_supplementalTransform)
            return *m_supplementalTransform * *concatenatedTransform;
        return m_supplementalTransform ? *m_supplementalTransform : concatenatedTransform.value_or(identity);
    }

    AffineTransform matrix;

    CheckedPtr renderer = this->renderer();
    CheckedPtr style = renderer ? &renderer->style() : nullptr;
    bool hasSpecifiedTransform = style && (!style->transform().isNone() || !style->offsetPath().isNone());

    // Honor any of the transform-related CSS properties if set.
    if (hasSpecifiedTransform || (style && (!style->translate().isNone() || !style->scale().isNone() || !style->rotate().isNone()))) {
        // Note: objectBoundingBox is an emptyRect for elements like pattern or clipPath.
        // See the "Object bounding box units" section of http://dev.w3.org/csswg/css3-transforms/
        auto transform = Style::TransformResolver::computeTransform(*style, TransformOperationData(renderer->transformReferenceBoxRect(), renderer.get()));

        // Flatten any 3D transform.
        matrix = transform.toAffineTransform();
    }

    // If we didn't have the CSS "transform" property set, we must account for the "transform" attribute.
    if (!hasSpecifiedTransform && style && !transform().isEmpty()) {
        auto t = Style::TransformResolver::computeTransformOrigin(*style, renderer->transformReferenceBoxRect()).xy();
        matrix.translate(t);
        matrix *= *transform().concatenate();
        matrix.translate(-t.x(), -t.y());
    }

    if (m_supplementalTransform)
        return *m_supplementalTransform * matrix;
    return matrix;
}

AffineTransform* SVGGraphicsElement::ensureSupplementalTransform()
{
    if (!m_supplementalTransform)
        m_supplementalTransform = makeUnique<AffineTransform>();
    return m_supplementalTransform.get();
}

void SVGGraphicsElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    if (name == SVGNames::transformAttr)
        m_transform->baseVal()->parse(newValue);

    SVGTests::parseAttribute(name, newValue);
    SVGElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

void SVGGraphicsElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (PropertyRegistry::isKnownAttribute(attrName)) {
        ASSERT(attrName == SVGNames::transformAttr);
        InstanceInvalidationGuard guard(*this);

        if (document().settings().layerBasedSVGEngineEnabled()) {
            if (CheckedPtr layerRenderer = dynamicDowncast<RenderLayerModelObject>(renderer()))
                layerRenderer->repaintOrRelayoutAfterSVGTransformChange();
            return;
        }

        if (CheckedPtr renderer = this->renderer()) {
            renderer->setNeedsTransformUpdate();
            updateSVGRendererForElementChange();
        }
        return;
    }

    SVGElement::svgAttributeChanged(attrName);
    SVGTests::svgAttributeChanged(attrName);
}

Ref<SVGRect> SVGGraphicsElement::getBBoxForBindings()
{
    return SVGRect::create(getBBox());
}

FloatRect SVGGraphicsElement::getBBox(StyleUpdateStrategy styleUpdateStrategy)
{
    return computeBBox(this, styleUpdateStrategy);
}

RenderPtr<RenderElement> SVGGraphicsElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    if (document().settings().layerBasedSVGEngineEnabled())
        return createRenderer<RenderSVGPath>(*this, WTF::move(style));
    return createRenderer<LegacyRenderSVGPath>(*this, WTF::move(style));
}

void SVGGraphicsElement::didAttachRenderers()
{
    if (document().settings().layerBasedSVGEngineEnabled()) {
        if (auto* svgRenderer = dynamicDowncast<RenderLayerModelObject>(renderer()); svgRenderer && lineageOfType<RenderSVGHiddenContainer>(*svgRenderer).first()) {
            if (auto* layer = svgRenderer->layer())
                layer->dirtyVisibleContentStatus();
        }
    }
}

Path SVGGraphicsElement::toClipPath()
{
    RELEASE_ASSERT(!document().settings().layerBasedSVGEngineEnabled());

    Path path = pathFromGraphicsElement(*this);
    // FIXME: How do we know the element has done a layout?
    path.transform(animatedLocalTransform());
    return path;
}

void SVGGraphicsElement::invalidateResourceImageBuffersIfNeeded()
{
    if (!document().settings().layerBasedSVGEngineEnabled())
        return;
    if (CheckedPtr svgRenderer = dynamicDowncast<RenderLayerModelObject>(renderer())) {
        CheckedPtr layer = svgRenderer->enclosingLayer();
        if (!layer)
            return;
        if (CheckedPtr container = layer->enclosingHiddenOrResourceContainerForSVG()) {
            if (auto* maskRenderer = dynamicDowncast<RenderSVGResourceMasker>(container.get()))
                maskRenderer->invalidateMask();
            if (auto* patternRenderer = dynamicDowncast<RenderSVGResourcePattern>(container.get()))
                patternRenderer->invalidatePattern(RenderSVGResourcePattern::SuppressRepaint::Yes);
        }
    }
}

}
