/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

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

#include "config.h"
#include <kdom/core/AttrImpl.h>
#include <kdom/core/DOMStringImpl.h>
#include <kdom/core/NamedAttrMapImpl.h>

#include <kcanvas/KCanvas.h>
#include <kcanvas/RenderPath.h>
#include <kcanvas/KCanvasMatrix.h>
#include <kcanvas/KCanvasCreator.h>
#include <kcanvas/KCanvasContainer.h>
#include <kcanvas/device/KRenderingDevice.h>

#include "ksvg.h"
#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGRectImpl.h"
#include "SVGAngleImpl.h"
#include "SVGPointImpl.h"
#include "SVGMatrixImpl.h"
#include "SVGNumberImpl.h"
#include "SVGLengthImpl.h"
#include "SVGRenderStyle.h"
#include "SVGZoomEventImpl.h"
#include "SVGTransformImpl.h"
#include "SVGSVGElementImpl.h"
#include "KSVGTimeScheduler.h"
#include "SVGZoomAndPanImpl.h"
#include "SVGFitToViewBoxImpl.h"
#include "SVGAnimatedRectImpl.h"
#include "SVGAnimatedLengthImpl.h"
#include "KCanvasRenderingStyle.h"
#include "SVGPreserveAspectRatioImpl.h"
#include "SVGAnimatedPreserveAspectRatioImpl.h"

#include "cssproperties.h"

#include <q3paintdevicemetrics.h>
#include <qtextstream.h>

#include "HTMLNames.h"
#include "EventNames.h"

using namespace KSVG;
using namespace DOM::HTMLNames;
using namespace DOM::EventNames;

SVGSVGElementImpl::SVGSVGElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc)
: SVGStyledLocatableElementImpl(tagName, doc), SVGTestsImpl(), SVGLangSpaceImpl(),
  SVGExternalResourcesRequiredImpl(), SVGFitToViewBoxImpl(),
  SVGZoomAndPanImpl()
{
    m_useCurrentView = false;
    m_timeScheduler = new TimeScheduler(getDocument());
}

SVGSVGElementImpl::~SVGSVGElementImpl()
{
    delete m_timeScheduler;
}

SVGAnimatedLengthImpl *SVGSVGElementImpl::x() const
{
    const SVGElementImpl *viewport = ownerDocument()->documentElement() == this ? this : viewportElement();
    return lazy_create<SVGAnimatedLengthImpl>(m_x, (SVGStyledElementImpl *)0, LM_WIDTH, viewport);
}

SVGAnimatedLengthImpl *SVGSVGElementImpl::y() const
{
    const SVGElementImpl *viewport = ownerDocument()->documentElement() == this ? this : viewportElement();
    return lazy_create<SVGAnimatedLengthImpl>(m_y, (SVGStyledElementImpl *)0, LM_HEIGHT, viewport);
}

SVGAnimatedLengthImpl *SVGSVGElementImpl::width() const
{
    if (!m_width) {
        KDOM::DOMString temp("100%");
        const SVGElementImpl *viewport = ownerDocument()->documentElement() == this ? this : viewportElement();
        lazy_create<SVGAnimatedLengthImpl>(m_width, (SVGStyledElementImpl *)0, LM_WIDTH, viewport);
        m_width->baseVal()->setValueAsString(temp.impl());
    }

    return m_width.get();
}

SVGAnimatedLengthImpl *SVGSVGElementImpl::height() const
{
    if (!m_height) {
        KDOM::DOMString temp("100%");
        const SVGElementImpl *viewport = ownerDocument()->documentElement() == this ? this : viewportElement();
        lazy_create<SVGAnimatedLengthImpl>(m_height, (SVGStyledElementImpl *)0, LM_HEIGHT, viewport);
        m_height->baseVal()->setValueAsString(temp.impl());
    }

    return m_height.get();
}

KDOM::AtomicString SVGSVGElementImpl::contentScriptType() const
{
    return tryGetAttribute("contentScriptType", "text/ecmascript");
}

