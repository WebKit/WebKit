/*
	Copyright (C) 2004 Nikolas Zimmermann <wildfox@kde.org>
				  2004 Rob Buis <buis@kde.org>

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

#include <qpaintdevicemetrics.h>
#include <qpaintdevice.h>

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasPath.h>
#include <kcanvas/KCanvasTypes.h>
#include <kcanvas/KCanvasRegistry.h>
#include <kcanvas/device/KRenderingDevice.h>
#include <kcanvas/device/KRenderingFillPainter.h>
#include <kcanvas/device/KRenderingStrokePainter.h>
#include <kcanvas/device/KRenderingPaintServerSolid.h>
#include <kcanvas/device/KRenderingPaintServerGradient.h>

#include <kdom/impl/DocumentImpl.h>
#include <kdom/css/impl/RenderStyle.h>
#include <kdom/css/impl/CSSValueListImpl.h>
#include <kdom/css/impl/CSSPrimitiveValueImpl.h>

#include "ksvg.h"
#include "SVGLengthImpl.h"
#include "SVGRenderStyle.h"
#include "SVGStyledElementImpl.h"
#include "KCanvasRenderingStyle.h"

using namespace KSVG;

KCanvasRenderingStyle::KCanvasRenderingStyle(KCanvas *canvas, const SVGRenderStyle *style) : KRenderingStyle()
{	
	m_style = style;
	m_canvas = canvas;
	m_fillPainter = 0;
	m_strokePainter = 0;
}

KCanvasRenderingStyle::~KCanvasRenderingStyle()
{
	disableFillPainter();
	disableStrokePainter();
}

void KCanvasRenderingStyle::updateFill(KCanvasItem *item)
{
	if(!m_canvas || !m_canvas->renderingDevice())
		return;

	SVGPaintImpl *fill = m_style->fillPaint();
	if(!fill) // initial value (black)
	{
		KRenderingPaintServer *fillPaintServer = m_canvas->renderingDevice()->createPaintServer(KCPaintServerType(PS_SOLID));
		KRenderingPaintServerSolid *fillPaintServerSolid = static_cast<KRenderingPaintServerSolid *>(fillPaintServer);
		fillPaintServerSolid->setColor(Qt::black);

		fillPainter()->setPaintServer(fillPaintServer);
	}
	else if(fill->paintType() == SVG_PAINTTYPE_URI)
	{
		KDOM::DOMString id(fill->uri());

		KRenderingPaintServer *fillPaintServer = m_canvas->registry()->getPaintServerById(id.string().mid(1));
		KCanvasResource *fillPaintResource = dynamic_cast<KCanvasResource *>(fillPaintServer);
		if(item && fillPaintResource)
			fillPaintResource->addClient(item);

		fillPainter()->setPaintServer(fillPaintServer);
	}
	else if(fill->paintType() != SVG_PAINTTYPE_NONE)
	{
		KRenderingPaintServer *fillPaintServer = m_canvas->renderingDevice()->createPaintServer(KCPaintServerType(PS_SOLID));
		KRenderingPaintServerSolid *fillPaintServerSolid = static_cast<KRenderingPaintServerSolid *>(fillPaintServer);

		if(fill->paintType() == SVG_PAINTTYPE_CURRENTCOLOR)
			fillPaintServerSolid->setColor(m_style->color());
		else
			fillPaintServerSolid->setColor(fill->color());

		fillPainter()->setPaintServer(fillPaintServer);
	}

	fillPainter()->setFillRule(m_style->fillRule() == WR_NONZERO ? RULE_NONZERO : RULE_EVENODD);
	fillPainter()->setOpacity(m_style->fillOpacity());
}

void KCanvasRenderingStyle::updateStroke(KCanvasItem *item)
{
	if(!m_canvas || !m_canvas->renderingDevice())
		return;

	SVGPaintImpl *stroke = m_style->strokePaint();
	if(stroke && stroke->paintType() == SVG_PAINTTYPE_URI)
	{
		KDOM::DOMString id(stroke->uri());

		KRenderingPaintServer *strokePaintServer = m_canvas->registry()->getPaintServerById(id.string().mid(1));
		KCanvasResource *strokePaintResource = dynamic_cast<KCanvasResource *>(strokePaintServer);
		if(item && strokePaintResource)
			strokePaintResource->addClient(item);

		strokePainter()->setPaintServer(strokePaintServer);
	}
	else if(stroke && stroke->paintType() != SVG_PAINTTYPE_NONE)
	{
		KRenderingPaintServer *strokePaintServer = m_canvas->renderingDevice()->createPaintServer(KCPaintServerType(PS_SOLID));
		KRenderingPaintServerSolid *strokePaintServerSolid = static_cast<KRenderingPaintServerSolid *>(strokePaintServer);
		
		if(stroke->paintType() == SVG_PAINTTYPE_CURRENTCOLOR)
			strokePaintServerSolid->setColor(m_style->color());
		else
			strokePaintServerSolid->setColor(stroke->color());

		strokePainter()->setPaintServer(strokePaintServer);
	}

	strokePainter()->setOpacity(m_style->strokeOpacity());
	strokePainter()->setStrokeWidth(cssPrimitiveToLength(item, m_style->strokeWidth(), 1.0));

	KDOM::CSSValueListImpl *dashes = m_style->strokeDashArray();
	if(dashes)
	{
		KDOM::CSSPrimitiveValueImpl *dash = 0;
		QPaintDeviceMetrics *paintDeviceMetrics = 0;

		SVGElementImpl *element = static_cast<SVGElementImpl *>(item->userData());
		if(element && element->ownerDocument())
			paintDeviceMetrics = element->ownerDocument()->paintDeviceMetrics();

		KCDashArray array;
		unsigned long len = dashes->length();
		for(unsigned long i = 0; i < len; i++)
		{
			dash = static_cast<KDOM::CSSPrimitiveValueImpl *>(dashes->item(i));
			if(dash)
				array.append((float) dash->computeLengthFloat(const_cast<SVGRenderStyle *>(m_style), paintDeviceMetrics));
		}

		strokePainter()->setDashArray(array);
		strokePainter()->setDashOffset(cssPrimitiveToLength(item, m_style->strokeDashOffset(), 0.0));
	}

	strokePainter()->setStrokeMiterLimit(m_style->strokeMiterLimit());
	strokePainter()->setStrokeCapStyle((KCCapStyle) m_style->capStyle());
	strokePainter()->setStrokeJoinStyle((KCJoinStyle) m_style->joinStyle());
}

void KCanvasRenderingStyle::updateStyle(const SVGRenderStyle *style, KCanvasItem *item)
{
	m_style = style;
	
	disableFillPainter();
	disableStrokePainter();
	
	updateFill(item);
	updateStroke(item);
}

void KCanvasRenderingStyle::disableFillPainter()
{
	if(m_fillPainter)
	{
		delete m_fillPainter;
		m_fillPainter = 0;
	}
}

double KCanvasRenderingStyle::cssPrimitiveToLength(KCanvasItem *item, KDOM::CSSValueImpl *value, double defaultValue) const
{
	KDOM::CSSPrimitiveValueImpl *primitive = static_cast<KDOM::CSSPrimitiveValueImpl *>(value);

	unsigned short cssType = (primitive ? primitive->primitiveType() : (unsigned short) KDOM::CSS_UNKNOWN);
	if(!(cssType > KDOM::CSS_UNKNOWN && cssType <= KDOM::CSS_PC))
		return defaultValue;

	QPaintDeviceMetrics *paintDeviceMetrics = 0;

	SVGElementImpl *element = static_cast<SVGElementImpl *>(item->userData());
	if(element && element->ownerDocument())
		paintDeviceMetrics = element->ownerDocument()->paintDeviceMetrics();

	if(cssType == KDOM::CSS_PERCENTAGE)
	{
		SVGElementImpl *viewportElement = (element ? element->viewportElement() : 0);
		if(viewportElement)
		{
			double result = primitive->getFloatValue(KDOM::CSS_PERCENTAGE) / 100.0;
			return SVGHelper::PercentageOfViewport(result, viewportElement, LM_OTHER);
		}
	}

	return primitive->computeLengthFloat(const_cast<SVGRenderStyle *>(m_style), paintDeviceMetrics);
}

// World matrix property
KCanvasMatrix KCanvasRenderingStyle::objectMatrix() const
{
	return m_matrix;
}

void KCanvasRenderingStyle::setObjectMatrix(const KCanvasMatrix &matrix)
{
	m_matrix = matrix;
}

// Stroke (aka Pen) properties
bool KCanvasRenderingStyle::isStroked() const
{
	return (m_strokePainter != 0);
}

KRenderingStrokePainter *KCanvasRenderingStyle::strokePainter()
{
	if(!m_strokePainter)
		m_strokePainter = new KRenderingStrokePainter();

	return m_strokePainter;
}

void KCanvasRenderingStyle::disableStrokePainter()
{
	if(m_strokePainter)
	{
		delete m_strokePainter;
		m_strokePainter = 0;
	}
}

// Fill (aka Bush) properties
bool KCanvasRenderingStyle::isFilled() const
{
	return (m_fillPainter != 0);
}

KRenderingFillPainter *KCanvasRenderingStyle::fillPainter()
{
	if(!m_fillPainter)
		m_fillPainter = new KRenderingFillPainter();
	
	return m_fillPainter;
}

// Display states
bool KCanvasRenderingStyle::visible() const
{
	return (m_style->display() != KDOM::DS_NONE) &&
		   (m_style->visibility() == KDOM::VS_VISIBLE);
}

void KCanvasRenderingStyle::setVisible(bool)
{
	// no-op
}

// Color interpolation
KCColorInterpolation KCanvasRenderingStyle::colorInterpolation() const
{
	return KCColorInterpolation();
}

void KCanvasRenderingStyle::setColorInterpolation(KCColorInterpolation)
{
	// nop-op
}

KCImageRendering KCanvasRenderingStyle::imageRendering() const
{
	return (m_style->imageRendering() == IR_OPTIMIZESPEED) ? IR_OPTIMIZE_SPEED : IR_OPTIMIZE_QUALITY;
}

void KCanvasRenderingStyle::setImageRendering(KCImageRendering)
{
	// no-op
}

// Overall opacity
int KCanvasRenderingStyle::opacity() const
{
	return int(m_style->opacity() * 255.);
}

void KCanvasRenderingStyle::setOpacity(int)
{
	// no-op
}

// Clipping
QStringList KCanvasRenderingStyle::clipPaths() const
{
	QString clipPathRef = m_style->clipPath();
	if(!clipPathRef.isEmpty() && (m_clipPaths.isEmpty() || (m_clipPaths.last() != clipPathRef)) )
		m_clipPaths.append(clipPathRef);

	return m_clipPaths;
}

void KCanvasRenderingStyle::addClipPath(const QString &clipPath)
{
	m_clipPaths.append(clipPath);
}

void KCanvasRenderingStyle::removeClipPaths()
{
	m_clipPaths.clear();
}

// Markers
KCanvasMarker *KCanvasRenderingStyle::startMarker() const
{
	return static_cast<KCanvasMarker *>(m_canvas->registry()->getResourceById(m_style->startMarker().mid(1)));
}

void KCanvasRenderingStyle::setStartMarker(KCanvasMarker *)
{
	// no-op
}

KCanvasMarker *KCanvasRenderingStyle::midMarker() const
{
	return static_cast<KCanvasMarker *>(m_canvas->registry()->getResourceById(m_style->midMarker().mid(1)));
}

void KCanvasRenderingStyle::setMidMarker(KCanvasMarker *)
{
}

KCanvasMarker *KCanvasRenderingStyle::endMarker() const
{
	return static_cast<KCanvasMarker *>(m_canvas->registry()->getResourceById(m_style->endMarker().mid(1)));
}

void KCanvasRenderingStyle::setEndMarker(KCanvasMarker *)
{
	// no-op
}

bool KCanvasRenderingStyle::hasMarkers() const
{
	return !m_style->startMarker().isEmpty() ||
		   !m_style->midMarker().isEmpty() ||
		   !m_style->endMarker().isEmpty();
}

KCanvasFilter *KCanvasRenderingStyle::filter() const
{
	QString lookup = m_style->filter().mid(1);
	if(lookup.isEmpty())
		return 0;

	return static_cast<KCanvasFilter *>(m_canvas->registry()->getResourceById(lookup));
}

// vim:ts=4:noet
