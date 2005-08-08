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
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include <kdom/Namespace.h>
#include <kdom/css/CSSValue.h>
#include <kdom/events/Event.h>
#include <kdom/impl/NodeImpl.h>
#include <kdom/events/impl/EventImpl.h>

#include "ksvg.h"
#include "Ecma.h"
#include "SVGPaint.h"
#include "SVGEvent.h"
#include "SVGColor.h"
#include "SVGPathSeg.h"
#include "SVGAElement.h"
#include "SVGDocument.h"
#include "SVGGElement.h"
#include "SVGPaintImpl.h"
#include "GlobalObject.h"
#include "SVGColorImpl.h"
#include "SVGEventImpl.h"
#include "SVGZoomEvent.h"
#include "SVGUseElement.h"
#include "SVGSVGElement.h"
#include "SVGPathSegArc.h"
#include "SVGSetElement.h"
#include "EcmaInterface.h"
#include "SVGDescElement.h"
#include "SVGRectElement.h"
#include "SVGDefsElement.h"
#include "SVGStopElement.h"
#include "SVGPathElement.h"
#include "SVGLineElement.h"
#ifdef TEXTSUPPORT
#include "SVGTextElement.h"
#include "SVGTSpanElement.h"
#endif
#include "SVGViewElement.h"
#include "SVGImageElement.h"
#include "SVGTitleElement.h"
#include "SVGFilterElement.h"
#include "SVGFEBlendElement.h"
#include "SVGFEFloodElement.h"
#include "SVGFEOffsetElement.h"
#include "SVGFEImageElement.h"
#include "SVGFEMergeElement.h"
#include "SVGFEMergeNodeElement.h"
#include "SVGDocumentImpl.h"
#include "SVGStyleElement.h"
#include "SVGPathSegMoveto.h"
#include "SVGPathSegLineto.h"
#include "SVGSwitchElement.h"
#include "SVGScriptElement.h"
#include "SVGCircleElement.h"
#include "SVGSymbolElement.h"
#include "SVGZoomEventImpl.h"
#include "SVGMarkerElement.h"
#include "SVGEllipseElement.h"
#include "SVGAnimateElement.h"
#include "SVGPolygonElement.h"
#include "SVGPatternElement.h"
#include "SVGPolylineElement.h"
#include "SVGClipPathElement.h"
#include "SVGPathSegClosePath.h"
#include "SVGStyledElementImpl.h"
#include "SVGFECompositeElement.h"
#include "SVGFEColorMatrixElement.h"
#include "SVGFEGaussianBlurElement.h"
#include "SVGAnimateColorElement.h"
#include "SVGPathSegCurvetoCubic.h"
#include "SVGLinearGradientElement.h"
#include "SVGRadialGradientElement.h"
#include "SVGPathSegLinetoVertical.h"
#include "SVGPathSegLinetoHorizontal.h"
#include "SVGPathSegCurvetoQuadratic.h"
#include "SVGAnimateTransformElement.h"
#include "SVGPathSegCurvetoCubicSmooth.h"
#include "SVGPathSegCurvetoQuadraticSmooth.h"

using namespace KSVG;

Ecma::Ecma(KDOM::DocumentImpl *doc) : KDOM::Ecma(doc)
{
}

Ecma::~Ecma()
{
}

void Ecma::setupDocument(KDOM::DocumentImpl *document)
{
	SVGDocumentImpl *svgDocument = dynamic_cast<SVGDocumentImpl *>(document);
	if(!svgDocument)
	{	
		kdFatal() << "Ecma::setupDocument -> The impossible happened..." << endl;
		return;
	}
	
	// Create base bridge for document
	SVGDocument docObj(svgDocument);

	KJS::ObjectImp *kjsObj = docObj.bridge(interpreter()->globalExec());
#ifndef APPLE_CHANGES
	kjsObj->ref();
#endif

	interpreter()->putDOMObject(svgDocument, kjsObj);
	svgDocument->deref();
}

KJS::ObjectImp *Ecma::inheritedGetDOMNode(KJS::ExecState *exec, KDOM::Node n)
{
	// Use svg element ids to distinguish between svg elements.
	KJS::ObjectImp *ret = 0;

	KDOM::NodeImpl *nodeImpl = static_cast<KDOM::NodeImpl *>(n.handle());
	if(!nodeImpl)
		return ret;

	if(nodeImpl->namespaceURI() != KDOM::NS_SVG)
		return ret;

	// Special case for our document
	if(n.nodeType() == KDOM::DOCUMENT_NODE)
		return SVGDocument(n).bridge(exec);

	switch(nodeImpl->id())
	{
		// TODO: Add all remaining nodes here...
		case ID_SVG:
		{
			ret = SVGSVGElement(n).bridge(exec);
			break;
		}
		case ID_STYLE:
		{
			ret = SVGStyleElement(n).bridge(exec);
			break;
		}
		case ID_SCRIPT:
		{
			ret = SVGScriptElement(n).bridge(exec);
			break;
		}
		case ID_RECT:
		{
			ret = SVGRectElement(n).bridge(exec);
			break;
		}
		case ID_CIRCLE:
		{
			ret = SVGCircleElement(n).bridge(exec);
			break;
		}
		case ID_ELLIPSE:
		{
			ret = SVGEllipseElement(n).bridge(exec);
			break;
		}
		case ID_POLYLINE:
		{
			ret = SVGPolylineElement(n).bridge(exec);
			break;
		}
		case ID_POLYGON:
		{
			ret = SVGPolygonElement(n).bridge(exec);
			break;
		}
		case ID_G:
		{
			ret = SVGGElement(n).bridge(exec);
			break;
		}
		case ID_SWITCH:
		{
			ret = SVGSwitchElement(n).bridge(exec);
			break;
		}
		case ID_DEFS:
		{
			ret = SVGDefsElement(n).bridge(exec);
			break;
		}
		case ID_STOP:
		{
			ret = SVGStopElement(n).bridge(exec);
			break;
		}
		case ID_PATH:
		{
			ret = SVGPathElement(n).bridge(exec);
			break;
		}
		case ID_IMAGE:
		{
			ret = SVGImageElement(n).bridge(exec);
			break;
		}
		case ID_CLIPPATH:
		{
			ret = SVGClipPathElement(n).bridge(exec);
			break;
		}
		case ID_A:
		{
			ret = SVGAElement(n).bridge(exec);
			break;
		}
		case ID_LINE:
		{
			ret = SVGLineElement(n).bridge(exec);
			break;
		}
		case ID_LINEARGRADIENT:
		{
			ret = SVGLinearGradientElement(n).bridge(exec);
			break;
		}
		case ID_RADIALGRADIENT:
		{
			ret = SVGRadialGradientElement(n).bridge(exec);
			break;
		}
		case ID_TITLE:
		{
			ret = SVGTitleElement(n).bridge(exec);
			break;
		}
		case ID_DESC:
		{
			ret = SVGDescElement(n).bridge(exec);
			break;
		}
		case ID_SYMBOL:
		{
			ret = SVGSymbolElement(n).bridge(exec);
			break;
		}
		case ID_USE:
		{
			ret = SVGUseElement(n).bridge(exec);
			break;
		}
		case ID_PATTERN:
		{
			ret = SVGPatternElement(n).bridge(exec);
			break;
		}
		case ID_ANIMATECOLOR:
		{
			ret = SVGAnimateColorElement(n).bridge(exec);
			break;
		}
		case ID_ANIMATETRANSFORM:
		{
			ret = SVGAnimateTransformElement(n).bridge(exec);
			break;
		}
		case ID_SET:
		{
			ret = SVGSetElement(n).bridge(exec);
			break;
		}
		case ID_ANIMATE:
		{
			ret = SVGAnimateElement(n).bridge(exec);
			break;
		}
		case ID_MARKER:
		{
			ret = SVGMarkerElement(n).bridge(exec);
			break;
		}
		case ID_VIEW:
		{
			ret = SVGViewElement(n).bridge(exec);
			break;
		}
		case ID_FILTER:
		{
			ret = SVGFilterElement(n).bridge(exec);
			break;
		}
		case ID_FEGAUSSIANBLUR:
		{
			ret = SVGFEGaussianBlurElement(n).bridge(exec);
			break;
		}
		case ID_FEFLOOD:
		{
			ret = SVGFEFloodElement(n).bridge(exec);
			break;
		}
		case ID_FEBLEND:
		{
			ret = SVGFEBlendElement(n).bridge(exec);
			break;
		}
		case ID_FEOFFSET:
		{
			ret = SVGFEOffsetElement(n).bridge(exec);
			break;
		}
		case ID_FECOMPOSITE:
		{
			ret = SVGFECompositeElement(n).bridge(exec);
			break;
		}
		case ID_FECOLORMATRIX:
		{
			ret = SVGFEColorMatrixElement(n).bridge(exec);
			break;
		}
		case ID_FEIMAGE:
		{
			ret = SVGFEImageElement(n).bridge(exec);
			break;
		}
		case ID_FEMERGE:
		{
			ret = SVGFEMergeElement(n).bridge(exec);
			break;
		}
		case ID_FEMERGENODE:
		{
			ret = SVGFEMergeNodeElement(n).bridge(exec);
			break;
		}
#ifdef TEXTSUPPORT
		case ID_TEXT:
		{
			ret = SVGTextElement(n).bridge(exec);
			break;
		}
		case ID_TSPAN:
		{
			ret = SVGTSpanElement(n).bridge(exec);
			break;
		}
#endif
		default: // Maybe it's an SVG?Element, w/o ecma bindings so far...
			ret = SVGElement(n).bridge(exec);
	}

	return ret;
}

