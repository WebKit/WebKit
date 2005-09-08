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

#include <kdom/core/AttrImpl.h>
#include <kdebug.h>

#include "svgattrs.h"
#include "SVGHelper.h"
#include "Namespace.h"
#include "SVGDocumentImpl.h"
#include "SVGCursorElementImpl.h"
#include "SVGAnimatedLengthImpl.h"
#include "SVGAnimatedStringImpl.h"

#ifndef APPLE_CHANGES
#include "imload/imagepainter.h"
#endif

using namespace KSVG;
#ifndef APPLE_CHANGES
using namespace khtmlImLoad;
#endif

SVGCursorElementImpl::SVGCursorElementImpl(KDOM::DocumentPtr *doc, KDOM::NodeImpl::Id id, KDOM::DOMStringImpl *prefix)
: SVGElementImpl(doc, id, prefix), SVGTestsImpl(), SVGExternalResourcesRequiredImpl(), SVGURIReferenceImpl(), KDOM::CachedObjectClient()
{
    m_x = m_y = 0;
    m_cachedImage = 0;
}

SVGCursorElementImpl::~SVGCursorElementImpl()
{
    if(m_x)
        m_x->deref();
    if(m_y)
        m_y->deref();
}

SVGAnimatedLengthImpl *SVGCursorElementImpl::x() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_x, static_cast<const SVGStyledElementImpl *>(0) /* correct? */, LM_WIDTH, viewportElement());
}

SVGAnimatedLengthImpl *SVGCursorElementImpl::y() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_y, static_cast<const SVGStyledElementImpl *>(0) /* correct? */, LM_HEIGHT, viewportElement());
}

void SVGCursorElementImpl::parseAttribute(KDOM::AttributeImpl *attr)
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
        default:
        {
            if(SVGTestsImpl::parseAttribute(attr)) return;
            if(SVGExternalResourcesRequiredImpl::parseAttribute(attr)) return;
            if(SVGURIReferenceImpl::parseAttribute(attr))
            {
                QString fname = KDOM::DOMString(href()->baseVal()).string();
                KURL fullUrl(ownerDocument()->documentKURI(), fname);
                //kdDebug() << "Loading : " << fullUrl << endl;
                m_cachedImage = ownerDocument()->docLoader()->requestImage(fullUrl);

                if(m_cachedImage)
                    m_cachedImage->ref(this);
                return;
            }

            SVGElementImpl::parseAttribute(attr);
        }
    };
}

void SVGCursorElementImpl::notifyFinished(KDOM::CachedObject *finishedObj)
{
    if(finishedObj == m_cachedImage)
    {
#ifndef APPLE_COMPILE_HACK
        QPixmap pixm(m_cachedImage->pixmap_size());
        QPainter p(&pixm);
        ImagePainter ip(m_cachedImage->image());
        ip.paint(0, 0, &p, 0, 0, m_cachedImage->pixmap_size().width(), m_cachedImage->pixmap_size().height());
        m_image = pixm;
        kdDebug() << "pixm w : " << m_image.size() << endl;
#endif // APPLE_COMPILE_HACK

        m_cachedImage->deref(this);
        m_cachedImage = 0;
    }
}

// vim:ts=4:noet
