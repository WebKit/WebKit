/*
    Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
                  2005 Alexander Kellett <lypanov@kde.org>
                  2009 Dirk Schulze <krit@webkit.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG)
#include "SVGMaskElement.h"

#include "CanvasPixelArray.h"
#include "CSSStyleSelector.h"
#include "GraphicsContext.h"
#include "Image.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "MappedAttribute.h"
#include "RenderObject.h"
#include "RenderSVGContainer.h"
#include "SVGLength.h"
#include "SVGNames.h"
#include "SVGRenderSupport.h"
#include "SVGUnitTypes.h"
#include <math.h>
#include <wtf/MathExtras.h>
#include <wtf/OwnPtr.h>
#include <wtf/Vector.h>

using namespace std;

namespace WebCore {

SVGMaskElement::SVGMaskElement(const QualifiedName& tagName, Document* doc)
    : SVGStyledLocatableElement(tagName, doc)
    , SVGURIReference()
    , SVGTests()
    , SVGLangSpace()
    , SVGExternalResourcesRequired()
    , m_maskUnits(SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX)
    , m_maskContentUnits(SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE)
    , m_x(LengthModeWidth, "-10%")
    , m_y(LengthModeHeight, "-10%")
    , m_width(LengthModeWidth, "120%")
    , m_height(LengthModeHeight, "120%")
{
    // Spec: If the x/y attribute is not specified, the effect is as if a value of "-10%" were specified.
    // Spec: If the width/height attribute is not specified, the effect is as if a value of "120%" were specified.
}

SVGMaskElement::~SVGMaskElement()
{
}

void SVGMaskElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == SVGNames::maskUnitsAttr) {
        if (attr->value() == "userSpaceOnUse")
            setMaskUnitsBaseValue(SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE);
        else if (attr->value() == "objectBoundingBox")
            setMaskUnitsBaseValue(SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
    } else if (attr->name() == SVGNames::maskContentUnitsAttr) {
        if (attr->value() == "userSpaceOnUse")
            setMaskContentUnitsBaseValue(SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE);
        else if (attr->value() == "objectBoundingBox")
            setMaskContentUnitsBaseValue(SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
    } else if (attr->name() == SVGNames::xAttr)
        setXBaseValue(SVGLength(LengthModeWidth, attr->value()));
    else if (attr->name() == SVGNames::yAttr)
        setYBaseValue(SVGLength(LengthModeHeight, attr->value()));
    else if (attr->name() == SVGNames::widthAttr)
        setWidthBaseValue(SVGLength(LengthModeWidth, attr->value()));
    else if (attr->name() == SVGNames::heightAttr)
        setHeightBaseValue(SVGLength(LengthModeHeight, attr->value()));
    else {
        if (SVGURIReference::parseMappedAttribute(attr))
            return;
        if (SVGTests::parseMappedAttribute(attr))
            return;
        if (SVGLangSpace::parseMappedAttribute(attr))
            return;
        if (SVGExternalResourcesRequired::parseMappedAttribute(attr))
            return;
        SVGStyledElement::parseMappedAttribute(attr);
    }
}

void SVGMaskElement::svgAttributeChanged(const QualifiedName& attrName)
{
    SVGStyledElement::svgAttributeChanged(attrName);

    if (attrName == SVGNames::maskUnitsAttr || attrName == SVGNames::maskContentUnitsAttr ||
        attrName == SVGNames::xAttr || attrName == SVGNames::yAttr ||
        attrName == SVGNames::widthAttr || attrName == SVGNames::heightAttr ||
        SVGURIReference::isKnownAttribute(attrName) ||
        SVGTests::isKnownAttribute(attrName) ||
        SVGLangSpace::isKnownAttribute(attrName) ||
        SVGExternalResourcesRequired::isKnownAttribute(attrName) ||
        SVGStyledElement::isKnownAttribute(attrName))
        invalidateCanvasResources();
}

void SVGMaskElement::synchronizeProperty(const QualifiedName& attrName)
{
    SVGStyledElement::synchronizeProperty(attrName);

    if (attrName == anyQName()) {
        synchronizeMaskUnits();
        synchronizeMaskContentUnits();
        synchronizeX();
        synchronizeY();
        synchronizeExternalResourcesRequired();
        synchronizeHref();
        return;
    }

    if (attrName == SVGNames::maskUnitsAttr)
        synchronizeMaskUnits();
    else if (attrName == SVGNames::maskContentUnitsAttr)
        synchronizeMaskContentUnits();
    else if (attrName == SVGNames::xAttr)
        synchronizeX();
    else if (attrName == SVGNames::yAttr)
        synchronizeY();
    else if (SVGExternalResourcesRequired::isKnownAttribute(attrName))
        synchronizeExternalResourcesRequired();
    else if (SVGURIReference::isKnownAttribute(attrName))
        synchronizeHref();
}

void SVGMaskElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    SVGStyledElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);
    invalidateCanvasResources();
}

FloatRect SVGMaskElement::maskBoundingBox(const FloatRect& objectBoundingBox) const
{
    FloatRect maskBBox;
    if (maskUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX)
        maskBBox = FloatRect(x().valueAsPercentage() * objectBoundingBox.width() + objectBoundingBox.x(),
                             y().valueAsPercentage() * objectBoundingBox.height() + objectBoundingBox.y(),
                             width().valueAsPercentage() * objectBoundingBox.width(),
                             height().valueAsPercentage() * objectBoundingBox.height());
    else
        maskBBox = FloatRect(x().value(this),
                             y().value(this),
                             width().value(this),
                             height().value(this));

    return maskBBox;
}

PassOwnPtr<ImageBuffer> SVGMaskElement::drawMaskerContent(const RenderObject* object, FloatRect& maskDestRect, bool& emptyMask) const
{    
    FloatRect objectBoundingBox = object->objectBoundingBox();

    // Mask rect clipped with clippingBoundingBox and filterBoundingBox as long as they are present.
    maskDestRect = object->repaintRectInLocalCoordinates();
    if (maskDestRect.isEmpty()) {
        emptyMask = true;
        return 0;
    }

    // Calculate the smallest rect for the mask ImageBuffer.
    FloatRect repaintRect;
    Vector<RenderObject*> rendererList;
    for (Node* node = firstChild(); node; node = node->nextSibling()) {
        if (!node->isSVGElement() || !static_cast<SVGElement*>(node)->isStyled() || !node->renderer())
            continue;

        rendererList.append(node->renderer());
        repaintRect.unite(node->renderer()->localToParentTransform().mapRect(node->renderer()->repaintRectInLocalCoordinates()));
    }

    AffineTransform contextTransform;
    // We need to scale repaintRect for objectBoundingBox to get the drawing area.
    if (maskContentUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        contextTransform.scaleNonUniform(objectBoundingBox.width(), objectBoundingBox.height());
        FloatPoint contextAdjustment = repaintRect.location();
        repaintRect = contextTransform.mapRect(repaintRect);
        repaintRect.move(objectBoundingBox.x(), objectBoundingBox.y());
        contextTransform.translate(-contextAdjustment.x(), -contextAdjustment.y());
    }
    repaintRect.intersect(maskDestRect);
    maskDestRect = repaintRect;
    IntRect maskImageRect = enclosingIntRect(maskDestRect);

    maskImageRect.setLocation(IntPoint());

    // Don't create ImageBuffers with image size of 0
    if (!maskImageRect.width() || !maskImageRect.height()) {
        emptyMask = true;
        return 0;
    }

    // FIXME: This changes color space to linearRGB, the default color space
    // for masking operations in SVG. We need a switch for the other color-space
    // attribute values sRGB, inherit and auto.
    OwnPtr<ImageBuffer> maskImage = ImageBuffer::create(maskImageRect.size(), LinearRGB);
    if (!maskImage)
        return 0;

    GraphicsContext* maskImageContext = maskImage->context();
    ASSERT(maskImageContext);

    maskImageContext->save();

    if (maskContentUnits() == SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE)
        maskImageContext->translate(-maskDestRect.x(), -maskDestRect.y());
    maskImageContext->concatCTM(contextTransform);

    // draw the content into the ImageBuffer
    Vector<RenderObject*>::iterator end = rendererList.end();
    for (Vector<RenderObject*>::iterator it = rendererList.begin(); it != end; it++)
        renderSubtreeToImage(maskImage.get(), *it);


    maskImageContext->restore();

    // create the luminance mask
    RefPtr<ImageData> imageData(maskImage->getUnmultipliedImageData(maskImageRect));
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

    maskImage->putUnmultipliedImageData(imageData.get(), maskImageRect, IntPoint());

    return maskImage.release();
}
 
RenderObject* SVGMaskElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    RenderSVGContainer* maskContainer = new (arena) RenderSVGContainer(this);
    maskContainer->setDrawsContents(false);
    return maskContainer;
}

SVGResource* SVGMaskElement::canvasResource(const RenderObject* object)
{
    ASSERT(object);

    if (m_masker.contains(object))
        return m_masker.get(object).get();

    RefPtr<SVGResourceMasker> masker = SVGResourceMasker::create(this);
    SVGResourceMasker* maskerPtr = masker.get();
    m_masker.set(object, masker.release());

    return maskerPtr;
}

void SVGMaskElement::invalidateCanvasResources()
{
    // Don't call through to the base class since the base class will just
    // invalidate one item in the HashMap. 
    HashMap<const RenderObject*, RefPtr<SVGResourceMasker> >::const_iterator end = m_masker.end();
    for (HashMap<const RenderObject*, RefPtr<SVGResourceMasker> >::const_iterator it = m_masker.begin(); it != end; ++it)
        it->second->invalidate();
}

}

#endif // ENABLE(SVG)
