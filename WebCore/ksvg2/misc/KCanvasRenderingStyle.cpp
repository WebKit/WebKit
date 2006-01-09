/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"
#include <q3paintdevicemetrics.h>
#include <qpaintdevice.h>

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasPath.h>
#include <kcanvas/KCanvasTypes.h>
#include <kcanvas/device/KRenderingDevice.h>
#include <kcanvas/device/KRenderingFillPainter.h>
#include <kcanvas/device/KRenderingStrokePainter.h>
#include <kcanvas/device/KRenderingPaintServerSolid.h>
#include <kcanvas/device/KRenderingPaintServerGradient.h>
#include <kcanvas/device/KRenderingPaintServer.h>

#include <kdom/core/DocumentImpl.h>
#include <kdom/DOMString.h>
#include <kdom/css/RenderStyle.h>
#include <kdom/css/CSSValueListImpl.h>
#include <kdom/css/CSSPrimitiveValueImpl.h>

#include "ksvg.h"
#include "SVGLengthImpl.h"
#include "SVGStyledElementImpl.h"
#include "KCanvasRenderingStyle.h"
#include "SVGRenderStyle.h"

using namespace KSVG;

KCanvasRenderingStyle::KCanvasRenderingStyle(const khtml::RenderStyle *style)
{
    m_style = style;
    m_fillPainter = 0;
    m_strokePainter = 0;
    m_fillPainterPaintServerOverride = 0;
    m_strokePainterPaintServerOverride = 0;
}

KCanvasRenderingStyle::~KCanvasRenderingStyle()
{
}

void KCanvasRenderingStyle::overrideFillPaintServer(KRenderingPaintServer *pserver)
{
    ASSERT(pserver->type() == PS_IMAGE);
    delete m_fillPainterPaintServerOverride;
    m_fillPainterPaintServerOverride = pserver;
}

bool KCanvasRenderingStyle::isFilled() const
{
    SVGPaintImpl *fill = m_style->svgStyle()->fillPaint();
    if (!fill)
        return true;
    return (m_fillPainterPaintServerOverride || (fill && fill->paintType() != SVG_PAINTTYPE_NONE));
}

KRenderingPaintServer *KCanvasRenderingStyle::fillPaintServer(const khtml::RenderStyle *style, const RenderPath* item)
{
    if (m_fillPainterPaintServerOverride)
        return m_fillPainterPaintServerOverride;

    SVGPaintImpl *fill = m_style->svgStyle()->fillPaint();

    if (fill && fill->paintType() == SVG_PAINTTYPE_NONE)
        return 0;

    KRenderingPaintServer *fillPaintServer;
    if (!fill) {
        // initial value (black)
        fillPaintServer = QPainter::renderingDevice()->createPaintServer(KCPaintServerType(PS_SOLID));
        KRenderingPaintServerSolid *fillPaintServerSolid = static_cast<KRenderingPaintServerSolid *>(fillPaintServer);
        fillPaintServerSolid->setColor(Qt::black);
    } else if (fill->paintType() == SVG_PAINTTYPE_URI) {
        KDOM::DOMString id(fill->uri());
        fillPaintServer = getPaintServerById(item->document(), id.qstring().mid(1));
        if (item && fillPaintServer)
            fillPaintServer->addClient(item);
    } else {
        fillPaintServer = QPainter::renderingDevice()->createPaintServer(KCPaintServerType(PS_SOLID));
        KRenderingPaintServerSolid *fillPaintServerSolid = static_cast<KRenderingPaintServerSolid *>(fillPaintServer);
        if (fill->paintType() == SVG_PAINTTYPE_CURRENTCOLOR)
            fillPaintServerSolid->setColor(m_style->color());
        else
            fillPaintServerSolid->setColor(fill->color());
    }

    return fillPaintServer;
}

void KCanvasRenderingStyle::overrideStrokePaintServer(KRenderingPaintServer *pserver)
{
    strokePainter()->setDirty();
    ASSERT(pserver->type() == PS_IMAGE);
    delete m_strokePainterPaintServerOverride;
    m_strokePainterPaintServerOverride = pserver;
}

bool KCanvasRenderingStyle::isStroked() const
{
    SVGPaintImpl *stroke = m_style->svgStyle()->strokePaint();
    return (stroke && stroke->paintType() != SVG_PAINTTYPE_NONE);
}

