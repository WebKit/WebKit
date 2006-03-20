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
#include "SVGMatrix.h"
#include "SVGTransform.h"
#include "SVGSVGElement.h"
#include "SVGTransformList.h"

using namespace WebCore;

SVGTransformList::SVGTransformList(const SVGStyledElement *context)
: SVGList<SVGTransform>(context)
{
}

SVGTransformList::~SVGTransformList()
{
}

SVGTransform *SVGTransformList::createSVGTransformFromMatrix(SVGMatrix *matrix) const
{
    return SVGSVGElement::createSVGTransformFromMatrix(matrix);
}

SVGTransform *SVGTransformList::consolidate()
{
    SVGTransform *obj = concatenate();
    if(!obj)
        return 0;

    // Disable notifications here...
    const SVGStyledElement *savedContext = m_context;

    m_context = 0;
    SVGTransform *ret = initialize(obj);
    m_context = savedContext;
    
    return ret;
}

SVGTransform *SVGTransformList::concatenate() const
{
    unsigned int length = numberOfItems();
    if(!length)
        return 0;
        
    SVGTransform *obj = SVGSVGElement::createSVGTransform();
    SVGMatrix *matrix = SVGSVGElement::createSVGMatrix();

    for(unsigned int i = 0; i < length; i++)
        matrix->multiply(getItem(i)->matrix());

    obj->setMatrix(matrix);
    return obj;
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

