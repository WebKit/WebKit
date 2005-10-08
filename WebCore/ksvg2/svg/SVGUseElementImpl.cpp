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
#include <kdom/Namespace.h>
#include <kdom/core/AttrImpl.h>

#include "ksvg.h"
#include "svgattrs.h"
#include "SVGHelper.h"
#include "SVGDocumentImpl.h"
#include "SVGGElementImpl.h"
#include "SVGSVGElementImpl.h"
#include "SVGUseElementImpl.h"
#include "SVGSymbolElementImpl.h"
#include "SVGAnimatedLengthImpl.h"
#include "SVGAnimatedStringImpl.h"
#include "KCanvasRenderingStyle.h"
#include "SVGAnimatedPreserveAspectRatioImpl.h"

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasCreator.h>
#include <kcanvas/KCanvasContainer.h>
#include <kcanvas/device/KRenderingStyle.h>
#include <kcanvas/device/KRenderingDevice.h>

using namespace KSVG;

SVGUseElementImpl::SVGUseElementImpl(KDOM::DocumentPtr *doc, KDOM::NodeImpl::Id id, KDOM::DOMStringImpl *prefix)
: SVGStyledElementImpl(doc, id, prefix), SVGTestsImpl(), SVGLangSpaceImpl(), SVGExternalResourcesRequiredImpl(), SVGTransformableImpl(), SVGURIReferenceImpl()
{
    m_x = m_y = m_width = m_height = 0;
}

SVGUseElementImpl::~SVGUseElementImpl()
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

SVGAnimatedLengthImpl *SVGUseElementImpl::x() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_x, this, LM_WIDTH, viewportElement());
}

SVGAnimatedLengthImpl *SVGUseElementImpl::y() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_y, this, LM_HEIGHT, viewportElement());
}

SVGAnimatedLengthImpl *SVGUseElementImpl::width() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_width, this, LM_WIDTH, viewportElement());
}

SVGAnimatedLengthImpl *SVGUseElementImpl::height() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_height, this, LM_HEIGHT, viewportElement());
}

void SVGUseElementImpl::parseAttribute(KDOM::AttributeImpl *attr)
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

void SVGUseElementImpl::close()
{
    QString ref = KDOM::DOMString(href()->baseVal()).string();
    KDOM::DOMString targetId = SVGURIReferenceImpl::getTarget(ref);
    KDOM::ElementImpl *targetElement = ownerDocument()->getElementById(targetId.handle());
    SVGElementImpl *target = svg_dynamic_cast(targetElement);
    if (!target)
    {
        getDocument()->addForwardReference(this);
        return;
    }

    float _x = x()->baseVal()->value(), _y = y()->baseVal()->value();
    float _w = width()->baseVal()->value(), _h = height()->baseVal()->value();
    
    QString w = QString::number(_w);
    QString h = QString::number(_h);
    
    QString trans = QString::fromLatin1("translate(%1, %2)").arg(_x).arg(_y);    
    if(target->id() == ID_SYMBOL)
    {
        SVGElementImpl *dummy = new SVGSVGElementImpl(docPtr(), ID_SVG, 0);
        dummy->setAttributeNS(KDOM::NS_SVG.handle(), KDOM::DOMString("width").handle(), KDOM::DOMString(w).handle());
        dummy->setAttributeNS(KDOM::NS_SVG.handle(), KDOM::DOMString("height").handle(), KDOM::DOMString(h).handle());
        
        SVGSymbolElementImpl *symbol = static_cast<SVGSymbolElementImpl *>(target);
        dummy->setAttributeNS(KDOM::NS_SVG.handle(), KDOM::DOMString("viewBox").handle(), symbol->getAttributeNS(KDOM::NS_SVG.handle(), KDOM::DOMString("viewBox").handle()));
        target->cloneChildNodes(dummy, docPtr());

        SVGElementImpl *dummy2 = new SVGDummyElementImpl(docPtr(), ID_G, 0);
        dummy2->setAttributeNS(KDOM::NS_SVG.handle(), KDOM::DOMString("transform").handle(), KDOM::DOMString(trans).handle());
        
        appendChild(dummy2);        
        dummy2->appendChild(dummy);
    }
    else if(target->id() == ID_SVG)
    {
        SVGDummyElementImpl *dummy = new SVGDummyElementImpl(docPtr(), ID_G, 0);
        dummy->setAttributeNS(KDOM::NS_SVG.handle(), KDOM::DOMString("transform").handle(), KDOM::DOMString(trans).handle());
        
        SVGElementImpl *root = static_cast<SVGElementImpl *>(target->cloneNode(true, docPtr()));
        if(hasAttribute(KDOM::DOMString("width").handle()))
            root->setAttributeNS(KDOM::NS_SVG.handle(), KDOM::DOMString("width").handle(), KDOM::DOMString(w).handle());
            
        if(hasAttribute(KDOM::DOMString("height").handle()))
            root->setAttributeNS(KDOM::NS_SVG.handle(), KDOM::DOMString("height").handle(), KDOM::DOMString(h).handle());
            
        appendChild(dummy);
        dummy->appendChild(root);
    }
    else
    {
        SVGDummyElementImpl *dummy = new SVGDummyElementImpl(docPtr(), ID_G, 0);
        dummy->setAttributeNS(KDOM::NS_SVG.handle(), KDOM::DOMString("transform").handle(), KDOM::DOMString(trans).handle());
        
        KDOM::NodeImpl *root = target->cloneNode(true, docPtr());
        
        appendChild(dummy);
        dummy->appendChild(root);
    }

    SVGElementImpl::close();
}

bool SVGUseElementImpl::hasChildNodes() const
{
    return false;
}

KCanvasItem *SVGUseElementImpl::createCanvasItem(KCanvas *canvas, KRenderingStyle *style) const
{
    return KCanvasCreator::self()->createContainer(canvas, style);
}

// vim:ts=4:noet
