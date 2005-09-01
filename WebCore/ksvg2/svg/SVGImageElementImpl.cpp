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

#include <kmimetype.h>

#include <qimage.h>
#include <qbuffer.h>

#include <kdom/core/AttrImpl.h>
#include <kdom/cache/KDOMLoader.h>
#include <kdom/cache/KDOMCachedObject.h>

#include "svgattrs.h"
#include "SVGHelper.h"
#include "Namespace.h"
#include "SVGDocumentImpl.h"
#include "SVGSVGElementImpl.h"
#include "SVGImageElementImpl.h"
#include "SVGAnimatedLengthImpl.h"
#include "SVGAnimatedStringImpl.h"
#include "KCanvasRenderingStyle.h"
#include "SVGAnimatedPreserveAspectRatioImpl.h"
#include "KSVGDocumentBuilder.h"
#include <ksvg2/KSVGView.h>

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasView.h>
#include <kcanvas/KCanvasCreator.h>
#include <kcanvas/KCanvasContainer.h>
#include <kcanvas/KCanvasImage.h>
#include <kcanvas/device/KRenderingStyle.h>
#include <kcanvas/device/KRenderingDevice.h>
#include <kcanvas/device/KRenderingFillPainter.h>
#include <kcanvas/device/KRenderingPaintServerImage.h>

#include <kdom/parser/KDOMParser.h>
#ifndef APPLE_CHANGES
#include <kdom/parser/KDOMParserFactory.h>
#endif
//#include <kdom/DOMConfiguration.h>
#include <kdom/core/DOMConfigurationImpl.h>

using namespace KSVG;

SVGImageElementImpl::SVGImageElementImpl(KDOM::DocumentPtr *doc, KDOM::NodeImpl::Id id, KDOM::DOMStringImpl *prefix)
: SVGStyledElementImpl(doc, id, prefix), SVGTestsImpl(), SVGLangSpaceImpl(), SVGExternalResourcesRequiredImpl(), SVGTransformableImpl(), SVGURIReferenceImpl(), KDOM::CachedObjectClient()
{
    m_x = m_y = m_width = m_height = 0;
    m_preserveAspectRatio = 0;
    m_cachedDocument = 0;
    m_cachedImage = 0;
    m_svgDoc = 0;
}

SVGImageElementImpl::~SVGImageElementImpl()
{
    if(m_x)
        m_x->deref();
    if(m_y)
        m_y->deref();
    if(m_width)
        m_width->deref();
    if(m_height)
        m_height->deref();
    if(m_preserveAspectRatio)
        m_preserveAspectRatio->deref();
    if(m_svgDoc)
        m_svgDoc->deref();
}

SVGAnimatedLengthImpl *SVGImageElementImpl::x() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_x, this, LM_WIDTH, viewportElement());
}

SVGAnimatedLengthImpl *SVGImageElementImpl::y() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_y, this, LM_HEIGHT, viewportElement());
}

SVGAnimatedLengthImpl *SVGImageElementImpl::width() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_width, this, LM_WIDTH, viewportElement());
}

SVGAnimatedLengthImpl *SVGImageElementImpl::height() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_height, this, LM_HEIGHT, viewportElement());
}

SVGAnimatedPreserveAspectRatioImpl *SVGImageElementImpl::preserveAspectRatio() const
{
    return lazy_create<SVGAnimatedPreserveAspectRatioImpl>(m_preserveAspectRatio, this);
}

void SVGImageElementImpl::parseAttribute(KDOM::AttributeImpl *attr)
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
        case ATTR_PRESERVEASPECTRATIO:
        {
            preserveAspectRatio()->baseVal()->parsePreserveAspectRatio(value);
            break;
        }
        case ATTR_WIDTH:
        {
            width()->baseVal()->setValueAsString(value);
            break;
        }
        case ATTR_HEIGHT:
        {
            height()->baseVal()->setValueAsString(value);
            break;
        }
        default:
        {
            if(SVGTestsImpl::parseAttribute(attr)) return;
            if(SVGLangSpaceImpl::parseAttribute(attr)) return;
            if(SVGExternalResourcesRequiredImpl::parseAttribute(attr)) return;
            if(SVGTransformableImpl::parseAttribute(attr)) return;
            if(SVGURIReferenceImpl::parseAttribute(attr)) return;
            
            SVGStyledElementImpl::parseAttribute(attr);
        }
    };
}

KCanvasItem *SVGImageElementImpl::createCanvasItem(KCanvas *canvas, KRenderingStyle *style) const
{
    QString fname = KDOM::DOMString(href()->baseVal()).string();
#ifndef APPLE_COMPILE_HACK
    KURL fullUrl(ownerDocument()->documentKURI(), fname);
    KMimeType::Ptr mimeType = KMimeType::findByURL(fullUrl);
    if(mimeType->is(QString::fromLatin1("image/svg+xml"))) // does it have svg content?
    {
        KCanvasItem *ret = canvas->renderingDevice()->createContainer(canvas, style);
        m_cachedDocument = ownerDocument()->docLoader()->requestDocument(fullUrl, QString());

        if(m_cachedDocument)
            m_cachedDocument->ref(const_cast<SVGImageElementImpl *>(this));
        return ret;
    }
#endif

    float _x = x()->baseVal()->value(), _y = y()->baseVal()->value();
    float _width = width()->baseVal()->value(), _height = height()->baseVal()->value();

    // Use dummy rect
    KCPathDataList pathData = KCanvasCreator::self()->createRectangle(_x, _y, _width, _height);
    if(pathData.isEmpty())
        return 0;

    return KCanvasCreator::self()->createPathItem(canvas, style, pathData);
}


void SVGImageElementImpl::notifyFinished(KDOM::CachedObject *finishedObj)
{
    if(finishedObj == m_cachedDocument)
    {
#ifndef APPLE_COMPILE_HACK
        KSVG::DocumentBuilder *builder = new KSVG::DocumentBuilder(0);
        KDOM::Parser *parser = KDOM::ParserFactory::self()->request(KURL(), builder);
        parser->domConfig()->setParameter(KDOM::ENTITIES.handle(), false);
        parser->domConfig()->setParameter(KDOM::ELEMENT_CONTENT_WHITESPACE.handle(), false);

        QBuffer *temp = new QBuffer(m_cachedDocument->documentBuffer()->buffer());
        m_svgDoc = static_cast<SVGDocumentImpl *>(parser->syncParse(temp));
        if(m_svgDoc)
        {
            SVGSVGElementImpl *root = m_svgDoc->rootElement();
            KDOM::DOMStringImpl *_x = x()->baseVal()->valueAsString(), *_y = y()->baseVal()->valueAsString();
            KDOM::DOMStringImpl *_width = width()->baseVal()->valueAsString(), *_height = height()->baseVal()->valueAsString();

            root->setAttributeNS(KDOM::NS_SVG.handle(), KDOM::DOMString("x").handle(), _x);
            root->setAttributeNS(KDOM::NS_SVG.handle(), KDOM::DOMString("y").handle(), _y);
            root->setAttributeNS(KDOM::NS_SVG.handle(), KDOM::DOMString("width").handle(), _width);
            root->setAttributeNS(KDOM::NS_SVG.handle(), KDOM::DOMString("height").handle(), _height);

            // TODO: viewBox handling? animations? ecmascript?

            m_svgDoc->setView(getDocument()->view());
            m_svgDoc->setCanvasView(getDocument()->canvasView());

            m_svgDoc->attach();    
            m_canvasItem->appendItem(root->canvasItem());
        }
#endif
        m_cachedDocument->deref(this);
        m_cachedDocument = 0;
    }
    else if(finishedObj == m_cachedImage)
    {
        if(m_canvasItem && m_canvasItem->style())
        {    
#ifndef APPLE_COMPILE_HACK
            KRenderingFillPainter *fillPainter = m_canvasItem->style()->fillPainter();
            if(!fillPainter)


            KRenderingPaintServer *fillPaintServer = fillPainter->paintServer();
            KRenderingPaintServerImage *fillPaintServerImage = static_cast<KRenderingPaintServerImage *>(fillPaintServer);

            KCanvasImageBuffer *imageBuffer = new KCanvasImageBuffer(m_cachedImage->pixmap().convertToImage());
            fillPaintServerImage->setImage(imageBuffer);
#endif

            m_canvasItem->invalidate();
        }

        m_cachedImage->deref(this);
        m_cachedImage = 0;
    }
}

void SVGImageElementImpl::finalizeStyle(KCanvasRenderingStyle *style, bool /* needFillStrokeUpdate */)
{
    SVGStyledElementImpl *parent = dynamic_cast<SVGStyledElementImpl *>(parentNode());
    if(parent && !parent->allowAttachChildren(this))
        return;

    // Set up image paint server
    style->disableFillPainter();
    style->disableStrokePainter();

    KRenderingPaintServer *fillPaintServer = m_canvasItem->canvas()->renderingDevice()->createPaintServer(KCPaintServerType(PS_IMAGE));
    style->fillPainter()->setPaintServer(fillPaintServer);

    if(!m_cachedDocument) // bitmap
    {
        QString fname = KDOM::DOMString(href()->baseVal()).string();
        KURL fullUrl(ownerDocument()->documentKURI(), fname);
        m_cachedImage = ownerDocument()->docLoader()->requestImage(fullUrl);

        if(m_cachedImage)
            m_cachedImage->ref(this);
    }
}

// vim:ts=4:noet
