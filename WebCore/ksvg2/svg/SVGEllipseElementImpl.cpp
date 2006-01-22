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
#if SVG_SUPPORT
#include <kdom/core/AttrImpl.h>

#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGEllipseElementImpl.h"
#include "SVGAnimatedLengthImpl.h"

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasCreator.h>

namespace KSVG {

SVGEllipseElementImpl::SVGEllipseElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc)
: SVGStyledTransformableElementImpl(tagName, doc), SVGTestsImpl(), SVGLangSpaceImpl(), SVGExternalResourcesRequiredImpl()
{
}

SVGEllipseElementImpl::~SVGEllipseElementImpl()
{
}

SVGAnimatedLengthImpl *SVGEllipseElementImpl::cx() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_cx, this, LM_WIDTH, viewportElement());
}

SVGAnimatedLengthImpl *SVGEllipseElementImpl::cy() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_cy, this, LM_HEIGHT, viewportElement());
}

SVGAnimatedLengthImpl *SVGEllipseElementImpl::rx() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_rx, this, LM_HEIGHT, viewportElement());
}

SVGAnimatedLengthImpl *SVGEllipseElementImpl::ry() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_ry, this, LM_HEIGHT, viewportElement());
}

void SVGEllipseElementImpl::parseMappedAttribute(KDOM::MappedAttributeImpl *attr)
{
    const KDOM::AtomicString& value = attr->value();
    if (attr->name() == SVGNames::cxAttr)
        cx()->baseVal()->setValueAsString(value.impl());
    if (attr->name() == SVGNames::cyAttr)
        cy()->baseVal()->setValueAsString(value.impl());
    if (attr->name() == SVGNames::rxAttr)
        rx()->baseVal()->setValueAsString(value.impl());
    if (attr->name() == SVGNames::ryAttr)
        ry()->baseVal()->setValueAsString(value.impl());
    else
    {
        if(SVGTestsImpl::parseMappedAttribute(attr)) return;
        if(SVGLangSpaceImpl::parseMappedAttribute(attr)) return;
        if(SVGExternalResourcesRequiredImpl::parseMappedAttribute(attr)) return;
        SVGStyledTransformableElementImpl::parseMappedAttribute(attr);
    }
}

KCanvasPath* SVGEllipseElementImpl::toPathData() const
{
    float _cx = cx()->baseVal()->value(), _cy = cy()->baseVal()->value();
    float _rx = rx()->baseVal()->value(), _ry = ry()->baseVal()->value();

    return KCanvasCreator::self()->createEllipse(_cx, _cy, _rx, _ry);
}

const SVGStyledElementImpl *SVGEllipseElementImpl::pushAttributeContext(const SVGStyledElementImpl *context)
{
    // All attribute's contexts are equal (so just take the one from 'cx').
    const SVGStyledElementImpl *restore = cx()->baseVal()->context();

    cx()->baseVal()->setContext(context);
    cy()->baseVal()->setContext(context);
    rx()->baseVal()->setContext(context);
    ry()->baseVal()->setContext(context);

    SVGStyledElementImpl::pushAttributeContext(context);
    return restore;
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT

