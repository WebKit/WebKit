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

#include <kdom/DOMString.h>
#include <kdom/core/AttrImpl.h>
#include <kdom/core/NamedAttrMapImpl.h>

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasItem.h>
#include <kcanvas/KCanvasView.h>
#include <kcanvas/KCanvasMatrix.h>
#include <kcanvas/KCanvasCreator.h>
#include <kcanvas/KCanvasRegistry.h>
#include <kcanvas/KCanvasContainer.h>
#include <kcanvas/device/KRenderingDevice.h>

#include "ksvg.h"
#include "svgattrs.h"
#include "SVGHelper.h"
#include "SVGRectImpl.h"
#include "SVGAngleImpl.h"
#include "SVGPointImpl.h"
#include "SVGEventImpl.h"
#include "SVGMatrixImpl.h"
#include "SVGNumberImpl.h"
#include "SVGLengthImpl.h"
#include "SVGRenderStyle.h"
#include "SVGDocumentImpl.h"
#include "SVGZoomEventImpl.h"
#include "SVGTransformImpl.h"
#include "SVGSVGElementImpl.h"
#include "KSVGTimeScheduler.h"
#include "SVGZoomAndPanImpl.h"
#include "SVGFitToViewBoxImpl.h"
#include "SVGAnimatedRectImpl.h"
#include "SVGAnimatedLengthImpl.h"
#include "KCanvasRenderingStyle.h"

#include <q3paintdevicemetrics.h>

using namespace KSVG;

SVGSVGElementImpl::SVGSVGElementImpl(KDOM::DocumentPtr *doc, KDOM::NodeImpl::Id id, KDOM::DOMStringImpl *prefix)
: SVGStyledElementImpl(doc, id, prefix), SVGTestsImpl(), SVGLangSpaceImpl(),
  SVGExternalResourcesRequiredImpl(), SVGLocatableImpl(), SVGFitToViewBoxImpl(),
  SVGZoomAndPanImpl(), KDOM::DocumentEventImpl()
{
    m_clipper = 0;

    m_useCurrentView = false;

    m_x = m_y = m_width = m_height = 0;
}

SVGSVGElementImpl::~SVGSVGElementImpl()
{
    if(m_x)
        m_x->deref();
    if(m_y)
        m_y->deref();
    if(m_width)
        m_width->deref();
    if(m_height)
        m_height->deref();
}

SVGAnimatedLengthImpl *SVGSVGElementImpl::x() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_x, static_cast<const SVGStyledElementImpl *>(0), LM_WIDTH, this);
}

SVGAnimatedLengthImpl *SVGSVGElementImpl::y() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_y, static_cast<const SVGStyledElementImpl *>(0), LM_HEIGHT, this);
}

SVGAnimatedLengthImpl *SVGSVGElementImpl::width() const
{
    if(!m_width)
    {
        lazy_create<SVGAnimatedLengthImpl>(m_width, static_cast<const SVGStyledElementImpl *>(0), LM_WIDTH, this);
        m_width->baseVal()->setValueAsString(KDOM::DOMString("100%").handle());
    }

    return m_width;
}

SVGAnimatedLengthImpl *SVGSVGElementImpl::height() const
{
    if(!m_height)
    {
        lazy_create<SVGAnimatedLengthImpl>(m_height, static_cast<const SVGStyledElementImpl *>(0), LM_HEIGHT, this);
        m_height->baseVal()->setValueAsString(KDOM::DOMString("100%").handle());
    }

    return m_height;
}

KDOM::DOMStringImpl *SVGSVGElementImpl::contentScriptType() const
{
    KDOM::DOMString name("contentScriptType");
    if(hasAttribute(name.handle()))
        return getAttribute(name.handle());

    return new KDOM::DOMStringImpl("text/ecmascript");
}

void SVGSVGElementImpl::setContentScriptType(KDOM::DOMStringImpl *type)
{
    KDOM::DOMString name("contentScriptType");    
    setAttribute(name.handle(), type);
}

KDOM::DOMStringImpl *SVGSVGElementImpl::contentStyleType() const
{
    KDOM::DOMString name("contentStyleType");
    if(hasAttribute(name.handle()))
        return getAttribute(name.handle());

    return new KDOM::DOMStringImpl("text/css");
}

