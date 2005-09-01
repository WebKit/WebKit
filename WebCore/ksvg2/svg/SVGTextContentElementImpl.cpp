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

#include <kdom/impl/AttrImpl.h>

#include "svgattrs.h"
#include "SVGHelper.h"
//#include "SVGDocument.h"
#include "SVGTextContentElementImpl.h"
#include "SVGAnimatedLengthImpl.h"
#include "SVGAnimatedEnumerationImpl.h"

using namespace KSVG;

SVGTextContentElementImpl::SVGTextContentElementImpl(KDOM::DocumentPtr *doc, KDOM::NodeImpl::Id id, KDOM::DOMStringImpl *prefix)
: SVGStyledElementImpl(doc, id, prefix), SVGTestsImpl(), SVGLangSpaceImpl(), SVGExternalResourcesRequiredImpl()
{
    m_textLength = 0;
    m_lengthAdjust = 0;
}

SVGTextContentElementImpl::~SVGTextContentElementImpl()
{
    if(m_textLength)
        m_textLength->deref();
    if(m_lengthAdjust)
        m_lengthAdjust->deref();
}

SVGAnimatedLengthImpl *SVGTextContentElementImpl::textLength() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_textLength, this, LM_WIDTH);
}

SVGAnimatedEnumerationImpl *SVGTextContentElementImpl::lengthAdjust() const
{
    return lazy_create<SVGAnimatedEnumerationImpl>(m_lengthAdjust, this);
}

long SVGTextContentElementImpl::getNumberOfChars() const
{
    return 0;
}

float SVGTextContentElementImpl::getComputedTextLength() const
{
    return 0.;
}

float SVGTextContentElementImpl::getSubStringLength(unsigned long charnum, unsigned long nchars) const
{
    return 0.;
}

SVGPointImpl *SVGTextContentElementImpl::getStartPositionOfChar(unsigned long charnum) const
{
    return 0;
}

SVGPointImpl *SVGTextContentElementImpl::getEndPositionOfChar(unsigned long charnum) const
{
    return 0;
}

SVGRectImpl *SVGTextContentElementImpl::getExtentOfChar(unsigned long charnum) const
{
    return 0;
}

float SVGTextContentElementImpl::getRotationOfChar(unsigned long charnum) const
{
    return 0.;
}

long SVGTextContentElementImpl::getCharNumAtPosition(SVGPointImpl *point) const
{
    return 0;
}

void SVGTextContentElementImpl::selectSubString(unsigned long charnum, unsigned long nchars) const
{
}

void SVGTextContentElementImpl::parseAttribute(KDOM::AttributeImpl *attr)
{
    int id = (attr->id() & NodeImpl_IdLocalMask);
    KDOM::DOMString value(attr->value());
    switch(id)
    {
        case ATTR_LENGTHADJUST:
        {
            //x()->baseVal()->setValueAsString(value);
            break;
        }
        default:
        {
            if(SVGTestsImpl::parseAttribute(attr)) return;
            if(SVGLangSpaceImpl::parseAttribute(attr)) return;
            if(SVGExternalResourcesRequiredImpl::parseAttribute(attr)) return;

            SVGStyledElementImpl::parseAttribute(attr);
        }
    };
}

// vim:ts=4:noet
