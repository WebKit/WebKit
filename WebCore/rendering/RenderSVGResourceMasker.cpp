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
#include "SVGElement.h"
#include "SVGMaskElement.h"
#include "SVGStyledElement.h"
#include "SVGUnitTypes.h"
#include <wtf/Vector.h>

namespace WebCore {

RenderSVGResourceType RenderSVGResourceMasker::s_resourceType = MaskerResourceType;

RenderSVGResourceMasker::RenderSVGResourceMasker(SVGStyledElement* node)
    : RenderSVGResource(node)
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
    for (HashMap<RenderObject*, MaskerData*>::const_iterator it = m_masker.begin(); it != end; ++it)
        it->first->setNeedsLayout(true);
    deleteAllValues(m_masker);
    m_masker.clear();
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
}

bool RenderSVGResourceMasker::applyResource(RenderObject* object, GraphicsContext* context)
{
    ASSERT(object);
    ASSERT(context);

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

FloatRect RenderSVGResourceMasker::resourceBoundingBox(const FloatRect& objectBoundingBox) const
{
    if (SVGMaskElement* element = static_cast<SVGMaskElement*>(node()))
        return element->maskBoundingBox(objectBoundingBox);

    return FloatRect();
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

    // Calculate the smallest rect for the mask ImageBuffer.
    FloatRect repaintRect;
    Vector<RenderObject*> rendererList;
    for (Node* node = maskElement->firstChild(); node; node = node->nextSibling()) {
        RenderObject* renderer = node->renderer();
        if (!node->isSVGElement() || !static_cast<SVGElement*>(node)->isStyled() || !renderer)
            continue;

        rendererList.append(renderer);
        repaintRect.unite(renderer->localToParentTransform().mapRect(renderer->repaintRectInLocalCoordinates()));
    }

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
    if (!maskImageRect.width() || !maskImageRect.height()) {
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
    Vector<RenderObject*>::iterator end = rendererList.end();
    for (Vector<RenderObject*>::iterator it = rendererList.begin(); it != end; it++)
        renderSubtreeToImage(maskerData->maskImage.get(), *it);

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

}