void SVGSVGElementImpl::setContentStyleType(KDOM::DOMStringImpl *type)
{
    KDOM::DOMString name("contentStyleType");    
    setAttribute(name.handle(), type);
}

SVGRectImpl *SVGSVGElementImpl::viewport() const
{
    SVGRectImpl *ret = createSVGRect();
    double _x = x()->baseVal()->value();
    double _y = y()->baseVal()->value();
    double w = width()->baseVal()->value();
    double h = height()->baseVal()->value();
    SVGMatrixImpl *viewBox = viewBoxToViewTransform(w, h);
    viewBox->ref();
    viewBox->qmatrix().map(_x, _y, &_x, &_y);
    viewBox->qmatrix().map(w, h, &w, &h);
    viewBox->deref();
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
    if(!canvasView())
        return 1.;

    return canvasView()->zoom();
}

void SVGSVGElementImpl::setCurrentScale(float scale)
{
    if(canvasView())
    {
        float oldzoom = canvasView()->zoom();
        canvasView()->setZoom(scale);
        getDocument()->dispatchZoomEvent(oldzoom, scale);
    }
}

SVGPointImpl *SVGSVGElementImpl::currentTranslate() const
{
    if(!canvas())
        return 0;

    return createSVGPoint(canvasView()->pan());
}

KDOM::EventImpl *SVGSVGElementImpl::createEvent(const KDOM::DOMString &eventType)
{
    if(eventType == "SVGEvents")
        return new SVGEventImpl();
    else if(eventType == "SVGZoomEvents")
        return new SVGZoomEventImpl();

    return DocumentEventImpl::createEvent(eventType.handle());
}

void SVGSVGElementImpl::parseAttribute(KDOM::AttributeImpl *attr)
{
    int id = (attr->id() & NodeImpl_IdLocalMask);
    KDOM::DOMStringImpl *value = attr->value();
    switch(id)
    {
        case ATTR_X:
        {
            x()->baseVal()->setValueAsString(value);
            break;
        }
        case ATTR_Y:
        {
            y()->baseVal()->setValueAsString(value);
            break;
        }
        case ATTR_WIDTH:
        case ATTR_HEIGHT:
        {
            if(id == ATTR_WIDTH)
                width()->baseVal()->setValueAsString(value);
            else
                height()->baseVal()->setValueAsString(value);

            SVGDocumentImpl *document = static_cast<SVGDocumentImpl *>(ownerDocument());
            if(document->canvas() && canvasItem() == document->canvas()->rootContainer())
            {
                document->canvas()->setCanvasSize(QSize(qRound(width()->baseVal()->value()),
                                                        qRound(height()->baseVal()->value())));
            }

            break;
        }
        default:
        {
            if(SVGTestsImpl::parseAttribute(attr)) return;
            if(SVGLangSpaceImpl::parseAttribute(attr)) return;
            if(SVGExternalResourcesRequiredImpl::parseAttribute(attr)) return;
            if(SVGFitToViewBoxImpl::parseAttribute(attr)) return;
            if(SVGZoomAndPanImpl::parseAttribute(attr)) return;

            SVGStyledElementImpl::parseAttribute(attr);
        }
    };
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
    if(!getDocument()->timeScheduler()->animationsPaused())
        getDocument()->timeScheduler()->toggleAnimations();
}

void SVGSVGElementImpl::unpauseAnimations()
{
    if(getDocument()->timeScheduler()->animationsPaused())
        getDocument()->timeScheduler()->toggleAnimations();
}

bool SVGSVGElementImpl::animationsPaused()
{
    return getDocument()->timeScheduler()->animationsPaused();
}

float SVGSVGElementImpl::getCurrentTime()
{
    SVGDocumentImpl *document = static_cast<SVGDocumentImpl *>(ownerDocument());
    return document->timeScheduler()->elapsed();
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
    SVGLocatableImpl *e = dynamic_cast<SVGLocatableImpl *>(element);
    if(!e)
        return false;

    SVGRectImpl *bbox = e->getBBox();
    bbox->ref();

    QRect r(int(rect->x()), int(rect->y()), int(rect->width()), int(rect->height()));
    QRect r2(int(bbox->x()), int(bbox->y()), int(bbox->width()), int(bbox->height()));

    bbox->deref();
    return r.intersects(r2);
}

