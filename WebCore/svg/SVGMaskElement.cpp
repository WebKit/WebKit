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
    , m_maskUnits(this, SVGNames::maskUnitsAttr, SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX)
    , m_maskContentUnits(this, SVGNames::maskContentUnitsAttr, SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE)
    , m_x(this, SVGNames::xAttr, LengthModeWidth, "-10%")
    , m_y(this, SVGNames::yAttr, LengthModeHeight, "-10%")
    , m_width(this, SVGNames::widthAttr, LengthModeWidth, "120%")
    , m_height(this, SVGNames::heightAttr, LengthModeHeight, "120%")
    , m_href(this, XLinkNames::hrefAttr)
    , m_externalResourcesRequired(this, SVGNames::externalResourcesRequiredAttr, false)
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

    if (m_masker.isEmpty())
        return;

    if (attrName == SVGNames::maskUnitsAttr || attrName == SVGNames::maskContentUnitsAttr ||
        attrName == SVGNames::xAttr || attrName == SVGNames::yAttr ||
        attrName == SVGNames::widthAttr || attrName == SVGNames::heightAttr ||
        SVGURIReference::isKnownAttribute(attrName) ||
        SVGTests::isKnownAttribute(attrName) ||
        SVGLangSpace::isKnownAttribute(attrName) ||
        SVGExternalResourcesRequired::isKnownAttribute(attrName) ||
        SVGStyledElement::isKnownAttribute(attrName))
        for (HashMap<const RenderObject*, RefPtr<SVGResourceMasker> >::iterator it = m_masker.begin(); it != m_masker.end(); ++it)
            it->second->invalidate();
}

void SVGMaskElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    SVGStyledElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);

    if (m_masker.isEmpty())
        return;

    for (HashMap<const RenderObject*, RefPtr<SVGResourceMasker> >::iterator it = m_masker.begin(); it != m_masker.end(); ++it)
        it->second->invalidate();
}

PassOwnPtr<ImageBuffer> SVGMaskElement::drawMaskerContent(const FloatRect& objectBoundingBox, FloatRect& maskDestRect) const
{    
    // Determine specified mask size
    if (maskUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX)
        maskDestRect = FloatRect(x().valueAsPercentage() * objectBoundingBox.width() + objectBoundingBox.x(),
                                 y().valueAsPercentage() * objectBoundingBox.height() + objectBoundingBox.y(),
                                 width().valueAsPercentage() * objectBoundingBox.width(),
                                 height().valueAsPercentage() * objectBoundingBox.height());
    else
        maskDestRect = FloatRect(x().value(this),
                                 y().value(this),
                                 width().value(this),
                                 height().value(this));

    // Calculate the smallest rect for the mask ImageBuffer.
    FloatRect repaintRect;
    Vector<RenderObject*> rendererList;
    for (Node* node = firstChild(); node; node = node->nextSibling()) {
        if (!node->isSVGElement() || !static_cast<SVGElement*>(node)->isStyled() || !node->renderer())
            continue;
        // FIXME: repaintRectInLocalCoordinates() is not correctly implemented.
        // The current implementation gives back the union rect of the
        // the stroke and marker from every child. This is what we need here.
        // We create the union of all direct childs to get the common drawing area.
        // See also bug: https://bugs.webkit.org/show_bug.cgi?id=32815
        rendererList.append(node->renderer());
        repaintRect.unite(node->renderer()->repaintRectInLocalCoordinates());
    }

    TransformationMatrix contextTransform;
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

    if (!maskImageRect.width() || !maskImageRect.height())
        return maskImage.release();

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

}

#endif // ENABLE(SVG)