void SVGSVGElementImpl::setContentScriptType(const KDOM::AtomicString& type)
{
    setAttribute(SVGNames::contentScriptTypeAttr, type);
}

KDOM::AtomicString SVGSVGElementImpl::contentStyleType() const
{
    return tryGetAttribute("contentStyleType", "text/css");
}

void SVGSVGElementImpl::setContentStyleType(const KDOM::AtomicString& type)
{
    setAttribute(SVGNames::contentStyleTypeAttr, type);
}

SVGRectImpl *SVGSVGElementImpl::viewport() const
{
    SVGRectImpl *ret = createSVGRect();
    double _x = x()->baseVal()->value();
    double _y = y()->baseVal()->value();
    double w = width()->baseVal()->value();
    double h = height()->baseVal()->value();
    SharedPtr<SVGMatrixImpl> viewBox = viewBoxToViewTransform(w, h);
    viewBox->qmatrix().map(_x, _y, &_x, &_y);
    viewBox->qmatrix().map(w, h, &w, &h);
    ret->setX(_x);
    ret->setY(_y);
    ret->setWidth(w);
    ret->setHeight(h);
    return ret;
}

float SVGSVGElementImpl::pixelUnitToMillimeterX() const
{
#ifndef APPLE_COMPILE_HACK
    if(ownerDocument() && ownerDocument()->paintDeviceMetrics())
    {
        Q3PaintDeviceMetrics *metrics = ownerDocument()->paintDeviceMetrics();
        return float(metrics->widthMM()) / float(metrics->width());
    }
#endif

    return .28;
}

float SVGSVGElementImpl::pixelUnitToMillimeterY() const
{
#ifndef APPLE_COMPILE_HACK
    if(ownerDocument() && ownerDocument()->paintDeviceMetrics())
    {
        Q3PaintDeviceMetrics *metrics = ownerDocument()->paintDeviceMetrics();
        return float(metrics->heightMM()) / float(metrics->height());
    }
#endif

    return .28;
}

float SVGSVGElementImpl::screenPixelToMillimeterX() const
{
    return pixelUnitToMillimeterX();
}

float SVGSVGElementImpl::screenPixelToMillimeterY() const
{
    return pixelUnitToMillimeterY();
}

bool SVGSVGElementImpl::useCurrentView() const
{
    return m_useCurrentView;
}

void SVGSVGElementImpl::setUseCurrentView(bool currentView)
{
    m_useCurrentView = currentView;
}

/*
SVGViewSpecImpl *SVGSVGElementImpl::currentView() const
{
    return 0;
}
*/

float SVGSVGElementImpl::currentScale() const
{
    //if(!canvasView())
        return 1.;

    //return canvasView()->zoom();
}

void SVGSVGElementImpl::setCurrentScale(float scale)
{
//    if(canvasView())
//    {
//        float oldzoom = canvasView()->zoom();
//        canvasView()->setZoom(scale);
//        getDocument()->dispatchZoomEvent(oldzoom, scale);
//    }
}

SVGPointImpl *SVGSVGElementImpl::currentTranslate() const
{
    //if(!canvas())
        return 0;

    //return createSVGPoint(canvasView()->pan());
}