bool SVGSVGElementImpl::checkEnclosure(SVGElementImpl *element, SVGRectImpl *rect)
{
    // TODO : take into account pointer-events?
    SVGLocatableImpl *e = dynamic_cast<SVGLocatableImpl *>(element);
    if(!e)
        return false;
    
    SVGRectImpl *bbox = e->getBBox();
    bbox->ref();
        
    QRect r(int(rect->x()), int(rect->y()), int(rect->width()), int(rect->height()));
    QRect r2(int(bbox->x()), int(bbox->y()), int(bbox->width()), int(bbox->height()));
        
    bbox->deref();
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

        if(attributes()->getValue(ATTR_VIEWBOX))
        {
            SVGMatrixImpl *viewBox = viewBoxToViewTransform(width()->baseVal()->value(), height()->baseVal()->value());
            viewBox->ref();
            mat->multiply(viewBox);
            viewBox->deref();
        }
    }

    return mat;
}

SVGMatrixImpl *SVGSVGElementImpl::getScreenCTM() const
{
    SVGMatrixImpl *mat = SVGLocatableImpl::getScreenCTM();
    if(mat)
    {
        mat->translate(x()->baseVal()->value(), y()->baseVal()->value());

        if(attributes()->getValue(ATTR_VIEWBOX))
        {
            SVGMatrixImpl *viewBox = viewBoxToViewTransform(width()->baseVal()->value(), height()->baseVal()->value());
            viewBox->ref();
            mat->multiply(viewBox);
            viewBox->deref();
        }
    }

    return mat;
}

QString SVGSVGElementImpl::adjustViewportClipping() const
{
    QString key;
    QTextStream keyStream(&key, IO_WriteOnly);
    keyStream << ((void *) this);

    if(renderStyle()->overflow() == KDOM::OF_VISIBLE)
        return QString();

    if(!m_clipper)
    {
        SVGDocumentImpl *doc = static_cast<SVGDocumentImpl *>(ownerDocument());
        KCanvas *canvas = (doc ? doc->canvas() : 0);
        if(!canvas)
            return QString::null;

        m_clipper = static_cast<KCanvasClipper *>(canvas->renderingDevice()->createResource(RS_CLIPPER));
        m_clipper->setViewportClipper(true);

        canvas->registry()->addResourceById(key, m_clipper);
    }
    else
        m_clipper->resetClipData();

    float _x = x()->baseVal()->value();
    float _y = y()->baseVal()->value();
    float _width = width()->baseVal()->value();
    float _height = height()->baseVal()->value();

    // We have to apply the matrix transformation here, as this <svg> may be some
    // inner viewport, which has a certain 'ctm matrix', that we have to respect.
    SVGMatrixImpl *ctm = SVGLocatableImpl::getScreenCTM();

    KCanvasMatrix matrix = ctm->qmatrix();
    KCPathDataList pathData = matrix.map(KCanvasCreator::self()->createRectangle(_x, _y, _width, _height));
    m_clipper->addClipData(pathData, RULE_NONZERO, false);

    ctm->deref();

    return key;
}

KCanvasItem *SVGSVGElementImpl::createCanvasItem(KCanvas *canvas, KRenderingStyle *style) const
{
    KCanvasItem *item = KCanvasCreator::self()->createContainer(canvas, style);
    if(ownerDocument()->documentElement() == static_cast<const KDOM::ElementImpl *>(this) && !canvas->rootContainer())
    {
        canvas->setRootContainer(static_cast<KCanvasContainer *>(item));
        canvas->setCanvasSize(QSize(qRound(width()->baseVal()->value()),
                                qRound(height()->baseVal()->value())));
    }

    return item;
}

void SVGSVGElementImpl::finalizeStyle(KCanvasRenderingStyle *style, bool)
{
    QString reference = adjustViewportClipping();
    if(!reference.isEmpty())
        style->addClipPath(QString::fromLatin1("#") + reference);
}

void SVGSVGElementImpl::setZoomAndPan(unsigned short zoomAndPan)
{
    SVGZoomAndPanImpl::setZoomAndPan(zoomAndPan);
    canvasView()->enableZoomAndPan(zoomAndPan == SVG_ZOOMANDPAN_MAGNIFY);
}

// vim:ts=4:noet
