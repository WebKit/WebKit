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
#include "DocumentImpl.h"

#include <kcanvas/KCanvas.h>
#include <kcanvas/device/KRenderingPaintServerGradient.h>

#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGRenderStyle.h"
#include "SVGStopElementImpl.h"
#include "SVGAnimatedNumberImpl.h"
#include "SVGDOMImplementationImpl.h"

using namespace KSVG;

SVGStopElementImpl::SVGStopElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc) : SVGStyledElementImpl(tagName, doc)
{
}

SVGStopElementImpl::~SVGStopElementImpl()
{
}

SVGAnimatedNumberImpl *SVGStopElementImpl::offset() const
{
    return lazy_create<SVGAnimatedNumberImpl>(m_offset, this);
}

void SVGStopElementImpl::parseMappedAttribute(KDOM::MappedAttributeImpl *attr)
{
    KDOM::DOMString value(attr->value());
    if (attr->name() == SVGNames::offsetAttr) {
        if(value.qstring().endsWith(QString::fromLatin1("%")))
            offset()->setBaseVal(value.qstring().left(value.length() - 1).toDouble() / 100.);
        else
            offset()->setBaseVal(value.qstring().toDouble());
    } else
        SVGStyledElementImpl::parseMappedAttribute(attr);

    if(!ownerDocument()->parsing() && attached())
    {
        recalcStyle(Force);
        
        SVGStyledElementImpl *parentStyled = static_cast<SVGStyledElementImpl *>(parentNode());
        if(parentStyled)
            parentStyled->notifyAttributeChange();
    }
}

// vim:ts=4:noet
