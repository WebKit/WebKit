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

#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGLineElementImpl.h"
#include "SVGAnimatedLengthImpl.h"

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasCreator.h>

using namespace KSVG;

SVGLineElementImpl::SVGLineElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc)
: SVGStyledTransformableElementImpl(tagName, doc), SVGTestsImpl(), SVGLangSpaceImpl(), SVGExternalResourcesRequiredImpl()
{
}

SVGLineElementImpl::~SVGLineElementImpl()
{
}

SVGAnimatedLengthImpl *SVGLineElementImpl::x1() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_x1, this, LM_WIDTH, viewportElement());
}

SVGAnimatedLengthImpl *SVGLineElementImpl::y1() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_y1, this, LM_HEIGHT, viewportElement());
}

SVGAnimatedLengthImpl *SVGLineElementImpl::x2() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_x2, this, LM_HEIGHT, viewportElement());
}

SVGAnimatedLengthImpl *SVGLineElementImpl::y2() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_y2, this, LM_HEIGHT, viewportElement());
}

void SVGLineElementImpl::parseMappedAttribute(KDOM::MappedAttributeImpl *attr)
{
    const KDOM::AtomicString& value = attr->value();
    if (attr->name() == SVGNames::x1Attr)
        x1()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::y1Attr)
        y1()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::x2Attr)
        x2()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::y2Attr)
        y2()->baseVal()->setValueAsString(value.impl());
    else
    {
        if(SVGTestsImpl::parseMappedAttribute(attr)) return;
        if(SVGLangSpaceImpl::parseMappedAttribute(attr)) return;
        if(SVGExternalResourcesRequiredImpl::parseMappedAttribute(attr)) return;
        SVGStyledTransformableElementImpl::parseMappedAttribute(attr);
    }
}

KCPathDataList SVGLineElementImpl::toPathData() const
{
    float _x1 = x1()->baseVal()->value(), _y1 = y1()->baseVal()->value();
    float _x2 = x2()->baseVal()->value(), _y2 = y2()->baseVal()->value();

    return KCanvasCreator::self()->createLine(_x1, _y1, _x2, _y2);
}

const SVGStyledElementImpl *SVGLineElementImpl::pushAttributeContext(const SVGStyledElementImpl *context)
{
    // All attribute's contexts are equal (so just take the one from 'x1').
    const SVGStyledElementImpl *restore = x1()->baseVal()->context();

    x1()->baseVal()->setContext(context);
    y1()->baseVal()->setContext(context);
    x2()->baseVal()->setContext(context);
    y2()->baseVal()->setContext(context);
    
    SVGStyledElementImpl::pushAttributeContext(context);
    return restore;
}

// vim:ts=4:noet