KJS::ObjectImp *Ecma::inheritedGetDOMEvent(KJS::ExecState *exec, KDOM::Event e)
{
	KDOM::EventImpl *eventImpl = e.handle();
	if(!eventImpl)
		return 0;

	KDOM::EventImplType identifier = eventImpl->identifier();
	if(identifier != KDOM::TypeLastEvent)
		return 0;

	SVGEventImpl *test1 = dynamic_cast<SVGEventImpl *>(eventImpl);
	if(test1)
		return SVGEvent(test1).bridge(exec);

	SVGZoomEventImpl *test2 = dynamic_cast<SVGZoomEventImpl *>(eventImpl);
	if(test2)
		return SVGZoomEvent(test2).bridge(exec);

	return 0;
}

KJS::ObjectImp *Ecma::inheritedGetDOMCSSValue(KJS::ExecState *exec, KDOM::CSSValue c)
{
	KDOM::CSSValueImpl *impl = c.handle();

	// Keep the order, as SVGPaintImpl inherits from SVGColorImpl...
	SVGPaintImpl *test1 = dynamic_cast<SVGPaintImpl *>(impl);
	if(test1)
		return SVGPaint(test1).bridge(exec);

	SVGColorImpl *test2 = dynamic_cast<SVGColorImpl *>(impl);
	if(test2)
		return SVGColor(test2).bridge(exec);

	return 0;
}

KJS::ValueImp *KSVG::getSVGPathSeg(KJS::ExecState *exec, SVGPathSeg s)
{
	if(s == SVGPathSeg::null)
		return KJS::Null();

	KDOM::ScriptInterpreter *interpreter = static_cast<KDOM::ScriptInterpreter *>(exec->interpreter());
	if(!interpreter)
		return KJS::Null();
	
	// Reuse existing bridge, if possible
	KJS::ObjectImp *request = interpreter->getDOMObject(s.handle());
	if(request)
		return request;
	
	KJS::ObjectImp *ret = 0;
	unsigned short type = s.pathSegType();

	switch(type)
	{
		case PATHSEG_CLOSEPATH:
		{
			ret = SVGPathSegClosePath(s).bridge(exec);
			break;
		}
		case PATHSEG_MOVETO_ABS:
		{
			ret = SVGPathSegMovetoAbs(s).bridge(exec);
			break;
		}
		case PATHSEG_MOVETO_REL:
		{
			ret = SVGPathSegMovetoRel(s).bridge(exec);
			break;
		}
		case PATHSEG_LINETO_ABS:
		{
			ret = SVGPathSegLinetoAbs(s).bridge(exec);
			break;
		}
		case PATHSEG_LINETO_REL:
		{
			ret = SVGPathSegLinetoRel(s).bridge(exec);
			break;
		}
		case PATHSEG_CURVETO_CUBIC_ABS:
		{
			ret = SVGPathSegCurvetoCubicAbs(s).bridge(exec);
			break;
		}
		case PATHSEG_CURVETO_CUBIC_REL:
		{
			ret = SVGPathSegCurvetoCubicRel(s).bridge(exec);
			break;
		}
		case PATHSEG_CURVETO_QUADRATIC_ABS:
		{
			ret = SVGPathSegCurvetoQuadraticAbs(s).bridge(exec);
			break;
		}
		case PATHSEG_CURVETO_QUADRATIC_REL:
		{
			ret = SVGPathSegCurvetoQuadraticRel(s).bridge(exec);
			break;
		}
		case PATHSEG_ARC_ABS:
		{
			ret = SVGPathSegArcAbs().bridge(exec);
			break;
		}
		case PATHSEG_ARC_REL:
		{
			ret = SVGPathSegArcRel(s).bridge(exec);
			break;
		}
		case PATHSEG_LINETO_HORIZONTAL_ABS:
		{
			ret = SVGPathSegLinetoHorizontalAbs(s).bridge(exec);
			break;
		}
		case PATHSEG_LINETO_HORIZONTAL_REL:
		{
			ret = SVGPathSegLinetoHorizontalRel(s).bridge(exec);
			break;
		}
		case PATHSEG_LINETO_VERTICAL_ABS:
		{
			ret = SVGPathSegLinetoVerticalAbs(s).bridge(exec);
			break;
		}
		case PATHSEG_LINETO_VERTICAL_REL:
		{
			ret = SVGPathSegLinetoVerticalRel(s).bridge(exec);
			break;
		}
		case PATHSEG_CURVETO_CUBIC_SMOOTH_ABS:
		{
			ret = SVGPathSegCurvetoCubicSmoothAbs(s).bridge(exec);
			break;
		}
		case PATHSEG_CURVETO_CUBIC_SMOOTH_REL:
		{
			ret = SVGPathSegCurvetoCubicSmoothRel(s).bridge(exec);
			break;
		}
		case PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS:
		{
			ret = SVGPathSegCurvetoQuadraticSmoothAbs(s).bridge(exec);
			break;
		}
		case PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL:
		{
			ret = SVGPathSegCurvetoQuadraticSmoothRel(s).bridge(exec);
			break;
		}
		default:
			ret = s.bridge(exec);
	}

	interpreter->putDOMObject(s.handle(), ret);
	return ret;
}



// vim:ts=4:noet
