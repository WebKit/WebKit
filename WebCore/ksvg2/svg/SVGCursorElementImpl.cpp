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
#include <kdom/core/DocumentImpl.h>
#include <kdebug.h>

#include "SVGNames.h"
#include "SVGHelper.h"
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

SVGCursorElementImpl::SVGCursorElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc)
: SVGElementImpl(tagName, doc), SVGTestsImpl(), SVGExternalResourcesRequiredImpl(), SVGURIReferenceImpl(), KDOM::CachedObjectClient()
{
    m_cachedImage = 0;
}

SVGCursorElementImpl::~SVGCursorElementImpl()
{
}

SVGAnimatedLengthImpl *SVGCursorElementImpl::x() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_x, static_cast<const SVGStyledElementImpl *>(0) /* correct? */, LM_WIDTH, viewportElement());
}

SVGAnimatedLengthImpl *SVGCursorElementImpl::y() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_y, static_cast<const SVGStyledElementImpl *>(0) /* correct? */, LM_HEIGHT, viewportElement());
}

void SVGCursorElementImpl::parseMappedAttribute(KDOM::MappedAttributeImpl *attr)
{
     const KDOM::AtomicString& value = attr->value();
    if (attr->name() == SVGNames::xAttr)
        x()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::yAttr)
        y()->baseVal()->setValueAsString(value.impl());
    else
    {
        if(SVGTestsImpl::parseMappedAttribute(attr)) return;
        if(SVGExternalResourcesRequiredImpl::parseMappedAttribute(attr)) return;
        if(SVGURIReferenceImpl::parseMappedAttribute(attr))
        {
            m_cachedImage = ownerDocument()->docLoader()->requestImage(href()->baseVal());

            if(m_cachedImage)
                m_cachedImage->ref(this);
            return;
        }

        SVGElementImpl::parseMappedAttribute(attr);
    }
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