KRenderingPaintServer *KCanvasRenderingStyle::strokePaintServer(const khtml::RenderStyle *style, const RenderPath* item)
{
    if (m_strokePainterPaintServerOverride) 
        return m_strokePainterPaintServerOverride;

    SVGPaintImpl *stroke = m_style->svgStyle()->strokePaint();

    if (!stroke || stroke->paintType() == SVG_PAINTTYPE_NONE)
        return 0;

    KRenderingPaintServer *strokePaintServer;
    if (stroke && stroke->paintType() == SVG_PAINTTYPE_URI) {
        KDOM::DOMString id(stroke->uri());
        strokePaintServer = getPaintServerById(item->document(), id.qstring().mid(1));
        if(item && strokePaintServer)
            strokePaintServer->addClient(item);
    } else {
        strokePaintServer = QPainter::renderingDevice()->createPaintServer(KCPaintServerType(PS_SOLID));
        KRenderingPaintServerSolid *strokePaintServerSolid = static_cast<KRenderingPaintServerSolid *>(strokePaintServer);
        if (stroke->paintType() == SVG_PAINTTYPE_CURRENTCOLOR)
            strokePaintServerSolid->setColor(m_style->color());
        else
            strokePaintServerSolid->setColor(stroke->color());
    }

    return strokePaintServer;
}

void KCanvasRenderingStyle::updateStyle(const khtml::RenderStyle *style, RenderPath *item)
{
    m_style = style;
    
    fillPainter()->setFillRule(m_style->svgStyle()->fillRule() == WR_NONZERO ? RULE_NONZERO : RULE_EVENODD);
    fillPainter()->setOpacity(m_style->svgStyle()->fillOpacity());

    strokePainter()->setOpacity(m_style->svgStyle()->strokeOpacity());
    strokePainter()->setStrokeWidth(KCanvasRenderingStyle::cssPrimitiveToLength(item, m_style->svgStyle()->strokeWidth(), 1.0));

    KDOM::CSSValueListImpl *dashes = m_style->svgStyle()->strokeDashArray();
    if (dashes) {
        KDOM::CSSPrimitiveValueImpl *dash = 0;
        Q3PaintDeviceMetrics *paintDeviceMetrics = 0;

        SVGElementImpl *element = static_cast<SVGElementImpl *>(item->element());
        if (element && element->ownerDocument())
            paintDeviceMetrics = element->ownerDocument()->paintDeviceMetrics();

        KCDashArray array;
        unsigned long len = dashes->length();
        for (unsigned long i = 0; i < len; i++) {
            dash = static_cast<KDOM::CSSPrimitiveValueImpl *>(dashes->item(i));
            if (dash)
                array.append((float) dash->computeLengthFloat(const_cast<khtml::RenderStyle *>(m_style), paintDeviceMetrics));
        }

        strokePainter()->setDashArray(array);
        strokePainter()->setDashOffset(KCanvasRenderingStyle::cssPrimitiveToLength(item, m_style->svgStyle()->strokeDashOffset(), 0.0));
    }

    strokePainter()->setStrokeMiterLimit(m_style->svgStyle()->strokeMiterLimit());
    strokePainter()->setStrokeCapStyle((KCCapStyle) m_style->svgStyle()->capStyle());
    strokePainter()->setStrokeJoinStyle((KCJoinStyle) m_style->svgStyle()->joinStyle());
}

double KCanvasRenderingStyle::cssPrimitiveToLength(const RenderPath *item, KDOM::CSSValueImpl *value, double defaultValue)
{
    KDOM::CSSPrimitiveValueImpl *primitive = static_cast<KDOM::CSSPrimitiveValueImpl *>(value);

    unsigned short cssType = (primitive ? primitive->primitiveType() : (unsigned short) KDOM::CSSPrimitiveValue::CSS_UNKNOWN);
    if(!(cssType > KDOM::CSSPrimitiveValue::CSS_UNKNOWN && cssType <= KDOM::CSSPrimitiveValue::CSS_PC))
        return defaultValue;

    Q3PaintDeviceMetrics *paintDeviceMetrics = 0;

    SVGElementImpl *element = static_cast<SVGElementImpl *>(item->element());
    if(element && element->ownerDocument())
        paintDeviceMetrics = element->ownerDocument()->paintDeviceMetrics();

    if(cssType == KDOM::CSSPrimitiveValue::CSS_PERCENTAGE)
    {
        SVGElementImpl *viewportElement = (element ? element->viewportElement() : 0);
        if(viewportElement)
        {
            double result = primitive->getFloatValue(KDOM::CSSPrimitiveValue::CSS_PERCENTAGE) / 100.0;
            return SVGHelper::PercentageOfViewport(result, viewportElement, LM_OTHER);
        }
    }

    return primitive->computeLengthFloat(const_cast<khtml::RenderStyle *>(item->style()), paintDeviceMetrics);
}

KRenderingStrokePainter *KCanvasRenderingStyle::strokePainter()
{
    if(!m_strokePainter)
        m_strokePainter = new KRenderingStrokePainter();

    return m_strokePainter;
}

KRenderingFillPainter *KCanvasRenderingStyle::fillPainter()
{
    if(!m_fillPainter)
        m_fillPainter = new KRenderingFillPainter();
    
    return m_fillPainter;
}

// vim:ts=4:noet
