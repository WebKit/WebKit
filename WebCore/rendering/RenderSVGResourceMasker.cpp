/*
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
#include "RenderSVGResourceMasker.h"

#include "AffineTransform.h"
#include "CanvasPixelArray.h"
#include "Element.h"
#include "FloatPoint.h"
#include "FloatRect.h"
#include "GraphicsContext.h"
#include "Image.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "IntRect.h"
#include "RenderSVGResource.h"
#include "SVGElement.h"
#include "SVGMaskElement.h"
#include "SVGStyledElement.h"
#include "SVGUnitTypes.h"
#include <wtf/Vector.h>
#include <wtf/UnusedParam.h>

namespace WebCore {

RenderSVGResourceType RenderSVGResourceMasker::s_resourceType = MaskerResourceType;

RenderSVGResourceMasker::RenderSVGResourceMasker(SVGMaskElement* node)
    : RenderSVGResourceContainer(node)
{
}

RenderSVGResourceMasker::~RenderSVGResourceMasker()
{
    deleteAllValues(m_masker);
    m_masker.clear();
}

void RenderSVGResourceMasker::invalidateClients()
{
    HashMap<RenderObject*, MaskerData*>::const_iterator end = m_masker.end();
    for (HashMap<RenderObject*, MaskerData*>::const_iterator it = m_masker.begin(); it != end; ++it) {
        RenderObject* renderer = it->first;
        renderer->setNeedsBoundariesUpdate();
        renderer->setNeedsLayout(true);
    }

    deleteAllValues(m_masker);
    m_masker.clear();
    m_maskBoundaries = FloatRect();
}

void RenderSVGResourceMasker::invalidateClient(RenderObject* object)
{
    ASSERT(object);

    // FIXME: The HashMap should always contain the object on calling invalidateClient. A race condition
    // during the parsing can causes a call of invalidateClient right before the call of applyResource.
    // We return earlier for the moment. This bug should be fixed in:
    // https://bugs.webkit.org/show_bug.cgi?id=35181
    if (!m_masker.contains(object))
        return;

    delete m_masker.take(object);
    markForLayoutAndResourceInvalidation(object);
}

bool RenderSVGResourceMasker::applyResource(RenderObject* object, RenderStyle*, GraphicsContext*& context, unsigned short resourceMode)
{
    ASSERT(object);
    ASSERT(context);
#ifndef NDEBUG
    ASSERT(resourceMode == ApplyToDefaultMode);
#else
    UNUSED_PARAM(resourceMode);
#endif

    if (!m_masker.contains(object))
        m_masker.set(object, new MaskerData);

    MaskerData* maskerData = m_masker.get(object);
    if (!maskerData->maskImage && !maskerData->emptyMask) {
        SVGMaskElement* maskElement = static_cast<SVGMaskElement*>(node());
        if (!maskElement)
            return false;

        createMaskImage(maskerData, maskElement, object);
    }

    if (!maskerData->maskImage)
        return false;

    context->clipToImageBuffer(maskerData->maskRect, maskerData->maskImage.get());
    return true;
}

void RenderSVGResourceMasker::createMaskImage(MaskerData* maskerData, const SVGMaskElement* maskElement, RenderObject* object)
{
    FloatRect objectBoundingBox = object->objectBoundingBox();

    // Mask rect clipped with clippingBoundingBox and filterBoundingBox as long as they are present.
    maskerData->maskRect = object->repaintRectInLocalCoordinates();
    if (maskerData->maskRect.isEmpty()) {
        maskerData->emptyMask = true;
        return;
    }
    
    if (m_maskBoundaries.isEmpty())
        calculateMaskContentRepaintRect();

    FloatRect repaintRect = m_maskBoundaries;
    AffineTransform contextTransform;
    // We need to scale repaintRect for objectBoundingBox to get the drawing area.
    if (maskElement->maskContentUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        contextTransform.scaleNonUniform(objectBoundingBox.width(), objectBoundingBox.height());
        FloatPoint contextAdjustment = repaintRect.location();
        repaintRect = contextTransform.mapRect(repaintRect);
        repaintRect.move(objectBoundingBox.x(), objectBoundingBox.y());
        contextTransform.translate(-contextAdjustment.x(), -contextAdjustment.y());
    }
    repaintRect.intersect(maskerData->maskRect);
    maskerData->maskRect = repaintRect;
    IntRect maskImageRect = enclosingIntRect(maskerData->maskRect);

    maskImageRect.setLocation(IntPoint());

    // Don't create ImageBuffers with image size of 0
    if (maskImageRect.isEmpty()) {
        maskerData->emptyMask = true;
        return;
    }

    // FIXME: This changes color space to linearRGB, the default color space
    // for masking operations in SVG. We need a switch for the other color-space
    // attribute values sRGB, inherit and auto.
    maskerData->maskImage = ImageBuffer::create(maskImageRect.size(), LinearRGB);
    if (!maskerData->maskImage)
        return;

    GraphicsContext* maskImageContext = maskerData->maskImage->context();
    ASSERT(maskImageContext);

    maskImageContext->save();

    if (maskElement->maskContentUnits() == SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE)
        maskImageContext->translate(-maskerData->maskRect.x(), -maskerData->maskRect.y());
    maskImageContext->concatCTM(contextTransform);

    // draw the content into the ImageBuffer
    for (Node* node = maskElement->firstChild(); node; node = node->nextSibling()) {
        RenderObject* renderer = node->renderer();
        if (!node->isSVGElement() || !static_cast<SVGElement*>(node)->isStyled() || !renderer)
            continue;
        RenderStyle* style = renderer->style();
        if (!style || style->display() == NONE || style->visibility() != VISIBLE)
            continue;
        renderSubtreeToImage(maskerData->maskImage.get(), renderer);
    }

    maskImageContext->restore();

    // create the luminance mask
    RefPtr<ImageData> imageData(maskerData->maskImage->getUnmultipliedImageData(maskImageRect));
    CanvasPixelArray* srcPixelArray(imageData->data());

    for (unsigned pixelOffset = 0; pixelOffset < srcPixelArray->length(); pixelOffset += 4) {
        unsigned char a = srcPixelArray->get(pixelOffset + 3);
        if (!a)
            continue;
        unsigned char r = srcPixelArray->get(pixelOffset);
        unsigned char g = srcPixelArray->get(pixelOffset + 1);
        unsigned char b = srcPixelArray->get(pixelOffset + 2);

        double luma = (r * 0.2125 + g * 0.7154 + b * 0.0721) * ((double)a / 255.0);
        srcPixelArray->set(pixelOffset + 3, luma);
    }

    maskerData->maskImage->putUnmultipliedImageData(imageData.get(), maskImageRect, IntPoint());
}

void RenderSVGResourceMasker::calculateMaskContentRepaintRect()
{
    for (Node* childNode = node()->firstChild(); childNode; childNode = childNode->nextSibling()) {
        RenderObject* renderer = childNode->renderer();
        if (!childNode->isSVGElement() || !static_cast<SVGElement*>(childNode)->isStyled() || !renderer)
            continue;
        RenderStyle* style = renderer->style();
        if (!style || style->display() == NONE || style->visibility() != VISIBLE)
             continue;
        m_maskBoundaries.unite(renderer->localToParentTransform().mapRect(renderer->repaintRectInLocalCoordinates()));
    }
}

FloatRect RenderSVGResourceMasker::resourceBoundingBox(RenderObject* object)
{
    // Save the reference to the calling object for relayouting it on changing resource properties.
    if (!m_masker.contains(object))
        m_masker.set(object, new MaskerData);

    // Resource was not layouted yet. Give back clipping rect of the mask.
    SVGMaskElement* maskElement = static_cast<SVGMaskElement*>(node());
    FloatRect objectBoundingBox = object->objectBoundingBox();
    FloatRect maskBoundaries = maskElement->maskBoundingBox(objectBoundingBox);
    if (selfNeedsLayout())
        return maskBoundaries;

    if (m_maskBoundaries.isEmpty())
        calculateMaskContentRepaintRect();

    if (!maskElement)
        return FloatRect();

    FloatRect maskRect = m_maskBoundaries;
    if (maskElement->maskContentUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        AffineTransform transform;
        transform.translate(objectBoundingBox.x(), objectBoundingBox.y());
        transform.scaleNonUniform(objectBoundingBox.width(), objectBoundingBox.height());
        maskRect =  transform.mapRect(maskRect);
    }

    maskRect.intersect(maskBoundaries);
    return maskRect;
}

}