void SVGSVGElementImpl::parseMappedAttribute(KDOM::MappedAttributeImpl *attr)
{
    const KDOM::AtomicString& value = attr->value();
    if (!nearestViewportElement()) {
        // Only handle events if we're the outermost <svg> element
        QString value = attr->value().qstring();
        if (attr->name() == onloadAttr)
            getDocument()->setHTMLWindowEventListener(loadEvent, getDocument()->createHTMLEventListener(value, this));
        else if (attr->name() == onunloadAttr)
            getDocument()->setHTMLWindowEventListener(unloadEvent, getDocument()->createHTMLEventListener(value, this));
        else if (attr->name() == onabortAttr)
            getDocument()->setHTMLWindowEventListener(abortEvent, getDocument()->createHTMLEventListener(value, this));
        else if (attr->name() == onerrorAttr)
            getDocument()->setHTMLWindowEventListener(errorEvent, getDocument()->createHTMLEventListener(value, this));
        else if (attr->name() == onresizeAttr)
            getDocument()->setHTMLWindowEventListener(resizeEvent, getDocument()->createHTMLEventListener(value, this));
        else if (attr->name() == onscrollAttr)
            getDocument()->setHTMLWindowEventListener(scrollEvent, getDocument()->createHTMLEventListener(value, this));
        else if (attr->name() == SVGNames::onzoomAttr)
            getDocument()->setHTMLWindowEventListener(zoomEvent, getDocument()->createHTMLEventListener(value, this));
    }
    if (attr->name() == SVGNames::xAttr) {
        x()->baseVal()->setValueAsString(value.impl());
    } else if (attr->name() == SVGNames::yAttr) {
        y()->baseVal()->setValueAsString(value.impl());
    } else if (attr->name() == SVGNames::widthAttr) {
        width()->baseVal()->setValueAsString(value.impl());
        addCSSProperty(attr, CSS_PROP_WIDTH, value);
    } else if (attr->name() == SVGNames::heightAttr) {
        height()->baseVal()->setValueAsString(value.impl());
        addCSSProperty(attr, CSS_PROP_HEIGHT, value);
    } else
    {
        if(SVGTestsImpl::parseMappedAttribute(attr)) return;
        if(SVGLangSpaceImpl::parseMappedAttribute(attr)) return;
        if(SVGExternalResourcesRequiredImpl::parseMappedAttribute(attr)) return;
        if(SVGFitToViewBoxImpl::parseMappedAttribute(attr)) return;
        if(SVGZoomAndPanImpl::parseMappedAttribute(attr)) return;

        SVGStyledLocatableElementImpl::parseMappedAttribute(attr);
    }
}

unsigned long SVGSVGElementImpl::suspendRedraw(unsigned long /* max_wait_milliseconds */)
{
    // TODO
    return 0;
}

void SVGSVGElementImpl::unsuspendRedraw(unsigned long /* suspend_handle_id */)
{
    // TODO
}

void SVGSVGElementImpl::unsuspendRedrawAll()
{
    // TODO
}

void SVGSVGElementImpl::forceRedraw()
{
    // TODO
}

void SVGSVGElementImpl::pauseAnimations()
{
    if(!m_timeScheduler->animationsPaused())
        m_timeScheduler->toggleAnimations();
}

void SVGSVGElementImpl::unpauseAnimations()
{
    if(m_timeScheduler->animationsPaused())
        m_timeScheduler->toggleAnimations();
}

bool SVGSVGElementImpl::animationsPaused()
{
    return m_timeScheduler->animationsPaused();
}

float SVGSVGElementImpl::getCurrentTime()
{
    return m_timeScheduler->elapsed();
}

void SVGSVGElementImpl::setCurrentTime(float /* seconds */)
{
    // TODO
}

KDOM::NodeListImpl *SVGSVGElementImpl::getIntersectionList(SVGRectImpl *rect, SVGElementImpl *)
{
    //KDOM::NodeListImpl *list;
    // TODO
    return 0;
}

KDOM::NodeListImpl *SVGSVGElementImpl::getEnclosureList(SVGRectImpl *rect, SVGElementImpl *)
{
    // TODO
    return 0;
}

bool SVGSVGElementImpl::checkIntersection(SVGElementImpl *element, SVGRectImpl *rect)
{
    // TODO : take into account pointer-events?
    SharedPtr<SVGRectImpl> bbox = getBBox();
    
    QRect r(int(rect->x()), int(rect->y()), int(rect->width()), int(rect->height()));
    QRect r2(int(bbox->x()), int(bbox->y()), int(bbox->width()), int(bbox->height()));
    
    return r.intersects(r2);
}

bool SVGSVGElementImpl::checkEnclosure(SVGElementImpl *element, SVGRectImpl *rect)
{
    // TODO : take into account pointer-events?
    SharedPtr<SVGRectImpl> bbox = getBBox();
    
    QRect r(int(rect->x()), int(rect->y()), int(rect->width()), int(rect->height()));
    QRect r2(int(bbox->x()), int(bbox->y()), int(bbox->width()), int(bbox->height()));
    
    return r.contains(r2);
}

