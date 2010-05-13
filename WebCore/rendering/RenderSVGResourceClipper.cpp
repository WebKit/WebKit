/*
 * Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 *               2004, 2005, 2006, 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
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
 *
 */

#include "config.h"
#include "RenderSVGResourceClipper.h"

#include "AffineTransform.h"
#include "FloatRect.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "IntRect.h"
#include "RenderObject.h"
#include "RenderStyle.h"
#include "RenderSVGResource.h"
#include "SVGClipPathElement.h"
#include "SVGElement.h"
#include "SVGRenderSupport.h"
#include "SVGStyledElement.h"
#include "SVGStyledTransformableElement.h"
#include "SVGUnitTypes.h"
#include "SVGUseElement.h"
#include <wtf/UnusedParam.h>

namespace WebCore {

RenderSVGResourceType RenderSVGResourceClipper::s_resourceType = ClipperResourceType;

RenderSVGResourceClipper::RenderSVGResourceClipper(SVGClipPathElement* node)
    : RenderSVGResourceContainer(node)
{
}

RenderSVGResourceClipper::~RenderSVGResourceClipper()
{
    deleteAllValues(m_clipper);
    m_clipper.clear();
}

void RenderSVGResourceClipper::invalidateClients()
{
    HashMap<RenderObject*, ClipperData*>::const_iterator end = m_clipper.end();
    for (HashMap<RenderObject*, ClipperData*>::const_iterator it = m_clipper.begin(); it != end; ++it) {
        RenderObject* renderer = it->first;
        renderer->setNeedsBoundariesUpdate();
        renderer->setNeedsLayout(true);
    }
    deleteAllValues(m_clipper);
    m_clipper.clear();
}

void RenderSVGResourceClipper::invalidateClient(RenderObject* object)
{
    ASSERT(object);

    // FIXME: The HashSet should always contain the object on calling invalidateClient. A race condition
    // during the parsing can causes a call of invalidateClient right before the call of applyResource.
    // We return earlier for the moment. This bug should be fixed in:
    // https://bugs.webkit.org/show_bug.cgi?id=35181
    if (!m_clipper.contains(object))
        return;

    delete m_clipper.take(object);
    markForLayoutAndResourceInvalidation(object);
}

bool RenderSVGResourceClipper::applyResource(RenderObject* object, RenderStyle*, GraphicsContext*& context, unsigned short resourceMode)
{
    ASSERT(object);
    ASSERT(context);
#ifndef NDEBUG
    ASSERT(resourceMode == ApplyToDefaultMode);
#else
    UNUSED_PARAM(resourceMode);
#endif
    applyClippingToContext(object, object->objectBoundingBox(), object->repaintRectInLocalCoordinates(), context);
    return true;
}

bool RenderSVGResourceClipper::pathOnlyClipping(GraphicsContext* context, const FloatRect& objectBoundingBox)
{
    // If the current clip-path gets clipped itself, we have to fallback to masking.
    if (!style()->svgStyle()->clipperResource().isEmpty())
        return false;
    WindRule clipRule = RULE_NONZERO;
    Path clipPath = Path();

    // If clip-path only contains one visible shape or path, we can use path-based clipping. Invisible
    // shapes don't affect the clipping and can be ignored. If clip-path contains more than one
    // visible shape, the additive clipping may not work, caused by the clipRule. EvenOdd
    // as well as NonZero can cause self-clipping of the elements.
    // See also http://www.w3.org/TR/SVG/painting.html#FillRuleProperty
    for (Node* childNode = node()->firstChild(); childNode; childNode = childNode->nextSibling()) {
        RenderObject* renderer = childNode->renderer();
        if (!renderer)
            continue;
        // Only shapes or paths are supported for direct clipping. We need to fallback to masking for texts.
        if (renderer->isSVGText())
            return false;
        if (!childNode->isSVGElement() || !static_cast<SVGElement*>(childNode)->isStyledTransformable())
            continue;
        SVGStyledTransformableElement* styled = static_cast<SVGStyledTransformableElement*>(childNode);
        RenderStyle* style = renderer->style();
        if (!style || style->display() == NONE || style->visibility() != VISIBLE)
             continue;
        const SVGRenderStyle* svgStyle = style->svgStyle();
        // Current shape in clip-path gets clipped too. Fallback to masking.
        if (!svgStyle->clipperResource().isEmpty())
            return false;
        // Fallback to masking, if there is more than one clipping path.
        if (clipPath.isEmpty()) {
            clipPath = styled->toClipPath();
            clipRule = svgStyle->clipRule();
        } else
            return false;
    }
    // Only one visible shape/path was found. Directly continue clipping and transform the content to userspace if necessary.
    if (static_cast<SVGClipPathElement*>(node())->clipPathUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        AffineTransform transform;
        transform.translate(objectBoundingBox.x(), objectBoundingBox.y());
        transform.scaleNonUniform(objectBoundingBox.width(), objectBoundingBox.height());
        clipPath.transform(transform);
    }
    // The SVG specification wants us to clip everything, if clip-path doesn't have a child.
    if (clipPath.isEmpty())
        clipPath.addRect(FloatRect());
    context->beginPath();
    context->addPath(clipPath);
    context->clipPath(clipRule);
    return true;
}

bool RenderSVGResourceClipper::applyClippingToContext(RenderObject* object, const FloatRect& objectBoundingBox,
                                                      const FloatRect& repaintRect, GraphicsContext* context)
{
    if (!m_clipper.contains(object))
        m_clipper.set(object, new ClipperData);

    ClipperData* clipperData = m_clipper.get(object);
    if (!clipperData->clipMaskImage) {
        if (pathOnlyClipping(context, objectBoundingBox))
            return true;
        createClipData(clipperData, objectBoundingBox, repaintRect);
    }

    if (!clipperData->clipMaskImage)
        return false;

    context->clipToImageBuffer(repaintRect, clipperData->clipMaskImage.get());
    return true;
}

bool RenderSVGResourceClipper::createClipData(ClipperData* clipperData, const FloatRect& objectBoundingBox, const FloatRect& repaintRect)
{
    IntRect clipMaskRect = enclosingIntRect(repaintRect);
    clipperData->clipMaskImage = ImageBuffer::create(clipMaskRect.size());
    if (!clipperData->clipMaskImage)
        return false;

    GraphicsContext* maskContext = clipperData->clipMaskImage->context();
    ASSERT(maskContext);

    maskContext->save();
    maskContext->translate(-repaintRect.x(), -repaintRect.y());

    // clipPath can also be clipped by another clipPath.
    bool clipperGetsClipped = false;
    if (RenderSVGResourceClipper* clipper = getRenderSVGResourceById<RenderSVGResourceClipper>(this->document(), style()->svgStyle()->clipperResource())) {
        clipperGetsClipped = true;
        if (!clipper->applyClippingToContext(this, objectBoundingBox, repaintRect, maskContext)) {
            maskContext->restore();
            return false;
        }            
    }

    SVGClipPathElement* clipPath = static_cast<SVGClipPathElement*>(node());
    if (clipPath->clipPathUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        maskContext->translate(objectBoundingBox.x(), objectBoundingBox.y());
        maskContext->scale(objectBoundingBox.size());
    }

    // Draw all clipPath children into a global mask.
    for (Node* childNode = node()->firstChild(); childNode; childNode = childNode->nextSibling()) {
        RenderObject* renderer = childNode->renderer();
        if (!childNode->isSVGElement() || !static_cast<SVGElement*>(childNode)->isStyled() || !renderer)
            continue;
        RenderStyle* style = renderer->style();
        if (!style || style->display() == NONE || style->visibility() != VISIBLE)
            continue;

        WindRule newClipRule = style->svgStyle()->clipRule();
        bool isUseElement = renderer->isSVGShadowTreeRootContainer();
        if (isUseElement) {
            SVGUseElement* useElement = static_cast<SVGUseElement*>(childNode);
            renderer = useElement->rendererClipChild();
            if (!renderer)
                continue;
            if (!useElement->hasAttribute(SVGNames::clip_ruleAttr))
                newClipRule = renderer->style()->svgStyle()->clipRule();
        }

        // Only shapes, paths and texts are allowed for clipping.
        if (!renderer->isRenderPath() && !renderer->isSVGText())
            continue;

        // Save the old RenderStyle of the current object for restoring after drawing
        // it to the MaskImage. The new intermediate RenderStyle needs to inherit from
        // the old one.
        RefPtr<RenderStyle> oldRenderStyle = renderer->style();
        RefPtr<RenderStyle> newRenderStyle = RenderStyle::clone(oldRenderStyle.get());
        SVGRenderStyle* svgStyle = newRenderStyle.get()->accessSVGStyle();
        svgStyle->setFillPaint(SVGPaint::defaultFill());
        svgStyle->setStrokePaint(SVGPaint::defaultStroke());
        svgStyle->setFillRule(newClipRule);
        newRenderStyle.get()->setOpacity(1.0f);
        svgStyle->setFillOpacity(1.0f);
        svgStyle->setStrokeOpacity(1.0f);
        svgStyle->setFilterResource(String());
        svgStyle->setMaskerResource(String());
        renderer->setStyle(newRenderStyle.release());

        // In the case of a <use> element, we obtained its renderere above, to retrieve its clipRule.
        // We hsve to pass the <use> renderer itself to renderSubtreeToImage() to apply it's x/y/transform/etc. values when rendering.
        // So if isUseElement is true, refetch the childNode->renderer(), as renderer got overriden above.
        renderSubtreeToImage(clipperData->clipMaskImage.get(), isUseElement ? childNode->renderer() : renderer);

        renderer->setStyle(oldRenderStyle.release());
    }

    maskContext->restore();

    return true;
}

FloatRect RenderSVGResourceClipper::resourceBoundingBox(const FloatRect& objectBoundingBox) const
{
    // This is a rough heuristic to appraise the clip size and doesn't consider clip on clip.
    FloatRect clipRect;
    for (Node* childNode = node()->firstChild(); childNode; childNode = childNode->nextSibling()) {
        RenderObject* renderer = childNode->renderer();
        if (!childNode->isSVGElement() || !static_cast<SVGElement*>(childNode)->isStyled() || !renderer)
            continue;
        if (!renderer->isRenderPath() && !renderer->isSVGText() && !renderer->isSVGShadowTreeRootContainer())
            continue;
        clipRect.unite(renderer->localToParentTransform().mapRect(renderer->repaintRectInLocalCoordinates()));
    }

    if (static_cast<SVGClipPathElement*>(node())->clipPathUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        AffineTransform transform;
        transform.translate(objectBoundingBox.x(), objectBoundingBox.y());
        transform.scaleNonUniform(objectBoundingBox.width(), objectBoundingBox.height());
        return transform.mapRect(clipRect);
    }

    return clipRect;
}

}
