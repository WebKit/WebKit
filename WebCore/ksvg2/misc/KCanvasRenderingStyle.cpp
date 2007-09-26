/*
    Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                        2006 Alexander Kellett <lypanov@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG)
#include "KCanvasRenderingStyle.h"

#include "CSSValueList.h"
#include "Document.h"
#include "RenderObject.h"
#include "RenderPath.h"
#include "SVGLength.h"
#include "SVGPaintServerGradient.h"
#include "SVGPaintServerSolid.h"
#include "SVGURIReference.h"
#include "SVGRenderStyle.h"
#include "SVGStyledElement.h"

#include <wtf/PassRefPtr.h>

namespace WebCore {

static SVGPaintServerSolid* sharedSolidPaintServer()
{
    static SVGPaintServerSolid* _sharedSolidPaintServer = 0;
    if (!_sharedSolidPaintServer)
        _sharedSolidPaintServer = new SVGPaintServerSolid();
    return _sharedSolidPaintServer;
}

SVGPaintServer* KSVGPainterFactory::fillPaintServer(const RenderStyle* style, const RenderObject* item)
{
    if (!style->svgStyle()->hasFill())
        return 0;

    SVGPaint* fill = style->svgStyle()->fillPaint();

    SVGPaintServer* fillPaintServer = 0;
    if (fill->paintType() == SVGPaint::SVG_PAINTTYPE_URI) {
        AtomicString id(SVGURIReference::getTarget(fill->uri()));
        fillPaintServer = getPaintServerById(item->document(), id);

        SVGElement* svgElement = static_cast<SVGElement*>(item->element());
        ASSERT(svgElement && svgElement->document() && svgElement->isStyled());

        if (item->isRenderPath() && fillPaintServer)
            fillPaintServer->addClient(static_cast<SVGStyledElement*>(svgElement));
        else if (!fillPaintServer)
            svgElement->document()->accessSVGExtensions()->addPendingResource(id, static_cast<SVGStyledElement*>(svgElement)); 
    } else {
        fillPaintServer = sharedSolidPaintServer();
        SVGPaintServerSolid* fillPaintServerSolid = static_cast<SVGPaintServerSolid*>(fillPaintServer);
        if (fill->paintType() == SVGPaint::SVG_PAINTTYPE_CURRENTCOLOR)
            fillPaintServerSolid->setColor(style->color());
        else
            fillPaintServerSolid->setColor(fill->color());
        // FIXME: Ideally invalid colors would never get set on the RenderStyle and this could turn into an ASSERT
        if (!fillPaintServerSolid->color().isValid())
            fillPaintServer = 0;
    }
    if (!fillPaintServer) {
        // default value (black), see bug 11017
        fillPaintServer = sharedSolidPaintServer();
        static_cast<SVGPaintServerSolid*>(fillPaintServer)->setColor(Color::black);
    }
    return fillPaintServer;
}

SVGPaintServer* KSVGPainterFactory::strokePaintServer(const RenderStyle* style, const RenderObject* item)
{
    if (!style->svgStyle()->hasStroke())
        return 0;

    SVGPaint* stroke = style->svgStyle()->strokePaint();

    SVGPaintServer* strokePaintServer = 0;
    if (stroke->paintType() == SVGPaint::SVG_PAINTTYPE_URI) {
        AtomicString id(SVGURIReference::getTarget(stroke->uri()));
        strokePaintServer = getPaintServerById(item->document(), id);

        SVGElement* svgElement = static_cast<SVGElement*>(item->element());
        ASSERT(svgElement && svgElement->document() && svgElement->isStyled());
 
        if (item->isRenderPath() && strokePaintServer)
            strokePaintServer->addClient(static_cast<SVGStyledElement*>(svgElement));
        else if (!strokePaintServer)
            svgElement->document()->accessSVGExtensions()->addPendingResource(id, static_cast<SVGStyledElement*>(svgElement)); 
    } else {
        strokePaintServer = sharedSolidPaintServer();
        SVGPaintServerSolid* strokePaintServerSolid = static_cast<SVGPaintServerSolid*>(strokePaintServer);
        if (stroke->paintType() == SVGPaint::SVG_PAINTTYPE_CURRENTCOLOR)
            strokePaintServerSolid->setColor(style->color());
        else
            strokePaintServerSolid->setColor(stroke->color());
        // FIXME: Ideally invalid colors would never get set on the RenderStyle and this could turn into an ASSERT
        if (!strokePaintServerSolid->color().isValid())
            strokePaintServer = 0;
    }

    return strokePaintServer;
}

double KSVGPainterFactory::cssPrimitiveToLength(const RenderObject* item, CSSValue *value, double defaultValue)
{
    CSSPrimitiveValue* primitive = static_cast<CSSPrimitiveValue*>(value);

    unsigned short cssType = (primitive ? primitive->primitiveType() : (unsigned short) CSSPrimitiveValue::CSS_UNKNOWN);
    if (!(cssType > CSSPrimitiveValue::CSS_UNKNOWN && cssType <= CSSPrimitiveValue::CSS_PC))
        return defaultValue;

    if (cssType == CSSPrimitiveValue::CSS_PERCENTAGE) {
        SVGStyledElement* element = static_cast<SVGStyledElement*>(item->element());
        SVGElement* viewportElement = (element ? element->viewportElement() : 0);
        if (viewportElement) {
            float result = primitive->getFloatValue() / 100.0f;
            return SVGLength::PercentageOfViewport(result, element, LengthModeOther);
        }
    }

    return primitive->computeLengthDouble(const_cast<RenderStyle*>(item->style()));
}

KCDashArray KSVGPainterFactory::dashArrayFromRenderingStyle(const RenderStyle* style)
{
    KCDashArray array;
    
    CSSValueList* dashes = style->svgStyle()->strokeDashArray();
    if (dashes) {
        CSSPrimitiveValue* dash = 0;
        unsigned long len = dashes->length();
        for (unsigned long i = 0; i < len; i++) {
            dash = static_cast<CSSPrimitiveValue*>(dashes->item(i));
            if (!dash)
                continue;

            array.append(dash->computeLengthFloat(const_cast<RenderStyle*>(style)));
        }
    }

    return array;
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