void SVGSVGElementImpl::deselectAll()
{
    // TODO
}

SVGNumberImpl *SVGSVGElementImpl::createSVGNumber()
{
    return new SVGNumberImpl(0);
}

SVGLengthImpl *SVGSVGElementImpl::createSVGLength()
{
    return new SVGLengthImpl(0);
}

SVGAngleImpl *SVGSVGElementImpl::createSVGAngle()
{
    return new SVGAngleImpl(0);
}

SVGPointImpl *SVGSVGElementImpl::createSVGPoint(const QPoint &p)
{
    if(p.isNull())
        return new SVGPointImpl();
    else
        return new SVGPointImpl(p);
}

SVGMatrixImpl *SVGSVGElementImpl::createSVGMatrix()
{
    return new SVGMatrixImpl();
}

SVGRectImpl *SVGSVGElementImpl::createSVGRect()
{
    return new SVGRectImpl(0);
}

SVGTransformImpl *SVGSVGElementImpl::createSVGTransform()
{
    return new SVGTransformImpl();
}

SVGTransformImpl *SVGSVGElementImpl::createSVGTransformFromMatrix(SVGMatrixImpl *matrix)
{    
    SVGTransformImpl *obj = SVGSVGElementImpl::createSVGTransform();
    obj->setMatrix(matrix);
    return obj;
}

SVGMatrixImpl *SVGSVGElementImpl::getCTM() const
{
    SVGMatrixImpl *mat = createSVGMatrix();
    if(mat)
    {
        mat->translate(x()->baseVal()->value(), y()->baseVal()->value());

        if(attributes()->getNamedItem(SVGNames::viewBoxAttr))
        {
            SharedPtr<SVGMatrixImpl> viewBox = viewBoxToViewTransform(width()->baseVal()->value(), height()->baseVal()->value());
            mat->multiply(viewBox.get());
        }
    }

    return mat;
}

SVGMatrixImpl *SVGSVGElementImpl::getScreenCTM() const
{
    SVGMatrixImpl *mat = SVGStyledLocatableElementImpl::getScreenCTM();
    if(mat)
    {
        mat->translate(x()->baseVal()->value(), y()->baseVal()->value());

        if(attributes()->getNamedItem(SVGNames::viewBoxAttr))
        {
            SharedPtr<SVGMatrixImpl> viewBox = viewBoxToViewTransform(width()->baseVal()->value(), height()->baseVal()->value());
            mat->multiply(viewBox.get());
        }
    }

    return mat;
}

khtml::RenderObject *SVGSVGElementImpl::createRenderer(RenderArena *arena, khtml::RenderStyle *style)
{
    KCanvasContainer *rootContainer = canvas()->renderingDevice()->createContainer(arena, style, this);

    // FIXME: all this setup should be done after attributesChanged, not here.
    float _x = x()->baseVal()->value();
    float _y = y()->baseVal()->value();
    float _width = width()->baseVal()->value();
    float _height = height()->baseVal()->value();

    rootContainer->setViewport(QRect((int)_x, (int)_y, (int)_width, (int)_height));
    rootContainer->setViewBox(QRect((int)viewBox()->baseVal()->x(), (int)viewBox()->baseVal()->y(), (int)viewBox()->baseVal()->width(), (int)viewBox()->baseVal()->height()));
    rootContainer->setAlign(KCAlign(preserveAspectRatio()->baseVal()->align() - 1));
    rootContainer->setSlice(preserveAspectRatio()->baseVal()->meetOrSlice() == SVG_MEETORSLICE_SLICE);
    
    return rootContainer;
}

void SVGSVGElementImpl::setZoomAndPan(unsigned short zoomAndPan)
{
    SVGZoomAndPanImpl::setZoomAndPan(zoomAndPan);
    //canvasView()->enableZoomAndPan(zoomAndPan == SVG_ZOOMANDPAN_MAGNIFY);
}

// vim:ts=4:noet
